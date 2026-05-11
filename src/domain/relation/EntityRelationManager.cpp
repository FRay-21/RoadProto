#include "domain/relation/EntityRelationManager.h"

#include <algorithm>
#include <set>

namespace roadproto::domain {

namespace {

bool containsId(const std::vector<EntityId>& ids, const EntityId& id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

void eraseId(std::vector<EntityId>& ids, const EntityId& id)
{
    ids.erase(std::remove(ids.begin(), ids.end(), id), ids.end());
}

} // namespace

std::wstring toWString(DesignEntityType type)
{
    switch (type) {
    case DesignEntityType::TerrainModel:
        return L"TerrainModel";
    case DesignEntityType::Alignment:
        return L"Alignment";
    case DesignEntityType::Interchange:
        return L"Interchange";
    case DesignEntityType::Intersection:
        return L"Intersection";
    case DesignEntityType::Profile:
        return L"Profile";
    case DesignEntityType::CrossSection:
        return L"CrossSection";
    case DesignEntityType::DrawingQuantity:
        return L"DrawingQuantity";
    case DesignEntityType::Utility:
        return L"Utility";
    default:
        return L"Unknown";
    }
}

std::wstring toWString(UpdateStatus status)
{
    switch (status) {
    case UpdateStatus::Clean:
        return L"Clean";
    case UpdateStatus::Dirty:
        return L"Dirty";
    case UpdateStatus::NeedsRebuild:
        return L"NeedsRebuild";
    case UpdateStatus::Rebuilding:
        return L"Rebuilding";
    case UpdateStatus::Failed:
        return L"Failed";
    default:
        return L"Unknown";
    }
}

bool EntityRelationManager::upsertEntity(DesignEntityDescriptor descriptor)
{
    if (descriptor.id.empty()) {
        return false;
    }

    const auto existing = entities_.find(descriptor.id);
    if (existing != entities_.end()) {
        descriptor.dependsOn = existing->second.dependsOn;
        descriptor.dependedBy = existing->second.dependedBy;
    }

    entities_[descriptor.id] = std::move(descriptor);
    return true;
}

bool EntityRelationManager::addDependency(const EntityId& entityId, const EntityId& dependsOnEntityId)
{
    auto entity = entities_.find(entityId);
    auto dependency = entities_.find(dependsOnEntityId);
    if (entity == entities_.end() || dependency == entities_.end() || entityId == dependsOnEntityId) {
        return false;
    }

    if (!containsId(entity->second.dependsOn, dependsOnEntityId)) {
        entity->second.dependsOn.push_back(dependsOnEntityId);
    }

    if (!containsId(dependency->second.dependedBy, entityId)) {
        dependency->second.dependedBy.push_back(entityId);
    }

    return true;
}

bool EntityRelationManager::removeDependency(const EntityId& entityId, const EntityId& dependsOnEntityId)
{
    auto entity = entities_.find(entityId);
    auto dependency = entities_.find(dependsOnEntityId);
    if (entity == entities_.end() || dependency == entities_.end()) {
        return false;
    }

    eraseId(entity->second.dependsOn, dependsOnEntityId);
    eraseId(dependency->second.dependedBy, entityId);
    return true;
}

std::vector<RebuildRequest> EntityRelationManager::markUpdated(
    const EntityId& sourceEntityId,
    const std::wstring& reason)
{
    auto entity = entities_.find(sourceEntityId);
    if (entity == entities_.end()) {
        return {};
    }

    entity->second.updatedAt = std::chrono::system_clock::now();
    entity->second.updateStatus = UpdateStatus::Clean;
    entity->second.needsRecalculate = false;
    entity->second.needsGraphicRefresh = false;

    return markDependentsForRebuild(UpdateEvent{sourceEntityId, UpdateEventType::SourceUpdated, reason});
}

std::vector<RebuildRequest> EntityRelationManager::markDirty(
    const EntityId& sourceEntityId,
    const std::wstring& reason)
{
    auto entity = entities_.find(sourceEntityId);
    if (entity == entities_.end()) {
        return {};
    }

    entity->second.updateStatus = UpdateStatus::Dirty;
    entity->second.needsRecalculate = true;
    entity->second.needsGraphicRefresh = true;

    return markDependentsForRebuild(UpdateEvent{sourceEntityId, UpdateEventType::ManualDirty, reason});
}

std::optional<DesignEntityDescriptor> EntityRelationManager::findEntity(const EntityId& entityId) const
{
    const auto it = entities_.find(entityId);
    if (it == entities_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::vector<DesignEntityDescriptor> EntityRelationManager::allEntities() const
{
    std::vector<DesignEntityDescriptor> result;
    for (const auto& item : entities_) {
        result.push_back(item.second);
    }
    return result;
}

void EntityRelationManager::clear()
{
    entities_.clear();
}

std::vector<RebuildRequest> EntityRelationManager::markDependentsForRebuild(const UpdateEvent& event)
{
    std::vector<RebuildRequest> requests;
    std::vector<EntityId> pending;
    std::set<EntityId> visited;

    const auto source = entities_.find(event.sourceEntityId);
    if (source == entities_.end()) {
        return requests;
    }

    pending.insert(pending.end(), source->second.dependedBy.begin(), source->second.dependedBy.end());

    while (!pending.empty()) {
        const auto currentId = pending.back();
        pending.pop_back();

        if (visited.find(currentId) != visited.end()) {
            continue;
        }
        visited.insert(currentId);

        auto current = entities_.find(currentId);
        if (current == entities_.end()) {
            continue;
        }

        current->second.updateStatus = UpdateStatus::NeedsRebuild;
        current->second.needsRecalculate = true;
        current->second.needsGraphicRefresh = true;

        requests.push_back(RebuildRequest{
            event.sourceEntityId,
            currentId,
            current->second.moduleCode,
            true,
            true,
            event.reason});

        pending.insert(pending.end(), current->second.dependedBy.begin(), current->second.dependedBy.end());
    }

    return requests;
}

} // namespace roadproto::domain
