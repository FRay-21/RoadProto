#pragma once

#include "cad_adapter/common/IEditor.h"
#include "core/command/CommandRegistry.h"
#include "core/config/AppConfig.h"
#include "core/logging/Logger.h"
#include "core/module/ModuleRegistry.h"
#include "domain/relation/EntityRelationManager.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::app {

class ApplicationContext {
public:
    static ApplicationContext& instance();

    void configure(cad_adapter::IEditor* editor, core::AppConfig config);
    void resetRuntimeState();

    cad_adapter::IEditor& editor();
    core::Logger& logger();
    core::CommandRegistry& commandRegistry();
    core::ModuleRegistry& moduleRegistry();
    domain::EntityRelationManager& relationManager();
    ui::RibbonModel& ribbonModel();
    const core::AppConfig& config() const;

private:
    ApplicationContext();

    cad_adapter::NullEditor nullEditor_;
    cad_adapter::IEditor* editor_ = nullptr;
    core::Logger logger_;
    core::AppConfig config_;
    core::CommandRegistry commandRegistry_;
    core::ModuleRegistry moduleRegistry_;
    domain::EntityRelationManager relationManager_;
    ui::RibbonModel ribbonModel_;
};

} // namespace roadproto::app
