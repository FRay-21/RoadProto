#include "application/cross_section/RoadModelBuildService.h"

namespace roadproto::application::cross_section {

RoadModelBuildResult RoadModelBuildService::build(const RoadModelBuildInput& input) const
{
    RoadModelBuildResult result;
    result.data.config = input.config;

    if (input.config.roadCenterlineHandle.empty()) {
        result.errorMessage = L"Road centerline handle is required.";
        return result;
    }

    if (input.config.profileVerticalCurveHandle.empty()) {
        result.errorMessage = L"Profile vertical curve handle is required.";
        return result;
    }

    return domain::cross_section::RoadModelBuilder::build(input);
}

} // namespace roadproto::application::cross_section
