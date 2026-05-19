#include "core/version/VersionInfo.h"

namespace roadproto::core {

VersionInfo VersionInfo::current()
{
    return VersionInfo{
        L"v0.1.13",
        L"20260519",
        L"RoadModelPickGrip",
        L"RoadProto_v0.1.13_20260519_RoadModelPickGrip.arx"};
}

} // namespace roadproto::core
