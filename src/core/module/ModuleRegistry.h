#pragma once

#include "core/command/CommandRegistry.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace roadproto::ui {
class RibbonModel;
}

namespace roadproto::core {

using ModuleLifecycle = std::function<bool()>;
using ModuleCommandRegistration = std::function<void(CommandRegistry&)>;
using ModuleRibbonRegistration = std::function<void(ui::RibbonModel&)>;

struct ModuleDefinition {
    std::wstring name;
    std::wstring code;
    std::wstring description;
    ModuleLifecycle initialize;
    ModuleLifecycle unload;
    ModuleCommandRegistration registerCommands;
    ModuleRibbonRegistration registerRibbon;
    std::wstring businessDocPath;
};

class ModuleRegistry {
public:
    bool registerModule(ModuleDefinition definition);
    bool initializeModules() const;
    bool unloadModules() const;
    void registerCommands(CommandRegistry& commandRegistry) const;
    void registerRibbon(ui::RibbonModel& ribbonModel) const;
    void clear();

    bool contains(const std::wstring& code) const;
    std::optional<ModuleDefinition> find(const std::wstring& code) const;
    std::vector<ModuleDefinition> allModules() const;

private:
    std::vector<ModuleDefinition> modules_;
};

} // namespace roadproto::core
