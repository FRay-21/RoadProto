#include "domain/cross_section/RoadModel.h"

#include "domain/profile/ProfileVerticalCurveCalculator.h"
#include "domain/terrain/TerrainTriangleSpatialIndex.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <utility>

namespace roadproto::domain::cross_section {
namespace {

bool isFinite(double value)
{
    return std::isfinite(value);
}

constexpr double kStationTolerance = 1.0e-7;

void reportProgress(const RoadModelBuildInput& input, int percent, const std::wstring& message)
{
    if (!input.progressCallback) {
        return;
    }
    input.progressCallback(std::clamp(percent, 0, 100), message);
}

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
    double station = 0.0;
    double offset = 0.0;
    double elevation = 0.0;
};

struct ActiveSlopeBoundaryPoint {
    RoadModelSlopeLineKey key;
    SlopeTemplateRgbColor color;
    RoadModelPoint3d point;
    double station = 0.0;
    double offset = 0.0;
    double elevation = 0.0;
};

struct ActivePavementLayerBoundaryPoint {
    RoadModelPavementLayerLineKey key;
    RoadModelWireColor color;
    RoadModelPoint3d point;
    double station = 0.0;
    double offset = 0.0;
    double elevation = 0.0;
    std::wstring label;
};

struct RoadModelLineAccumulator {
    RoadModelComponentLine line;
    double lastStation = 0.0;
};

struct RoadModelSlopeLineAccumulator {
    RoadModelSlopeComponentLine line;
    double lastStation = 0.0;
};

struct RoadModelPavementLayerLineAccumulator {
    RoadModelPavementLayerLine line;
    double lastStation = 0.0;
};

struct SubgradeOuterEdge {
    double offset = 0.0;
    double elevationOffset = 0.0;
};

struct GroundProfilePoint {
    double offset = 0.0;
    double elevation = 0.0;
};

struct SectionGroundProfiles {
    double station = 0.0;
    std::vector<GroundProfilePoint> left;
    std::vector<GroundProfilePoint> right;
};

struct LocalSlopePoint {
    double offset = 0.0;
    double elevation = 0.0;
};

struct LocalSlopeBoundaryPoint {
    RoadModelSlopeLineKey key;
    SlopeTemplateRgbColor color;
    LocalSlopePoint point;
};

struct PreviewBoundarySample {
    RoadModelSectionPreviewSegmentKind kind = RoadModelSectionPreviewSegmentKind::Subgrade;
    std::wstring templateHandle;
    SubgradeSide side = SubgradeSide::Right;
    int componentType = 0;
    std::size_t groupIndex = 0;
    std::size_t templateOrder = 0;
    std::size_t componentIndex = 0;
    std::size_t boundaryIndex = 0;
    RoadModelSectionPreviewColor color;
    RoadModelSectionPreviewPoint point;
};

struct SlopeTemplateAttempt {
    bool succeeded = false;
    std::wstring errorMessage;
    std::wstring templateHandle;
    SubgradeSide side = SubgradeSide::Right;
    std::size_t groupIndex = 0;
    std::size_t templateOrder = 0;
    std::vector<LocalSlopeBoundaryPoint> points;
};

std::vector<GroundProfilePoint> buildGroundProfile(
    const AlignmentFrame& frame,
    SubgradeSide side,
    double searchWidth,
    const RoadModelTerrainSurface& surface,
    const roadproto::domain::terrain::TerrainTriangleSpatialIndex* terrainIndex = nullptr,
    RoadModelBuildResult::Diagnostics* diagnostics = nullptr);

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

bool sameLineKey(const RoadModelSlopeLineKey& lhs, const RoadModelSlopeLineKey& rhs)
{
    return lhs.templateHandle == rhs.templateHandle &&
        lhs.side == rhs.side &&
        lhs.componentType == rhs.componentType &&
        lhs.groupIndex == rhs.groupIndex &&
        lhs.templateOrder == rhs.templateOrder &&
        lhs.componentIndex == rhs.componentIndex &&
        lhs.boundaryIndex == rhs.boundaryIndex;
}

bool sameLineKey(const RoadModelPavementLayerLineKey& lhs, const RoadModelPavementLayerLineKey& rhs)
{
    return lhs.subgradeTemplateHandle == rhs.subgradeTemplateHandle &&
        lhs.pavementLayerTemplateHandle == rhs.pavementLayerTemplateHandle &&
        lhs.side == rhs.side &&
        lhs.componentIndex == rhs.componentIndex &&
        lhs.layerIndex == rhs.layerIndex &&
        lhs.boundaryIndex == rhs.boundaryIndex;
}

bool samePreviewComponentKey(const PreviewBoundarySample& lhs, const PreviewBoundarySample& rhs)
{
    return lhs.kind == rhs.kind &&
        lhs.templateHandle == rhs.templateHandle &&
        lhs.side == rhs.side &&
        lhs.componentType == rhs.componentType &&
        lhs.groupIndex == rhs.groupIndex &&
        lhs.templateOrder == rhs.templateOrder &&
        lhs.componentIndex == rhs.componentIndex;
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

std::optional<std::vector<alignment::AlignmentSamplePoint>> normalizeAlignmentSamples(
    const std::vector<alignment::AlignmentSamplePoint>& input,
    std::wstring& errorMessage)
{
    errorMessage.clear();
    if (input.size() < 2) {
        errorMessage = L"Road model requires at least two alignment samples.";
        return std::nullopt;
    }

    for (const auto& sample : input) {
        if (!isFinite(sample.station) || !isFinite(sample.point.x) || !isFinite(sample.point.y)) {
            errorMessage = L"Road model alignment samples must contain finite station and XY values.";
            return std::nullopt;
        }
    }

    for (std::size_t i = 1; i < input.size(); ++i) {
        const auto& previous = input[i - 1];
        const auto& current = input[i];
        if (current.station - previous.station <= kStationTolerance) {
            errorMessage = L"Road model alignment sample stations must be strictly increasing.";
            return std::nullopt;
        }

        const double dx = current.point.x - previous.point.x;
        const double dy = current.point.y - previous.point.y;
        if (std::hypot(dx, dy) <= kStationTolerance) {
            errorMessage = L"Road model alignment samples must not contain zero-length segments.";
            return std::nullopt;
        }
    }

    return input;
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

std::optional<double> projectedStationOnAlignment(
    const std::vector<alignment::AlignmentSamplePoint>& samples,
    const RoadModelPoint3d& point)
{
    if (samples.size() < 2 || !isFinite(point.x) || !isFinite(point.y)) {
        return std::nullopt;
    }

    double bestDistance = std::numeric_limits<double>::infinity();
    double bestStation = 0.0;
    for (std::size_t i = 1; i < samples.size(); ++i) {
        const auto& start = samples[i - 1];
        const auto& end = samples[i];
        const double dx = end.point.x - start.point.x;
        const double dy = end.point.y - start.point.y;
        const double lengthSquared = dx * dx + dy * dy;
        const double stationSpan = end.station - start.station;
        if (!isFinite(lengthSquared) || !isFinite(stationSpan) ||
            lengthSquared <= kStationTolerance || stationSpan <= kStationTolerance) {
            continue;
        }

        const double t = std::clamp(
            ((point.x - start.point.x) * dx + (point.y - start.point.y) * dy) / lengthSquared,
            0.0,
            1.0);
        const double projectedX = start.point.x + dx * t;
        const double projectedY = start.point.y + dy * t;
        const double distance = std::hypot(point.x - projectedX, point.y - projectedY);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestStation = start.station + stationSpan * t;
        }
    }

    if (!isFinite(bestDistance)) {
        return std::nullopt;
    }
    return bestStation;
}

std::optional<RoadModelPoint3d> interpolateLinePointAtStation(
    const std::vector<RoadModelPoint3d>& points,
    const std::vector<alignment::AlignmentSamplePoint>& alignmentSamples,
    double station)
{
    if (points.empty() || !isFinite(station)) {
        return std::nullopt;
    }

    std::vector<std::pair<double, RoadModelPoint3d>> stationPoints;
    stationPoints.reserve(points.size());
    for (const auto& point : points) {
        if (const auto pointStation = projectedStationOnAlignment(alignmentSamples, point)) {
            stationPoints.push_back({*pointStation, point});
        }
    }

    if (stationPoints.empty()) {
        return std::nullopt;
    }
    std::sort(
        stationPoints.begin(),
        stationPoints.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });

    for (const auto& stationPoint : stationPoints) {
        if (std::fabs(stationPoint.first - station) <= kStationTolerance) {
            return stationPoint.second;
        }
    }

    for (std::size_t i = 1; i < stationPoints.size(); ++i) {
        const auto& start = stationPoints[i - 1];
        const auto& end = stationPoints[i];
        if (station < start.first - kStationTolerance || station > end.first + kStationTolerance) {
            continue;
        }

        const double span = end.first - start.first;
        if (span <= kStationTolerance) {
            continue;
        }
        const double t = std::clamp((station - start.first) / span, 0.0, 1.0);
        return RoadModelPoint3d{
            start.second.x + (end.second.x - start.second.x) * t,
            start.second.y + (end.second.y - start.second.y) * t,
            start.second.z + (end.second.z - start.second.z) * t};
    }

    return std::nullopt;
}

RoadModelSectionPreviewPoint toPreviewPoint(
    const AlignmentFrame& frame,
    const RoadModelPoint3d& point)
{
    return RoadModelSectionPreviewPoint{
        (point.x - frame.x) * frame.normalX + (point.y - frame.y) * frame.normalY,
        point.z};
}

RoadModelSectionPreviewColor toPreviewColor(const SubgradeTemplateRgbColor& color)
{
    return RoadModelSectionPreviewColor{color.r, color.g, color.b};
}

RoadModelSectionPreviewColor toPreviewColor(const SlopeTemplateRgbColor& color)
{
    return RoadModelSectionPreviewColor{color.r, color.g, color.b};
}

RoadModelSectionPreviewColor toPreviewColor(const RoadModelWireColor& color)
{
    return RoadModelSectionPreviewColor{color.r, color.g, color.b};
}

std::wstring previewLabelForKind(RoadModelSectionPreviewSegmentKind kind)
{
    if (kind == RoadModelSectionPreviewSegmentKind::Slope) {
        return L"边坡模板";
    }
    if (kind == RoadModelSectionPreviewSegmentKind::PavementLayer) {
        return L"路面结构层";
    }
    return L"路基模板";
}

RoadModelSectionPreviewSegmentKind previewKindFromSectionNode(const RoadModelSectionNode& node)
{
    if (node.kind == RoadModelSectionNodeKind::Ground) {
        return RoadModelSectionPreviewSegmentKind::Ground;
    }
    if (node.kind == RoadModelSectionNodeKind::PavementLayer) {
        return RoadModelSectionPreviewSegmentKind::PavementLayer;
    }
    return node.kind == RoadModelSectionNodeKind::Slope
        ? RoadModelSectionPreviewSegmentKind::Slope
        : RoadModelSectionPreviewSegmentKind::Subgrade;
}

bool isFinitePreviewPoint(const RoadModelSectionPreviewPoint& point)
{
    return isFinite(point.offset) && isFinite(point.elevation);
}

void appendSectionNodePreviewSegment(
    RoadModelSectionPreview& preview,
    const RoadModelSectionNode& start,
    const RoadModelSectionNode& end)
{
    if (!isFinite(start.offset) || !isFinite(start.elevation) ||
        !isFinite(end.offset) || !isFinite(end.elevation)) {
        return;
    }

    RoadModelSectionPreviewSegment segment;
    segment.kind = previewKindFromSectionNode(end);
    segment.color = toPreviewColor(end.color);
    segment.label = previewLabelForKind(segment.kind);
    segment.points = {
        RoadModelSectionPreviewPoint{start.offset, start.elevation},
        RoadModelSectionPreviewPoint{end.offset, end.elevation}};
    preview.segments.push_back(std::move(segment));
}

void appendSectionNodePreviewSegments(
    RoadModelSectionPreview& preview,
    const std::vector<RoadModelSectionNode>& nodes)
{
    for (std::size_t i = 1; i < nodes.size(); ++i) {
        appendSectionNodePreviewSegment(preview, nodes[i - 1], nodes[i]);
    }
}

void appendPavementLayerSectionPreviewSegments(
    RoadModelSectionPreview& preview,
    const std::vector<RoadModelSectionNode>& nodes)
{
    for (std::size_t i = 0; i + 3 < nodes.size(); i += 4) {
        appendSectionNodePreviewSegment(preview, nodes[i], nodes[i + 1]);
        appendSectionNodePreviewSegment(preview, nodes[i + 1], nodes[i + 3]);
        appendSectionNodePreviewSegment(preview, nodes[i + 3], nodes[i + 2]);
        appendSectionNodePreviewSegment(preview, nodes[i + 2], nodes[i]);
    }
}

const RoadModelSection* findSectionAtStation(
    const std::vector<RoadModelSection>& sections,
    double station)
{
    const RoadModelSection* best = nullptr;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (const auto& section : sections) {
        const double distance = std::fabs(section.station - station);
        if (distance < bestDistance) {
            best = &section;
            bestDistance = distance;
        }
    }
    return bestDistance <= kStationTolerance ? best : nullptr;
}

void appendPreviewSegmentPairs(
    RoadModelSectionPreview& preview,
    const std::vector<PreviewBoundarySample>& samples)
{
    for (const auto& inner : samples) {
        if (inner.boundaryIndex != 0 ||
            inner.kind == RoadModelSectionPreviewSegmentKind::PavementLayer) {
            continue;
        }

        const auto outer = std::find_if(
            samples.begin(),
            samples.end(),
            [&inner](const auto& candidate) {
                return candidate.boundaryIndex == 1 && samePreviewComponentKey(inner, candidate);
            });
        if (outer == samples.end()) {
            continue;
        }

        RoadModelSectionPreviewSegment segment;
        segment.kind = inner.kind;
        segment.color = inner.color;
        segment.label = previewLabelForKind(inner.kind);
        segment.points = {inner.point, outer->point};
        preview.segments.push_back(std::move(segment));
    }
}

const PreviewBoundarySample* findPreviewBoundarySample(
    const std::vector<PreviewBoundarySample>& samples,
    const PreviewBoundarySample& anchor,
    std::size_t boundaryIndex)
{
    const auto found = std::find_if(
        samples.begin(),
        samples.end(),
        [&anchor, boundaryIndex](const auto& candidate) {
            return candidate.boundaryIndex == boundaryIndex &&
                samePreviewComponentKey(anchor, candidate);
        });
    return found == samples.end() ? nullptr : &*found;
}

void appendPreviewBoundarySampleSegment(
    RoadModelSectionPreview& preview,
    const PreviewBoundarySample& start,
    const PreviewBoundarySample& end)
{
    if (!isFinitePreviewPoint(start.point) || !isFinitePreviewPoint(end.point)) {
        return;
    }

    RoadModelSectionPreviewSegment segment;
    segment.kind = start.kind;
    segment.color = start.color;
    segment.label = previewLabelForKind(segment.kind);
    segment.points = {start.point, end.point};
    preview.segments.push_back(std::move(segment));
}

void appendPavementLayerPreviewSampleSegments(
    RoadModelSectionPreview& preview,
    const std::vector<PreviewBoundarySample>& samples)
{
    for (const auto& topInner : samples) {
        if (topInner.kind != RoadModelSectionPreviewSegmentKind::PavementLayer ||
            topInner.boundaryIndex != 0) {
            continue;
        }

        const auto* topOuter = findPreviewBoundarySample(samples, topInner, 1);
        const auto* bottomInner = findPreviewBoundarySample(samples, topInner, 2);
        const auto* bottomOuter = findPreviewBoundarySample(samples, topInner, 3);
        if (topOuter == nullptr || bottomInner == nullptr || bottomOuter == nullptr) {
            continue;
        }

        appendPreviewBoundarySampleSegment(preview, topInner, *topOuter);
        appendPreviewBoundarySampleSegment(preview, *topOuter, *bottomOuter);
        appendPreviewBoundarySampleSegment(preview, *bottomOuter, *bottomInner);
        appendPreviewBoundarySampleSegment(preview, *bottomInner, topInner);
    }
}

std::vector<RoadModelGroundProfilePoint> toGroundProfileSnapshot(
    const std::vector<GroundProfilePoint>& groundProfile)
{
    std::vector<RoadModelGroundProfilePoint> snapshot;
    snapshot.reserve(groundProfile.size());
    for (const auto& point : groundProfile) {
        snapshot.push_back({point.offset, point.elevation});
    }
    return snapshot;
}

void appendGroundPreviewPointsFromProfile(
    std::vector<RoadModelSectionPreviewPoint>& groundPoints,
    const std::vector<RoadModelGroundProfilePoint>& groundProfile,
    double sign)
{
    for (const auto& point : groundProfile) {
        if (!isFinite(point.offset) || !isFinite(point.elevation)) {
            continue;
        }
        groundPoints.push_back({sign * point.offset, point.elevation});
    }
}

void appendGroundPreviewPointsFromProfile(
    std::vector<RoadModelSectionPreviewPoint>& groundPoints,
    const std::vector<GroundProfilePoint>& groundProfile,
    double sign)
{
    for (const auto& point : groundProfile) {
        if (!isFinite(point.offset) || !isFinite(point.elevation)) {
            continue;
        }
        groundPoints.push_back({sign * point.offset, point.elevation});
    }
}

void appendGroundPreviewSegment(
    RoadModelSectionPreview& preview,
    const RoadModelSectionPreviewRequest& request,
    const AlignmentFrame& frame,
    const RoadModelSection* section)
{
    std::vector<RoadModelSectionPreviewPoint> groundPoints;
    if (section != nullptr &&
        (!section->leftGroundProfile.empty() || !section->rightGroundProfile.empty())) {
        appendGroundPreviewPointsFromProfile(groundPoints, section->leftGroundProfile, 1.0);
        appendGroundPreviewPointsFromProfile(groundPoints, section->rightGroundProfile, -1.0);
    }

    if (groundPoints.empty()) {
        const auto leftGround = buildGroundProfile(
            frame,
            SubgradeSide::Left,
            request.data.config.slopeConfig.leftSearchWidth,
            request.terrainSurface);
        appendGroundPreviewPointsFromProfile(groundPoints, leftGround, 1.0);

        const auto rightGround = buildGroundProfile(
            frame,
            SubgradeSide::Right,
            request.data.config.slopeConfig.rightSearchWidth,
            request.terrainSurface);
        appendGroundPreviewPointsFromProfile(groundPoints, rightGround, -1.0);
    }

    std::sort(
        groundPoints.begin(),
        groundPoints.end(),
        [](const auto& lhs, const auto& rhs) {
            if (std::fabs(lhs.offset - rhs.offset) > kStationTolerance) {
                return lhs.offset < rhs.offset;
            }
            return lhs.elevation < rhs.elevation;
        });

    std::vector<RoadModelSectionPreviewPoint> unique;
    for (const auto& point : groundPoints) {
        if (unique.empty() || std::fabs(unique.back().offset - point.offset) > kStationTolerance) {
            unique.push_back(point);
        } else {
            unique.back().elevation = (unique.back().elevation + point.elevation) * 0.5;
        }
    }

    if (unique.size() < 2) {
        preview.statusMessage = L"未找到地面线。";
        return;
    }

    RoadModelSectionPreviewSegment segment;
    segment.kind = RoadModelSectionPreviewSegmentKind::Ground;
    segment.color = {132, 96, 56};
    segment.label = L"地面线";
    segment.points = std::move(unique);
    preview.hasGroundLine = true;
    preview.segments.push_back(std::move(segment));
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

const RoadModelTemplateSource* findOrNormalizeTemplate(
    const std::vector<RoadModelTemplateSource>& templates,
    std::vector<RoadModelTemplateSource>& normalizedTemplates,
    const std::wstring& templateHandle,
    std::wstring& errorMessage)
{
    if (const auto* cached = findTemplate(normalizedTemplates, templateHandle)) {
        return cached;
    }

    const auto* source = findTemplate(templates, templateHandle);
    if (source == nullptr) {
        errorMessage = L"Road model template source was not found: " + templateHandle;
        return nullptr;
    }

    RoadModelTemplateSource normalized = *source;
    std::wstring normalizeError;
    if (!SubgradeTemplateRules::normalize(normalized.data, normalizeError)) {
        errorMessage = L"Road model template source is invalid (" + templateHandle + L"): " + normalizeError;
        return nullptr;
    }

    normalizedTemplates.push_back(std::move(normalized));
    return &normalizedTemplates.back();
}

const RoadModelSlopeTemplateSource* findSlopeTemplate(
    const std::vector<RoadModelSlopeTemplateSource>& templates,
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

const RoadModelSlopeTemplateSource* findOrNormalizeSlopeTemplate(
    const std::vector<RoadModelSlopeTemplateSource>& templates,
    std::vector<RoadModelSlopeTemplateSource>& normalizedTemplates,
    const std::wstring& templateHandle,
    std::wstring& errorMessage)
{
    if (const auto* cached = findSlopeTemplate(normalizedTemplates, templateHandle)) {
        return cached;
    }

    const auto* source = findSlopeTemplate(templates, templateHandle);
    if (source == nullptr) {
        errorMessage = L"Road model slope template source was not found: " + templateHandle;
        return nullptr;
    }

    RoadModelSlopeTemplateSource normalized = *source;
    std::wstring normalizeError;
    if (!SlopeTemplateRules::normalize(normalized.data, normalizeError)) {
        errorMessage = L"Road model slope template source is invalid (" + templateHandle + L"): " + normalizeError;
        return nullptr;
    }

    normalizedTemplates.push_back(std::move(normalized));
    return &normalizedTemplates.back();
}

const RoadModelPavementLayerTemplateSource* findPavementLayerTemplate(
    const std::vector<RoadModelPavementLayerTemplateSource>& templates,
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

const RoadModelPavementLayerTemplateSource* findOrNormalizePavementLayerTemplate(
    const std::vector<RoadModelPavementLayerTemplateSource>& templates,
    std::vector<RoadModelPavementLayerTemplateSource>& normalizedTemplates,
    const std::wstring& templateHandle,
    std::wstring& errorMessage)
{
    if (const auto* cached = findPavementLayerTemplate(normalizedTemplates, templateHandle)) {
        return cached;
    }

    const auto* source = findPavementLayerTemplate(templates, templateHandle);
    if (source == nullptr) {
        errorMessage = L"Road model pavement layer template source was not found: " + templateHandle;
        return nullptr;
    }

    RoadModelPavementLayerTemplateSource normalized = *source;
    std::wstring normalizeError;
    if (!PavementLayerTemplateRules::normalize(normalized.data, normalizeError)) {
        errorMessage = L"Road model pavement layer template source is invalid (" + templateHandle + L"): " + normalizeError;
        return nullptr;
    }

    normalizedTemplates.push_back(std::move(normalized));
    return &normalizedTemplates.back();
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

double signedOffset(SubgradeSide side, double offset)
{
    return side == SubgradeSide::Left ? offset : -offset;
}

RoadModelWireColor toWireColor(const SubgradeTemplateRgbColor& color)
{
    return RoadModelWireColor{color.r, color.g, color.b};
}

RoadModelWireColor toWireColor(const SlopeTemplateRgbColor& color)
{
    return RoadModelWireColor{color.r, color.g, color.b};
}

SubgradeOuterEdge subgradeOuterEdgeAtStation(
    const SubgradeTemplateData& data,
    SubgradeSide side,
    double station)
{
    SubgradeOuterEdge edge;
    const auto components = componentsForSide(data, side);
    for (const auto* component : components) {
        const double width = SubgradeTemplateRules::widthAtStation(*component, station);
        const double slope = SubgradeTemplateRules::slopeAtStation(*component, station);
        edge.offset += width;
        edge.elevationOffset += component->height + std::fabs(width) * slope;
    }
    return edge;
}

double cross2(double ax, double ay, double bx, double by)
{
    return ax * by - ay * bx;
}

double dot2(double ax, double ay, double bx, double by)
{
    return ax * bx + ay * by;
}

void addGroundProfilePoint(
    std::vector<GroundProfilePoint>& points,
    double offset,
    double elevation,
    double searchWidth)
{
    if (!isFinite(offset) || !isFinite(elevation)) {
        return;
    }
    if (offset < -kStationTolerance || offset > searchWidth + kStationTolerance) {
        return;
    }
    points.push_back({std::clamp(offset, 0.0, searchWidth), elevation});
}

void addEdgeCrossSectionIntersections(
    std::vector<GroundProfilePoint>& points,
    const AlignmentFrame& frame,
    SubgradeSide side,
    double searchWidth,
    const terrain::TinPoint& a,
    const terrain::TinPoint& b)
{
    const double sideSign = side == SubgradeSide::Left ? 1.0 : -1.0;
    const double dx = frame.normalX * sideSign;
    const double dy = frame.normalY * sideSign;
    const double ex = b.x - a.x;
    const double ey = b.y - a.y;
    const double denom = cross2(dx, dy, ex, ey);
    const double ax = a.x - frame.x;
    const double ay = a.y - frame.y;

    if (std::fabs(denom) > kStationTolerance) {
        const double offset = cross2(ax, ay, ex, ey) / denom;
        const double t = cross2(ax, ay, dx, dy) / denom;
        if (t >= -kStationTolerance && t <= 1.0 + kStationTolerance) {
            const double z = a.z + (b.z - a.z) * std::clamp(t, 0.0, 1.0);
            addGroundProfilePoint(points, offset, z, searchWidth);
        }
        return;
    }

    if (std::fabs(cross2(ax, ay, dx, dy)) > kStationTolerance) {
        return;
    }

    const double aOffset = dot2(ax, ay, dx, dy);
    const double bOffset = dot2(b.x - frame.x, b.y - frame.y, dx, dy);
    addGroundProfilePoint(points, aOffset, a.z, searchWidth);
    addGroundProfilePoint(points, bOffset, b.z, searchWidth);
}

std::vector<GroundProfilePoint> buildGroundProfile(
    const AlignmentFrame& frame,
    SubgradeSide side,
    double searchWidth,
    const RoadModelTerrainSurface& surface,
    const roadproto::domain::terrain::TerrainTriangleSpatialIndex* terrainIndex,
    RoadModelBuildResult::Diagnostics* diagnostics)
{
    std::vector<GroundProfilePoint> points;
    if (!RoadModelRules::isSupportedSearchWidth(searchWidth) ||
        surface.points.empty() || surface.triangles.empty()) {
        return points;
    }

    const auto addTriangle = [&](std::size_t triangleIndex) {
        if (triangleIndex >= surface.triangles.size()) {
            return;
        }
        const auto& triangle = surface.triangles[triangleIndex];
        if (triangle.a >= surface.points.size() ||
            triangle.b >= surface.points.size() ||
            triangle.c >= surface.points.size()) {
            return;
        }

        const auto& a = surface.points[triangle.a];
        const auto& b = surface.points[triangle.b];
        const auto& c = surface.points[triangle.c];
        addEdgeCrossSectionIntersections(points, frame, side, searchWidth, a, b);
        addEdgeCrossSectionIntersections(points, frame, side, searchWidth, b, c);
        addEdgeCrossSectionIntersections(points, frame, side, searchWidth, c, a);
    };

    if (terrainIndex != nullptr && terrainIndex->enabled()) {
        const double sideSign = side == SubgradeSide::Left ? 1.0 : -1.0;
        const double endX = frame.x + frame.normalX * sideSign * searchWidth;
        const double endY = frame.y + frame.normalY * sideSign * searchWidth;
        const auto candidates = terrainIndex->querySegment(frame.x, frame.y, endX, endY);
        if (diagnostics != nullptr) {
            ++diagnostics->groundProfileQueryCount;
            diagnostics->groundProfileCandidateTotal += candidates.size();
            diagnostics->groundProfileCandidateMax = std::max(
                diagnostics->groundProfileCandidateMax,
                candidates.size());
        }
        for (const auto triangleIndex : candidates) {
            addTriangle(triangleIndex);
        }
    } else {
        if (diagnostics != nullptr) {
            ++diagnostics->groundProfileQueryCount;
            diagnostics->groundProfileCandidateTotal += surface.triangles.size();
            diagnostics->groundProfileCandidateMax = std::max(
                diagnostics->groundProfileCandidateMax,
                surface.triangles.size());
        }
        for (std::size_t triangleIndex = 0; triangleIndex < surface.triangles.size(); ++triangleIndex) {
            addTriangle(triangleIndex);
        }
    }

    std::sort(
        points.begin(),
        points.end(),
        [](const auto& lhs, const auto& rhs) {
            if (std::fabs(lhs.offset - rhs.offset) > kStationTolerance) {
                return lhs.offset < rhs.offset;
            }
            return lhs.elevation < rhs.elevation;
        });

    std::vector<GroundProfilePoint> unique;
    for (const auto& point : points) {
        if (unique.empty() || std::fabs(unique.back().offset - point.offset) > kStationTolerance) {
            unique.push_back(point);
        } else {
            unique.back().elevation = (unique.back().elevation + point.elevation) * 0.5;
        }
    }
    return unique;
}

SectionGroundProfiles buildSectionGroundProfiles(
    const AlignmentFrame& frame,
    const RoadModelSlopeConfig& slopeConfig,
    const RoadModelTerrainSurface& terrainSurface,
    const roadproto::domain::terrain::TerrainTriangleSpatialIndex* terrainIndex,
    RoadModelBuildResult::Diagnostics* diagnostics)
{
    return SectionGroundProfiles{
        frame.station,
        buildGroundProfile(
            frame,
            SubgradeSide::Left,
            slopeConfig.leftSearchWidth,
            terrainSurface,
            terrainIndex,
            diagnostics),
        buildGroundProfile(
            frame,
            SubgradeSide::Right,
            slopeConfig.rightSearchWidth,
            terrainSurface,
            terrainIndex,
            diagnostics)};
}

std::optional<LocalSlopePoint> firstGroundIntersection(
    const LocalSlopePoint& start,
    const LocalSlopePoint& end,
    const std::vector<GroundProfilePoint>& groundProfile,
    double minSegmentParameter)
{
    if (groundProfile.size() < 2) {
        return std::nullopt;
    }

    const double rx = end.offset - start.offset;
    const double ry = end.elevation - start.elevation;
    if (std::hypot(rx, ry) <= kStationTolerance) {
        return std::nullopt;
    }

    double bestT = std::numeric_limits<double>::infinity();
    LocalSlopePoint bestPoint;
    for (std::size_t i = 1; i < groundProfile.size(); ++i) {
        const LocalSlopePoint groundStart{groundProfile[i - 1].offset, groundProfile[i - 1].elevation};
        const LocalSlopePoint groundEnd{groundProfile[i].offset, groundProfile[i].elevation};
        const double sx = groundEnd.offset - groundStart.offset;
        const double sy = groundEnd.elevation - groundStart.elevation;
        if (std::hypot(sx, sy) <= kStationTolerance) {
            continue;
        }

        const double qpx = groundStart.offset - start.offset;
        const double qpy = groundStart.elevation - start.elevation;
        const double denom = cross2(rx, ry, sx, sy);
        if (std::fabs(denom) <= kStationTolerance) {
            if (std::fabs(cross2(qpx, qpy, rx, ry)) > kStationTolerance) {
                continue;
            }
            const double rr = dot2(rx, ry, rx, ry);
            const double t0 = dot2(groundStart.offset - start.offset, groundStart.elevation - start.elevation, rx, ry) / rr;
            const double t1 = dot2(groundEnd.offset - start.offset, groundEnd.elevation - start.elevation, rx, ry) / rr;
            const double overlapStart = std::max(std::min(t0, t1), minSegmentParameter);
            const double overlapEnd = std::min(std::max(t0, t1), 1.0);
            if (overlapStart <= overlapEnd + kStationTolerance && overlapStart < bestT) {
                bestT = overlapStart;
                bestPoint = {start.offset + rx * overlapStart, start.elevation + ry * overlapStart};
            }
            continue;
        }

        const double t = cross2(qpx, qpy, sx, sy) / denom;
        const double u = cross2(qpx, qpy, rx, ry) / denom;
        if (t >= minSegmentParameter - kStationTolerance && t <= 1.0 + kStationTolerance &&
            u >= -kStationTolerance && u <= 1.0 + kStationTolerance &&
            t < bestT) {
            bestT = t;
            const double clampedT = std::clamp(t, 0.0, 1.0);
            bestPoint = {start.offset + rx * clampedT, start.elevation + ry * clampedT};
        }
    }

    if (isFinite(bestT)) {
        return bestPoint;
    }
    return std::nullopt;
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
                    pointAtOffset(frame, centerElevation, side, offset, innerElevationOffset),
                    frame.station,
                    signedOffset(side, offset),
                    centerElevation + innerElevationOffset});
            points.push_back(
                ActiveBoundaryPoint{
                    RoadModelLineKey{
                        assignment.templateHandle,
                        side,
                        component.type,
                        index,
                        1},
                    component.color,
                    pointAtOffset(frame, centerElevation, side, offset + width, outerElevationOffset),
                    frame.station,
                    signedOffset(side, offset + width),
                    centerElevation + outerElevationOffset});

            offset += width;
            elevationOffset = outerElevationOffset;
        }
    }
}

void appendPavementLayerBoundaryPoint(
    std::vector<ActivePavementLayerBoundaryPoint>& points,
    const RoadModelTemplateAssignment& assignment,
    const RoadModelPavementLayerTemplateSource& pavementSource,
    SubgradeSide side,
    std::size_t componentIndex,
    std::size_t layerIndex,
    std::size_t boundaryIndex,
    RoadModelWireColor color,
    const AlignmentFrame& frame,
    double centerElevation,
    double componentInnerOffset,
    const PavementLayerSectionPoint& sectionPoint,
    const std::wstring& label)
{
    const double absoluteOffset = componentInnerOffset + sectionPoint.offset;
    points.push_back(
        ActivePavementLayerBoundaryPoint{
            RoadModelPavementLayerLineKey{
                assignment.templateHandle,
                pavementSource.templateHandle,
                side,
                componentIndex,
                layerIndex,
                boundaryIndex},
            color,
            pointAtOffset(
                frame,
                centerElevation,
                side,
                absoluteOffset,
                sectionPoint.elevation - centerElevation),
            frame.station,
            signedOffset(side, absoluteOffset),
            sectionPoint.elevation,
            label});
}

bool appendPavementLayerBoundaryPoints(
    std::vector<ActivePavementLayerBoundaryPoint>& points,
    const RoadModelTemplateAssignment& assignment,
    const SubgradeTemplateData& data,
    const std::vector<RoadModelPavementLayerTemplateSource>& pavementLayerTemplates,
    std::vector<RoadModelPavementLayerTemplateSource>& normalizedPavementLayerTemplates,
    const AlignmentFrame& frame,
    double centerElevation,
    std::wstring& errorMessage)
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

