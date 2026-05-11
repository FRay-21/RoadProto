#pragma once

#include "domain/terrain/TerrainTinData.h"

#include <vector>

namespace roadproto::domain::terrain {

class TerrainPointNormalizer {
public:
    TinNormalizeResult normalize(
        const std::vector<TinPoint>& rawPoints,
        const TinExtractOptions& options) const;
};

} // namespace roadproto::domain::terrain
