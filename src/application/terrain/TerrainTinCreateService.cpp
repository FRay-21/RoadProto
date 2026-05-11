#include "application/terrain/TerrainTinCreateService.h"

namespace roadproto::application {

domain::terrain::TinBuildResult TerrainTinCreateService::buildFromExtractedPoints(
    const std::vector<domain::terrain::TinPoint>& rawPoints,
    const domain::terrain::TinExtractOptions& extractOptions,
    const domain::terrain::TinBuildOptions& buildOptions) const
{
    domain::terrain::TerrainPointNormalizer normalizer;
    auto normalized = normalizer.normalize(rawPoints, extractOptions);

    domain::terrain::TerrainTinBuilder builder;
    auto result = builder.build(normalized.points, buildOptions);
    result.extractSummary = normalized.summary;
    result.extractOptions = extractOptions;
    result.buildOptions = buildOptions;
    result.extractSummary.triangleCount = result.triangles.size();
    return result;
}

} // namespace roadproto::application