            if (component.pavementLayerLinked) {
                if (component.pavementLayerHandle.empty()) {
                    errorMessage = L"Road model pavement layer template handle must not be empty.";
                    return false;
                }

                const auto* pavementSource = findOrNormalizePavementLayerTemplate(
                    pavementLayerTemplates,
                    normalizedPavementLayerTemplates,
                    component.pavementLayerHandle,
                    errorMessage);
                if (pavementSource == nullptr) {
                    return false;
                }

                const auto pavementSection = PavementLayerTemplateRules::buildSection(
                    pavementSource->data,
                    width,
                    side,
                    centerElevation + innerElevationOffset,
                    centerElevation + outerElevationOffset);
                if (!pavementSection.succeeded) {
                    errorMessage = L"Road model pavement layer section is invalid (" +
                        component.pavementLayerHandle + L"): " + pavementSection.errorMessage;
                    return false;
                }

                const auto color = toWireColor(component.color);
                for (std::size_t layerIndex = 0; layerIndex < pavementSection.layers.size(); ++layerIndex) {
                    const auto& layer = pavementSection.layers[layerIndex];
                    const auto label = layer.name.empty() ? L"路面结构层" : layer.name;
                    appendPavementLayerBoundaryPoint(
                        points,
                        assignment,
                        *pavementSource,
                        side,
                        index,
                        layerIndex,
                        0,
                        color,
                        frame,
                        centerElevation,
                        offset,
                        layer.topInner,
                        label);
                    appendPavementLayerBoundaryPoint(
                        points,
                        assignment,
                        *pavementSource,
                        side,
                        index,
                        layerIndex,
                        1,
                        color,
                        frame,
                        centerElevation,
                        offset,
                        layer.topOuter,
                        label);
                    appendPavementLayerBoundaryPoint(
                        points,
                        assignment,
                        *pavementSource,
                        side,
                        index,
                        layerIndex,
                        2,
                        color,
                        frame,
                        centerElevation,
                        offset,
                        layer.bottomInner,
                        label);
                    appendPavementLayerBoundaryPoint(
                        points,
                        assignment,
                        *pavementSource,
                        side,
                        index,
                        layerIndex,
                        3,
                        color,
                        frame,
                        centerElevation,
                        offset,
                        layer.bottomOuter,
                        label);
                }
            }

