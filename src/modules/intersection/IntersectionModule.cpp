#include "modules/intersection/IntersectionModule.h"

#include "app/startup/ApplicationContext.h"
#include "application/intersection/IntersectionInfoService.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::intersection {

namespace {

void runIntersectionInfoCommand()
{
    application::IntersectionInfoService service;
    app::ApplicationContext::instance().editor().writeMessage(service.describePrototype());
}

void registerIntersectionCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_INTERSECTION_INFO",
        L"Intersection Info",
        L"INTERSECTION",
        L"Prints intersection module scaffold status.",
        &runIntersectionInfoCommand,
        true,
        true,
        L"docs/business/intersection/\u5e73\u4ea4\u53e3\u6a21\u5757\u6846\u67b6\u8bf4\u660e.md",
        true});
}

void registerIntersectionRibbon(ui::RibbonModel& ribbonModel)
{
    ribbonModel.ensurePanel(L"INTERSECTION", L"Intersection");
}

} // namespace

core::ModuleDefinition createIntersectionModule()
{
    return core::ModuleDefinition{
        L"Intersection",
        L"INTERSECTION",
        L"At-grade intersection prototype module.",
        []() { return true; },
        []() { return true; },
        &registerIntersectionCommands,
        &registerIntersectionRibbon,
        L"docs/modules/intersection.md"};
}

} // namespace roadproto::modules::intersection
