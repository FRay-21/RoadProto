#include "application/terrain/TerrainUpdateSampleService.h"

#include <sstream>

namespace roadproto::application {

namespace {

const auto kTerrainEntityId = L"terrain.sample.tin.001";
const auto kProfileEntityId = L"profile.sample.groundline.001";
const auto kCrossSectionEntityId = L"cross_section.sample.groundline.001";

domain::DesignEntityDescriptor makeEntity(
    const wchar_t* id,
    domain::DesignEntityType type,
    const wchar_t* moduleCode)
{
    return domain::DesignEntityDescriptor{
        domain::EntityId::fromString(id),
        type,
        moduleCode};
}

} // namespace

TerrainUpdateSampleService::TerrainUpdateSampleService(domain::EntityRelationManager& relationManager)
    : relationManager_(relationManager)
{
}

TerrainUpdateSampleResult TerrainUpdateSampleService::run()
{
    ensureSampleEntities();

    const auto terrainId = domain::EntityId::fromString(kTerrainEntityId);
    auto requests = relationManager_.markUpdated(terrainId, L"Sample terrain model updated");

    std::wstringstream summary;
    summary << L"RD_TERRAIN_MARKDIRTY completed. "
            << requests.size()
            << L" dependent design entities were marked as NeedsRebuild.";

    return TerrainUpdateSampleResult{terrainId, requests, summary.str()};
}

void TerrainUpdateSampleService::ensureSampleEntities()
{
    const auto terrainId = domain::EntityId::fromString(kTerrainEntityId);
    const auto profileId = domain::EntityId::fromString(kProfileEntityId);
    const auto crossSectionId = domain::EntityId::fromString(kCrossSectionEntityId);

    relationManager_.upsertEntity(makeEntity(kTerrainEntityId, domain::DesignEntityType::TerrainModel, L"TERRAIN"));
    relationManager_.upsertEntity(makeEntity(kProfileEntityId, domain::DesignEntityType::Profile, L"PROFILE"));
    relationManager_.upsertEntity(makeEntity(kCrossSectionEntityId, domain::DesignEntityType::CrossSection, L"CROSS_SECTION"));
    relationManager_.addDependency(profileId, terrainId);
    relationManager_.addDependency(crossSectionId, terrainId);
}

} // namespace roadproto::application
