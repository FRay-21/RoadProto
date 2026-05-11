#include "domain/terrain/TerrainSurfaceQuery.h"

#include <cmath>

namespace roadproto::domain::terrain {

namespace {

bool barycentric(
    const TinPoint& a,
    const TinPoint& b,
    const TinPoint& c,
    double x,
    double y,
    double& wa,
    double& wb,
    double& wc)
{
    const auto denominator =
        (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
    if (std::fabs(denominator) < 1e-12) {
        return false;
    }

    wa = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / denominator;
    wb = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / denominator;
    wc = 1.0 - wa - wb;
    constexpr auto tolerance = -1e-9;
    return wa >= tolerance && wb >= tolerance && wc >= tolerance;
}

} // namespace

bool TerrainSurfaceQuery::findTriangle(
    const std::vector<TinPoint>& points,
    const std::vector<TinTriangle>& triangles,
    double x,
    double y,
    TinTriangle& triangle)
{
    for (const auto& candidate : triangles) {
        if (candidate.a >= points.size() || candidate.b >= points.size() || candidate.c >= points.size()) {
            continue;
        }

        double wa = 0.0;
        double wb = 0.0;
        double wc = 0.0;
        if (barycentric(points[candidate.a], points[candidate.b], points[candidate.c], x, y, wa, wb, wc)) {
            triangle = candidate;
            return true;
        }
    }

    return false;
}

bool TerrainSurfaceQuery::sampleElevation(
    const std::vector<TinPoint>& points,
    const std::vector<TinTriangle>& triangles,
    double x,
    double y,
    double& z)
{
    TinTriangle triangle;
    if (!findTriangle(points, triangles, x, y, triangle)) {
        return false;
    }

    double wa = 0.0;
    double wb = 0.0;
    double wc = 0.0;
    if (!barycentric(points[triangle.a], points[triangle.b], points[triangle.c], x, y, wa, wb, wc)) {
        return false;
    }

    z = wa * points[triangle.a].z + wb * points[triangle.b].z + wc * points[triangle.c].z;
    return true;
}

} // namespace roadproto::domain::terrain
