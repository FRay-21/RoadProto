#include "cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.h"

#include "app/startup/ApplicationContext.h"
#include "application/alignment/AlignmentCreateService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/alignment/AlignmentDialogBridge.h"
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"
#include "cad_adapter/objectarx/terrain/DnTerrainTinEntity.h"
#include "domain/alignment/IcdAlignmentFile.h"

#include "aced.h"
#include "adscodes.h"
#include "acutmem.h"
#include "dbapserv.h"
#include "dbjig.h"
#include "dbsymtb.h"

#include <algorithm>
#include <cwctype>
#include <cmath>
#include <filesystem>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

namespace roadproto::cad_adapter::objectarx {
namespace {

using domain::alignment::AlignmentPoint2d;
using domain::alignment::HorizontalAlignmentInput;
using domain::alignment::HorizontalCurveParameters;
using domain::alignment::IcdAlignmentFile;

AcDbObjectId g_entityHiddenDuringAlignmentPreview;

std::wstring trimDialogCommandPath(std::wstring path)
{
    while (!path.empty() && std::iswspace(path.front()) != 0) {
        path.erase(path.begin());
    }
    while (!path.empty() && std::iswspace(path.back()) != 0) {
        path.pop_back();
    }

    if (path.size() >= 2) {
        const auto first = path.front();
        const auto last = path.back();
        if ((first == L'"' && last == L'"') || (first == L'\'' && last == L'\'')) {
            path = path.substr(1, path.size() - 2);
        }
    }

    return path;
}

bool appendEntityToModelSpace(AcDbEntity* entity, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr) {
        return false;
    }

    AcDbBlockTable* blockTable = nullptr;
    if (database->getBlockTable(blockTable, AcDb::kForRead) != Acad::eOk || blockTable == nullptr) {
        return false;
    }

    AcDbBlockTableRecord* modelSpace = nullptr;
    const auto status = blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForWrite);
    blockTable->close();
    if (status != Acad::eOk || modelSpace == nullptr) {
        return false;
    }

    const auto appendStatus = modelSpace->appendAcDbEntity(entityId, entity);
    modelSpace->close();
    return appendStatus == Acad::eOk;
}

int countExistingRoadCenterlines()
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr) {
        return 0;
    }

    AcDbBlockTable* blockTable = nullptr;
    if (database->getBlockTable(blockTable, AcDb::kForRead) != Acad::eOk || blockTable == nullptr) {
        return 0;
    }

    AcDbBlockTableRecord* modelSpace = nullptr;
    const auto status = blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForRead);
    blockTable->close();
    if (status != Acad::eOk || modelSpace == nullptr) {
        return 0;
    }

    int count = 0;
    AcDbBlockTableRecordIterator* iterator = nullptr;
    if (modelSpace->newIterator(iterator) == Acad::eOk && iterator != nullptr) {
        for (; !iterator->done(); iterator->step()) {
            AcDbEntity* entity = nullptr;
            if (iterator->getEntity(entity, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
                continue;
            }
            if (entity->isKindOf(DnRoadCenterlineEntity::desc())) {
                ++count;
            }
            entity->close();
        }
        delete iterator;
    }
    modelSpace->close();
    return count;
}

bool getPoint(
    const wchar_t* prompt,
    AlignmentPoint2d& point,
    bool allowNone = false,
    const AlignmentPoint2d* basePoint = nullptr)
{
    ads_point raw;
    ads_point baseRaw = {};
    const ads_point* baseRawPointer = nullptr;
    if (basePoint != nullptr) {
        baseRaw[0] = basePoint->x;
        baseRaw[1] = basePoint->y;
        baseRaw[2] = 0.0;
        baseRawPointer = &baseRaw;
    }

    const auto status = acedGetPoint(baseRawPointer == nullptr ? nullptr : *baseRawPointer, prompt, raw);
    if (status == RTNONE && allowNone) {
        return false;
    }
    if (status != RTNORM) {
        return false;
    }
    point.x = raw[0];
    point.y = raw[1];
    return true;
}