            offset += width;
            elevationOffset = outerElevationOffset;
        }
    }

    return true;
}

void appendLocalSlopeBoundaryPair(
    std::vector<LocalSlopeBoundaryPoint>& points,
    const std::wstring& templateHandle,
    SubgradeSide side,
    const SlopeTemplateComponent& component,
    const LocalSlopePoint& inner,
    const LocalSlopePoint& outer,
    std::size_t groupIndex,
    std::size_t templateOrder,
    std::size_t componentIndex)
{
    points.push_back(
        LocalSlopeBoundaryPoint{
            RoadModelSlopeLineKey{
                templateHandle,
                side,
                component.type,
                groupIndex,
                templateOrder,
                componentIndex,
                0},
            component.color,
            inner});
    points.push_back(
        LocalSlopeBoundaryPoint{
            RoadModelSlopeLineKey{
                templateHandle,
                side,
                component.type,
                groupIndex,
                templateOrder,
                componentIndex,
                1},
            component.color,
            outer});
}

LocalSlopePoint componentEndPoint(
    const LocalSlopePoint& start,
    const SlopeResolvedGeometry& geometry)
{
    return LocalSlopePoint{
        start.offset + geometry.width,
        start.elevation + geometry.elevationDelta};
}

LocalSlopePoint componentExtensionPoint(
    const LocalSlopePoint& end,
    const SlopeResolvedGeometry& geometry,
    double heightIncrement)
{
    if (heightIncrement <= 0.0 || std::fabs(geometry.slope) <= kStationTolerance) {
        return end;
    }

    const double sign = geometry.slope > 0.0 ? 1.0 : -1.0;
    return LocalSlopePoint{
        end.offset + heightIncrement / std::fabs(geometry.slope),
        end.elevation + sign * heightIncrement};
}

