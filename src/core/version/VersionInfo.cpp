#include "core/version/VersionInfo.h"

namespace roadproto::core {

VersionInfo VersionInfo::current()
{
    return VersionInfo{
        L"v0.1.18",
        L"20260520",
        L"RoadModelSectionSnapshot",
        L"RoadProto_v0.1.18_20260520_RoadModelSectionSnapshot.arx"};
}

} // namespace roadproto::core
