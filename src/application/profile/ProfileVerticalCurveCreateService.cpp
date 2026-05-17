#include "application/profile/ProfileVerticalCurveCreateService.h"

#include <cmath>

namespace roadproto::application::profile {
namespace {

bool isFiniteSample(const domain::profile::ProfileGroundSample& sample)
{
    return std::isfinite(sample.station) && std::isfinite(sample.elevation);
}

} // namespace

ProfileVerticalCurveCreateResult ProfileVerticalCurveCreateService::buildDefaultFromGraph(
    const ProfileVerticalCurveCreateInput& input) const
{
    ProfileVerticalCurveCreateResult result;
    if (input.profileGraphHandle.empty()) {
        result.errorMessage = L"Profile grade graph handle is required.";
        return result;
    }
    if (input.groundSamples.size() < 2) {
        result.errorMessage = L"At least two profile ground samples are required.";
        return result;
    }

    const auto& first = input.groundSamples.front();
    const auto& last = input.groundSamples.back();
    if (!isFiniteSample(first) || !isFiniteSample(last) || last.station <= first.station) {
        result.errorMessage = L"Profile ground samples must provide finite increasing start and end stations.";
        return result;
    }

    result.data.profileGraphHandle = input.profileGraphHandle;
    result.data.controlPoints = {
        domain::profile::VerticalCurveControlPoint{
            domain::profile::VerticalCurvePointRole::Start,
            first.station,
            first.elevation},
        domain::profile::VerticalCurveControlPoint{
            domain::profile::VerticalCurvePointRole::End,
            last.station,
            last.elevation},
    };
    result.succeeded = true;
    return result;
}

} // namespace roadproto::application::profile
