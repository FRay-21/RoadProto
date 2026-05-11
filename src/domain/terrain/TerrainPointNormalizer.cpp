#include "domain/terrain/TerrainPointNormalizer.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_map>

namespace roadproto::domain::terrain {

namespace {

struct GridKey {
    std::int64_t x = 0;
    std::int64_t y = 0;

    bool operator==(const GridKey& other) const
    {
        return x == other.x && y == other.y;
    }
};

struct GridKeyHash {
    std::size_t operator()(const GridKey& key) const
    {
        const auto hx = std::hash<std::int64_t>{}(key.x);
        const auto hy = std::hash<std::int64_t>{}(key.y);
        return hx ^ (hy + 0x9e3779b97f4a7c15ULL + (hx << 6) + (hx >> 2));
    }
};

class SpatialIndex {
public:
    explicit SpatialIndex(double cellSize)
        : cellSize_(cellSize > 0.0 ? cellSize : 1e-9)
    {
    }

    void add(const TinPoint& point, std::size_t index)
    {
        buckets_[keyFor(point.x, point.y)].push_back(index);
    }

    std::pair<std::size_t, std::size_t> findNearest(
        const std::vector<TinPoint>& points,
        const TinPoint& point,
        double radius) const
    {
        const auto center = keyFor(point.x, point.y);
        const auto range = static_cast<int>(std::ceil(radius / cellSize_));
        const auto radiusSquared = radius * radius;
        auto bestIndex = std::numeric_limits<std::size_t>::max();
        auto matchCount = std::size_t{0};
        auto bestDistanceSquared = std::numeric_limits<double>::max();

        for (int dx = -range; dx <= range; ++dx) {
            for (int dy = -range; dy <= range; ++dy) {
                const auto it = buckets_.find(GridKey{center.x + dx, center.y + dy});
                if (it == buckets_.end()) {
                    continue;
                }

                for (const auto candidateIndex : it->second) {
                    const auto& candidate = points[candidateIndex];
                    const auto xDiff = candidate.x - point.x;
                    const auto yDiff = candidate.y - point.y;
                    const auto distanceSquared = xDiff * xDiff + yDiff * yDiff;
                    if (distanceSquared <= radiusSquared) {
                        ++matchCount;
                        if (distanceSquared < bestDistanceSquared) {
                            bestDistanceSquared = distanceSquared;
                            bestIndex = candidateIndex;
                        }
                    }
                }
            }
        }

        return {bestIndex, matchCount};
    }

private:
    GridKey keyFor(double x, double y) const
    {
        return GridKey{
            static_cast<std::int64_t>(std::floor(x / cellSize_)),
            static_cast<std::int64_t>(std::floor(y / cellSize_))};
    }

