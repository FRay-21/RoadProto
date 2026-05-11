#pragma once

#include "domain/terrain/TerrainTinData.h"

namespace roadproto::domain::terrain {

class TerrainSurfaceQuery {
public:
    static bool findTriangle(
        const std::vector<TinPoint>& points,
        const std::vector<TinTriangle>& triangles,
        double x,
        double y,
        TinTriangle& triangle);

    static bool sampleElevation(
        const std::vector<TinPoint>& points,
        const std::vector<TinTriangle>& triangles,
        double x,
        double y,
        double& z);
};

} // namespace roadproto::domain::terrain
