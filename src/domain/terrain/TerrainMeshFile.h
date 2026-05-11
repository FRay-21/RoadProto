#pragma once

#include "domain/terrain/TerrainTinData.h"

#include <string>

namespace roadproto::domain::terrain {

struct TerrainMeshFileReadResult {
    bool succeeded = false;
    std::wstring errorMessage;
    TinBuildResult mesh;
};

class TerrainMeshFile final {
public:
    bool write(const std::wstring& path, const TinBuildResult& mesh, std::wstring& errorMessage) const;
    TerrainMeshFileReadResult read(const std::wstring& path) const;
};

} // namespace roadproto::domain::terrain