bool appendSlopeComponent(
    SlopeTemplateAttempt& attempt,
    const SlopeTemplateComponent& component,
    LocalSlopePoint& current,
    const std::vector<GroundProfilePoint>& groundProfile,
    bool needsGroundIntersection,
    std::size_t componentIndex)
{
    const auto geometry = SlopeTemplateRules::resolveGeometry(component);
    if (!geometry.succeeded) {
        attempt.errorMessage = geometry.errorMessage;
        return false;
    }

    const LocalSlopePoint inner = current;
    LocalSlopePoint outer = componentEndPoint(inner, geometry);
    if (needsGroundIntersection) {
        if (const auto intersection = firstGroundIntersection(inner, outer, groundProfile, 1.0e-7)) {
            outer = *intersection;
            appendLocalSlopeBoundaryPair(
                attempt.points,
                attempt.templateHandle,
                attempt.side,
                component,
                inner,
                outer,
                attempt.groupIndex,
                attempt.templateOrder,
                componentIndex);
            current = outer;
            return true;
        }

        if (component.groundSearchHeightIncrement > 0.0) {
            const LocalSlopePoint extended = componentExtensionPoint(
                outer,
                geometry,
                component.groundSearchHeightIncrement);
            if (const auto intersection = firstGroundIntersection(
                    outer,
                    extended,
                    groundProfile,
                    0.0)) {
                outer = *intersection;
                appendLocalSlopeBoundaryPair(
                    attempt.points,
                    attempt.templateHandle,
                    attempt.side,
                    component,
                    inner,
                    outer,
                    attempt.groupIndex,
                    attempt.templateOrder,
                    componentIndex);
                current = outer;
                return true;
            }
        }
    }

    appendLocalSlopeBoundaryPair(
        attempt.points,
        attempt.templateHandle,
        attempt.side,
        component,
        inner,
        outer,
        attempt.groupIndex,
        attempt.templateOrder,
        componentIndex);
    current = outer;
    return false;
}

