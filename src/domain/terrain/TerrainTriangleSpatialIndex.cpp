#include "domain/terrain/TerrainTriangleSpatialIndex.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace roadproto::domain::terrain {

namespace {

constexpr double kCoordinateTolerance = 1.0e-9;

bool isFinite(double value)
{
    return std::isfinite(value);
}

std::size_t clampedDimension(std::size_t value, std::size_t minValue, std::size_t maxValue)
{
    return std::clamp(value, minValue, std::max(minValue, maxValue));
}

} // namespace

TerrainTriangleSpatialIndex::TerrainTriangleSpatialIndex(
    const std::vector<TinPoint>& points,
    const std::vector<TinTriangle>& triangles,
    TerrainTriangleSpatialIndexOptions options)
    : points_(points),
      triangles_(triangles),
      options_(options)
{
    if (options_.minGridDimension == 0) {
        options_.minGridDimension = 1;
    }
    if (options_.maxGridDimension < options_.minGridDimension) {
        options_.maxGridDimension = options_.minGridDimension;
    }
    if (options_.targetTrianglesPerCell == 0) {
        options_.targetTrianglesPerCell = 1;
    }
    build();
}

bool TerrainTriangleSpatialIndex::enabled() const
{
    return enabled_;
}

std::size_t TerrainTriangleSpatialIndex::columns() const
{
    return columns_;
}

std::size_t TerrainTriangleSpatialIndex::rows() const
{
    return rows_;
}

std::size_t TerrainTriangleSpatialIndex::triangleReferenceCount() const
{
    return triangleIds_.size();
}

std::size_t TerrainTriangleSpatialIndex::globalTriangleCount() const
{
    return globalTriangleIds_.size();
}

std::vector<std::size_t> TerrainTriangleSpatialIndex::querySegment(
    double startX,
    double startY,
    double endX,
    double endY) const
{
    std::vector<std::size_t> candidates;
    if (!enabled_) {
        return candidates;
    }

    const double queryMinX = std::min(startX, endX);
    const double queryMaxX = std::max(startX, endX);
    const double queryMinY = std::min(startY, endY);
    const double queryMaxY = std::max(startY, endY);
    if (!cellRangeForBounds(queryMinX, queryMinY, queryMaxX, queryMaxY).has_value()) {
        return candidates;
    }

    candidates = globalTriangleIds_;
    const auto appendCell = [this, &candidates](std::size_t col, std::size_t row) {
        if (col >= columns_ || row >= rows_) {
            return;
        }
        const std::size_t cellIndex = row * columns_ + col;
        const std::size_t offset = cellOffsets_[cellIndex];
        const std::size_t count = cellCounts_[cellIndex];
        candidates.insert(
            candidates.end(),
            triangleIds_.begin() + static_cast<std::ptrdiff_t>(offset),
            triangleIds_.begin() + static_cast<std::ptrdiff_t>(offset + count));
    };

    std::int64_t col = static_cast<std::int64_t>(columnFor(startX));
    std::int64_t row = static_cast<std::int64_t>(rowFor(startY));
    const std::int64_t endCol = static_cast<std::int64_t>(columnFor(endX));
    const std::int64_t endRow = static_cast<std::int64_t>(rowFor(endY));

    const double dx = endX - startX;
    const double dy = endY - startY;
    const std::int64_t stepCol = (dx > kCoordinateTolerance) ? 1 : ((dx < -kCoordinateTolerance) ? -1 : 0);
    const std::int64_t stepRow = (dy > kCoordinateTolerance) ? 1 : ((dy < -kCoordinateTolerance) ? -1 : 0);
    const double infinity = std::numeric_limits<double>::infinity();
    double nextColT = infinity;
    double nextRowT = infinity;
    const double deltaColT = (stepCol == 0) ? infinity : (cellWidth_ / std::abs(dx));
    const double deltaRowT = (stepRow == 0) ? infinity : (cellHeight_ / std::abs(dy));

    if (stepCol != 0) {
        const double boundaryX = minX_ + static_cast<double>(col + (stepCol > 0 ? 1 : 0)) * cellWidth_;
        nextColT = (boundaryX - startX) / dx;
    }
    if (stepRow != 0) {
        const double boundaryY = minY_ + static_cast<double>(row + (stepRow > 0 ? 1 : 0)) * cellHeight_;
        nextRowT = (boundaryY - startY) / dy;
    }

    const auto appendSignedCell = [&appendCell](std::int64_t signedCol, std::int64_t signedRow) {
        if (signedCol < 0 || signedRow < 0) {
            return;
        }
        appendCell(static_cast<std::size_t>(signedCol), static_cast<std::size_t>(signedRow));
    };

    const std::size_t maxTraversalSteps = columns_ + rows_ + 4;
    for (std::size_t traversed = 0; traversed <= maxTraversalSteps; ++traversed) {
        appendSignedCell(col, row);
        if (col == endCol && row == endRow) {
            break;
        }

        if (std::abs(nextColT - nextRowT) <= kCoordinateTolerance) {
            appendSignedCell(col + stepCol, row);
            appendSignedCell(col, row + stepRow);
            col += stepCol;
            row += stepRow;
            nextColT += deltaColT;
            nextRowT += deltaRowT;
        } else if (nextColT < nextRowT) {
            col += stepCol;
            nextColT += deltaColT;
        } else {
            row += stepRow;
            nextRowT += deltaRowT;
        }

        if (col < 0 || row < 0 ||
            col >= static_cast<std::int64_t>(columns_) ||
            row >= static_cast<std::int64_t>(rows_)) {
            break;
        }
    }

    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    return candidates;
}

