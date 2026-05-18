#include "domain/cross_section/RoadModel.h"

#include <cmath>
#include <utility>

namespace roadproto::domain::cross_section {
namespace {

constexpr double kStationTolerance = 1.0e-9;

bool isFinite(double value)
{
    return std::isfinite(value);
}

} // namespace

bool RoadModelRules::isSupportedSampleInterval(double sampleInterval)
{
    return isFinite(sampleInterval) && sampleInterval > 0.0;
}

bool RoadModelRules::validateAssignments(
    const std::vector<RoadModelTemplateAssignment>& assignments,
    std::wstring& errorMessage)
{
    errorMessage.clear();

    for (const auto& assignment : assignments) {
        if (!isFinite(assignment.startStation) || !isFinite(assignment.endStation)) {
            errorMessage = L"Road model template assignment station must be finite.";
            return false;
        }

        if (assignment.endStation < assignment.startStation) {
            errorMessage = L"Road model template assignment end station must not be before start station.";
            return false;
        }

        if (assignment.templateHandle.empty()) {
            errorMessage = L"Road model template assignment template handle must not be empty.";
            return false;
        }
    }

    return true;
}

RoadModelTemplateResolver::RoadModelTemplateResolver(std::vector<RoadModelTemplateAssignment> assignments)
    : assignments_(std::move(assignments))
{
}

const RoadModelTemplateAssignment* RoadModelTemplateResolver::resolve(double station) const
{
    if (!isFinite(station)) {
        return nullptr;
    }

    for (const auto& assignment : assignments_) {
        if (station + kStationTolerance >= assignment.startStation
            && station - kStationTolerance <= assignment.endStation) {
            return &assignment;
        }
    }

    return nullptr;
}

std::vector<double> RoadModelStationSampler::collectStations(
    const RoadModelConfig&,
    const std::vector<alignment::AlignmentSamplePoint>& alignmentSamples)
{
    std::vector<double> stations;
    stations.reserve(alignmentSamples.size());
    for (const auto& sample : alignmentSamples) {
        stations.push_back(sample.station);
    }
    return stations;
}

RoadModelBuildResult RoadModelBuilder::build(const RoadModelBuildInput& input) const
{
    RoadModelBuildResult result;
    result.data.config = input.config;
    result.sampledStations = RoadModelStationSampler::collectStations(input.config, input.alignmentSamples);
    result.succeeded = true;
    return result;
}

} // namespace roadproto::domain::cross_section
