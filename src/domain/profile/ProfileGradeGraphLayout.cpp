#include "domain/profile/ProfileGradeGraphLayout.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::profile {
namespace {

double positiveOrDefault(double value, double fallback)
{
    return value > 0.0 ? value : fallback;
}

} // namespace

ProfileGradeGraphLayoutResult ProfileGradeGraphLayout::calculate(const ProfileGradeGraphData& graph)
{
    ProfileGradeGraphLayoutResult result;
    if (graph.groundSamples.size() < 2) {
        result.errorMessage = L"At least two ground samples are required.";
        return result;
    }

    for (const auto& sample : graph.groundSamples) {
        if (!std::isfinite(sample.station) || !std::isfinite(sample.elevation)) {
            result.errorMessage = L"Profile grade graph samples must use finite station and elevation values.";
            return result;
        }
    }

    if (!std::isfinite(graph.properties.gridSpacing) || !std::isfinite(graph.properties.verticalScale)) {
        result.errorMessage = L"Profile grade graph layout properties must use finite grid spacing and vertical scale.";
        return result;
    }

    result.minStation = graph.groundSamples.front().station;
    result.maxStation = graph.groundSamples.front().station;
    result.minElevation = graph.groundSamples.front().elevation;
    result.maxElevation = graph.groundSamples.front().elevation;

    for (const auto& sample : graph.groundSamples) {
        result.minStation = std::min(result.minStation, sample.station);
        result.maxStation = std::max(result.maxStation, sample.station);
        result.minElevation = std::min(result.minElevation, sample.elevation);
        result.maxElevation = std::max(result.maxElevation, sample.elevation);
    }

    const auto gridSpacing = positiveOrDefault(graph.properties.gridSpacing, 10.0);
    const auto verticalScale = positiveOrDefault(graph.properties.verticalScale, 10.0);
    result.baseElevation = std::floor(result.minElevation / gridSpacing) * gridSpacing;
    result.graphWidth = result.maxStation - result.minStation;
    const auto elevationSpanForHeight = std::max(gridSpacing, result.maxElevation - result.baseElevation);
    result.graphHeight = elevationSpanForHeight * verticalScale;
    if (!std::isfinite(result.graphWidth) || !std::isfinite(result.graphHeight)) {
        result.errorMessage = L"Profile grade graph layout dimensions must be finite.";
        return result;
    }

    result.succeeded = true;
    return result;
}

double ProfileGradeGraphLayout::mapX(const ProfileGradeGraphLayoutResult& layout, double station)
{
    return station - layout.minStation;
}

double ProfileGradeGraphLayout::mapY(const ProfileGradeGraphLayoutResult& layout, double elevation, double verticalScale)
{
    return (elevation - layout.baseElevation) * verticalScale;
}

double ProfileGradeGraphLayout::mapY(
    const ProfileGradeGraphData& graph,
    const ProfileGradeGraphLayoutResult& layout,
    double elevation)
{
    return mapY(layout, elevation, positiveOrDefault(graph.properties.verticalScale, 10.0));
}

} // namespace roadproto::domain::profile