SlopeTemplateAttempt tryApplySlopeTemplate(
    const RoadModelSlopeTemplateSource& source,
    SubgradeSide side,
    const LocalSlopePoint& startPoint,
    const std::vector<GroundProfilePoint>& groundProfile,
    double searchWidth,
    std::size_t groupIndex,
    std::size_t templateOrder)
{
    SlopeTemplateAttempt attempt;
    attempt.templateHandle = source.templateHandle;
    attempt.side = side;
    attempt.groupIndex = groupIndex;
    attempt.templateOrder = templateOrder;

    const bool needsGroundIntersection =
        source.data.properties.stopAtGround || source.data.properties.repeatLastGroupUntilGround;
    if (source.data.properties.repeatLastGroupUntilGround && groundProfile.size() < 2) {
        attempt.errorMessage = L"Slope template repeat-until-ground requires terrain ground profile.";
        return attempt;
    }

    LocalSlopePoint current = startPoint;
    for (std::size_t i = 0; i < source.data.components.size(); ++i) {
        const bool reachedGround = appendSlopeComponent(
            attempt,
            source.data.components[i],
            current,
            groundProfile,
            needsGroundIntersection,
            i);
        if (attempt.errorMessage.empty() && reachedGround) {
            attempt.succeeded = true;
            return attempt;
        }
        if (!attempt.errorMessage.empty()) {
            return attempt;
        }
    }

    if (!source.data.properties.repeatLastGroupUntilGround) {
        attempt.succeeded = true;
        return attempt;
    }

    const auto repeatGroup = SlopeTemplateRules::lastRepeatGroup(source.data);
    if (!repeatGroup.has_value()) {
        attempt.errorMessage = L"Slope template repeat-until-ground requires a final berm plus slope group.";
        return attempt;
    }

    std::size_t sequenceIndex = source.data.components.size();
    constexpr std::size_t kMaxRepeatIterations = 256;
    for (std::size_t repeat = 0; repeat < kMaxRepeatIterations; ++repeat) {
        for (const std::size_t componentIndex : {repeatGroup->bermIndex, repeatGroup->slopeIndex}) {
            const bool reachedGround = appendSlopeComponent(
                attempt,
                source.data.components[componentIndex],
                current,
                groundProfile,
                true,
                sequenceIndex++);
            if (attempt.errorMessage.empty() && reachedGround) {
                attempt.succeeded = true;
                return attempt;
            }
            if (!attempt.errorMessage.empty()) {
                return attempt;
            }
        }

        if (RoadModelRules::isSupportedSearchWidth(searchWidth) &&
            current.offset > searchWidth + kStationTolerance) {
            attempt.errorMessage = L"Slope template did not intersect ground within search width.";
            return attempt;
        }
    }

    attempt.errorMessage = L"Slope template repeat-until-ground reached iteration limit.";
    return attempt;
}

RoadModelPoint3d slopePointToWorld(
    const AlignmentFrame& frame,
    SubgradeSide side,
    const LocalSlopePoint& point)
{
    const double sideSign = side == SubgradeSide::Left ? 1.0 : -1.0;
    return RoadModelPoint3d{
        frame.x + frame.normalX * sideSign * point.offset,
        frame.y + frame.normalY * sideSign * point.offset,
        point.elevation};
}

void appendSlopeBoundaryPointsForSide(
    std::vector<ActiveSlopeBoundaryPoint>& points,
    std::vector<RoadModelSlopeSectionResult>& sectionResults,
    const std::vector<RoadModelSlopeTemplateGroup>& groups,
    const std::vector<RoadModelSlopeTemplateSource>& slopeTemplates,
    std::vector<RoadModelSlopeTemplateSource>& normalizedSlopeTemplates,
    const std::vector<GroundProfilePoint>& groundProfile,
    const SubgradeTemplateData& subgrade,
    const AlignmentFrame& frame,
    double centerElevation,
    double resolveStation,
    SubgradeSide side,
    double searchWidth)
{
    if (groups.empty()) {
        return;
    }

    const SubgradeOuterEdge edge = subgradeOuterEdgeAtStation(subgrade, side, frame.station);
    const LocalSlopePoint startPoint{edge.offset, centerElevation + edge.elevationOffset};
    std::vector<std::pair<std::size_t, const RoadModelSlopeTemplateGroup*>> matchedGroups;
    for (std::size_t i = 0; i < groups.size(); ++i) {
        if (resolveStation >= groups[i].startStation && resolveStation <= groups[i].endStation) {
            matchedGroups.push_back({i, &groups[i]});
        }
    }
    std::wstring lastError;

    for (const auto& matchedGroup : matchedGroups) {
        const auto groupIndex = matchedGroup.first;
        const auto* group = matchedGroup.second;
        for (std::size_t templateOrder = 0; templateOrder < group->templates.size(); ++templateOrder) {
            const auto& reference = group->templates[templateOrder];
            std::wstring templateError;
            const auto* source = findOrNormalizeSlopeTemplate(
                slopeTemplates,
                normalizedSlopeTemplates,
                reference.templateHandle,
                templateError);
            if (source == nullptr) {
                lastError = templateError;
                continue;
            }

            auto attempt = tryApplySlopeTemplate(
                *source,
                side,
                startPoint,
                groundProfile,
                searchWidth,
                groupIndex,
                templateOrder);
            if (!attempt.succeeded) {
                lastError = attempt.errorMessage;
                continue;
            }

            sectionResults.push_back(
                RoadModelSlopeSectionResult{
                    frame.station,
                    side,
                    true,
                    groupIndex,
                    templateOrder,
                    source->templateHandle,
                    L""});
            for (const auto& localPoint : attempt.points) {
                points.push_back(
                    ActiveSlopeBoundaryPoint{
                        localPoint.key,
                        localPoint.color,
                        slopePointToWorld(frame, side, localPoint.point),
                        frame.station,
                        signedOffset(side, localPoint.point.offset),
                        localPoint.point.elevation});
            }
            return;
        }
    }

    sectionResults.push_back(
        RoadModelSlopeSectionResult{
            frame.station,
            side,
            false,
            matchedGroups.empty() ? 0 : matchedGroups.front().first,
            0,
            L"",
            lastError.empty() ? L"No slope template group matched or succeeded." : lastError});
}

void appendBoundaryPoint(
    std::vector<RoadModelLineAccumulator>& accumulators,
    const ActiveBoundaryPoint& previous,
    const ActiveBoundaryPoint& current)
{
    for (auto& accumulator : accumulators) {
        if (sameLineKey(accumulator.line.key, current.key) &&
            !accumulator.line.points.empty() &&
            std::fabs(accumulator.lastStation - previous.station) <= kStationTolerance &&
            samePoint(accumulator.line.points.back(), previous.point)) {
            accumulator.line.points.push_back(current.point);
            accumulator.lastStation = current.station;
            return;
        }
    }

    RoadModelLineAccumulator accumulator;
    accumulator.line.key = current.key;
    accumulator.line.color = current.color;
    accumulator.line.points.push_back(previous.point);
    accumulator.line.points.push_back(current.point);
    accumulator.lastStation = current.station;
    accumulators.push_back(std::move(accumulator));
}

void appendSlopeBoundaryPoint(
    std::vector<RoadModelSlopeLineAccumulator>& accumulators,
    const ActiveSlopeBoundaryPoint& previous,
    const ActiveSlopeBoundaryPoint& current)
{
    for (auto& accumulator : accumulators) {
        if (sameLineKey(accumulator.line.key, current.key) &&
            !accumulator.line.points.empty() &&
            std::fabs(accumulator.lastStation - previous.station) <= kStationTolerance &&
            samePoint(accumulator.line.points.back(), previous.point)) {
            accumulator.line.points.push_back(current.point);
            accumulator.lastStation = current.station;
            return;
        }
    }

    RoadModelSlopeLineAccumulator accumulator;
    accumulator.line.key = current.key;
    accumulator.line.color = current.color;
    accumulator.line.points.push_back(previous.point);
    accumulator.line.points.push_back(current.point);
    accumulator.lastStation = current.station;
    accumulators.push_back(std::move(accumulator));
}

void appendPavementLayerBoundaryPoint(
    std::vector<RoadModelPavementLayerLineAccumulator>& accumulators,
    const ActivePavementLayerBoundaryPoint& previous,
    const ActivePavementLayerBoundaryPoint& current)
{
    for (auto& accumulator : accumulators) {
        if (sameLineKey(accumulator.line.key, current.key) &&
            !accumulator.line.points.empty() &&
            std::fabs(accumulator.lastStation - previous.station) <= kStationTolerance &&
            samePoint(accumulator.line.points.back(), previous.point)) {
            accumulator.line.points.push_back(current.point);
            accumulator.lastStation = current.station;
            return;
        }
    }

    RoadModelPavementLayerLineAccumulator accumulator;
    accumulator.line.key = current.key;
    accumulator.line.color = current.color;
    accumulator.line.points.push_back(previous.point);
    accumulator.line.points.push_back(current.point);
    accumulator.lastStation = current.station;
    accumulators.push_back(std::move(accumulator));
}

RoadModelSectionNode sectionNodeFromBoundary(const ActiveBoundaryPoint& point)
{
    return RoadModelSectionNode{
        RoadModelSectionNodeKind::Subgrade,
        point.key.side,
        point.offset,
        point.elevation,
        point.point,
        toWireColor(point.color),
        L"路基"};
}

RoadModelSectionNode sectionNodeFromSlopeBoundary(const ActiveSlopeBoundaryPoint& point)
{
    return RoadModelSectionNode{
        RoadModelSectionNodeKind::Slope,
        point.key.side,
        point.offset,
        point.elevation,
        point.point,
        toWireColor(point.color),
        L"边坡"};
}

RoadModelSectionNode sectionNodeFromPavementLayerBoundary(const ActivePavementLayerBoundaryPoint& point)
{
    return RoadModelSectionNode{
        RoadModelSectionNodeKind::PavementLayer,
        point.key.side,
        point.offset,
        point.elevation,
        point.point,
        point.color,
        point.label.empty() ? L"路面结构层" : point.label};
}

