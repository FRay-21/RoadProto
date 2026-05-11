#include "app/startup/ProfileStartupRegistration.h"

#include "modules/profile/ProfileModule.h"

namespace roadproto::app {

void registerProfileModuleForStartup(core::ModuleRegistry& moduleRegistry)
{
    moduleRegistry.registerModule(modules::profile::createProfileModule());
}

} // namespace roadproto::app
