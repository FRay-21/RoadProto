#include "domain/terrain/TerrainTinBuilder.h"

#include "third_party/delaunator-cpp/include/delaunator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <limits>
#include <set>
#include <unordered_map>

namespace roadproto::domain::terrain {

namespace {

struct WorkPoint {
    double x = 0.0;
    double y = 0.0;
};

struct EdgeKey {
    std::size_t a = 0;
    std::size_t b = 0;

    bool operator==(const EdgeKey& other) const
    {
        return a == other.a && b == other.b;
    }
};

struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& edge) const
    {
        const auto ha = std::hash<std::size_t>{}(edge.a);
        const auto hb = std::hash<std::size_t>{}(edge.b);
        return ha ^ (hb + 0x9e3779b97f4a7c15ULL + (ha << 6) + (ha >> 2));
    }
};

EdgeKey normalizeEdge(std::size_t a, std::size_t b)
{
    return a < b ? EdgeKey{a, b} : EdgeKey{b, a};
}

double area2(const WorkPoint& a, const WorkPoint& b, const WorkPoint& c)
{
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

double squaredDistance(const WorkPoint& a, const WorkPoint& b)
{
    const auto dx = a.x - b.x;
    const auto dy = a.y - b.y;
    return dx * dx + dy * dy;
}

bool hasEnoughNonCollinearArea(const std::vector<TinPoint>& points, double minArea)
{
    if (points.size() < 3) {
        return false;
    }

    const WorkPoint first{points[0].x, points[0].y};
    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
        for (std::size_t j = i + 1; j < points.size(); ++j) {
            const WorkPoint b{points[i].x, points[i].y};
            const WorkPoint c{points[j].x, points[j].y};
            if (std::fabs(area2(first, b, c)) * 0.5 > minArea) {
                return true;
            }
        }
    }

    return false;
}

bool isFinitePoint(const TinPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

double edgeLength(const WorkPoint& a, const WorkPoint& b)
{
    return std::sqrt(squaredDistance(a, b));
}

std::vector<std::pair<std::size_t, std::size_t>> collectBoundaryEdges(const std::vector<TinTriangle>& triangles)
{
    std::unordered_map<EdgeKey, int, EdgeKeyHash> edgeCounts;
    for (const auto& triangle : triangles) {
        ++edgeCounts[normalizeEdge(triangle.a, triangle.b)];
        ++edgeCounts[normalizeEdge(triangle.b, triangle.c)];
        ++edgeCounts[normalizeEdge(triangle.c, triangle.a)];
    }

    std::vector<std::pair<std::size_t, std::size_t>> boundaryEdges;
    boundaryEdges.reserve(edgeCounts.size());
    for (const auto& entry : edgeCounts) {
        if (entry.second == 1) {
            boundaryEdges.push_back({entry.first.a, entry.first.b});
        }
    }
    return boundaryEdges;
}

} // namespace

TinBuildResult TerrainTinBuilder::build(
    const std::vector<TinPoint>& points,
    const TinBuildOptions& options) const
{
    TinBuildResult result;
    result.points = points;
    result.buildOptions = options;

    if (points.size() < 3) {
        result.errorMessage = L"\u6709\u6548\u70b9\u4e0d\u8db3";
        return result;
    }

    if (!std::all_of(points.begin(), points.end(), isFinitePoint)) {
        result.errorMessage = L"\u8f93\u5165\u70b9\u5305\u542b\u975e\u6cd5\u5750\u6807";
        return result;
    }

    if (!hasEnoughNonCollinearArea(points, options.minTriangleArea)) {
        result.errorMessage = L"\u70b9\u96c6\u5171\u7ebf";
        return result;
    }

    std::vector<WorkPoint> workPoints;
    std::vector<double> coordinates;
    workPoints.reserve(points.size());
    coordinates.reserve(points.size() * 2);
    for (const auto& point : points) {
        workPoints.push_back(WorkPoint{point.x, point.y});
        coordinates.push_back(point.x);
        coordinates.push_back(point.y);
    }

    std::set<std::array<std::size_t, 3>> uniqueTriangles;
    try {
        const delaunator::Delaunator triangulation(coordinates);
        result.triangles.reserve(triangulation.triangles.size() / 3);

        for (std::size_t i = 0; i + 2 < triangulation.triangles.size(); i += 3) {
            auto a = triangulation.triangles[i];
            auto b = triangulation.triangles[i + 1];
            auto c = triangulation.triangles[i + 2];

            if (a >= points.size() || b >= points.size() || c >= points.size()) {
                continue;
            }

            const auto signedArea2 = area2(workPoints[a], workPoints[b], workPoints[c]);
            const auto area = std::fabs(signedArea2) * 0.5;
            if (options.removeDegenerateTriangles && area <= options.minTriangleArea) {
                continue;
            }

            if (options.maxEdgeLength > 0.0) {
                const auto maxEdge = std::max({
                    edgeLength(workPoints[a], workPoints[b]),
                    edgeLength(workPoints[b], workPoints[c]),
                    edgeLength(workPoints[c], workPoints[a])});
                if (maxEdge > options.maxEdgeLength) {
                    continue;
                }
            }

            if (signedArea2 < 0.0) {
                std::swap(a, b);
            }

            std::array<std::size_t, 3> sorted{a, b, c};
            std::sort(sorted.begin(), sorted.end());
            if (uniqueTriangles.insert(sorted).second) {
                result.triangles.push_back(TinTriangle{a, b, c});
            }
        }
    } catch (const std::exception&) {
        result.errorMessage = L"Delaunay \u7b97\u6cd5\u5931\u8d25";
        return result;
    } catch (...) {
        result.errorMessage = L"Delaunay \u7b97\u6cd5\u5931\u8d25";
        return result;
    }

    if (result.triangles.empty()) {
        result.errorMessage = L"\u4e09\u89d2\u5f62\u5168\u90e8\u88ab\u8fc7\u6ee4";
        return result;
    }

    result.boundaryEdges = collectBoundaryEdges(result.triangles);
    result.minElevation = points.front().z;
    result.maxElevation = points.front().z;
    for (const auto& point : points) {
        result.minElevation = std::min(result.minElevation, point.z);
        result.maxElevation = std::max(result.maxElevation, point.z);
    }

    result.extractSummary.validPointCount = points.size();
    result.extractSummary.triangleCount = result.triangles.size();
    result.extractSummary.status = L"TIN \u6784\u5efa\u5b8c\u6210";
    result.succeeded = true;
    return result;
}

} // namespace roadproto::domain::terrain