void appendSectionNode(std::vector<RoadModelSectionNode>& nodes, RoadModelSectionNode node)
{
    if (!nodes.empty() && samePoint(nodes.back().point, node.point)) {
        return;
    }
    nodes.push_back(std::move(node));
}

void appendSectionNodesForSide(
    std::vector<RoadModelSectionNode>& nodes,
    SubgradeSide side,
    const std::vector<ActiveBoundaryPoint>& boundaryPoints,
    const std::vector<ActiveSlopeBoundaryPoint>& slopePoints)
{
    for (const auto& point : boundaryPoints) {
        if (point.key.side == side) {
            appendSectionNode(nodes, sectionNodeFromBoundary(point));
        }
    }
    for (const auto& point : slopePoints) {
        if (point.key.side == side) {
            appendSectionNode(nodes, sectionNodeFromSlopeBoundary(point));
        }
    }
}

void appendPavementLayerSectionNodesForSide(
    std::vector<RoadModelSectionNode>& nodes,
    SubgradeSide side,
    const std::vector<ActivePavementLayerBoundaryPoint>& pavementLayerPoints)
{
    for (const auto& point : pavementLayerPoints) {
        if (point.key.side == side) {
            appendSectionNode(nodes, sectionNodeFromPavementLayerBoundary(point));
        }
    }
}

RoadModelSection makeRoadModelSection(
    double station,
    const std::vector<ActiveBoundaryPoint>& boundaryPoints,
    const std::vector<ActiveSlopeBoundaryPoint>& slopePoints,
    const std::vector<ActivePavementLayerBoundaryPoint>& pavementLayerPoints,
    const SectionGroundProfiles& groundProfiles)
{
    RoadModelSection section;
    section.station = station;
    appendSectionNodesForSide(section.leftNodes, SubgradeSide::Left, boundaryPoints, slopePoints);
    appendSectionNodesForSide(section.rightNodes, SubgradeSide::Right, boundaryPoints, slopePoints);
    appendPavementLayerSectionNodesForSide(
        section.leftPavementLayerNodes,
        SubgradeSide::Left,
        pavementLayerPoints);
    appendPavementLayerSectionNodesForSide(
        section.rightPavementLayerNodes,
        SubgradeSide::Right,
        pavementLayerPoints);
    section.leftGroundProfile = toGroundProfileSnapshot(groundProfiles.left);
    section.rightGroundProfile = toGroundProfileSnapshot(groundProfiles.right);
    section.succeeded = !section.leftNodes.empty() || !section.rightNodes.empty() ||
        !section.leftPavementLayerNodes.empty() || !section.rightPavementLayerNodes.empty();
    if (!section.succeeded) {
        section.errorMessage = L"Road model section has no nodes.";
    }
    return section;
}

void upsertRoadModelSection(std::vector<RoadModelSection>& sections, RoadModelSection section)
{
    const auto found = std::find_if(
        sections.begin(),
        sections.end(),
        [&section](const auto& existing) {
            return std::fabs(existing.station - section.station) <= kStationTolerance;
        });
    if (found == sections.end()) {
        sections.push_back(std::move(section));
    }
}

bool isFiniteRoadModelPoint(const RoadModelPoint3d& point)
{
    return isFinite(point.x) && isFinite(point.y) && isFinite(point.z);
}

double distance3d(const RoadModelPoint3d& lhs, const RoadModelPoint3d& rhs)
{
    const double dx = rhs.x - lhs.x;
    const double dy = rhs.y - lhs.y;
    const double dz = rhs.z - lhs.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

void appendWireLine(
    std::vector<RoadModelWireLine>& wireLines,
    RoadModelWireLineKind kind,
    SubgradeSide side,
    RoadModelWireColor color,
    const RoadModelPoint3d& start,
    const RoadModelPoint3d& end)
{
    if (!isFiniteRoadModelPoint(start) || !isFiniteRoadModelPoint(end) || samePoint(start, end)) {
        return;
    }

    RoadModelWireLine line;
    line.kind = kind;
    line.side = side;
    line.color = color;
    line.points = {start, end};
    wireLines.push_back(std::move(line));
}

void appendSectionRibs(
    std::vector<RoadModelWireLine>& wireLines,
    const RoadModelSection& section,
    SubgradeSide side,
    const std::vector<RoadModelSectionNode>& nodes,
    bool endCap)
{
    for (std::size_t i = 1; i < nodes.size(); ++i) {
        appendWireLine(
            wireLines,
            RoadModelWireLineKind::SectionRib,
            side,
            nodes[i].color,
            nodes[i - 1].point,
            nodes[i].point);
        if (endCap) {
            appendWireLine(
                wireLines,
                RoadModelWireLineKind::EndCap,
                side,
                nodes[i].color,
                nodes[i - 1].point,
                nodes[i].point);
        }
    }
}

void appendPavementLayerSectionWires(
    std::vector<RoadModelWireLine>& wireLines,
    SubgradeSide side,
    const std::vector<RoadModelSectionNode>& nodes)
{
    for (std::size_t i = 0; i + 3 < nodes.size(); i += 4) {
        const auto& topInner = nodes[i];
        const auto& topOuter = nodes[i + 1];
        const auto& bottomInner = nodes[i + 2];
        const auto& bottomOuter = nodes[i + 3];
        appendWireLine(
            wireLines,
            RoadModelWireLineKind::PavementLayer,
            side,
            topOuter.color,
            topInner.point,
            topOuter.point);
        appendWireLine(
            wireLines,
            RoadModelWireLineKind::PavementLayer,
            side,
            bottomOuter.color,
            topOuter.point,
            bottomOuter.point);
        appendWireLine(
            wireLines,
            RoadModelWireLineKind::PavementLayer,
            side,
            bottomOuter.color,
            bottomInner.point,
            bottomOuter.point);
        appendWireLine(
            wireLines,
            RoadModelWireLineKind::PavementLayer,
            side,
            bottomInner.color,
            topInner.point,
            bottomInner.point);
    }
}

void appendPavementLayerLongitudinalWires(
    std::vector<RoadModelWireLine>& wireLines,
    const std::vector<RoadModelPavementLayerLine>& pavementLayerLines)
{
    for (const auto& line : pavementLayerLines) {
        for (std::size_t i = 1; i < line.points.size(); ++i) {
            appendWireLine(
                wireLines,
                RoadModelWireLineKind::PavementLayer,
                line.key.side,
                line.color,
                line.points[i - 1],
                line.points[i]);
        }
    }
}

std::vector<double> normalizedNodePositions(const std::vector<RoadModelSectionNode>& nodes)
{
    std::vector<double> positions(nodes.size(), 0.0);
    if (nodes.size() < 2) {
        return positions;
    }

    double total = 0.0;
    for (std::size_t i = 1; i < nodes.size(); ++i) {
        total += distance3d(nodes[i - 1].point, nodes[i].point);
        positions[i] = total;
    }

    if (total <= kStationTolerance) {
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            positions[i] = static_cast<double>(i) / static_cast<double>(nodes.size() - 1);
        }
        return positions;
    }

    for (auto& position : positions) {
        position /= total;
    }
    positions.back() = 1.0;
    return positions;
}

RoadModelPoint3d interpolatePoint3d(
    const RoadModelPoint3d& start,
    const RoadModelPoint3d& end,
    double t)
{
    const double clamped = std::clamp(t, 0.0, 1.0);
    return RoadModelPoint3d{
        start.x + (end.x - start.x) * clamped,
        start.y + (end.y - start.y) * clamped,
        start.z + (end.z - start.z) * clamped};
}

RoadModelPoint3d pointAtNormalizedPosition(
    const std::vector<RoadModelSectionNode>& nodes,
    const std::vector<double>& positions,
    double position)
{
    if (nodes.empty()) {
        return {};
    }
    if (nodes.size() == 1 || position <= positions.front() + kStationTolerance) {
        return nodes.front().point;
    }

    for (std::size_t i = 1; i < nodes.size(); ++i) {
        if (position <= positions[i] + kStationTolerance) {
            const double span = positions[i] - positions[i - 1];
            const double t = span <= kStationTolerance
                ? 0.0
                : (position - positions[i - 1]) / span;
            return interpolatePoint3d(nodes[i - 1].point, nodes[i].point, t);
        }
    }

    return nodes.back().point;
}

std::vector<double> mergedNormalizedPositions(
    const std::vector<double>& previous,
    const std::vector<double>& current)
{
    std::vector<double> result;
    result.reserve(previous.size() + current.size());
    result.insert(result.end(), previous.begin(), previous.end());
    result.insert(result.end(), current.begin(), current.end());
    std::sort(result.begin(), result.end());

    std::vector<double> unique;
    for (const auto value : result) {
        if (unique.empty() || std::fabs(unique.back() - value) > kStationTolerance) {
            unique.push_back(std::clamp(value, 0.0, 1.0));
        }
    }
    return unique;
}

void appendBetweenSectionNodes(
    std::vector<RoadModelWireLine>& wireLines,
    SubgradeSide side,
    const std::vector<RoadModelSectionNode>& previous,
    const std::vector<RoadModelSectionNode>& current)
{
    if (previous.empty() || current.empty()) {
        return;
    }

    appendWireLine(
        wireLines,
        RoadModelWireLineKind::OuterBoundary,
        side,
        current.back().color,
        previous.back().point,
        current.back().point);

    const auto commonCount = std::min(previous.size(), current.size());
    for (std::size_t i = 0; i < commonCount; ++i) {
        appendWireLine(
            wireLines,
            RoadModelWireLineKind::Longitudinal,
            side,
            current[i].color,
            previous[i].point,
            current[i].point);
    }

    if (previous.size() == current.size()) {
        return;
    }

    const auto tailStart = commonCount > 0 ? commonCount - 1 : 0;
    std::vector<RoadModelSectionNode> previousTail(previous.begin() + tailStart, previous.end());
    std::vector<RoadModelSectionNode> currentTail(current.begin() + tailStart, current.end());
    const auto transitionColor = previousTail.size() > currentTail.size()
        ? previousTail.back().color
        : currentTail.back().color;
    const auto previousPositions = normalizedNodePositions(previousTail);
    const auto currentPositions = normalizedNodePositions(currentTail);
    const auto positions = mergedNormalizedPositions(previousPositions, currentPositions);
    for (const auto position : positions) {
        appendWireLine(
            wireLines,
            RoadModelWireLineKind::Transition,
            side,
            transitionColor,
            pointAtNormalizedPosition(previousTail, previousPositions, position),
            pointAtNormalizedPosition(currentTail, currentPositions, position));
    }
}

