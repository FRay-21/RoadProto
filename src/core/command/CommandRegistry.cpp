#include "core/command/CommandRegistry.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace roadproto::core {

namespace {

bool isValidCommand(const CommandDefinition& definition)
{
    return !definition.internalName.empty()
        && !definition.displayName.empty()
        && !definition.moduleCode.empty()
        && definition.procedure != nullptr
        && !definition.businessDocPath.empty();
}

} // namespace

bool CommandRegistry::registerCommand(CommandDefinition definition)
{
    if (!isValidCommand(definition) || contains(definition.internalName)) {
        return false;
    }

    commands_.push_back(std::move(definition));
    return true;
}

bool CommandRegistry::unregisterCommand(const std::wstring& internalName)
{
    const auto before = commands_.size();
    commands_.erase(
        std::remove_if(commands_.begin(), commands_.end(), [&](const CommandDefinition& command) {
            return command.internalName == internalName;
        }),
        commands_.end());
    return commands_.size() != before;
}

void CommandRegistry::unregisterModuleCommands(const std::wstring& moduleCode)
{
    commands_.erase(
        std::remove_if(commands_.begin(), commands_.end(), [&](const CommandDefinition& command) {
            return command.moduleCode == moduleCode;
        }),
        commands_.end());
}

void CommandRegistry::clear()
{
    commands_.clear();
}

bool CommandRegistry::contains(const std::wstring& internalName) const
{
    return std::any_of(commands_.begin(), commands_.end(), [&](const CommandDefinition& command) {
        return command.internalName == internalName;
    });
}

std::optional<CommandDefinition> CommandRegistry::find(const std::wstring& internalName) const
{
    const auto it = std::find_if(commands_.begin(), commands_.end(), [&](const CommandDefinition& command) {
        return command.internalName == internalName;
    });

    if (it == commands_.end()) {
        return std::nullopt;
    }

    return *it;
}

std::vector<CommandDefinition> CommandRegistry::allCommands() const
{
    return commands_;
}

std::vector<CommandDefinition> CommandRegistry::commandsForModule(const std::wstring& moduleCode) const
{
    std::vector<CommandDefinition> result;
    std::copy_if(commands_.begin(), commands_.end(), std::back_inserter(result), [&](const CommandDefinition& command) {
        return command.moduleCode == moduleCode;
    });
    return result;
}

} // namespace roadproto::core
