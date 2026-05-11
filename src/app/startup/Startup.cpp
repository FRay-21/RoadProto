#include "app/startup/Startup.h"

#include "app/startup/ApplicationContext.h"
#include "core/version/VersionInfo.h"
#include "modules/alignment/AlignmentModule.h"
#include "modules/intersection/IntersectionModule.h"
#include "modules/profile/ProfileModule.h"
#include "modules/terrain/TerrainModule.h"
#include "ui/ribbon/RibbonRegistrationService.h"

namespace roadproto::app {

namespace {

void registerBuiltInModules(core::ModuleRegistry& moduleRegistry)
{
    moduleRegistry.registerModule(modules::terrain::createTerrainModule());
    moduleRegistry.registerModule(modules::alignment::createAlignmentModule());
    moduleRegistry.registerModule(modules::profile::createProfileModule());
    moduleRegistry.registerModule(modules::intersection::createIntersectionModule());
}

} // namespace

bool initialize(cad_adapter::IEditor& editor)
{
    auto& context = ApplicationContext::instance();
    context.configure(&editor, core::defaultAppConfig());
    context.resetRuntimeState();

    registerBuiltInModules(context.moduleRegistry());

    if (!context.moduleRegistry().initializeModules()) {
        context.logger().error(L"RoadProto module initialization failed.");
        return false;
    }

    context.moduleRegistry().registerCommands(context.commandRegistry());

    ui::RibbonRegistrationService ribbonService;
    ribbonService.rebuildRibbonModel(
        context.moduleRegistry(),
        context.commandRegistry(),
        context.ribbonModel());

    const auto version = core::VersionInfo::current();
    context.logger().info(L"RoadProto " + version.version + L" initialized.");
    return true;
}

bool shutdown()
{
    auto& context = ApplicationContext::instance();
    const auto unloaded = context.moduleRegistry().unloadModules();
    context.resetRuntimeState();
    context.logger().info(L"RoadProto unloaded.");
    return unloaded;
}

} // namespace roadproto::app
