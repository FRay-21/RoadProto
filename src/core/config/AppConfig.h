#pragma once

#include <string>

namespace roadproto::core {

struct AppConfig {
    std::wstring commandGroupName = L"ROADPROTO_V010";
    std::wstring ribbonTabName = L"RoadProto";
    std::wstring iconRoot = L"assets/icons";
    std::wstring businessDocRoot = L"docs/business";
    std::wstring reuseDocRoot = L"docs/reuse";
};

AppConfig defaultAppConfig();

} // namespace roadproto::core
