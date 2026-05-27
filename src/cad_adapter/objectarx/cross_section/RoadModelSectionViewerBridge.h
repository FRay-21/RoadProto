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
    std::wstring responsePath;
    std::wstring roadCenterlineHandle;
    std::vector<RoadModelSectionViewerPreview> previews;
};

enum class RoadModelSectionViewerAction {
    None,
    DrawSections
};

struct RoadModelSectionViewerResponse {
    RoadModelSectionViewerAction action = RoadModelSectionViewerAction::None;
    bool accepted = false;
    std::wstring handle;
};

bool queueRoadModelSectionViewerWpfDialog(
    const RoadModelSectionViewerRequest& request,
    std::wstring& errorMessage);

bool readRoadModelSectionViewerResponse(
    const std::wstring& responsePath,
    RoadModelSectionViewerResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
