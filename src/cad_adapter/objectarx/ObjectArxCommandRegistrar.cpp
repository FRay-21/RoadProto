#include "cad_adapter/objectarx/ObjectArxCommandRegistrar.h"

#include "cad_adapter/common/IEditor.h"

#include "aced.h"

#include <sstream>

namespace roadproto::cad_adapter::objectarx {

bool registerCommands(
    const core::CommandRegistry& commandRegistry,
    const std::wstring& commandGroupName,
    IEditor& editor)
{
    bool allSucceeded = true;

    for (const auto& command : commandRegistry.allCommands()) {
        const auto status = acedRegCmds->addCommand(
            commandGroupName.c_str(),
            command.internalName.c_str(),
            command.internalName.c_str(),
            ACRX_CMD_MODAL,
            command.procedure);

        if (status != Acad::eOk) {
            allSucceeded = false;
            std::wstringstream stream;
            stream << L"Failed to register command " << command.internalName << L". Acad status: " << status;
            editor.writeError(stream.str());
        }
    }

    return allSucceeded;
}

void unregisterCommands(const std::wstring& commandGroupName)
{
    if (!commandGroupName.empty()) {
        acedRegCmds->removeGroup(commandGroupName.c_str());
    }
}

} // namespace roadproto::cad_adapter::objectarx
