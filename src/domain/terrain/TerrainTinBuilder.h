#pragma once

#include "domain/terrain/TerrainTinData.h"

#include <vector>

namespace roadproto::domain::terrain {

class TerrainTinBuilder {
public:
    TinBuildResult build(
        const std::vector<TinPoint>& points,
        const TinBuildOptions& options) const;
};

} // namespace roadproto::domain::terrain
