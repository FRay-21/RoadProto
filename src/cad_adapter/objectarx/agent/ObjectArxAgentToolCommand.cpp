#include "cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/agent/AgentToolRequest.h"
#include "application/agent/SubgradeTemplateToolMapper.h"
#include "cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <fstream>
#include <iterator>
#include <string>
#endif

namespace roadproto::cad_adapter::objectarx::agent {
namespace {

#ifndef ROADPROTO_TEST_BUILD

std::string readBinaryFile(const std::wstring& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool promptInsertionPoint(AcGePoint3d& insertionPoint)
{
    ads_point raw;
    if (acedGetPoint(nullptr, L"\n请选择 Agent 路基模板插入位置: ", raw) != RTNORM) {
        return false;
    }

    insertionPoint = AcGePoint3d(raw[0], raw[1], raw[2]);
    return true;
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

void runAgentExecuteToolFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto Agent tool request file: ", pathBuffer) != RTNORM) {
        return;
    }

    std::wstring error;
    const auto request = application::agent::parseAgentToolRequestJson(readBinaryFile(pathBuffer), error);
    if (!request.succeeded) {
        editor.writeError(error.empty() ? L"Agent 工具请求解析失败。" : error);
        return;
    }

    const auto dataResult = application::agent::buildSubgradeTemplateToolData(request.arguments, error);
    if (!dataResult.succeeded) {
        editor.writeError(error.empty() ? L"Agent 路基模板参数无效。" : error);
        return;
    }

    AcGePoint3d insertionPoint(0.0, 0.0, 0.0);
    if (request.arguments.insertionPoint.mode == L"Explicit"
        && request.arguments.insertionPoint.hasX
        && request.arguments.insertionPoint.hasY) {
        insertionPoint = AcGePoint3d(
            request.arguments.insertionPoint.x,
            request.arguments.insertionPoint.y,
            request.arguments.insertionPoint.hasZ ? request.arguments.insertionPoint.z : 0.0);
    } else if (!promptInsertionPoint(insertionPoint)) {
        editor.writeWarning(L"Agent 路基模板创建已取消。");
        return;
    }

    auto* entity = new DnSubgradeTemplateEntity();
    entity->setTemplateData(dataResult.data);
    entity->setInsertionPoint(insertionPoint);

    AcDbObjectId entityId;
    if (!appendEntityToModelSpace(entity, entityId)) {
        delete entity;
        editor.writeError(L"Agent 插入 DnSubgradeTemplateEntity 失败。");
        return;
    }

    const auto handle = entityHandleText(entity);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"Agent 已创建路基模板实体，handle: " + handle);
}

#else

void runAgentExecuteToolFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure agentExecuteToolFileCommandProcedure()
{
    return &runAgentExecuteToolFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::agent
