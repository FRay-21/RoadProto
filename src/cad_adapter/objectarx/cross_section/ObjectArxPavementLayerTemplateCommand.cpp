#include "cad_adapter/objectarx/cross_section/ObjectArxPavementLayerTemplateCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/cross_section/PavementLayerTemplateCreateService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.h"

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

using roadproto::application::cross_section::PavementLayerTemplateCreateInput;
using roadproto::application::cross_section::PavementLayerTemplateCreateService;

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
    if (acedGetPoint(nullptr, L"\n请选择路面结构层模板插入位置: ", raw) != RTNORM) {
        return false;
    }
    insertionPoint = AcGePoint3d(raw[0], raw[1], raw[2]);
    return true;
}

bool queueDialogForPavementLayerTemplate(AcDbObjectId entityId)
{
    auto& editor = app::ApplicationContext::instance().editor();

    DnPavementLayerTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开路面结构层模板实体。");
        return false;
    }

    PavementLayerTemplateDialogRequest request;
    request.handle = entityHandleText(entity);
    request.insertionPoint = entity->insertionPoint();
    request.data = entity->templateData();
    entity->close();

    std::wstring errorMessage;
    if (!queuePavementLayerTemplateWpfDialog(request, errorMessage)) {
        editor.writeError(L"打开路面结构层模板 WPF 参数窗口失败: " + errorMessage);
        return false;
    }
    return true;
}

void writeCreatedMessage(
    roadproto::cad_adapter::IEditor& editor,
    const std::wstring& handle,
    const roadproto::domain::cross_section::PavementLayerTemplateData& data)
{
    std::wstringstream stream;
    stream << L"已创建路面结构层模板实体，handle: " << handle
           << L"，结构层数: " << data.layers.size()
           << L"，显示比例 1:" << data.properties.displayScale << L"。";
    editor.writeMessage(stream.str());
}

void runPavementLayerTemplateCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE: 正在打开路面结构层创建向导。");

    PavementLayerTemplateCreateInput input;
    const PavementLayerTemplateCreateService service;
    const auto result = service.create(input);
    if (!result.succeeded) {
        editor.writeError(result.errorMessage.empty() ? L"路面结构层模板默认数据生成失败。" : result.errorMessage);
        return;
    }

    std::wstring errorMessage;
    PavementLayerTemplateDialogRequest request;
    request.showCreateWizard = true;
    request.data = result.templateData;
    if (!queuePavementLayerTemplateWpfDialog(request, errorMessage)) {
        editor.writeError(L"打开路面结构层模板 WPF 参数窗口失败: " + errorMessage);
    }
}

void runPavementLayerTemplateEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR handleBuffer[64] = {};
    if (acedGetString(Adesk::kFalse, L"\n路面结构层模板 handle: ", handleBuffer) != RTNORM) {
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handleBuffer, entityId)) {
        editor.writeWarning(L"未找到 handle 对应的路面结构层模板实体。");
        return;
    }

    DnPavementLayerTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开路面结构层模板实体。");
        return;
    }
    const auto isPavementLayerTemplate = entity->isKindOf(DnPavementLayerTemplateEntity::desc());
    entity->close();
    if (!isPavementLayerTemplate) {
        editor.writeWarning(L"handle 对应对象不是路面结构层模板实体。");
        return;
    }

    queueDialogForPavementLayerTemplate(entityId);
}

void runPavementLayerTemplateApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto pavement layer template dialog response file: ", pathBuffer) != RTNORM) {
        return;
    }

    PavementLayerTemplateDialogResponse response;
    std::wstring errorMessage;
    const auto responsePath = trimDialogCommandPath(pathBuffer);
    if (!readPavementLayerTemplateDialogResponse(responsePath, response, errorMessage)) {
        editor.writeError(L"读取路面结构层模板对话框结果失败: " + errorMessage);
        return;
    }

    if (!response.accepted) {
        return;
    }

    if (response.handle.empty()) {
        if (!promptInsertionPoint(response.insertionPoint)) {
            editor.writeWarning(L"路面结构层模板创建已取消。");
            return;
        }

        auto* entity = new DnPavementLayerTemplateEntity();
        if (entity->setTemplateData(response.data) != Acad::eOk) {
            delete entity;
            editor.writeError(L"路面结构层模板对话框结果无效。");
            return;
        }
        entity->setInsertionPoint(response.insertionPoint);

        AcDbObjectId entityId;
        if (!appendEntityToModelSpace(entity, entityId)) {
            delete entity;
            editor.writeError(L"插入 DnPavementLayerTemplateEntity 失败。");
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
        editor.writeWarning(L"未找到对话框结果对应的路面结构层模板实体。");
        return;
    }

    DnPavementLayerTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开路面结构层模板实体。");
        return;
    }
    if (!entity->isKindOf(DnPavementLayerTemplateEntity::desc())) {
        entity->close();
        editor.writeWarning(L"handle 对应对象不是路面结构层模板实体。");
        return;
    }

    if (entity->setTemplateData(response.data) != Acad::eOk) {
        entity->close();
        editor.writeError(L"路面结构层模板对话框结果无效。");
        return;
    }
    entity->setInsertionPoint(response.insertionPoint);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"路面结构层模板参数已更新。");
}

#else

void runPavementLayerTemplateCreateCommand()
{
}

void runPavementLayerTemplateEditHandleCommand()
{
}

void runPavementLayerTemplateApplyDialogFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure pavementLayerTemplateCreateCommandProcedure()
{
    return &runPavementLayerTemplateCreateCommand;
}

core::CommandProcedure pavementLayerTemplateEditHandleCommandProcedure()
{
    return &runPavementLayerTemplateEditHandleCommand;
}

core::CommandProcedure pavementLayerTemplateApplyDialogFileCommandProcedure()
{
    return &runPavementLayerTemplateApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
