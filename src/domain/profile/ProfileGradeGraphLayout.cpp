#include "domain/profile/ProfileGradeGraphLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace roadproto::domain::profile {
namespace {

double positiveOrDefault(double value, double fallback)
{
    return value > 0.0 ? value : fallback;
}

bool isSupportedVerticalScale(double value)
{
    return value == 1.0 || value == 10.0 || value == 100.0;
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
    const auto verticalScale = graph.properties.verticalScale;
    if (!isSupportedVerticalScale(verticalScale)) {
        result.errorMessage = L"Profile grade graph vertical scale must be 1.0, 10.0, or 100.0.";
        return result;
    }

    result.baseElevation = std::floor(result.minElevation / gridSpacing) * gridSpacing;
    result.graphWidth = result.maxStation - result.minStation;
    if (result.graphWidth <= 0.0) {
        result.errorMessage = L"Profile grade graph station span must be greater than zero.";
        return result;
    }

    const auto elevationSpanForHeight = std::max(gridSpacing, result.maxElevation - result.baseElevation);
    result.graphHeight = elevationSpanForHeight * verticalScale;
    if (!std::isfinite(result.graphWidth) || !std::isfinite(result.graphHeight)) {
        result.errorMessage = L"Profile grade graph layout dimensions must be finite.";
        return result;
    }

    result.mappedPoints.reserve(graph.groundSamples.size());
    for (const auto& sample : graph.groundSamples) {
        result.mappedPoints.push_back(ProfileGradeGraphMappedPoint{
            sample.station,
            sample.elevation,
            sample.station - result.minStation,
            (sample.elevation - result.baseElevation) * verticalScale});
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
    if (!std::isfinite(graph.properties.verticalScale) || !isSupportedVerticalScale(graph.properties.verticalScale)) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    return mapY(layout, elevation, graph.properties.verticalScale);
}

} // namespace roadproto::domain::profile
