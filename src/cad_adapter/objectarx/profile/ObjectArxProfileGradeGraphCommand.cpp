#include "cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/profile/ProfileGradeGraphCreateService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "cad_adapter/objectarx/terrain/DnTerrainTinEntity.h"
#include "domain/profile/ProfileDmxFile.h"

#include "aced.h"
#include "adscodes.h"
#include "acutmem.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>
#endif

namespace roadproto::cad_adapter::objectarx::profile {
namespace {

#ifndef ROADPROTO_TEST_BUILD

using roadproto::application::profile::ProfileGradeGraphCreateInput;
using roadproto::application::profile::ProfileGradeGraphCreateService;
using roadproto::domain::alignment::HorizontalAlignment;
using roadproto::domain::alignment::RoadCenterlineProperties;
using roadproto::domain::profile::ProfileDmxFile;
using roadproto::domain::profile::ProfileGroundSample;
using roadproto::domain::profile::ProfileGroundSourceType;

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

bool readRoadCenterline(
    AcDbObjectId entityId,
    HorizontalAlignment& alignment,
    RoadCenterlineProperties& properties,
    std::wstring& centerlineHandle)
{
    DnRoadCenterlineEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }

    alignment = entity->alignment();
    properties = entity->properties();
    centerlineHandle = entityHandleText(entity);
    entity->close();
    return true;
}

bool collectTerrainTinSamples(
    const HorizontalAlignment& alignment,
    const RoadCenterlineProperties& properties,
    std::vector<ProfileGroundSample>& samples,
    std::wstring& terrainTinHandle)
{
    if (!properties.linkedTerrainEnabled || properties.linkedTerrainHandle.empty()) {
        return false;
    }

    AcDbObjectId terrainId;
    if (!resolveObjectIdFromHandle(properties.linkedTerrainHandle, terrainId)) {
        return false;
    }

    DnTerrainTinEntity* terrain = nullptr;
    if (acdbOpenObject(terrain, terrainId, AcDb::kForRead) != Acad::eOk || terrain == nullptr) {
        return false;
    }

    samples.clear();
    for (const auto& element : alignment.elements) {
        for (const auto& samplePoint : element.samples) {
            double elevation = 0.0;
            if (!terrain->SampleElevation(samplePoint.point.x, samplePoint.point.y, elevation)) {
                continue;
            }

            ProfileGroundSample sample;
            sample.station = samplePoint.station;
            sample.elevation = elevation;
            samples.push_back(std::move(sample));
        }
    }

    terrainTinHandle = entityHandleText(terrain);
    terrain->close();
    if (samples.size() < 2) {
        samples.clear();
        terrainTinHandle.clear();
        return false;
    }

    return true;
}

bool promptDmxFilePath(std::wstring& path, roadproto::cad_adapter::IEditor& editor)
{
    resbuf* result = acutNewRb(RTSTR);
    if (result == nullptr) {
        editor.writeError(L"无法创建 DMX 文件选择结果缓冲。");
        return false;
    }
    result->resval.rstring = nullptr;

    const auto status = acedGetFileD(L"选择纵断面地面线 DMX 文件", L"", L"dmx", 2, result);
    if (status != RTNORM || result->resval.rstring == nullptr || result->resval.rstring[0] == L'\0') {
        acutRelRb(result);
        editor.writeWarning(L"已取消 DMX 文件选择。");
        return false;
    }

    path = result->resval.rstring;
    acutRelRb(result);
    return true;
}

bool promptInsertionPoint(AcGePoint3d& insertionPoint)
{
    ads_point raw;
    if (acedGetPoint(nullptr, L"\n请选择纵断面拉坡图插入位置: ", raw) != RTNORM) {
        return false;
    }

    insertionPoint = AcGePoint3d(raw[0], raw[1], raw[2]);
    return true;
}

bool loadDmxSamples(
    std::wstring& dmxFilePath,
    std::vector<ProfileGroundSample>& samples,
    roadproto::cad_adapter::IEditor& editor)
{
    if (!promptDmxFilePath(dmxFilePath, editor)) {
        return false;
    }

    const auto parsed = ProfileDmxFile::read(dmxFilePath);
    if (!parsed.succeeded) {
        editor.writeError(parsed.errorMessage.empty() ? L"DMX 文件解析失败。" : parsed.errorMessage);
        return false;
    }

    samples = parsed.samples;
    return true;
}

std::wstring sourceTypeText(ProfileGroundSourceType sourceType)
{
    return sourceType == ProfileGroundSourceType::TerrainTin
        ? L"关联地形 TIN"
        : L"DMX 文件";
}

void runProfileGradeGraphCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_PROFILE_GRADE_GRAPH_CREATE: 请选择道路中线自定义实体。");

