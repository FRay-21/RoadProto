#include "domain/cross_section/RoadModel.h"

#include "domain/profile/ProfileVerticalCurveCalculator.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace roadproto::domain::cross_section {
namespace {

bool isFinite(double value)
{
    return std::isfinite(value);
}

constexpr double kStationTolerance = 1.0e-7;

struct StationRange {
    double start = 0.0;
    double end = 0.0;
};

void addStationInRange(std::vector<double>& stations, double station, const StationRange& range)
{
    if (!isFinite(station)) {
        return;
    }

    if (station < range.start - kStationTolerance || station > range.end + kStationTolerance) {
        return;
    }

    stations.push_back(std::clamp(station, range.start, range.end));
}

std::optional<StationRange> verticalCurveRangeFromControlPoints(
    const profile::ProfileVerticalCurveData& verticalCurve)
{
    std::optional<StationRange> range;
    for (const auto& point : verticalCurve.controlPoints) {
        if (!isFinite(point.station)) {
            continue;
        }

        if (!range.has_value()) {
            range = StationRange{point.station, point.station};
        } else {
            range->start = std::min(range->start, point.station);
            range->end = std::max(range->end, point.station);
        }
    }

    if (!range.has_value() || range->end <= range->start) {
        return std::nullopt;
    }

    return range;
}

std::optional<StationRange> verticalCurveRange(const profile::ProfileVerticalCurveData& verticalCurve)
{
    const auto built = profile::ProfileVerticalCurveCalculator::rebuild(verticalCurve);
    if (built.succeeded && built.orderedControlPoints.size() >= 2) {
        return StationRange{
            built.orderedControlPoints.front().station,
            built.orderedControlPoints.back().station};
    }

    if (const auto fallbackRange = verticalCurveRangeFromControlPoints(verticalCurve)) {
        return *fallbackRange;
    }

    return std::nullopt;
}

std::vector<double> sortedUniqueStations(std::vector<double> stations)
{
    std::sort(stations.begin(), stations.end());
    std::vector<double> result;
    for (const double station : stations) {
        if (result.empty() || std::fabs(station - result.back()) > kStationTolerance) {
            result.push_back(station);
        }
    }
    return result;
}

std::optional<double> stationCoveredByAssignment(
    const RoadModelTemplateAssignment& assignment,
    double station)
{
    if (!isFinite(assignment.startStation) || !isFinite(assignment.endStation) ||
        assignment.endStation < assignment.startStation) {
        return std::nullopt;
    }

    if (std::fabs(station - assignment.startStation) <= kStationTolerance) {
        return assignment.startStation;
    }
    if (std::fabs(station - assignment.endStation) <= kStationTolerance) {
        return assignment.endStation;
    }
    if (assignment.startStation <= station && station <= assignment.endStation) {
        return station;
    }
    return std::nullopt;
}

std::vector<double> filterTemplateCoveredStations(
    const std::vector<double>& stations,
    const std::vector<RoadModelTemplateAssignment>& assignments)
{
    std::vector<double> result;
    for (const double station : stations) {
        for (const auto& assignment : assignments) {
            if (const auto coveredStation = stationCoveredByAssignment(assignment, station)) {
                result.push_back(*coveredStation);
                break;
            }
        }
    }
    return result;
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
        if (station >= assignment.startStation && station <= assignment.endStation) {
            return &assignment;
        }
    }

    return nullptr;
}

std::vector<double> RoadModelStationSampler::collectStations(
    double alignmentStart,
    double alignmentEnd,
    const profile::ProfileVerticalCurveData& verticalCurve,
    const std::vector<RoadModelTemplateAssignment>& assignments,
    double sampleInterval)
{
    if (!isFinite(alignmentStart) || !isFinite(alignmentEnd) || alignmentEnd <= alignmentStart ||
        !RoadModelRules::isSupportedSampleInterval(sampleInterval) || assignments.empty()) {
        return {};
    }

    const StationRange alignmentRange{alignmentStart, alignmentEnd};
    const auto verticalRange = verticalCurveRange(verticalCurve);
    if (!verticalRange.has_value()) {
        return {};
    }

    const StationRange effectiveRange{
        std::max(alignmentRange.start, verticalRange->start),
        std::min(alignmentRange.end, verticalRange->end)};
    if (effectiveRange.end <= effectiveRange.start) {
        return {};
    }

    if (effectiveRange.start + sampleInterval <= effectiveRange.start) {
        return {};
    }

    std::vector<double> stations;
    addStationInRange(stations, effectiveRange.start, effectiveRange);
    addStationInRange(stations, effectiveRange.end, effectiveRange);

    for (double station = effectiveRange.start;;) {
        addStationInRange(stations, station, effectiveRange);

        const double next = station + sampleInterval;
        if (!isFinite(next) || next <= station) {
            return {};
        }
        if (next > effectiveRange.end + kStationTolerance) {
            break;
        }
        station = next;
    }

    for (const auto& assignment : assignments) {
        if (!isFinite(assignment.startStation) || !isFinite(assignment.endStation)) {
            continue;
        }

        const double start = std::max(assignment.startStation, effectiveRange.start);
        const double end = std::min(assignment.endStation, effectiveRange.end);
        if (end < start) {
            continue;
        }

        addStationInRange(stations, start, effectiveRange);
        addStationInRange(stations, end, effectiveRange);
    }

    for (const auto& point : verticalCurve.controlPoints) {
        addStationInRange(stations, point.station, effectiveRange);
    }
    for (const auto& pvi : verticalCurve.pvis) {
        addStationInRange(stations, pvi.station, effectiveRange);
    }

    const auto built = profile::ProfileVerticalCurveCalculator::rebuild(verticalCurve);
    if (built.succeeded) {
        for (const auto& element : built.elements) {
            addStationInRange(stations, element.bvcStation, effectiveRange);
            addStationInRange(stations, element.evcStation, effectiveRange);
            if (element.highLowPoint.has_value()) {
                addStationInRange(stations, element.highLowPoint->station, effectiveRange);
            }
        }
    }

    return sortedUniqueStations(filterTemplateCoveredStations(stations, assignments));
}

RoadModelBuildResult RoadModelBuilder::build(const RoadModelBuildInput& input)
{
    RoadModelBuildResult result;
    result.data.config = input.config;
    return result;
}

} // namespace roadproto::domain::cross_section
