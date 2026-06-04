#include "app/startup/AgentStartupRegistration.h"

#include "modules/agent/AgentModule.h"

namespace roadproto::app {

void registerAgentModuleForStartup(core::ModuleRegistry& moduleRegistry)
{
    moduleRegistry.registerModule(modules::agent::createAgentModule());
}

} // namespace roadproto::app