class AlignmentPreviewJig final : public AcEdJig {
public:
    AlignmentPreviewJig(
        std::vector<AlignmentPoint2d> fixedPoints,
        HorizontalAlignmentInput input,
        const wchar_t* prompt,
        bool allowNull)
        : fixedPoints_(std::move(fixedPoints))
        , input_(std::move(input))
        , prompt_(prompt)
        , allowNull_(allowNull)
    {
        entity_ = std::make_unique<DnRoadCenterlineEntity>();
        setDispPrompt(prompt_.c_str());
        applyInputControls();

        if (fixedPoints_.size() >= 3) {
            auto baseInput = input_;
            baseInput.controlPoints = fixedPoints_;
            application::AlignmentCreateService service;
            const auto result = service.build(baseInput);
            if (result.succeeded) {
                entity_->setAlignment(result.alignment);
            }
        }
    }

    DragStatus sampler() override
    {
        applyInputControls();

        AcGePoint3d raw;
        const auto status = acquirePoint(raw);

        if (status != kNormal) {
            return status;
        }

        AlignmentPoint2d candidate{raw.x, raw.y};
        if (hasPoint_ && domain::alignment::distance(candidate, currentPoint_) < 1e-6) {
            return kNoChange;
        }

        currentPoint_ = candidate;
        hasPoint_ = true;
        return kNormal;
    }

    Adesk::Boolean update() override
    {
        if (!hasPoint_) {
            return Adesk::kTrue;
        }

        auto previewPoints = fixedPoints_;
        previewPoints.push_back(currentPoint_);
        if (previewPoints.size() < 3) {
            return Adesk::kTrue;
        }

        auto input = input_;
        input.controlPoints = previewPoints;
        application::AlignmentCreateService service;
        const auto result = service.build(input);
        if (!result.succeeded) {
            return Adesk::kTrue;
        }

        entity_->setAlignment(result.alignment);
        return Adesk::kTrue;
    }

    AcDbEntity* entity() const override
    {
        return entity_.get();
    }

    AlignmentPoint2d point() const
    {
        return currentPoint_;
    }

private:
    std::vector<AlignmentPoint2d> fixedPoints_;
    HorizontalAlignmentInput input_;
    std::wstring prompt_;
    std::unique_ptr<DnRoadCenterlineEntity> entity_;
    AlignmentPoint2d currentPoint_;
    bool allowNull_ = false;
    bool hasPoint_ = false;

    void applyInputControls()
    {
        auto controls = AcEdJig::kAccept3dCoordinates
            | AcEdJig::kNoZeroResponseAccepted
            | AcEdJig::kGovernedByOrthoMode
            | AcEdJig::kDontUpdateLastPoint;
        if (allowNull_) {
            controls |= AcEdJig::kNullResponseAccepted
                | AcEdJig::kDontEchoCancelForCtrlC
                | AcEdJig::kAnyBlankTerminatesInput
                | AcEdJig::kInitialBlankTerminatesInput;
        }
        setUserInputControls(static_cast<AcEdJig::UserInputControls>(controls));
    }
};

bool setEntityVisibility(AcDbObjectId entityId, AcDb::Visibility visibility);

bool getPointWithAlignmentPreview(
    const std::vector<AlignmentPoint2d>& fixedPoints,
    const HorizontalAlignmentInput& input,
    const wchar_t* prompt,
    AlignmentPoint2d& point,
    bool allowNone)
{
    const auto hideExistingEntity = allowNone && !g_entityHiddenDuringAlignmentPreview.isNull();
    const auto hidden = hideExistingEntity
        ? setEntityVisibility(g_entityHiddenDuringAlignmentPreview, AcDb::kInvisible)
        : false;

    AlignmentPreviewJig jig(fixedPoints, input, prompt, allowNone);
    const auto status = jig.drag();

    if (hidden) {
        setEntityVisibility(g_entityHiddenDuringAlignmentPreview, AcDb::kVisible);
        acedUpdateDisplay();
    }

    if (status == AcEdJig::kNormal) {
        point = jig.point();
        return true;
    }
    if (allowNone
        && (status == AcEdJig::kNull
            || status == AcEdJig::kCancel
            || status == AcEdJig::kOther)) {
        return false;
    }
    return false;
}

bool setEntityVisibility(AcDbObjectId entityId, AcDb::Visibility visibility)
{
    if (entityId.isNull()) {
        return false;
    }

    AcDbEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        return false;
    }

    entity->setVisibility(visibility);
    entity->recordGraphicsModified(true);
    entity->close();
    return true;
}

