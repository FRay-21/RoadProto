#pragma once

#include "domain/cross_section/RoadModel.h"

#include <string>
#include <vector>

namespace roadproto::cad_adapter::objectarx::cross_section {

enum class RoadModelDialogAction {
    None,
    PickTemplate,
    PickLeftSlopeTemplate,
    PickRightSlopeTemplate,
};

struct RoadModelDialogRequest {
    std::wstring handle;
    std::wstring roadCenterlineHandle;
    std::wstring profileVerticalCurveHandle;
    std::wstring responsePath;
    double sampleInterval = 10.0;
    double leftSlopeSearchWidth = 50.0;
    double rightSlopeSearchWidth = 50.0;
    int selectedAssignmentIndex = -1;
    int selectedLeftSlopeGroupIndex = -1;
    int selectedRightSlopeGroupIndex = -1;
    std::vector<roadproto::domain::cross_section::RoadModelTemplateAssignment> assignments;
    std::vector<roadproto::domain::cross_section::RoadModelStructureRange> structures;
    std::vector<roadproto::domain::cross_section::RoadModelSlopeTemplateGroup> leftSlopeGroups;
    std::vector<roadproto::domain::cross_section::RoadModelSlopeTemplateGroup> rightSlopeGroups;
};

struct RoadModelDialogResponse {
    RoadModelDialogAction action = RoadModelDialogAction::None;
    bool accepted = false;
    int pickAssignmentIndex = -1;
    int pickSlopeGroupIndex = -1;
    std::wstring handle;
    std::wstring roadCenterlineHandle;
    std::wstring profileVerticalCurveHandle;
    double sampleInterval = 10.0;
    double leftSlopeSearchWidth = 50.0;
    double rightSlopeSearchWidth = 50.0;
    std::vector<roadproto::domain::cross_section::RoadModelTemplateAssignment> assignments;
    std::vector<roadproto::domain::cross_section::RoadModelStructureRange> structures;
    std::vector<roadproto::domain::cross_section::RoadModelSlopeTemplateGroup> leftSlopeGroups;
    std::vector<roadproto::domain::cross_section::RoadModelSlopeTemplateGroup> rightSlopeGroups;
};

bool queueRoadModelWpfDialog(
    const RoadModelDialogRequest& request,
    std::wstring& errorMessage);

bool readRoadModelDialogResponse(
    const std::wstring& responsePath,
    RoadModelDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
