#include "core/version/VersionInfo.h"

namespace roadproto::core {

VersionInfo VersionInfo::current()
{
    return VersionInfo{
        L"v0.1.8",
        L"20260509",
        L"AlignmentWpfPreview",
        L"RoadProto_v0.1.8_20260509_AlignmentWpfPreview.arx"};
}

} // namespace roadproto::core