bool findRoadCenterlineEntityInSelectionSet(const ads_name selectionSet, AcDbObjectId& entityId)
{
    Adesk::Int32 length = 0;
    if (acedSSLength(selectionSet, &length) != RTNORM || length <= 0) {
        return false;
    }

    for (Adesk::Int32 i = 0; i < length; ++i) {
        ads_name entityName;
        if (acedSSName(selectionSet, i, entityName) != RTNORM) {
            continue;
        }

        AcDbObjectId candidateId;
        if (acdbGetObjectId(candidateId, entityName) != Acad::eOk) {
            continue;
        }

        DnRoadCenterlineEntity* entity = nullptr;
        if (acdbOpenObject(entity, candidateId, AcDb::kForRead) == Acad::eOk && entity != nullptr) {
            entity->close();
            entityId = candidateId;
            return true;
        }
    }

    return false;
}

bool findImpliedRoadCenterlineEntity(AcDbObjectId& entityId)
{
    ads_name selectionSet;
    if (acedSSGet(L"_I", nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }

    SelectionSetGuard guard(selectionSet);
    return findRoadCenterlineEntityInSelectionSet(selectionSet, entityId);
}

bool selectRoadCenterlineEntity(AcDbObjectId& entityId)
{
    if (findImpliedRoadCenterlineEntity(entityId)) {
        return true;
    }

    ads_name selectionSet;
    if (acedSSGet(nullptr, nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }
    SelectionSetGuard guard(selectionSet);
    return findRoadCenterlineEntityInSelectionSet(selectionSet, entityId);
}

bool resolveObjectIdFromHandle(const std::wstring& handleText, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr || handleText.empty()) {
        return false;
    }

    const AcDbHandle handle(handleText.c_str());
    return database->getAcDbObjectId(entityId, false, handle) == Acad::eOk && !entityId.isNull();
}

std::wstring entityHandleText(AcDbEntity* entity)
{
    if (entity == nullptr) {
        return L"";
    }
    AcDbHandle handle;
    entity->getAcDbHandle(handle);
    ACHAR handleText[32] = {};
    handle.getIntoAsciiBuffer(handleText);
    return handleText;
}

std::wstring toUpper(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towupper(ch));
    });
    return value;
}

std::wstring ensureIcdExtension(const std::wstring& path)
{
    std::filesystem::path filePath(path);
    if (toUpper(filePath.extension().wstring()) != L".ICD") {
        filePath.replace_extension(L".icd");
    }
    return filePath.wstring();
}

bool promptIcdFilePath(
    bool forWrite,
    const ACHAR* title,
    const ACHAR* defaultFileName,
    std::wstring& path,
    IEditor& editor)
{
    resbuf* result = acutNewRb(RTSTR);
    if (result == nullptr) {
        editor.writeError(L"无法创建 ICD 文件选择结果缓冲。");
        return false;
    }
    result->resval.rstring = nullptr;

    const int flags = 2 + (forWrite ? 1 : 0);
    const auto status = acedGetFileD(title, defaultFileName, L"icd", flags, result);
    if (status != RTNORM || result->resval.rstring == nullptr || result->resval.rstring[0] == L'\0') {
        acutRelRb(result);
        editor.writeWarning(L"已取消 ICD 文件选择。");
        return false;
    }

    path = ensureIcdExtension(result->resval.rstring);
    acutRelRb(result);
    return true;
}

bool nearlyEqual(double left, double right)
{
    return std::fabs(left - right) <= 1e-8;
}

bool curveParametersEqual(const HorizontalCurveParameters& left, const HorizontalCurveParameters& right)
{
    return nearlyEqual(left.tangentIn, right.tangentIn)
        && nearlyEqual(left.tangentOut, right.tangentOut)
        && nearlyEqual(left.ls1, right.ls1)
        && nearlyEqual(left.radius, right.radius)
        && nearlyEqual(left.ls2, right.ls2);
}

