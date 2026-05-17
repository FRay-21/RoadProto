#pragma once

#include <string>

namespace roadproto::cad_adapter {

std::wstring formatAutoCadCommandWithQuotedArgument(
    const std::wstring& commandName,
    const std::wstring& argument);

std::wstring normalizeAutoCadCommandArgument(const std::wstring& argument);

} // namespace roadproto::cad_adapter
