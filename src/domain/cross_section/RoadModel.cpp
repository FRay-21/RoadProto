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

struct AlignmentFrame {
    double station = 0.0;
    double x = 0.0;
    double y = 0.0;
    double tangentX = 1.0;
    double tangentY = 0.0;
    double normalX = 0.0;
    double normalY = 1.0;
};

struct ActiveBoundaryPoint {
    RoadModelLineKey key;
    SubgradeTemplateRgbColor color;
    RoadModelPoint3d point;
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

bool sameLineKey(const RoadModelLineKey& lhs, const RoadModelLineKey& rhs)
{
    return lhs.templateHandle == rhs.templateHandle &&
        lhs.side == rhs.side &&
        lhs.componentType == rhs.componentType &&
        lhs.componentIndex == rhs.componentIndex &&
        lhs.boundaryIndex == rhs.boundaryIndex;
}

bool samePoint(const RoadModelPoint3d& lhs, const RoadModelPoint3d& rhs)
{
    return std::fabs(lhs.x - rhs.x) <= 1.0e-9 &&
        std::fabs(lhs.y - rhs.y) <= 1.0e-9 &&
        std::fabs(lhs.z - rhs.z) <= 1.0e-9;
}

std::vector<alignment::AlignmentSamplePoint> sortedAlignmentSamples(
    std::vector<alignment::AlignmentSamplePoint> samples)
{
    std::sort(
        samples.begin(),
        samples.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.station < rhs.station;
        });
    return samples;
}

std::optional<AlignmentFrame> interpolateAlignmentFrame(
    const std::vector<alignment::AlignmentSamplePoint>& samples,
    double station)
{
    if (samples.size() < 2 || !isFinite(station)) {
        return std::nullopt;
    }

    const auto upper = std::lower_bound(
        samples.begin(),
        samples.end(),
        station,
        [](const auto& sample, double value) {
            return sample.station < value;
        });

    std::size_t rightIndex = 0;
    if (upper == samples.begin()) {
        rightIndex = 1;
    } else if (upper == samples.end()) {
        rightIndex = samples.size() - 1;
    } else {
        rightIndex = static_cast<std::size_t>(upper - samples.begin());
    }
    const std::size_t leftIndex = rightIndex - 1;

    const auto& left = samples[leftIndex];
    const auto& right = samples[rightIndex];
    const double span = right.station - left.station;
    if (!isFinite(span) || span <= 0.0) {
        return std::nullopt;
    }

    const double t = std::clamp((station - left.station) / span, 0.0, 1.0);
    const double dx = right.point.x - left.point.x;
    const double dy = right.point.y - left.point.y;
    const double length = std::hypot(dx, dy);
    if (!isFinite(length) || length <= 0.0) {
        return std::nullopt;
    }

    const double tangentX = dx / length;
    const double tangentY = dy / length;
    return AlignmentFrame{
        station,
        left.point.x + dx * t,
        left.point.y + dy * t,
        tangentX,
        tangentY,
        -tangentY,
        tangentX};
}

const RoadModelTemplateSource* findTemplate(
    const std::vector<RoadModelTemplateSource>& templates,
    const std::wstring& templateHandle)
{
    const auto found = std::find_if(
        templates.begin(),
        templates.end(),
        [&templateHandle](const auto& source) {
            return source.templateHandle == templateHandle;
        });
    return found == templates.end() ? nullptr : &*found;
}

std::vector<const RoadModelTemplateAssignment*> activeAssignmentsAtStation(
    const RoadModelTemplateResolver& resolver,
    const std::vector<RoadModelTemplateAssignment>& assignments,
    double station)
{
    std::vector<const RoadModelTemplateAssignment*> result;
    if (const auto* resolved = resolver.resolve(station)) {
        result.push_back(resolved);
    }

    for (const auto& assignment : assignments) {
        const bool atBoundary =
            std::fabs(station - assignment.startStation) <= kStationTolerance ||
            std::fabs(station - assignment.endStation) <= kStationTolerance;
        if (!atBoundary || station < assignment.startStation - kStationTolerance ||
            station > assignment.endStation + kStationTolerance) {
            continue;
        }

        const bool alreadyAdded = std::any_of(
            result.begin(),
            result.end(),
            [&assignment](const auto* item) {
                return item == &assignment || item->templateHandle == assignment.templateHandle;
            });
        if (!alreadyAdded) {
            result.push_back(&assignment);
        }
    }

    return result;
}

std::vector<const SubgradeTemplateComponent*> componentsForSide(
    const SubgradeTemplateData& data,
    SubgradeSide side)
{
    std::vector<const SubgradeTemplateComponent*> result;
    for (const auto& component : data.components) {
        if (component.side == side) {
            result.push_back(&component);
        }
    }
    return result;
}

RoadModelPoint3d pointAtOffset(
    const AlignmentFrame& frame,
    double centerElevation,
    SubgradeSide side,
    double offset,
    double elevationOffset)
{
    const double sideSign = side == SubgradeSide::Left ? 1.0 : -1.0;
    return RoadModelPoint3d{
        frame.x + frame.normalX * sideSign * offset,
        frame.y + frame.normalY * sideSign * offset,
        centerElevation + elevationOffset};
}