bool curveParameterListEqual(
    const std::vector<HorizontalCurveParameters>& left,
    const std::vector<HorizontalCurveParameters>& right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (!curveParametersEqual(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

bool queueDialogForAlignment(
    const std::wstring& handle,
    const roadproto::domain::alignment::HorizontalAlignment& alignment,
    AlignmentDialogMode mode,
    bool deleteOnCancel,
    std::size_t curveIndex = 0)
{
    AlignmentDialogRequest request;
    request.mode = mode;
    request.handle = handle;
    request.alignment = alignment;
    request.curveIndex = curveIndex;
    request.deleteOnCancel = deleteOnCancel;

    std::wstring errorMessage;
    if (!queueAlignmentWpfDialog(request, errorMessage)) {
        app::ApplicationContext::instance().editor().writeError(L"Failed to open alignment WPF dialog: " + errorMessage);
        return false;
    }
    return true;
}

bool queueDialogForEntity(
    AcDbObjectId entityId,
    AlignmentDialogMode mode,
    bool deleteOnCancel = false,
    std::size_t curveIndex = 0)
{
    DnRoadCenterlineEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        app::ApplicationContext::instance().editor().writeError(L"Failed to open road centerline entity.");
        return false;
    }

    const auto handle = entityHandleText(entity);
    const auto alignment = entity->alignment();
    entity->close();
    return queueDialogForAlignment(handle, alignment, mode, deleteOnCancel, curveIndex);
}

void clearImpliedSelection()
{
    acedSSSetFirst(nullptr, nullptr);
}

bool promptTerrainTinHandle(std::wstring& terrainHandle)
{
    auto& editor = app::ApplicationContext::instance().editor();

    ads_name entityName;
    ads_point pickedPoint;
    if (acedEntSel(L"\n请选择关联的 TIN 数模实体: ", entityName, pickedPoint) != RTNORM) {
        editor.writeWarning(L"已取消数模选择。");
        return false;
    }

    AcDbObjectId entityId;
    if (acdbGetObjectId(entityId, entityName) != Acad::eOk) {
        editor.writeWarning(L"无法识别选择对象。");
        return false;
    }

    DnTerrainTinEntity* terrain = nullptr;
    if (acdbOpenObject(terrain, entityId, AcDb::kForRead) != Acad::eOk || terrain == nullptr) {
        editor.writeWarning(L"选择对象不是 RoadProto TIN 数模实体。");
        return false;
    }

    terrainHandle = entityHandleText(terrain);
    terrain->close();
    clearImpliedSelection();
    return true;
}

bool handlePickTerrainAction(
    AcDbObjectId centerlineId,
    const AlignmentDialogResponse& response)
{
    std::wstring terrainHandle = response.properties.linkedTerrainHandle;
    const auto picked = promptTerrainTinHandle(terrainHandle);

    DnRoadCenterlineEntity* entity = nullptr;
    if (acdbOpenObject(entity, centerlineId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        app::ApplicationContext::instance().editor().writeError(L"无法重新打开道路中线实体。");
        return false;
    }

    auto alignment = entity->alignment();
    entity->close();
    alignment.properties = response.properties;
    if (!response.curveParameters.empty()) {
        alignment.curveParameters = response.curveParameters;
    }
    if (picked) {
        alignment.properties.linkedTerrainEnabled = true;
        alignment.properties.linkedTerrainHandle = terrainHandle;
    }

    return queueDialogForAlignment(
        response.handle,
        alignment,
        response.mode,
        response.deleteOnCancel,
        response.curveIndex);
}

bool eraseEntityById(AcDbObjectId entityId)
{
    AcDbEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        return false;
    }
    const auto status = entity->erase();
    entity->close();
    return status == Acad::eOk;
}

void runAlignmentCenterlineCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_ALIGN_CENTERLINE_CREATE: 依次选择路线起点、交点和第三点。");

    std::vector<AlignmentPoint2d> controlPoints;
    controlPoints.reserve(8);

    AlignmentPoint2d point;
    if (!getPoint(L"\n请选择路线起点: ", point)) {
        editor.writeWarning(L"平面布线已取消。");
        return;
    }
    controlPoints.push_back(point);

    if (!getPoint(L"\n请选择路线交点: ", point, false, &controlPoints.back())) {
        editor.writeWarning(L"平面布线已取消。");
        return;
    }
    controlPoints.push_back(point);

    HorizontalAlignmentInput input;
    input.properties.roadName = L"K" + std::to_wstring(countExistingRoadCenterlines() + 1);
    input.properties.stationLabelInterval = 100.0;

    if (!getPointWithAlignmentPreview(controlPoints, input, L"\n请选择第三点: ", point, false)) {
        editor.writeWarning(L"平面布线已取消。");
        return;
    }
    controlPoints.push_back(point);
    input.controlPoints = controlPoints;

    application::AlignmentCreateService service;
    auto buildResult = service.build(input);
    if (!buildResult.succeeded) {
        editor.writeError(L"平面布线失败: " + buildResult.errorMessage);
        return;
    }
    auto latestAlignment = buildResult.alignment;

    auto* entity = new DnRoadCenterlineEntity();
    entity->setAlignment(latestAlignment);

    AcDbObjectId entityId;
    if (!appendEntityToModelSpace(entity, entityId)) {
        delete entity;
        editor.writeError(L"插入 DnRoadCenterlineEntity 失败。");
        return;
    }

    const auto handle = entityHandleText(entity);
    entity->close();
    acedUpdateDisplay();
    g_entityHiddenDuringAlignmentPreview = entityId;

    while (getPointWithAlignmentPreview(controlPoints, input, L"\n继续选择下一控制点，或按 Enter 结束: ", point, true)) {
        controlPoints.push_back(point);
        input.controlPoints = controlPoints;
        buildResult = service.build(input);
        if (!buildResult.succeeded) {
            controlPoints.pop_back();
            input.controlPoints = controlPoints;
            editor.writeWarning(L"控制点已忽略: " + buildResult.errorMessage);
            continue;
        }
        latestAlignment = buildResult.alignment;

        DnRoadCenterlineEntity* writableEntity = nullptr;
        if (acdbOpenObject(writableEntity, entityId, AcDb::kForWrite) != Acad::eOk || writableEntity == nullptr) {
            editor.writeError(L"无法更新道路中线实时预览。");
            g_entityHiddenDuringAlignmentPreview = AcDbObjectId();
            return;
        }
        writableEntity->setAlignment(latestAlignment);
        writableEntity->recordGraphicsModified(true);
        writableEntity->close();
        acedUpdateDisplay();
    }

    g_entityHiddenDuringAlignmentPreview = AcDbObjectId();

    std::wstringstream stream;
    stream << L"已创建道路中线自定义实体，handle: " << handle;
    editor.writeMessage(stream.str());
    queueDialogForAlignment(handle, latestAlignment, AlignmentDialogMode::Full, true);
}

void runAlignmentCurveParamEditCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_ALIGN_CURVE_PARAM_EDIT: 请选择道路中线自定义实体。");

    AcDbObjectId entityId;
    if (!selectRoadCenterlineEntity(entityId)) {
        editor.writeWarning(L"未选择道路中线自定义实体。");
        return;
    }

    queueDialogForEntity(entityId, AlignmentDialogMode::Curve);
}

void runAlignmentCenterlineExportIcdCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_ALIGN_CENTERLINE_EXPORT_ICD: 请选择要导出的道路中线实体。");

    AcDbObjectId entityId;
    if (!selectRoadCenterlineEntity(entityId)) {
        editor.writeWarning(L"未选择道路中线自定义实体。");
        return;
    }

    DnRoadCenterlineEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开道路中线实体。");
        return;
    }

    const auto alignment = entity->alignment();
    entity->close();
    if (alignment.elements.empty()) {
        editor.writeError(L"当前道路中线没有可导出的线形单元。");
        return;
    }

    std::wstring defaultFileName = alignment.properties.roadName.empty()
        ? L"centerline.icd"
        : alignment.properties.roadName + L".icd";
    std::wstring path;
    if (!promptIcdFilePath(true, L"导出道路中线 ICD", defaultFileName.c_str(), path, editor)) {
        return;
    }

    IcdAlignmentFile file;
    std::wstring errorMessage;
    if (!file.write(path, file.documentFromAlignment(alignment), errorMessage)) {
        editor.writeError(errorMessage.empty() ? L"ICD 导出失败。" : errorMessage);
        return;
    }

    std::wstringstream stream;
    stream << L"ICD 已导出: " << path << L"，线形单元 " << alignment.elements.size() << L"。";
    editor.writeMessage(stream.str());
}

void runAlignmentCenterlineImportIcdCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    std::wstring path;
    if (!promptIcdFilePath(false, L"导入道路中线 ICD", L"", path, editor)) {
        return;
    }

    IcdAlignmentFile file;
    const auto loaded = file.read(path);
    if (!loaded.succeeded) {
        editor.writeError(loaded.errorMessage.empty() ? L"ICD 导入失败。" : loaded.errorMessage);
        return;
    }

    domain::alignment::RoadCenterlineProperties properties;
    properties.roadName = std::filesystem::path(path).stem().wstring();
    if (properties.roadName.empty()) {
        properties.roadName = L"ICD";
    }
    properties.stationLabelInterval = 100.0;

    const auto built = file.alignmentFromDocument(loaded.document, properties);
    if (!built.succeeded) {
        editor.writeError(built.errorMessage.empty() ? L"ICD 线形生成失败。" : built.errorMessage);
        return;
    }

    auto* entity = new DnRoadCenterlineEntity();
    entity->setAlignment(built.alignment);

    AcDbObjectId entityId;
    if (!appendEntityToModelSpace(entity, entityId)) {
        delete entity;
        editor.writeError(L"插入 DnRoadCenterlineEntity 失败。");
        return;
    }

    const auto handle = entityHandleText(entity);
    entity->close();
    acedUpdateDisplay();

    std::wstringstream stream;
    stream << L"ICD 已导入: " << path
           << L"，线形单元 " << built.alignment.elements.size()
           << L"，实体 handle: " << handle << L"。";
    editor.writeMessage(stream.str());
}

void runAlignmentCenterlineEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR handleBuffer[64] = {};
    if (acedGetString(Adesk::kFalse, L"\n道路中线 handle: ", handleBuffer) != RTNORM) {
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handleBuffer, entityId)) {
        editor.writeWarning(L"未找到 handle 对应的道路中线实体。");
        return;
    }

    queueDialogForEntity(entityId, AlignmentDialogMode::Full);
}

void runAlignmentApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto alignment dialog response file: ", pathBuffer) != RTNORM) {
        return;
    }

    AlignmentDialogResponse response;
    std::wstring errorMessage;
    const auto responsePath = trimDialogCommandPath(pathBuffer);
    if (!readAlignmentDialogResponse(responsePath, response, errorMessage)) {
        editor.writeError(L"读取道路中线对话框结果失败: " + errorMessage);
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(response.handle, entityId)) {
        editor.writeWarning(L"未找到对话框结果对应的道路中线实体。");
        return;
    }

    if (response.action == AlignmentDialogAction::PickTerrain) {
        handlePickTerrainAction(entityId, response);
        return;
    }

    if (!response.accepted) {
        if (response.deleteOnCancel) {
            eraseEntityById(entityId);
            acedUpdateDisplay();
            editor.writeWarning(L"道路中线创建已取消。");
        }
        return;
    }

    DnRoadCenterlineEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开道路中线实体。");
        return;
    }

    const auto originalAlignment = entity->alignment();
    if (response.mode == AlignmentDialogMode::Properties || response.mode == AlignmentDialogMode::Full) {
        if (response.properties.roadName.empty()) {
            response.properties.roadName = originalAlignment.properties.roadName.empty()
                ? L"K1"
                : originalAlignment.properties.roadName;
        }
        if (response.properties.stationLabelInterval <= 0.0) {
            response.properties.stationLabelInterval = originalAlignment.properties.stationLabelInterval > 0.0
                ? originalAlignment.properties.stationLabelInterval
                : 100.0;
        }
        entity->setProperties(response.properties);
    }

    if (response.mode == AlignmentDialogMode::Curve || response.mode == AlignmentDialogMode::Full) {
        const auto curveParametersChanged = response.curveParameters.empty()
            ? (response.curveIndex >= originalAlignment.curveParameters.size()
                || !curveParametersEqual(response.parameters, originalAlignment.curveParameters[response.curveIndex]))
            : !curveParameterListEqual(response.curveParameters, originalAlignment.curveParameters);
        const auto rebuilt = !curveParametersChanged
            || (response.curveParameters.empty()
                ? entity->rebuildWithCurveParameters(response.parameters, response.curveIndex)
                : entity->rebuildWithCurveParameterList(response.curveParameters));
        if (!rebuilt) {
            entity->setAlignment(originalAlignment);
            entity->close();
            editor.writeError(L"平曲线参数无效，无法重建道路中线。");
            return;
        }
    }

    entity->recordGraphicsModified(true);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"道路中线参数已更新。");
}

} // namespace

core::CommandProcedure alignmentCenterlineCreateCommandProcedure()
{
    return &runAlignmentCenterlineCreateCommand;
}

core::CommandProcedure alignmentCurveParamEditCommandProcedure()
{
    return &runAlignmentCurveParamEditCommand;
}

core::CommandProcedure alignmentCenterlineExportIcdCommandProcedure()
{
    return &runAlignmentCenterlineExportIcdCommand;
}

core::CommandProcedure alignmentCenterlineImportIcdCommandProcedure()
{
    return &runAlignmentCenterlineImportIcdCommand;
}

core::CommandProcedure alignmentCenterlineEditHandleCommandProcedure()
{
    return &runAlignmentCenterlineEditHandleCommand;
}

core::CommandProcedure alignmentApplyDialogFileCommandProcedure()
{
    return &runAlignmentApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx
