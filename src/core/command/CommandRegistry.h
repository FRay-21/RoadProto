#pragma once

#include <optional>
#include <string>
#include <vector>

namespace roadproto::core {

using CommandProcedure = void (*)();

struct CommandDefinition {
    std::wstring internalName;
    std::wstring displayName;
    std::wstring moduleCode;
    std::wstring description;
    CommandProcedure procedure = nullptr;
    bool isPrototype = true;
    bool reusable = false;
    std::wstring businessDocPath;
    bool ribbonAttachable = true;
};

class CommandRegistry {
public:
    bool registerCommand(CommandDefinition definition);
    bool unregisterCommand(const std::wstring& internalName);
    void unregisterModuleCommands(const std::wstring& moduleCode);
    void clear();

    bool contains(const std::wstring& internalName) const;
    std::optional<CommandDefinition> find(const std::wstring& internalName) const;
    std::vector<CommandDefinition> allCommands() const;
    std::vector<CommandDefinition> commandsForModule(const std::wstring& moduleCode) const;

private:
    std::vector<CommandDefinition> commands_;
};

} // namespace roadproto::core
