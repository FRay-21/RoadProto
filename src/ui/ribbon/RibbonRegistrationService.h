#pragma once

#include "core/command/CommandRegistry.h"
#include "core/module/ModuleRegistry.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::ui {

class RibbonRegistrationService {
public:
    void rebuildRibbonModel(
        const core::ModuleRegistry& moduleRegistry,
        const core::CommandRegistry& commandRegistry,
        RibbonModel& ribbonModel) const;
};

} // namespace roadproto::ui
