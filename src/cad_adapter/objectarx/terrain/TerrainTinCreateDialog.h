#pragma once

#include "domain/terrain/TerrainTinData.h"

#include <functional>
#include <string>

namespace roadproto::cad_adapter::objectarx {

using TerrainTinProgressCallback = std::function<void(int, const std::wstring&)>;
using TerrainTinGenerateCallback = std::function<bool(
    const domain::terrain::TinExtractOptions&,
    const domain::terrain::TinBuildOptions&,
    const TerrainTinProgressCallback&,
    std::wstring&)>;

struct TerrainTinCreateDialogInput {
    domain::terrain::TinExtractSummary summary;
    domain::terrain::TinExtractOptions extractOptions;
    domain::terrain::TinBuildOptions buildOptions;
    std::wstring sampleLayer;
    std::wstring sampleType;
    std::size_t hiddenSourceObjectCount = 0;
};

struct TerrainTinCreateDialogResult {
    bool accepted = false;
    bool generated = false;
    domain::terrain::TinExtractOptions extractOptions;
    domain::terrain::TinBuildOptions buildOptions;
};

struct TerrainTinEditDialogInput {
    std::size_t pointCount = 0;
    std::size_t triangleCount = 0;
    std::size_t boundaryEdgeCount = 0;
    double minElevation = 0.0;
    double maxElevation = 0.0;
    domain::terrain::TinBuildOptions buildOptions;
};

struct TerrainTinEditDialogResult {
    bool accepted = false;
    bool rebuildRequested = false;
    domain::terrain::TinBuildOptions buildOptions;
};

bool showTerrainTinCreateDialog(
    const TerrainTinCreateDialogInput& input,
    TerrainTinCreateDialogResult& result,
    const TerrainTinGenerateCallback& generateCallback);

bool showTerrainTinEditDialog(
    const TerrainTinEditDialogInput& input,
    TerrainTinEditDialogResult& result);

} // namespace roadproto::cad_adapter::objectarx
