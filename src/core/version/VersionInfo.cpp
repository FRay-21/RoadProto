#include "core/version/VersionInfo.h"

namespace roadproto::core {

VersionInfo VersionInfo::current()
{
    return VersionInfo{
        L"v0.1.10",
        L"20260512",
        L"SubgradeTemplate",
        L"RoadProto_v0.1.10_20260512_SubgradeTemplate.arx"};
}

} // namespace roadproto::core
