#include "cad_adapter/objectarx/cross_section/ObjectArxSlopeTemplateCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/cross_section/SlopeTemplateCreateService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/cross_section/DnSlopeTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/SlopeTemplateDialogBridge.h"

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

using roadproto::application::cross_section::SlopeTemplateCreateInput;
using roadproto::application::cross_section::SlopeTemplateCreateService;

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
    if (acedGetPoint(nullptr, L"\n请选择边坡模板实体插入位置: ", raw) != RTNORM) {
        return false;
    }
    insertionPoint = AcGePoint3d(raw[0], raw[1], raw[2]);
    return true;
}

bool queueDialogForSlopeTemplate(AcDbObjectId entityId)
{
    auto& editor = app::ApplicationContext::instance().editor();

    DnSlopeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开边坡模板实体。");
        return false;
    }

    SlopeTemplateDialogRequest request;
    request.handle = entityHandleText(entity);
    request.insertionPoint = entity->insertionPoint();
    request.data = entity->templateData();
    entity->close();

    std::wstring errorMessage;
    if (!queueSlopeTemplateWpfDialog(request, errorMessage)) {
        editor.writeError(L"打开边坡模板 WPF 参数窗口失败: " + errorMessage);
        return false;
    }
    return true;
}

void writeCreatedMessage(
    roadproto::cad_adapter::IEditor& editor,
    const std::wstring& handle,
    const roadproto::domain::cross_section::SlopeTemplateData& data)
{
    std::wstringstream stream;
    stream << L"已创建边坡模板实体，handle: " << handle
           << L"，部件数: " << data.components.size()
           << L"，显示比例 1:" << data.properties.displayScale << L"。";
    editor.writeMessage(stream.str());
}

void runSlopeTemplateCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_SECTION_SLOPE_TEMPLATE_CREATE: 请选择边坡模板实体插入点。");

    AcGePoint3d insertionPoint;
    if (!promptInsertionPoint(insertionPoint)) {
        editor.writeWarning(L"边坡模板创建已取消。");
        return;
    }

    SlopeTemplateCreateInput input;
    const SlopeTemplateCreateService service;
    const auto result = service.create(input);
    if (!result.succeeded) {
        editor.writeError(result.errorMessage.empty() ? L"边坡模板默认数据生成失败。" : result.errorMessage);
        return;
    }

    SlopeTemplateDialogRequest request;
    request.insertionPoint = insertionPoint;
    request.data = result.templateData;

    std::wstring errorMessage;
    if (!queueSlopeTemplateWpfDialog(request, errorMessage)) {
        editor.writeError(L"打开边坡模板 WPF 参数窗口失败: " + errorMessage);
    }
}

void runSlopeTemplateEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR handleBuffer[64] = {};
    if (acedGetString(Adesk::kFalse, L"\n边坡模板 handle: ", handleBuffer) != RTNORM) {
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handleBuffer, entityId)) {
        editor.writeWarning(L"未找到 handle 对应的边坡模板实体。");
        return;
    }

    DnSlopeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开边坡模板实体。");
        return;
    }
    const auto isSlopeTemplate = entity->isKindOf(DnSlopeTemplateEntity::desc());
    entity->close();
    if (!isSlopeTemplate) {
        editor.writeWarning(L"handle 对应对象不是边坡模板实体。");
        return;
    }

    queueDialogForSlopeTemplate(entityId);
}

void runSlopeTemplateApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto slope template dialog response file: ", pathBuffer) != RTNORM) {
        return;
    }

    SlopeTemplateDialogResponse response;
    std::wstring errorMessage;
    const auto responsePath = trimDialogCommandPath(pathBuffer);
    if (!readSlopeTemplateDialogResponse(responsePath, response, errorMessage)) {
        editor.writeError(L"读取边坡模板对话框结果失败: " + errorMessage);
        return;
    }

    if (!response.accepted) {
        return;
    }

    if (response.handle.empty()) {
        auto* entity = new DnSlopeTemplateEntity();
        entity->setTemplateData(response.data);
        entity->setInsertionPoint(response.insertionPoint);

        AcDbObjectId entityId;
        if (!appendEntityToModelSpace(entity, entityId)) {
            delete entity;
            editor.writeError(L"插入 DnSlopeTemplateEntity 失败。");
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
        editor.writeWarning(L"未找到对话框结果对应的边坡模板实体。");
        return;
    }

    DnSlopeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开边坡模板实体。");
        return;
    }
    if (!entity->isKindOf(DnSlopeTemplateEntity::desc())) {
        entity->close();
        editor.writeWarning(L"handle 对应对象不是边坡模板实体。");
        return;
    }

    entity->setTemplateData(response.data);
    entity->setInsertionPoint(response.insertionPoint);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"边坡模板参数已更新。");
}

#else

void runSlopeTemplateCreateCommand()
{
}

void runSlopeTemplateEditHandleCommand()
{
}

void runSlopeTemplateApplyDialogFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure slopeTemplateCreateCommandProcedure()
{
    return &runSlopeTemplateCreateCommand;
}

core::CommandProcedure slopeTemplateEditHandleCommandProcedure()
{
    return &runSlopeTemplateEditHandleCommand;
}

core::CommandProcedure slopeTemplateApplyDialogFileCommandProcedure()
{
    return &runSlopeTemplateApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
