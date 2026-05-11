#include "app/startup/ApplicationContext.h"

#include <utility>

namespace roadproto::app {

ApplicationContext& ApplicationContext::instance()
{
    static ApplicationContext context;
    return context;
}

ApplicationContext::ApplicationContext()
    : editor_(&nullEditor_)
    , config_(core::defaultAppConfig())
    , ribbonModel_(config_.ribbonTabName)
{
    logger_.setSink([this](core::LogLevel level, const std::wstring& message) {
        switch (level) {
        case core::LogLevel::Warning:
            editor().writeWarning(message);
            break;
        case core::LogLevel::Error:
            editor().writeError(message);
            break;
        default:
            editor().writeMessage(message);
            break;
        }
    });
}

void ApplicationContext::configure(cad_adapter::IEditor* editor, core::AppConfig config)
{
    editor_ = editor == nullptr ? &nullEditor_ : editor;
    config_ = std::move(config);
    ribbonModel_.setTabTitle(config_.ribbonTabName);
}

void ApplicationContext::resetRuntimeState()
{
    commandRegistry_.clear();
    moduleRegistry_.clear();
    relationManager_.clear();
    ribbonModel_.clear();
}

cad_adapter::IEditor& ApplicationContext::editor()
{
    return *editor_;
}

core::Logger& ApplicationContext::logger()
{
    return logger_;
}

core::CommandRegistry& ApplicationContext::commandRegistry()
{
    return commandRegistry_;
}

core::ModuleRegistry& ApplicationContext::moduleRegistry()
{
    return moduleRegistry_;
}

domain::EntityRelationManager& ApplicationContext::relationManager()
{
    return relationManager_;
}

ui::RibbonModel& ApplicationContext::ribbonModel()
{
    return ribbonModel_;
}

const core::AppConfig& ApplicationContext::config() const
{
    return config_;
}

} // namespace roadproto::app
