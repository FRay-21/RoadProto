#include "core/version/VersionInfo.h"

namespace roadproto::core {

VersionInfo VersionInfo::current()
{
    return VersionInfo{
        L"v0.1.12",
        L"20260518",
        L"RoadModelWpfFix",
        L"RoadProto_v0.1.12_20260518_RoadModelWpfFix.arx"};
}

} // namespace roadproto::core
