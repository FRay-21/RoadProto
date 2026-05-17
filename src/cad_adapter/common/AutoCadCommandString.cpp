#include "cad_adapter/common/AutoCadCommandString.h"

#include <cwctype>

namespace roadproto::cad_adapter {
namespace {

std::size_t firstNonSpace(const std::wstring& value)
{
    std::size_t index = 0;
    while (index < value.size() && std::iswspace(value[index]) != 0) {
        ++index;
    }
    return index;
}

std::size_t lastNonSpaceExclusive(const std::wstring& value)
{
    std::size_t index = value.size();
    while (index > 0 && std::iswspace(value[index - 1]) != 0) {
        --index;
    }
    return index;
}

} // namespace

std::wstring formatAutoCadCommandWithQuotedArgument(
    const std::wstring& commandName,
    const std::wstring& argument)
{
    return commandName + L" \"" + argument + L"\"\n";
}

std::wstring normalizeAutoCadCommandArgument(const std::wstring& argument)
{
    const auto begin = firstNonSpace(argument);
    const auto end = lastNonSpaceExclusive(argument);
    if (begin >= end) {
        return {};
    }

    auto normalized = argument.substr(begin, end - begin);
    if (normalized.size() >= 2 && normalized.front() == L'"' && normalized.back() == L'"') {
        normalized = normalized.substr(1, normalized.size() - 2);
    }
    return normalized;
}

} // namespace roadproto::cad_adapter
