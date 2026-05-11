#pragma once

#include <string>

namespace roadproto::core {

struct VersionInfo {
    std::wstring version;
    std::wstring buildDate;
    std::wstring stage;
    std::wstring arxFileName;

    static VersionInfo current();
};

} // namespace roadproto::core
