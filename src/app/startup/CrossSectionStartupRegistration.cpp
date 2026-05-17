#include "app/startup/CrossSectionStartupRegistration.h"

#include "modules/cross_section/CrossSectionModule.h"

namespace roadproto::app {

void registerCrossSectionModuleForStartup(core::ModuleRegistry& moduleRegistry)
{
    moduleRegistry.registerModule(modules::cross_section::createCrossSectionModule());
}

} // namespace roadproto::app