void TerrainTriangleSpatialIndex::build()
{
    clear();
    if (points_.empty() || triangles_.empty()) {
        return;
    }

    bool hasFinitePoint = false;
    for (const auto& point : points_) {
        if (!isFinite(point.x) || !isFinite(point.y)) {
            continue;
        }
        if (!hasFinitePoint) {
            minX_ = maxX_ = point.x;
            minY_ = maxY_ = point.y;
            hasFinitePoint = true;
            continue;
        }
        minX_ = std::min(minX_, point.x);
        maxX_ = std::max(maxX_, point.x);
        minY_ = std::min(minY_, point.y);
        maxY_ = std::max(maxY_, point.y);
    }

    const double width = maxX_ - minX_;
    const double height = maxY_ - minY_;
    if (!hasFinitePoint || width <= kCoordinateTolerance || height <= kCoordinateTolerance) {
        clear();
        return;
    }

    const double aspect = std::clamp(width / height, 0.125, 8.0);
    const auto targetCellCount = std::clamp<std::size_t>(
        triangles_.size() / options_.targetTrianglesPerCell + 1,
        options_.minGridDimension * options_.minGridDimension,
        options_.maxGridDimension * options_.maxGridDimension);
    auto columns = clampedDimension(
        static_cast<std::size_t>(std::sqrt(static_cast<double>(targetCellCount) * aspect)),
        options_.minGridDimension,
        options_.maxGridDimension);
    auto rows = clampedDimension(
        targetCellCount / columns + 1,
        options_.minGridDimension,
        options_.maxGridDimension);

    for (int attempt = 0; attempt < 12; ++attempt) {
        if (tryBuild(columns, rows)) {
            enabled_ = true;
            return;
        }

        if (columns <= options_.minGridDimension && rows <= options_.minGridDimension) {
            break;
        }
        if (columns >= rows && columns > options_.minGridDimension) {
            columns = std::max(options_.minGridDimension, (columns + 1) / 2);
        } else if (rows > options_.minGridDimension) {
            rows = std::max(options_.minGridDimension, (rows + 1) / 2);
        }
    }

    clear();
}

