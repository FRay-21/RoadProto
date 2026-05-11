#pragma once

#include "domain/common/EntityId.h"
#include "domain/relation/EntityRelationManager.h"

#include <string>
#include <vector>

namespace roadproto::application {

struct TerrainUpdateSampleResult {
    domain::EntityId terrainId;
    std::vector<domain::RebuildRequest> rebuildRequests;
    std::wstring summary;
};

class TerrainUpdateSampleService {
public:
    explicit TerrainUpdateSampleService(domain::EntityRelationManager& relationManager);

    TerrainUpdateSampleResult run();

private:
    void ensureSampleEntities();

    domain::EntityRelationManager& relationManager_;
};

} // namespace roadproto::application
