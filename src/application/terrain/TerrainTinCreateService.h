#pragma once

#include "domain/terrain/TerrainPointNormalizer.h"
#include "domain/terrain/TerrainTinBuilder.h"

#include <vector>

namespace roadproto::application {

class TerrainTinCreateService {
public:
    domain::terrain::TinBuildResult buildFromExtractedPoints(
        const std::vector<domain::terrain::TinPoint>& rawPoints,
        const domain::terrain::TinExtractOptions& extractOptions,
        const domain::terrain::TinBuildOptions& buildOptions) const;
};

} // namespace roadproto::application
