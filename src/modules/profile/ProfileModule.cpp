#include "modules/profile/ProfileModule.h"

#include "cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::profile {
namespace {

void registerProfileCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_GRADE_GRAPH_CREATE",
        L"\u7eb5\u65ad\u9762\u62c9\u5761\u56fe",
        L"PROFILE",
        L"Creates a longitudinal profile grade graph entity.",
        cad_adapter::objectarx::profile::profileGradeGraphCreateCommandProcedure(),
        true,
        true,
        L"docs/business/profile/\u7eb5\u65ad\u9762\u62c9\u5761\u56fe_\u521b\u5efa.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE",
        L"\u6309 handle \u7f16\u8f91\u7eb5\u65ad\u9762\u62c9\u5761\u56fe",
        L"PROFILE",
        L"Internal double-click bridge command that edits a profile grade graph by handle.",
        cad_adapter::objectarx::profile::profileGradeGraphEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/profile/\u7eb5\u65ad\u9762\u62c9\u5761\u56fe_\u5c5e\u6027\u7f16\u8f91.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_APPLY_DIALOG_FILE",
        L"\u5e94\u7528\u7eb5\u65ad\u9762\u62c9\u5761\u56fe\u5bf9\u8bdd\u6846\u7ed3\u679c",
        L"PROFILE",
        L"Internal WPF bridge command that applies profile grade graph dialog response files.",
        cad_adapter::objectarx::profile::profileApplyDialogFileCommandProcedure(),
        true,
        false,
        L"docs/business/profile/\u7eb5\u65ad\u9762\u62c9\u5761\u56fe_\u5c5e\u6027\u7f16\u8f91.md",
        false});
}

void registerProfileRibbon(ui::RibbonModel& ribbonModel)
{
    ribbonModel.ensurePanel(L"PROFILE", L"\u7eb5\u65ad\u9762\u8bbe\u8ba1");
}

} // namespace

core::ModuleDefinition createProfileModule()
{
    return core::ModuleDefinition{
        L"Profile",
        L"PROFILE",
        L"Longitudinal profile design prototype module.",
        []() { return true; },
        []() { return true; },
        &registerProfileCommands,
        &registerProfileRibbon,
        L"docs/modules/profile.md"};
}

} // namespace roadproto::modules::profile
