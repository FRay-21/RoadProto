#include "core/version/VersionInfo.h"

namespace roadproto::core {

VersionInfo VersionInfo::current()
{
    return VersionInfo{
        L"v0.1.20",
        L"20260522",
        L"PavementLayerTemplateDisplay",
        L"RoadProto_v0.1.20_20260522_PavementLayerTemplateDisplay.arx"};
}

} // namespace roadproto::core