    double cellSize_;
    std::unordered_map<GridKey, std::vector<std::size_t>, GridKeyHash> buckets_;
};

bool isTextSource(TinPointSourceType sourceType)
{
    return sourceType == TinPointSourceType::TextElevation || sourceType == TinPointSourceType::BlockText;
}

int sourcePriority(TinPointSourceType sourceType)
{
    switch (sourceType) {
    case TinPointSourceType::BlockAttribute:
        return 50;
    case TinPointSourceType::TextElevation:
    case TinPointSourceType::BlockText:
        return 40;
    case TinPointSourceType::CadPoint:
    case TinPointSourceType::PolylineVertex:
    case TinPointSourceType::BlockGeometry:
        return 30;
    case TinPointSourceType::LineVertex:
        return 20;
    default:
        return 0;
    }
}

bool isFlatOrMissingElevation(const TinPoint& point, const TinExtractOptions& options)
{
    return !point.hasValidElevation || std::fabs(point.z) <= options.zEqualTolerance;
}

void mergeElevation(TinPoint& target, const TinPoint& incoming, const TinExtractOptions& options, TinExtractSummary& summary)
{
    if (!incoming.hasValidElevation) {
        return;
    }

    if (!target.hasValidElevation) {
        target.z = incoming.z;
        target.hasValidElevation = true;
        return;
    }

    if (std::fabs(target.z - incoming.z) <= options.zEqualTolerance) {
        target.z = (target.z + incoming.z) * 0.5;
        return;
    }

    ++summary.zConflictPointCount;
    if (sourcePriority(incoming.sourceType) >= sourcePriority(target.sourceType)) {
        target.z = incoming.z;
    } else {
        target.z = (target.z + incoming.z) * 0.5;
    }
}

void mergeMetadata(TinPoint& target, const TinPoint& incoming)
{
    if (target.sourceObjectHandle.empty()) {
        target.sourceObjectHandle = incoming.sourceObjectHandle;
    }
    if (!incoming.associatedTextHandle.empty()) {
        target.associatedTextHandle = incoming.associatedTextHandle;
    }
    if (!incoming.associatedGeometryHandle.empty()) {
        target.associatedGeometryHandle = incoming.associatedGeometryHandle;
    }
    if (target.blockName.empty()) {
        target.blockName = incoming.blockName;
    }
    if (target.attributeTag.empty()) {
        target.attributeTag = incoming.attributeTag;
    }
}

void addOrMergeDuplicate(
    const TinPoint& point,
    const TinExtractOptions& options,
    std::vector<TinPoint>& accepted,
    SpatialIndex& duplicateIndex,
    TinExtractSummary& summary)
{
    const auto duplicate = duplicateIndex.findNearest(accepted, point, options.xyMergeTolerance);
    if (duplicate.first == std::numeric_limits<std::size_t>::max()) {
        accepted.push_back(point);
        duplicateIndex.add(point, accepted.size() - 1);
        return;
    }

    ++summary.duplicatePointCount;
    auto& target = accepted[duplicate.first];
    mergeElevation(target, point, options, summary);
    mergeMetadata(target, point);
    if (sourcePriority(point.sourceType) > sourcePriority(target.sourceType)) {
        target.sourceType = point.sourceType;
    }
}

} // namespace

TinNormalizeResult TerrainPointNormalizer::normalize(
    const std::vector<TinPoint>& rawPoints,
    const TinExtractOptions& options) const
{
    TinNormalizeResult result;
    result.points.reserve(rawPoints.size());
    result.summary.rawPointCount = rawPoints.size();

    for (const auto& point : rawPoints) {
        if (isTextSource(point.sourceType)) {
            ++result.summary.textElevationPointCount;
        }
    }

    SpatialIndex duplicateIndex(options.xyMergeTolerance);
    for (const auto& point : rawPoints) {
        if (isTextSource(point.sourceType)) {
            continue;
        }
        addOrMergeDuplicate(point, options, result.points, duplicateIndex, result.summary);
    }

    SpatialIndex nearbyIndex(options.textPointSearchDistance);
    for (std::size_t i = 0; i < result.points.size(); ++i) {
        nearbyIndex.add(result.points[i], i);
    }

    for (const auto& textPoint : rawPoints) {
        if (!isTextSource(textPoint.sourceType)) {
            continue;
        }

        const auto nearest = nearbyIndex.findNearest(result.points, textPoint, options.textPointSearchDistance);
        if (nearest.second > 1) {
            ++result.summary.ambiguousTextPointMatchCount;
        }

        if (nearest.first != std::numeric_limits<std::size_t>::max()) {
            auto& target = result.points[nearest.first];
            if (options.useTextElevationForNearbyFlatPoint && isFlatOrMissingElevation(target, options)) {
                target.z = textPoint.z;
                target.hasValidElevation = textPoint.hasValidElevation;
                target.associatedTextHandle = textPoint.sourceObjectHandle;
                target.mergedFromText = true;
                ++result.summary.textAssignedElevationPointCount;
            } else {
                mergeElevation(target, textPoint, options, result.summary);
                target.associatedTextHandle = textPoint.sourceObjectHandle;
                target.mergedFromText = true;
                ++result.summary.textPointMergeCount;
            }
            continue;
        }

        addOrMergeDuplicate(textPoint, options, result.points, duplicateIndex, result.summary);
    }

    result.summary.validPointCount = result.points.size();
    result.summary.status = result.points.size() >= 3 ? L"点清洗完成" : L"有效点不足";
    return result;
}

} // namespace roadproto::domain::terrain