std::vector<RoadModelWireLine> buildWireLinesFromSections(std::vector<RoadModelSection> sections)
{
    std::sort(
        sections.begin(),
        sections.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.station < rhs.station;
        });

    std::vector<RoadModelWireLine> wireLines;
    for (std::size_t i = 0; i < sections.size(); ++i) {
        const bool endCap = i == 0 || i + 1 == sections.size();
        appendSectionRibs(wireLines, sections[i], SubgradeSide::Left, sections[i].leftNodes, endCap);
        appendSectionRibs(wireLines, sections[i], SubgradeSide::Right, sections[i].rightNodes, endCap);
        appendPavementLayerSectionWires(
            wireLines,
            SubgradeSide::Left,
            sections[i].leftPavementLayerNodes);
        appendPavementLayerSectionWires(
            wireLines,
            SubgradeSide::Right,
            sections[i].rightPavementLayerNodes);

        if (i == 0) {
            continue;
        }

        appendBetweenSectionNodes(
            wireLines,
            SubgradeSide::Left,
            sections[i - 1].leftNodes,
            sections[i].leftNodes);
        appendBetweenSectionNodes(
            wireLines,
            SubgradeSide::Right,
            sections[i - 1].rightNodes,
            sections[i].rightNodes);
    }

    return wireLines;
}

} // namespace

bool RoadModelRules::isSupportedSampleInterval(double sampleInterval)
{
    return isFinite(sampleInterval) && sampleInterval > 0.0;
}

