#include "core/version/VersionInfo.h"

namespace roadproto::core {

VersionInfo VersionInfo::current()
{
    return VersionInfo{
        L"v0.1.11",
        L"20260518",
        L"RoadModel",
        L"RoadProto_v0.1.11_20260518_RoadModel.arx"};
}

} // namespace roadproto::core
