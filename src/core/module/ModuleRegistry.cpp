#include "core/module/ModuleRegistry.h"

#include <algorithm>
#include <utility>

namespace roadproto::core {

namespace {

bool isValidModule(const ModuleDefinition& definition)
{
    return !definition.name.empty()
        && !definition.code.empty()
        && !definition.description.empty()
        && !definition.businessDocPath.empty();
}

} // namespace

bool ModuleRegistry::registerModule(ModuleDefinition definition)
{
    if (!isValidModule(definition) || contains(definition.code)) {
        return false;
    }

    modules_.push_back(std::move(definition));
    return true;
}

bool ModuleRegistry::initializeModules() const
{
    for (const auto& module : modules_) {
        if (module.initialize && !module.initialize()) {
            return false;
        }
    }
    return true;
}

bool ModuleRegistry::unloadModules() const
{
    bool allSucceeded = true;
    for (auto it = modules_.rbegin(); it != modules_.rend(); ++it) {
        if (it->unload && !it->unload()) {
            allSucceeded = false;
        }
    }
    return allSucceeded;
}

void ModuleRegistry::registerCommands(CommandRegistry& commandRegistry) const
{
    for (const auto& module : modules_) {
        if (module.registerCommands) {
            module.registerCommands(commandRegistry);
        }
    }
}

void ModuleRegistry::registerRibbon(ui::RibbonModel& ribbonModel) const
{
    for (const auto& module : modules_) {
        if (module.registerRibbon) {
            module.registerRibbon(ribbonModel);
        }
    }
}

void ModuleRegistry::clear()
{
    modules_.clear();
}

bool ModuleRegistry::contains(const std::wstring& code) const
{
    return std::any_of(modules_.begin(), modules_.end(), [&](const ModuleDefinition& module) {
        return module.code == code;
    });
}

std::optional<ModuleDefinition> ModuleRegistry::find(const std::wstring& code) const
{
    const auto it = std::find_if(modules_.begin(), modules_.end(), [&](const ModuleDefinition& module) {
        return module.code == code;
    });

    if (it == modules_.end()) {
        return std::nullopt;
    }

    return *it;
}

std::vector<ModuleDefinition> ModuleRegistry::allModules() const
{
    return modules_;
}

} // namespace roadproto::core
