#pragma once

#include "core/command/CommandRegistry.h"

#include <string>

namespace roadproto::cad_adapter {
class IEditor;
}

namespace roadproto::cad_adapter::objectarx {

bool registerCommands(
    const core::CommandRegistry& commandRegistry,
    const std::wstring& commandGroupName,
    IEditor& editor);

void unregisterCommands(const std::wstring& commandGroupName);

} // namespace roadproto::cad_adapter::objectarx