void appendTemplateBoundaryPoints(
    std::vector<ActiveBoundaryPoint>& points,
    const RoadModelTemplateAssignment& assignment,
    const SubgradeTemplateData& data,
    const AlignmentFrame& frame,
    double centerElevation)
{
    for (const auto side : {SubgradeSide::Left, SubgradeSide::Right}) {
        double offset = 0.0;
        double elevationOffset = 0.0;
        const auto components = componentsForSide(data, side);
        for (std::size_t index = 0; index < components.size(); ++index) {
            const auto& component = *components[index];
            const double width = SubgradeTemplateRules::widthAtStation(component, frame.station);
            const double slope = SubgradeTemplateRules::slopeAtStation(component, frame.station);
            const double innerElevationOffset = elevationOffset;
            const double outerElevationOffset =
                elevationOffset + component.height + std::fabs(width) * slope;

            points.push_back(
                ActiveBoundaryPoint{
                    RoadModelLineKey{
                        assignment.templateHandle,
                        side,
                        component.type,
                        index,
                        0},
                    component.color,
                    pointAtOffset(frame, centerElevation, side, offset, innerElevationOffset)});
            points.push_back(
                ActiveBoundaryPoint{
                    RoadModelLineKey{
                        assignment.templateHandle,
                        side,
                        component.type,
                        index,
                        1},
                    component.color,
                    pointAtOffset(frame, centerElevation, side, offset + width, outerElevationOffset)});

            offset += width;
            elevationOffset = outerElevationOffset;
        }
    }
}

void appendBoundaryPoint(
    std::vector<RoadModelComponentLine>& lines,
    const ActiveBoundaryPoint& previous,
    const ActiveBoundaryPoint& current)
{
    for (auto& line : lines) {
        if (sameLineKey(line.key, current.key) &&
            !line.points.empty() &&
            samePoint(line.points.back(), previous.point)) {
            line.points.push_back(current.point);
            return;
        }
    }

    RoadModelComponentLine line;
    line.key = current.key;
    line.color = current.color;
    line.points.push_back(previous.point);
    line.points.push_back(current.point);
    lines.push_back(std::move(line));
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

    if (!RoadModelRules::isSupportedSampleInterval(input.config.sampleInterval)) {
        result.errorMessage = L"Road model sample interval must be positive.";
        return result;
    }

    if (input.alignmentSamples.size() < 2) {
        result.errorMessage = L"Road model requires at least two alignment samples.";
        return result;
    }

    std::wstring assignmentError;
    if (!RoadModelRules::validateAssignments(input.config.assignments, assignmentError)) {
        result.errorMessage = assignmentError;
        return result;
    }

    const auto builtVertical = profile::ProfileVerticalCurveCalculator::rebuild(input.verticalCurve);
    if (!builtVertical.succeeded) {
        result.errorMessage = builtVertical.errorMessage.empty()
            ? L"Road model vertical curve rebuild failed."
            : builtVertical.errorMessage;
        return result;
    }

    const auto alignmentSamples = sortedAlignmentSamples(input.alignmentSamples);
    const double alignmentStart = alignmentSamples.front().station;
    const double alignmentEnd = alignmentSamples.back().station;
    if (!isFinite(alignmentStart) || !isFinite(alignmentEnd) || alignmentEnd <= alignmentStart) {
        result.errorMessage = L"Road model alignment station range is invalid.";
        return result;
    }

    const auto sampledStations = RoadModelStationSampler::collectStations(
        alignmentStart,
        alignmentEnd,
        input.verticalCurve,
        input.config.assignments,
        input.config.sampleInterval);
    if (sampledStations.empty()) {
        result.errorMessage = L"Road model has no stations covered by alignment, vertical curve, and templates.";
        return result;
    }
    result.sampledStations = sampledStations;

    RoadModelTemplateResolver resolver(input.config.assignments);
    std::vector<ActiveBoundaryPoint> previousPoints;

    for (const double station : sampledStations) {
        const auto activeAssignments = activeAssignmentsAtStation(resolver, input.config.assignments, station);
        if (activeAssignments.empty()) {
            previousPoints.clear();
            continue;
        }

        const auto frame = interpolateAlignmentFrame(alignmentSamples, station);
        if (!frame.has_value()) {
            previousPoints.clear();
            continue;
        }

        const auto elevation = profile::ProfileVerticalCurveCalculator::elevationAt(builtVertical, station);
        if (!elevation.succeeded) {
            previousPoints.clear();
            continue;
        }

        std::vector<ActiveBoundaryPoint> currentPoints;
        for (const auto* assignment : activeAssignments) {
            const auto* templateSource = findTemplate(input.templates, assignment->templateHandle);
            if (templateSource == nullptr) {
                result.errorMessage = L"Road model template source was not found.";
                result.data.componentLines.clear();
                result.sampledStations.clear();
                return result;
            }

            appendTemplateBoundaryPoints(
                currentPoints,
                *assignment,
                templateSource->data,
                *frame,
                elevation.value);
        }

        for (const auto& current : currentPoints) {
            const auto previous = std::find_if(
                previousPoints.begin(),
                previousPoints.end(),
                [&current](const auto& point) {
                    return sameLineKey(point.key, current.key);
                });
            if (previous != previousPoints.end()) {
                appendBoundaryPoint(result.data.componentLines, *previous, current);
            }
        }

        previousPoints = std::move(currentPoints);
    }

    if (result.data.componentLines.empty()) {
        result.errorMessage = L"Road model produced no component boundary lines.";
        return result;
    }

    result.succeeded = true;
    result.errorMessage.clear();
    return result;
}

} // namespace roadproto::domain::cross_section
