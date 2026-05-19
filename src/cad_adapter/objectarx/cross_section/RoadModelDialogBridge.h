#pragma once

#include "domain/cross_section/RoadModel.h"

#include <string>
#include <vector>

namespace roadproto::cad_adapter::objectarx::cross_section {

enum class RoadModelDialogAction {
    None,
    PickTemplate,
};

struct RoadModelDialogRequest {
    std::wstring handle;
    std::wstring roadCenterlineHandle;
    std::wstring profileVerticalCurveHandle;
    std::wstring responsePath;
    double sampleInterval = 10.0;
    int selectedAssignmentIndex = -1;
    std::vector<roadproto::domain::cross_section::RoadModelTemplateAssignment> assignments;
};

struct RoadModelDialogResponse {
    RoadModelDialogAction action = RoadModelDialogAction::None;
    bool accepted = false;
    int pickAssignmentIndex = -1;
    std::wstring handle;
    std::wstring roadCenterlineHandle;
    std::wstring profileVerticalCurveHandle;
    double sampleInterval = 10.0;
    std::vector<roadproto::domain::cross_section::RoadModelTemplateAssignment> assignments;
};

bool queueRoadModelWpfDialog(
    const RoadModelDialogRequest& request,
    std::wstring& errorMessage);

bool readRoadModelDialogResponse(
    const std::wstring& responsePath,
    RoadModelDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
