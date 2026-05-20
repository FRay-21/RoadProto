#pragma once

#include "domain/cross_section/RoadModel.h"

#include <string>
#include <vector>

namespace roadproto::cad_adapter::objectarx::cross_section {

struct RoadModelSectionViewerPreview {
    roadproto::domain::cross_section::RoadModelSectionPreview data;
};

struct RoadModelSectionViewerRequest {
    std::wstring handle;
    std::wstring roadCenterlineHandle;
    std::vector<RoadModelSectionViewerPreview> previews;
};

bool queueRoadModelSectionViewerWpfDialog(
    const RoadModelSectionViewerRequest& request,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
