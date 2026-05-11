#include "core/version/VersionInfo.h"

namespace roadproto::core {

VersionInfo VersionInfo::current()
{
    return VersionInfo{
        L"v0.1.9",
        L"20260511",
        L"ProfileGradeGraph",
        L"RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx"};
}

} // namespace roadproto::core