    AcDbObjectId centerlineId;
    if (!selectRoadCenterlineEntity(centerlineId)) {
        editor.writeWarning(L"未选择道路中线自定义实体。");
        return;
    }

    HorizontalAlignment alignment;
    RoadCenterlineProperties centerlineProperties;
    std::wstring centerlineHandle;
    if (!readRoadCenterline(centerlineId, alignment, centerlineProperties, centerlineHandle)) {
        editor.writeError(L"无法打开道路中线实体。");
        return;
    }

    ProfileGroundSourceType sourceType = ProfileGroundSourceType::DmxFile;
    std::wstring terrainTinHandle;
    std::wstring dmxFilePath;
    std::vector<ProfileGroundSample> groundSamples;
    if (collectTerrainTinSamples(alignment, centerlineProperties, groundSamples, terrainTinHandle)) {
        sourceType = ProfileGroundSourceType::TerrainTin;
    } else if (!loadDmxSamples(dmxFilePath, groundSamples, editor)) {
        return;
    }

    AcGePoint3d insertionPoint;
    if (!promptInsertionPoint(insertionPoint)) {
        editor.writeWarning(L"纵断面拉坡图创建已取消。");
        return;
    }

    ProfileGradeGraphCreateInput input;
    input.sourceType = sourceType;
    input.roadName = centerlineProperties.roadName;
    input.roadCenterlineHandle = centerlineHandle;
    input.terrainTinHandle = sourceType == ProfileGroundSourceType::TerrainTin ? terrainTinHandle : L"";
    input.dmxFilePath = sourceType == ProfileGroundSourceType::DmxFile ? dmxFilePath : L"";
    input.groundSamples = std::move(groundSamples);

    const ProfileGradeGraphCreateService service;
    const auto buildResult = service.build(input);
    if (!buildResult.succeeded) {
        editor.writeError(buildResult.errorMessage.empty() ? L"纵断面拉坡图数据生成失败。" : buildResult.errorMessage);
        return;
    }

    auto* entity = new DnProfileGradeGraphEntity();
    entity->setGraphData(buildResult.graph);
    entity->setInsertionPoint(insertionPoint);

    AcDbObjectId graphId;
    if (!appendEntityToModelSpace(entity, graphId)) {
        delete entity;
        editor.writeError(L"插入 DnProfileGradeGraphEntity 失败。");
        return;
    }

    const auto graphHandle = entityHandleText(entity);
    entity->close();
    acedUpdateDisplay();

    std::wstringstream stream;
    stream << L"已创建纵断面拉坡图，handle: " << graphHandle
           << L"，样本数量: " << buildResult.graph.groundSamples.size()
           << L"，数据来源: " << sourceTypeText(sourceType);
    if (sourceType == ProfileGroundSourceType::TerrainTin) {
        stream << L"，地形 handle: " << terrainTinHandle;
    } else {
        stream << L"，DMX: " << dmxFilePath;
    }
    stream << L"。";
    editor.writeMessage(stream.str());
}

void runProfileGradeGraphEditHandleCommand()
{
    app::ApplicationContext::instance().editor().writeMessage(
        L"RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE: 属性编辑入口将在后续任务实现。");
}

void runProfileApplyDialogFileCommand()
{
    app::ApplicationContext::instance().editor().writeMessage(
        L"RD_PROFILE_APPLY_DIALOG_FILE: WPF 对话框回写入口将在后续任务实现。");
}

#else

void runProfileGradeGraphCreateCommand()
{
}

void runProfileGradeGraphEditHandleCommand()
{
}

void runProfileApplyDialogFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure profileGradeGraphCreateCommandProcedure()
{
    return &runProfileGradeGraphCreateCommand;
}

core::CommandProcedure profileGradeGraphEditHandleCommandProcedure()
{
    return &runProfileGradeGraphEditHandleCommand;
}

core::CommandProcedure profileApplyDialogFileCommandProcedure()
{
    return &runProfileApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::profile
