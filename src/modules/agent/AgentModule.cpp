#include "modules/agent/AgentModule.h"

#include "cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::agent {
namespace {

void registerAgentCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_AI_EXECUTE_TOOL_FILE",
        L"执行 Agent 工具文件",
        L"AI_AGENT",
        L"Executes a whitelisted RoadProto Agent tool request file.",
        cad_adapter::objectarx::agent::agentExecuteToolFileCommandProcedure(),
        true,
        true,
        L"docs/business/agent/设计软件原型Agent.md",
        false});
}

void registerAgentRibbon(ui::RibbonModel&)
{
}

} // namespace

core::ModuleDefinition createAgentModule()
{
    return core::ModuleDefinition{
        L"Agent",
        L"AI_AGENT",
        L"RoadProto design software prototype Agent module.",
        []() { return true; },
        []() { return true; },
        &registerAgentCommands,
        &registerAgentRibbon,
        L"docs/business/agent/设计软件原型Agent.md"};
}

} // namespace roadproto::modules::agent
