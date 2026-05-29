#include "domain/quantity/RoadModelPavementQuantitySampler.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <vector>

namespace roadproto::domain::quantity {
namespace {

using roadproto::domain::cross_section::RoadModelComponentLine;
using roadproto::domain::cross_section::RoadModelData;
using roadproto::domain::cross_section::RoadModelPavementLayerLine;
using roadproto::domain::cross_section::RoadModelPoint3d;
using roadproto::domain::cross_section::RoadModelSection;
using roadproto::domain::cross_section::RoadModelSectionNode;
using roadproto::domain::cross_section::SubgradeComponentType;
using roadproto::domain::cross_section::SubgradeSide;
using roadproto::domain::cross_section::subgradeComponentTypeDisplayName;

constexpr double kStationTolerance = 1.0e-7;
constexpr double kPointTolerance = 1.0e-6;
constexpr std::size_t kNodesPerLayer = 4;

struct LocalLayerTotals {
    double projectedWidth = 0.0;
    double sectionArea = 0.0;
};

struct ComponentKey {
    SubgradeSide side = SubgradeSide::Right;
    std::size_t componentIndex = 0;

    bool operator<(const ComponentKey& other) const
    {
        if (side != other.side) {
            return static_cast<int>(side) < static_cast<int>(other.side);
        }
        return componentIndex < other.componentIndex;
    }
};

struct ComponentSpan {
    ComponentKey key;
    SubgradeComponentType type = SubgradeComponentType::TravelLane;
    double minOffset = 0.0;
    double maxOffset = 0.0;
};

using LocalLayerKey = std::pair<std::wstring, std::wstring>;

bool sameStation(double lhs, double rhs)
{
    return std::fabs(lhs - rhs) <= kStationTolerance;
}

bool samePoint(const RoadModelPoint3d& lhs, const RoadModelPoint3d& rhs)
{
    return std::fabs(lhs.x - rhs.x) <= kPointTolerance
        && std::fabs(lhs.y - rhs.y) <= kPointTolerance
        && std::fabs(lhs.z - rhs.z) <= kPointTolerance;
}

bool lineContainsPoint(const std::vector<RoadModelPoint3d>& points, const RoadModelPoint3d& point)
{
    return std::any_of(
        points.begin(),
        points.end(),
        [&](const auto& candidate) {
            return samePoint(candidate, point);
        });
}

const RoadModelSection* findSectionAtStation(const std::vector<RoadModelSection>& sections, double station)
{
    const RoadModelSection* best = nullptr;
    auto bestDistance = std::numeric_limits<double>::infinity();
    for (const auto& section : sections) {
        const auto distance = std::fabs(section.station - station);
        if (distance < bestDistance) {
            best = &section;
            bestDistance = distance;
        }
    }
    return bestDistance <= kStationTolerance ? best : nullptr;
}

std::wstring normalizeComponentName(const std::wstring& name)
{
    return name.empty() ? L"未分部件" : name;
}

std::wstring normalizeLayerName(const std::wstring& name)
{
    return name.empty() ? L"路面结构层" : name;
}

double polygonArea(const std::vector<std::pair<double, double>>& points)
{
    if (points.size() < 3) {
        return 0.0;
    }

    double sum = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& first = points[i];
        const auto& second = points[(i + 1) % points.size()];
        sum += first.first * second.second - second.first * first.second;
    }
    return std::fabs(sum) * 0.5;
}

std::map<ComponentKey, SubgradeComponentType> componentTypesByKey(
    const std::vector<RoadModelComponentLine>& componentLines)
{
    std::map<ComponentKey, SubgradeComponentType> result;
    for (const auto& line : componentLines) {
        result[ComponentKey{line.key.side, line.key.componentIndex}] = line.key.componentType;
    }
    return result;
}

std::vector<ComponentSpan> componentSpansForSide(
    const RoadModelData& data,
    const std::vector<RoadModelSectionNode>& sideNodes,
    SubgradeSide side)
{
    struct PendingSpan {
        SubgradeComponentType type = SubgradeComponentType::TravelLane;
        bool hasOffset = false;
        double minOffset = 0.0;
        double maxOffset = 0.0;
    };

    std::map<ComponentKey, PendingSpan> pending;
    for (const auto& line : data.componentLines) {
        if (line.key.side != side) {
            continue;
        }

        const ComponentKey key{line.key.side, line.key.componentIndex};
        auto& span = pending[key];
        span.type = line.key.componentType;
        for (const auto& node : sideNodes) {
            if (!lineContainsPoint(line.points, node.point)) {
                continue;
            }
            if (!span.hasOffset) {
                span.minOffset = node.offset;
                span.maxOffset = node.offset;
                span.hasOffset = true;
            } else {
                span.minOffset = std::min(span.minOffset, node.offset);
                span.maxOffset = std::max(span.maxOffset, node.offset);
            }
        }
    }

    std::vector<ComponentSpan> spans;
    for (const auto& item : pending) {
        if (!item.second.hasOffset
            || std::fabs(item.second.maxOffset - item.second.minOffset) <= kPointTolerance) {
            continue;
        }
        spans.push_back(
            ComponentSpan{
                item.first,
                item.second.type,
                item.second.minOffset,
                item.second.maxOffset});
    }
    return spans;
}

std::optional<SubgradeComponentType> inferComponentTypeFromPavementLayerLines(
    const RoadModelData& data,
    const std::map<ComponentKey, SubgradeComponentType>& componentTypes,
    const std::vector<RoadModelSectionNode>& layerNodes,
    std::size_t baseIndex,
    SubgradeSide side)
{
    std::map<ComponentKey, int> scores;
    for (const auto& line : data.pavementLayerLines) {
        if (line.key.side != side) {
            continue;
        }

        int score = 0;
        for (std::size_t i = 0; i < kNodesPerLayer; ++i) {
            if (lineContainsPoint(line.points, layerNodes[baseIndex + i].point)) {
                ++score;
            }
        }
        if (score > 0) {
            scores[ComponentKey{line.key.side, line.key.componentIndex}] += score;
        }
    }

    ComponentKey bestKey;
    int bestScore = 0;
    for (const auto& item : scores) {
        if (item.second > bestScore) {
            bestKey = item.first;
            bestScore = item.second;
        }
    }
    if (bestScore <= 0) {
        return std::nullopt;
    }

    const auto found = componentTypes.find(bestKey);
    if (found == componentTypes.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::optional<SubgradeComponentType> inferComponentTypeFromSpans(
    const std::vector<ComponentSpan>& spans,
    const RoadModelSectionNode& topInner,
    const RoadModelSectionNode& topOuter,
    SubgradeSide side)
{
    const auto minOffset = std::min(topInner.offset, topOuter.offset);
    const auto maxOffset = std::max(topInner.offset, topOuter.offset);
    const auto midpoint = (minOffset + maxOffset) * 0.5;

    const ComponentSpan* best = nullptr;
    double bestWidth = std::numeric_limits<double>::infinity();
    for (const auto& span : spans) {
        if (span.key.side != side) {
            continue;
        }
        if (midpoint < span.minOffset - kPointTolerance
            || midpoint > span.maxOffset + kPointTolerance) {
            continue;
        }
        const auto width = span.maxOffset - span.minOffset;
        if (width < bestWidth) {
            best = &span;
            bestWidth = width;
        }
    }

    if (best == nullptr) {
        return std::nullopt;
    }
    return best->type;
}

std::wstring componentNameForLayer(
    const RoadModelData& data,
    const std::map<ComponentKey, SubgradeComponentType>& componentTypes,
    const std::vector<ComponentSpan>& spans,
    const std::vector<RoadModelSectionNode>& nodes,
    std::size_t baseIndex,
    SubgradeSide side)
{
    const auto& topInner = nodes[baseIndex];
    if (!topInner.componentName.empty() && topInner.componentName != L"未分部件") {
        return topInner.componentName;
    }

    const auto fromLines = inferComponentTypeFromPavementLayerLines(
        data,
        componentTypes,
        nodes,
        baseIndex,
        side);
    if (fromLines.has_value()) {
        return subgradeComponentTypeDisplayName(*fromLines);
    }

    const auto fromSpans = inferComponentTypeFromSpans(
        spans,
        nodes[baseIndex],
        nodes[baseIndex + 1],
        side);
    if (fromSpans.has_value()) {
        return subgradeComponentTypeDisplayName(*fromSpans);
    }

    return L"未分部件";
}

void appendPavementLayerNodes(
    const RoadModelData& data,
    const std::vector<RoadModelSectionNode>& sectionNodes,
    const std::vector<RoadModelSectionNode>& subgradeNodes,
    SubgradeSide side,
    std::map<LocalLayerKey, LocalLayerTotals>& totalsByLayer)
{
    const auto componentTypes = componentTypesByKey(data.componentLines);
    const auto spans = componentSpansForSide(data, subgradeNodes, side);
    for (std::size_t i = 0; i + kNodesPerLayer - 1 < sectionNodes.size(); i += kNodesPerLayer) {
        const auto& topInner = sectionNodes[i];
        const auto& topOuter = sectionNodes[i + 1];
        const auto& bottomInner = sectionNodes[i + 2];
        const auto& bottomOuter = sectionNodes[i + 3];
        const auto componentName = componentNameForLayer(
            data,
            componentTypes,
            spans,
            sectionNodes,
            i,
            side);
        const auto key = LocalLayerKey{
            normalizeComponentName(componentName),
            normalizeLayerName(topInner.label)};

        const auto minOffset = std::min(
            std::min(topInner.offset, topOuter.offset),
            std::min(bottomInner.offset, bottomOuter.offset));
        const auto maxOffset = std::max(
            std::max(topInner.offset, topOuter.offset),
            std::max(bottomInner.offset, bottomOuter.offset));
        const auto area = polygonArea(
            {{topInner.offset, topInner.elevation},
             {topOuter.offset, topOuter.elevation},
             {bottomOuter.offset, bottomOuter.elevation},
             {bottomInner.offset, bottomInner.elevation}});

        auto& totals = totalsByLayer[key];
        totals.projectedWidth += std::max(0.0, maxOffset - minOffset);
        totals.sectionArea += area;
    }
}

} // namespace

std::optional<PavementQuantitySectionSample> RoadModelPavementQuantitySampler::sampleAtStation(
    const RoadModelData& data,
    double station)
{
    const auto* section = findSectionAtStation(data.sections, station);
    if (section == nullptr) {
        return std::nullopt;
    }

    std::map<LocalLayerKey, LocalLayerTotals> totalsByLayer;
    appendPavementLayerNodes(
        data,
        section->leftPavementLayerNodes,
        section->leftNodes,
        SubgradeSide::Left,
        totalsByLayer);
    appendPavementLayerNodes(
        data,
        section->rightPavementLayerNodes,
        section->rightNodes,
        SubgradeSide::Right,
        totalsByLayer);
    if (totalsByLayer.empty()) {
        return std::nullopt;
    }

    PavementQuantitySectionSample sample;
    sample.station = station;
    for (const auto& item : totalsByLayer) {
        sample.layers.push_back(
            PavementQuantityLayerSample{
                item.first.second,
                item.second.projectedWidth,
                item.second.sectionArea,
                item.first.first});
    }
    return sample;
}

} // namespace roadproto::domain::quantity
