#pragma once

#include "domain/terrain/TerrainTinData.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace roadproto::domain::terrain {

struct TerrainTriangleSpatialIndexOptions {
    std::size_t minGridDimension = 4;
    std::size_t maxGridDimension = 256;
    std::size_t targetTrianglesPerCell = 4;
    std::size_t maxTriangleReferences = 8000000;
};

class TerrainTriangleSpatialIndex {
public:
    TerrainTriangleSpatialIndex(
        const std::vector<TinPoint>& points,
        const std::vector<TinTriangle>& triangles,
        TerrainTriangleSpatialIndexOptions options = {});

    bool enabled() const;
    std::size_t columns() const;
    std::size_t rows() const;
    std::size_t triangleReferenceCount() const;
    std::size_t globalTriangleCount() const;

    std::vector<std::size_t> querySegment(
        double startX,
        double startY,
        double endX,
        double endY) const;

private:
    struct CellRange {
        std::size_t minCol = 0;
        std::size_t maxCol = 0;
        std::size_t minRow = 0;
        std::size_t maxRow = 0;
    };

    const std::vector<TinPoint>& points_;
    const std::vector<TinTriangle>& triangles_;
    TerrainTriangleSpatialIndexOptions options_;
    bool enabled_ = false;
    double minX_ = 0.0;
    double minY_ = 0.0;
    double maxX_ = 0.0;
    double maxY_ = 0.0;
    double cellWidth_ = 0.0;
    double cellHeight_ = 0.0;
    std::size_t columns_ = 0;
    std::size_t rows_ = 0;
    std::vector<std::size_t> cellOffsets_;
    std::vector<std::size_t> cellCounts_;
    std::vector<std::size_t> triangleIds_;
    std::vector<std::size_t> globalTriangleIds_;

    void build();
    bool tryBuild(std::size_t columns, std::size_t rows);
    void clear();
    bool triangleBounds(
        std::size_t triangleIndex,
        double& minX,
        double& minY,
        double& maxX,
        double& maxY) const;
    std::optional<CellRange> cellRangeForBounds(
        double queryMinX,
        double queryMinY,
        double queryMaxX,
        double queryMaxY) const;
    std::size_t columnFor(double x) const;
    std::size_t rowFor(double y) const;
};

} // namespace roadproto::domain::terrain