bool RoadModelRules::isSupportedSearchWidth(double searchWidth)
{
    return isFinite(searchWidth) && searchWidth > 0.0;
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

bool RoadModelRules::validateSlopeTemplateGroups(
    const std::vector<RoadModelSlopeTemplateGroup>& groups,
    std::wstring& errorMessage)
{
    errorMessage.clear();

    for (const auto& group : groups) {
        if (!isFinite(group.startStation) || !isFinite(group.endStation)) {
            errorMessage = L"Road model slope template group station must be finite.";
            return false;
        }
        if (group.endStation < group.startStation) {
            errorMessage = L"Road model slope template group end station must not be before start station.";
            return false;
        }
        if (group.templates.empty()) {
            errorMessage = L"Road model slope template group requires at least one template.";
            return false;
        }
        for (const auto& reference : group.templates) {
            if (reference.templateHandle.empty()) {
                errorMessage = L"Road model slope template reference must not be empty.";
                return false;
            }
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

RoadModelSlopeTemplateGroupResolver::RoadModelSlopeTemplateGroupResolver(
    std::vector<RoadModelSlopeTemplateGroup> groups)
    : groups_(std::move(groups))
{
}

std::vector<const RoadModelSlopeTemplateGroup*> RoadModelSlopeTemplateGroupResolver::resolve(double station) const
{
    std::vector<const RoadModelSlopeTemplateGroup*> result;
    if (!isFinite(station)) {
        return result;
    }

    for (const auto& group : groups_) {
        if (station >= group.startStation && station <= group.endStation) {
            result.push_back(&group);
        }
    }

    return result;
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

RoadModelSectionPreview RoadModelSectionPreviewBuilder::build(const RoadModelSectionPreviewRequest& request)
{
    RoadModelSectionPreview preview;
    preview.station = request.station;

    if (!isFinite(request.station)) {
        preview.errorMessage = L"横断面预览桩号无效。";
        return preview;
    }

    const auto normalizedAlignmentSamples = normalizeAlignmentSamples(request.alignmentSamples, preview.errorMessage);
    if (!normalizedAlignmentSamples.has_value()) {
        return preview;
    }

    const auto frame = interpolateAlignmentFrame(*normalizedAlignmentSamples, request.station);
    if (!frame.has_value()) {
        preview.errorMessage = L"横断面预览无法定位道路中线。";
        return preview;
    }

    if (const auto* section = findSectionAtStation(request.data.sections, request.station)) {
        appendSectionNodePreviewSegments(preview, section->leftNodes);
        appendSectionNodePreviewSegments(preview, section->rightNodes);
        appendPavementLayerSectionPreviewSegments(preview, section->leftPavementLayerNodes);
        appendPavementLayerSectionPreviewSegments(preview, section->rightPavementLayerNodes);
        appendGroundPreviewSegment(preview, request, *frame, section);
        preview.succeeded = true;
        if (preview.statusMessage.empty()) {
            preview.statusMessage = L"已生成横断面预览。";
        }
        return preview;
    }

    std::vector<PreviewBoundarySample> samples;
    for (const auto& line : request.data.componentLines) {
        const auto point = interpolateLinePointAtStation(line.points, *normalizedAlignmentSamples, request.station);
        if (!point.has_value()) {
            continue;
        }

        samples.push_back(
            PreviewBoundarySample{
                RoadModelSectionPreviewSegmentKind::Subgrade,
                line.key.templateHandle,
                line.key.side,
                static_cast<int>(line.key.componentType),
                0,
                0,
                line.key.componentIndex,
                line.key.boundaryIndex,
                toPreviewColor(line.color),
                toPreviewPoint(*frame, *point)});
    }

    for (const auto& line : request.data.slopeLines) {
        const auto point = interpolateLinePointAtStation(line.points, *normalizedAlignmentSamples, request.station);
        if (!point.has_value()) {
            continue;
        }

        samples.push_back(
            PreviewBoundarySample{
                RoadModelSectionPreviewSegmentKind::Slope,
                line.key.templateHandle,
                line.key.side,
                static_cast<int>(line.key.componentType),
                line.key.groupIndex,
                line.key.templateOrder,
                line.key.componentIndex,
                line.key.boundaryIndex,
                toPreviewColor(line.color),
                toPreviewPoint(*frame, *point)});
    }

    for (const auto& line : request.data.pavementLayerLines) {
        const auto point = interpolateLinePointAtStation(line.points, *normalizedAlignmentSamples, request.station);
        if (!point.has_value()) {
            continue;
        }

        samples.push_back(
            PreviewBoundarySample{
                RoadModelSectionPreviewSegmentKind::PavementLayer,
                line.key.pavementLayerTemplateHandle,
                line.key.side,
                0,
                line.key.layerIndex,
                0,
                line.key.componentIndex,
                line.key.boundaryIndex,
                toPreviewColor(line.color),
                toPreviewPoint(*frame, *point)});
    }

    appendPreviewSegmentPairs(preview, samples);
    appendPavementLayerPreviewSampleSegments(preview, samples);
    appendGroundPreviewSegment(preview, request, *frame, nullptr);

    preview.succeeded = true;
    if (preview.statusMessage.empty()) {
        preview.statusMessage = L"已生成横断面预览。";
    }
    return preview;
}

RoadModelBuildResult RoadModelBuilder::build(const RoadModelBuildInput& input)
{
    RoadModelBuildResult result;
    result.data.config = input.config;
    reportProgress(input, 0, L"正在校验道路模型输入...");

    if (!RoadModelRules::isSupportedSampleInterval(input.config.sampleInterval)) {
        result.errorMessage = L"Road model sample interval must be positive.";
        return result;
    }

    std::wstring assignmentError;
    if (!RoadModelRules::validateAssignments(input.config.assignments, assignmentError)) {
        result.errorMessage = assignmentError;
        return result;
    }

    if (!RoadModelRules::isSupportedSearchWidth(input.config.slopeConfig.leftSearchWidth) ||
        !RoadModelRules::isSupportedSearchWidth(input.config.slopeConfig.rightSearchWidth)) {
        result.errorMessage = L"Road model slope search width must be positive.";
        return result;
    }

    std::wstring slopeGroupError;
    if (!RoadModelRules::validateSlopeTemplateGroups(
            input.config.slopeConfig.leftGroups,
            slopeGroupError) ||
        !RoadModelRules::validateSlopeTemplateGroups(
            input.config.slopeConfig.rightGroups,
            slopeGroupError)) {
        result.errorMessage = slopeGroupError;
        return result;
    }

    reportProgress(input, 10, L"正在重建纵断面设计线...");
    const auto builtVertical = profile::ProfileVerticalCurveCalculator::rebuild(input.verticalCurve);
    if (!builtVertical.succeeded) {
        result.errorMessage = builtVertical.errorMessage.empty()
            ? L"Road model vertical curve rebuild failed."
            : builtVertical.errorMessage;
        return result;
    }

    std::wstring alignmentError;
    const auto normalizedAlignmentSamples = normalizeAlignmentSamples(input.alignmentSamples, alignmentError);
    if (!normalizedAlignmentSamples.has_value()) {
        result.errorMessage = alignmentError;
        return result;
    }

    const auto& alignmentSamples = *normalizedAlignmentSamples;
    const double alignmentStart = alignmentSamples.front().station;
    const double alignmentEnd = alignmentSamples.back().station;
    if (!isFinite(alignmentStart) || !isFinite(alignmentEnd) || alignmentEnd <= alignmentStart) {
        result.errorMessage = L"Road model alignment station range is invalid.";
        return result;
    }

    reportProgress(input, 20, L"正在采样横断面桩号...");
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
    result.data.sampledStations = sampledStations;

    RoadModelTemplateResolver resolver(input.config.assignments);
    std::vector<RoadModelTemplateSource> normalizedTemplates;
    std::vector<RoadModelSlopeTemplateSource> normalizedSlopeTemplates;
    std::vector<RoadModelPavementLayerTemplateSource> normalizedPavementLayerTemplates;
    std::vector<RoadModelLineAccumulator> lineAccumulators;
    std::vector<RoadModelSlopeLineAccumulator> slopeLineAccumulators;
    std::vector<RoadModelPavementLayerLineAccumulator> pavementLayerLineAccumulators;
    std::vector<std::vector<RoadModelSection>> sectionRuns;
    std::vector<RoadModelSection>* activeSectionRun = nullptr;
    double activeSectionRunLastEnd = std::numeric_limits<double>::quiet_NaN();
    roadproto::domain::terrain::TerrainTriangleSpatialIndex terrainIndex(
        input.terrainSurface.points,
        input.terrainSurface.triangles);
    result.diagnostics.terrainTriangleCount = input.terrainSurface.triangles.size();
    result.diagnostics.terrainIndexEnabled = terrainIndex.enabled();
    result.diagnostics.terrainIndexColumns = terrainIndex.columns();
    result.diagnostics.terrainIndexRows = terrainIndex.rows();
    result.diagnostics.terrainIndexTriangleReferences = terrainIndex.triangleReferenceCount();
    result.diagnostics.terrainIndexGlobalTriangles = terrainIndex.globalTriangleCount();
    std::optional<SectionGroundProfiles> previousGroundProfiles;
    reportProgress(input, 30, L"正在生成横断面道路模型...");

    int lastReportedPercent = 30;
    const auto intervalCount = sampledStations.size() > 1 ? sampledStations.size() - 1 : std::size_t{1};
    for (std::size_t i = 1; i < sampledStations.size(); ++i) {
        const double startStation = sampledStations[i - 1];
        const double endStation = sampledStations[i];
        if (endStation - startStation <= kStationTolerance) {
            result.errorMessage = L"Road model sampled station intervals must be strictly increasing.";
            result.data.componentLines.clear();
            return result;
        }

        const double midStation = (startStation + endStation) * 0.5;
        const auto* assignment = resolver.resolve(midStation);
        if (assignment == nullptr) {
            activeSectionRun = nullptr;
            activeSectionRunLastEnd = std::numeric_limits<double>::quiet_NaN();
            previousGroundProfiles.reset();
            continue;
        }

        std::wstring templateError;
        const auto* templateSource = findOrNormalizeTemplate(
            input.templates,
            normalizedTemplates,
            assignment->templateHandle,
            templateError);
        if (templateSource == nullptr) {
            result.errorMessage = templateError;
            result.sampledStations.clear();
            return result;
        }

        const auto startFrame = interpolateAlignmentFrame(alignmentSamples, startStation);
        const auto endFrame = interpolateAlignmentFrame(alignmentSamples, endStation);
        if (!startFrame.has_value() || !endFrame.has_value()) {
            result.errorMessage = L"Road model failed to interpolate alignment frame.";
            result.data.componentLines.clear();
            return result;
        }

        const auto startElevation = profile::ProfileVerticalCurveCalculator::elevationAt(builtVertical, startStation);
        const auto endElevation = profile::ProfileVerticalCurveCalculator::elevationAt(builtVertical, endStation);
        if (!startElevation.succeeded || !endElevation.succeeded) {
            result.errorMessage = L"Road model failed to query vertical curve elevation.";
            result.data.componentLines.clear();
            return result;
        }

        std::vector<ActiveBoundaryPoint> startPoints;
        std::vector<ActiveBoundaryPoint> endPoints;
        std::vector<ActiveSlopeBoundaryPoint> startSlopePoints;
        std::vector<ActiveSlopeBoundaryPoint> endSlopePoints;
        std::vector<ActivePavementLayerBoundaryPoint> startPavementLayerPoints;
        std::vector<ActivePavementLayerBoundaryPoint> endPavementLayerPoints;
        appendTemplateBoundaryPoints(
            startPoints,
            *assignment,
            templateSource->data,
            *startFrame,
            startElevation.value);
        appendTemplateBoundaryPoints(
            endPoints,
            *assignment,
            templateSource->data,
            *endFrame,
            endElevation.value);
        std::wstring pavementLayerError;
        if (!appendPavementLayerBoundaryPoints(
                startPavementLayerPoints,
                *assignment,
                templateSource->data,
                input.pavementLayerTemplates,
                normalizedPavementLayerTemplates,
                *startFrame,
                startElevation.value,
                pavementLayerError) ||
            !appendPavementLayerBoundaryPoints(
                endPavementLayerPoints,
                *assignment,
                templateSource->data,
                input.pavementLayerTemplates,
                normalizedPavementLayerTemplates,
                *endFrame,
                endElevation.value,
                pavementLayerError)) {
            result.errorMessage = pavementLayerError;
            result.data.componentLines.clear();
            result.data.pavementLayerLines.clear();
            return result;
        }

        const auto startGroundProfiles = previousGroundProfiles.has_value() &&
                std::fabs(previousGroundProfiles->station - startStation) <= kStationTolerance
            ? *previousGroundProfiles
            : buildSectionGroundProfiles(
                *startFrame,
                input.config.slopeConfig,
                input.terrainSurface,
                &terrainIndex,
                &result.diagnostics);
        const auto endGroundProfiles = buildSectionGroundProfiles(
            *endFrame,
            input.config.slopeConfig,
            input.terrainSurface,
            &terrainIndex,
            &result.diagnostics);
        previousGroundProfiles = endGroundProfiles;

        appendSlopeBoundaryPointsForSide(
            startSlopePoints,
            result.data.slopeSectionResults,
            input.config.slopeConfig.leftGroups,
            input.slopeTemplates,
            normalizedSlopeTemplates,
            startGroundProfiles.left,
            templateSource->data,
            *startFrame,
            startElevation.value,
            midStation,
            SubgradeSide::Left,
            input.config.slopeConfig.leftSearchWidth);
        appendSlopeBoundaryPointsForSide(
            endSlopePoints,
            result.data.slopeSectionResults,
            input.config.slopeConfig.leftGroups,
            input.slopeTemplates,
            normalizedSlopeTemplates,
            endGroundProfiles.left,
            templateSource->data,
            *endFrame,
            endElevation.value,
            midStation,
            SubgradeSide::Left,
            input.config.slopeConfig.leftSearchWidth);
        appendSlopeBoundaryPointsForSide(
            startSlopePoints,
            result.data.slopeSectionResults,
            input.config.slopeConfig.rightGroups,
            input.slopeTemplates,
            normalizedSlopeTemplates,
            startGroundProfiles.right,
            templateSource->data,
            *startFrame,
            startElevation.value,
            midStation,
            SubgradeSide::Right,
            input.config.slopeConfig.rightSearchWidth);
        appendSlopeBoundaryPointsForSide(
            endSlopePoints,
            result.data.slopeSectionResults,
            input.config.slopeConfig.rightGroups,
            input.slopeTemplates,
            normalizedSlopeTemplates,
            endGroundProfiles.right,
            templateSource->data,
            *endFrame,
            endElevation.value,
            midStation,
            SubgradeSide::Right,
            input.config.slopeConfig.rightSearchWidth);

        const auto startSection = makeRoadModelSection(
            startStation,
            startPoints,
            startSlopePoints,
            startPavementLayerPoints,
            startGroundProfiles);
        const auto endSection = makeRoadModelSection(
            endStation,
            endPoints,
            endSlopePoints,
            endPavementLayerPoints,
            endGroundProfiles);
        const bool continuesCurrentRun = activeSectionRun != nullptr
            && std::fabs(activeSectionRunLastEnd - startStation) <= kStationTolerance;
        if (!continuesCurrentRun) {
            sectionRuns.emplace_back();
            activeSectionRun = &sectionRuns.back();
        }
        upsertRoadModelSection(*activeSectionRun, startSection);
        upsertRoadModelSection(*activeSectionRun, endSection);
        activeSectionRunLastEnd = endStation;

        upsertRoadModelSection(result.data.sections, startSection);
        upsertRoadModelSection(result.data.sections, endSection);

        for (const auto& startPoint : startPoints) {
            const auto endPoint = std::find_if(
                endPoints.begin(),
                endPoints.end(),
                [&startPoint](const auto& point) {
                    return sameLineKey(point.key, startPoint.key);
                });
            if (endPoint != endPoints.end()) {
                appendBoundaryPoint(lineAccumulators, startPoint, *endPoint);
            }
        }

        for (const auto& startPoint : startSlopePoints) {
            const auto endPoint = std::find_if(
                endSlopePoints.begin(),
                endSlopePoints.end(),
                [&startPoint](const auto& point) {
                    return sameLineKey(point.key, startPoint.key);
                });
            if (endPoint != endSlopePoints.end()) {
                appendSlopeBoundaryPoint(slopeLineAccumulators, startPoint, *endPoint);
            }
        }

        for (const auto& startPoint : startPavementLayerPoints) {
            const auto endPoint = std::find_if(
                endPavementLayerPoints.begin(),
                endPavementLayerPoints.end(),
                [&startPoint](const auto& point) {
                    return sameLineKey(point.key, startPoint.key);
                });
            if (endPoint != endPavementLayerPoints.end()) {
                appendPavementLayerBoundaryPoint(
                    pavementLayerLineAccumulators,
                    startPoint,
                    *endPoint);
            }
        }

        const auto percent = 30 + static_cast<int>((i * 60) / intervalCount);
        if (percent > lastReportedPercent) {
            reportProgress(input, percent, L"正在生成横断面道路模型...");
            lastReportedPercent = percent;
        }
    }

    reportProgress(input, 95, L"正在整理道路模型线框...");
    result.data.componentLines.reserve(lineAccumulators.size());
    for (auto& accumulator : lineAccumulators) {
        result.data.componentLines.push_back(std::move(accumulator.line));
    }
    result.data.slopeLines.reserve(slopeLineAccumulators.size());
    for (auto& accumulator : slopeLineAccumulators) {
        result.data.slopeLines.push_back(std::move(accumulator.line));
    }
    result.data.pavementLayerLines.reserve(pavementLayerLineAccumulators.size());
    for (auto& accumulator : pavementLayerLineAccumulators) {
        result.data.pavementLayerLines.push_back(std::move(accumulator.line));
    }
    result.data.wireLines.clear();
    for (const auto& sectionRun : sectionRuns) {
        auto runWireLines = buildWireLinesFromSections(sectionRun);
        result.data.wireLines.insert(
            result.data.wireLines.end(),
            std::make_move_iterator(runWireLines.begin()),
            std::make_move_iterator(runWireLines.end()));
    }
    appendPavementLayerLongitudinalWires(result.data.wireLines, result.data.pavementLayerLines);

    if (result.data.componentLines.empty()) {
        result.errorMessage = L"Road model produced no component boundary lines.";
        return result;
    }

    result.succeeded = true;
    result.errorMessage.clear();
    reportProgress(input, 100, L"道路模型生成完成。");
    return result;
}

} // namespace roadproto::domain::cross_section
