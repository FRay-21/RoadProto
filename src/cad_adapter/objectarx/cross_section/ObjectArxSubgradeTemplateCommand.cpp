#include "cad_adapter/objectarx/cross_section/ObjectArxSubgradeTemplateCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/cross_section/SubgradeTemplateCreateService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/SubgradeTemplateDialogBridge.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <cwctype>
#include <sstream>
#include <string>
#endif

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

#ifndef ROADPROTO_TEST_BUILD

using roadproto::application::cross_section::SubgradeTemplateCreateInput;
using roadproto::application::cross_section::SubgradeTemplateCreateService;

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

bool promptInsertionPoint(AcGePoint3d& insertionPoint)
{
    ads_point raw;
    if (acedGetPoint(nullptr, L"\n请选择路基模板实体插入位置: ", raw) != RTNORM) {
        return false;
    }

    insertionPoint = AcGePoint3d(raw[0], raw[1], raw[2]);
    return true;
}

bool promptPavementLayerTemplateForSubgrade(std::wstring& pavementLayerHandle, std::wstring& pavementLayerName)
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"请选择路面结构层模板实体。");

    ads_name entityName;
    ads_point pickedPoint;
    if (acedEntSel(L"\n请选择路面结构层模板实体: ", entityName, pickedPoint) != RTNORM) {
        editor.writeWarning(L"未选择路面结构层模板实体。");
        return false;
    }

    AcDbObjectId entityId;
    if (acdbGetObjectId(entityId, entityName) != Acad::eOk || entityId.isNull()) {
        editor.writeWarning(L"无法识别所选路面结构层模板实体。");
        return false;
    }

    AcDbEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开路面结构层模板实体。");
        return false;
    }
    if (!entity->isKindOf(DnPavementLayerTemplateEntity::desc())) {
        entity->close();
        editor.writeWarning(L"选择对象不是 RoadProto 路面结构层模板实体。");
        return false;
    }

    auto* pavementEntity = static_cast<DnPavementLayerTemplateEntity*>(entity);
    pavementLayerHandle = entityHandleText(pavementEntity);
    pavementLayerName = pavementEntity->templateData().properties.name.empty()
        ? pavementLayerHandle
        : pavementEntity->templateData().properties.name;
    pavementEntity->close();
    return !pavementLayerHandle.empty();
}

bool queueDialogForSubgradeTemplate(AcDbObjectId entityId)
{
    auto& editor = app::ApplicationContext::instance().editor();

    DnSubgradeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开路基模板实体。");
        return false;
    }

    SubgradeTemplateDialogRequest request;
    request.handle = entityHandleText(entity);
    request.insertionPoint = entity->insertionPoint();
    request.data = entity->templateData();
    entity->close();

    std::wstring errorMessage;
    if (!queueSubgradeTemplateWpfDialog(request, errorMessage)) {
        editor.writeError(L"打开路基模板 WPF 参数窗口失败: " + errorMessage);
        return false;
    }

    return true;
}

bool handlePickPavementLayerTemplateAction(const SubgradeTemplateDialogResponse& response)
{
    auto& editor = app::ApplicationContext::instance().editor();

    SubgradeTemplateDialogRequest request;
    request.handle = response.handle;
    request.insertionPoint = response.insertionPoint;
    request.data = response.data;
    request.pickComponentIndex = response.pickComponentIndex;

    if (response.pickComponentIndex < 0
        || response.pickComponentIndex >= static_cast<int>(request.data.components.size())) {
        editor.writeWarning(L"路面结构层模板点选部件索引无效，已重新打开路基模板窗口。");
    } else {
        std::wstring pavementLayerHandle;
        std::wstring pavementLayerName;
        if (promptPavementLayerTemplateForSubgrade(pavementLayerHandle, pavementLayerName)) {
            auto& component = request.data.components[static_cast<std::size_t>(response.pickComponentIndex)];
            component.pavementLayerLinked = true;
            component.pavementLayerHandle = pavementLayerHandle;
            component.pavementLayerName = pavementLayerName;
        }
    }

    std::wstring errorMessage;
    if (!queueSubgradeTemplateWpfDialog(request, errorMessage)) {
        editor.writeError(L"重新打开路基模板 WPF 参数窗口失败: " + errorMessage);
        return false;
    }
    return true;
}

