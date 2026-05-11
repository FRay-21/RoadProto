#include "ui/ribbon/RibbonRegistrationService.h"

namespace roadproto::ui {

void RibbonRegistrationService::rebuildRibbonModel(
    const core::ModuleRegistry& moduleRegistry,
    const core::CommandRegistry& commandRegistry,
    RibbonModel& ribbonModel) const
{
    ribbonModel.clear();
    moduleRegistry.registerRibbon(ribbonModel);

    for (const auto& command : commandRegistry.allCommands()) {
        if (!command.ribbonAttachable) {
            continue;
        }

        ribbonModel.addButton(
            command.moduleCode,
            RibbonButtonDefinition{
                command.internalName,
                command.displayName,
                command.description,
                L"assets/icons/" + command.internalName + L".png",
                command.businessDocPath});
    }
}

} // namespace roadproto::ui