bool TerrainTriangleSpatialIndex::tryBuild(std::size_t columns, std::size_t rows)
{
    if (columns == 0 || rows == 0) {
        return false;
    }

    columns_ = columns;
    rows_ = rows;
    cellWidth_ = (maxX_ - minX_) / static_cast<double>(columns_);
    cellHeight_ = (maxY_ - minY_) / static_cast<double>(rows_);
    if (cellWidth_ <= 0.0 || cellHeight_ <= 0.0) {
        return false;
    }

    const std::size_t cellTotal = columns_ * rows_;
    std::vector<std::size_t> counts(cellTotal, 0);
    std::vector<std::size_t> globalIds;
    std::size_t referenceCount = 0;
    const std::size_t broadTriangleCellLimit = std::max<std::size_t>(64, cellTotal / 8);

    for (std::size_t triangleIndex = 0; triangleIndex < triangles_.size(); ++triangleIndex) {
        double triMinX = 0.0;
        double triMinY = 0.0;
        double triMaxX = 0.0;
        double triMaxY = 0.0;
        if (!triangleBounds(triangleIndex, triMinX, triMinY, triMaxX, triMaxY)) {
            continue;
        }

        const auto range = cellRangeForBounds(triMinX, triMinY, triMaxX, triMaxY);
        if (!range.has_value()) {
            continue;
        }
        const auto& r = *range;
        const std::size_t cellCount = (r.maxCol - r.minCol + 1) * (r.maxRow - r.minRow + 1);
        if (cellCount > broadTriangleCellLimit) {
            globalIds.push_back(triangleIndex);
            continue;
        }
        if (cellCount > options_.maxTriangleReferences ||
            referenceCount > options_.maxTriangleReferences - cellCount) {
            clear();
            return false;
        }
        referenceCount += cellCount;
        for (std::size_t row = r.minRow; row <= r.maxRow; ++row) {
            for (std::size_t col = r.minCol; col <= r.maxCol; ++col) {
                ++counts[row * columns_ + col];
            }
        }
    }

    std::vector<std::size_t> offsets(cellTotal + 1, 0);
    for (std::size_t cell = 0; cell < cellTotal; ++cell) {
        offsets[cell + 1] = offsets[cell] + counts[cell];
    }

    std::vector<std::size_t> ids(referenceCount, 0);
    std::vector<std::size_t> cursors = offsets;
    for (std::size_t triangleIndex = 0; triangleIndex < triangles_.size(); ++triangleIndex) {
        double triMinX = 0.0;
        double triMinY = 0.0;
        double triMaxX = 0.0;
        double triMaxY = 0.0;
        if (!triangleBounds(triangleIndex, triMinX, triMinY, triMaxX, triMaxY)) {
            continue;
        }

        const auto range = cellRangeForBounds(triMinX, triMinY, triMaxX, triMaxY);
        if (!range.has_value()) {
            continue;
        }
        const auto& r = *range;
        const std::size_t cellCount = (r.maxCol - r.minCol + 1) * (r.maxRow - r.minRow + 1);
        if (cellCount > broadTriangleCellLimit) {
            continue;
        }

        for (std::size_t row = r.minRow; row <= r.maxRow; ++row) {
            for (std::size_t col = r.minCol; col <= r.maxCol; ++col) {
                const std::size_t cell = row * columns_ + col;
                ids[cursors[cell]++] = triangleIndex;
            }
        }
    }

    cellOffsets_ = std::move(offsets);
    cellCounts_ = std::move(counts);
    triangleIds_ = std::move(ids);
    globalTriangleIds_ = std::move(globalIds);
    return true;
}

void TerrainTriangleSpatialIndex::clear()
{
    enabled_ = false;
    cellOffsets_.clear();
    cellCounts_.clear();
    triangleIds_.clear();
    globalTriangleIds_.clear();
    columns_ = 0;
    rows_ = 0;
    cellWidth_ = 0.0;
    cellHeight_ = 0.0;
}

bool TerrainTriangleSpatialIndex::triangleBounds(
    std::size_t triangleIndex,
    double& minX,
    double& minY,
    double& maxX,
    double& maxY) const
{
    if (triangleIndex >= triangles_.size()) {
        return false;
    }
    const auto& triangle = triangles_[triangleIndex];
    if (triangle.a >= points_.size() || triangle.b >= points_.size() || triangle.c >= points_.size()) {
        return false;
    }

    const auto& a = points_[triangle.a];
    const auto& b = points_[triangle.b];
    const auto& c = points_[triangle.c];
    if (!isFinite(a.x) || !isFinite(a.y) ||
        !isFinite(b.x) || !isFinite(b.y) ||
        !isFinite(c.x) || !isFinite(c.y)) {
        return false;
    }

    minX = std::min({a.x, b.x, c.x});
    maxX = std::max({a.x, b.x, c.x});
    minY = std::min({a.y, b.y, c.y});
    maxY = std::max({a.y, b.y, c.y});
    return true;
}

std::optional<TerrainTriangleSpatialIndex::CellRange> TerrainTriangleSpatialIndex::cellRangeForBounds(
    double queryMinX,
    double queryMinY,
    double queryMaxX,
    double queryMaxY) const
{
    if (columns_ == 0 || rows_ == 0 ||
        queryMaxX < minX_ - kCoordinateTolerance || queryMinX > maxX_ + kCoordinateTolerance ||
        queryMaxY < minY_ - kCoordinateTolerance || queryMinY > maxY_ + kCoordinateTolerance) {
        return std::nullopt;
    }

    CellRange range{
        columnFor(queryMinX),
        columnFor(queryMaxX),
        rowFor(queryMinY),
        rowFor(queryMaxY)};
    return range;
}

std::size_t TerrainTriangleSpatialIndex::columnFor(double x) const
{
    const double normalized = (x - minX_) / cellWidth_;
    return static_cast<std::size_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(std::floor(normalized)),
        0,
        static_cast<std::int64_t>(columns_ - 1)));
}

std::size_t TerrainTriangleSpatialIndex::rowFor(double y) const
{
    const double normalized = (y - minY_) / cellHeight_;
    return static_cast<std::size_t>(std::clamp<std::int64_t>(
        static_cast<std::int64_t>(std::floor(normalized)),
        0,
        static_cast<std::int64_t>(rows_ - 1)));
}

} // namespace roadproto::domain::terrain