void writeCreatedMessage(
    roadproto::cad_adapter::IEditor& editor,
    const std::wstring& handle,
    const roadproto::domain::cross_section::SubgradeTemplateData& data)
{
    std::wstringstream stream;
    stream << L"已创建路基模板实体，handle: " << handle
           << L"，部件数: " << data.components.size()
           << L"，显示比例 1:" << data.properties.displayScale << L"。";
    editor.writeMessage(stream.str());
}

void runSubgradeTemplateCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_SECTION_SUBGRADE_TEMPLATE_CREATE: 请选择路基模板实体插入点。");

    AcGePoint3d insertionPoint;
    if (!promptInsertionPoint(insertionPoint)) {
        editor.writeWarning(L"路基模板创建已取消。");
        return;
    }

    SubgradeTemplateCreateInput input;
    const SubgradeTemplateCreateService service;
    const auto result = service.create(input);
    if (!result.succeeded) {
        editor.writeError(result.errorMessage.empty() ? L"路基模板默认数据生成失败。" : result.errorMessage);
        return;
    }

    SubgradeTemplateDialogRequest request;
    request.insertionPoint = insertionPoint;
    request.data = result.templateData;

    std::wstring errorMessage;
    if (!queueSubgradeTemplateWpfDialog(request, errorMessage)) {
        editor.writeError(L"打开路基模板 WPF 参数窗口失败: " + errorMessage);
        return;
    }
}

void runSubgradeTemplateEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR handleBuffer[64] = {};
    if (acedGetString(Adesk::kFalse, L"\n路基模板 handle: ", handleBuffer) != RTNORM) {
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handleBuffer, entityId)) {
        editor.writeWarning(L"未找到 handle 对应的路基模板实体。");
        return;
    }

    DnSubgradeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开路基模板实体。");
        return;
    }
    const auto isSubgradeTemplate = entity->isKindOf(DnSubgradeTemplateEntity::desc());
    entity->close();
    if (!isSubgradeTemplate) {
        editor.writeWarning(L"handle 对应对象不是路基模板实体。");
        return;
    }

    queueDialogForSubgradeTemplate(entityId);
}

void runSubgradeTemplateApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto subgrade template dialog response file: ", pathBuffer) != RTNORM) {
        return;
    }

    SubgradeTemplateDialogResponse response;
    std::wstring errorMessage;
    const auto responsePath = trimDialogCommandPath(pathBuffer);
    if (!readSubgradeTemplateDialogResponse(responsePath, response, errorMessage)) {
        editor.writeError(L"读取路基模板对话框结果失败: " + errorMessage);
        return;
    }

    if (response.action == SubgradeTemplateDialogAction::PickPavementLayerTemplate) {
        handlePickPavementLayerTemplateAction(response);
        return;
    }

    if (!response.accepted) {
        return;
    }

    if (response.handle.empty()) {
        auto* entity = new DnSubgradeTemplateEntity();
        entity->setTemplateData(response.data);
        entity->setInsertionPoint(response.insertionPoint);

        AcDbObjectId entityId;
        if (!appendEntityToModelSpace(entity, entityId)) {
            delete entity;
            editor.writeError(L"插入 DnSubgradeTemplateEntity 失败。");
            return;
        }

        const auto handle = entityHandleText(entity);
        entity->close();
        acedUpdateDisplay();
        writeCreatedMessage(editor, handle, response.data);
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(response.handle, entityId)) {
        editor.writeWarning(L"未找到对话框结果对应的路基模板实体。");
        return;
    }

    DnSubgradeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开路基模板实体。");
        return;
    }
    if (!entity->isKindOf(DnSubgradeTemplateEntity::desc())) {
        entity->close();
        editor.writeWarning(L"handle 对应对象不是路基模板实体。");
        return;
    }

    entity->setTemplateData(response.data);
    entity->setInsertionPoint(response.insertionPoint);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"路基模板参数已更新。");
}

#else

void runSubgradeTemplateCreateCommand()
{
}

void runSubgradeTemplateEditHandleCommand()
{
}

void runSubgradeTemplateApplyDialogFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure subgradeTemplateCreateCommandProcedure()
{
    return &runSubgradeTemplateCreateCommand;
}

core::CommandProcedure subgradeTemplateEditHandleCommandProcedure()
{
    return &runSubgradeTemplateEditHandleCommand;
}

core::CommandProcedure subgradeTemplateApplyDialogFileCommandProcedure()
{
    return &runSubgradeTemplateApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
