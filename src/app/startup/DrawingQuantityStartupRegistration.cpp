#include "app/startup/DrawingQuantityStartupRegistration.h"

#include "modules/drawing_quantity/DrawingQuantityModule.h"

namespace roadproto::app {

void registerDrawingQuantityModuleForStartup(core::ModuleRegistry& moduleRegistry)
{
    moduleRegistry.registerModule(modules::drawing_quantity::createDrawingQuantityModule());
}

} // namespace roadproto::app
