#pragma once

#include "domain/relation/DesignEntity.h"

#include <map>
#include <optional>
#include <vector>

namespace roadproto::domain {

class EntityRelationManager {
public:
    bool upsertEntity(DesignEntityDescriptor descriptor);
    bool addDependency(const EntityId& entityId, const EntityId& dependsOnEntityId);
    bool removeDependency(const EntityId& entityId, const EntityId& dependsOnEntityId);

    std::vector<RebuildRequest> markUpdated(const EntityId& sourceEntityId, const std::wstring& reason);
    std::vector<RebuildRequest> markDirty(const EntityId& sourceEntityId, const std::wstring& reason);

    std::optional<DesignEntityDescriptor> findEntity(const EntityId& entityId) const;
    std::vector<DesignEntityDescriptor> allEntities() const;
    void clear();

private:
    std::vector<RebuildRequest> markDependentsForRebuild(const UpdateEvent& event);

    std::map<EntityId, DesignEntityDescriptor> entities_;
};

} // namespace roadproto::domain
