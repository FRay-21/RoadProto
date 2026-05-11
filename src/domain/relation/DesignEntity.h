#pragma once

#include "domain/common/EntityId.h"

#include <chrono>
#include <string>
#include <vector>

namespace roadproto::domain {

enum class DesignEntityType {
    Unknown,
    TerrainModel,
    Alignment,
    Interchange,
    Intersection,
    Profile,
    CrossSection,
    DrawingQuantity,
    Utility
};

enum class UpdateStatus {
    Clean,
    Dirty,
    NeedsRebuild,
    Rebuilding,
    Failed
};

struct DesignEntityDescriptor {
    EntityId id;
    DesignEntityType type = DesignEntityType::Unknown;
    std::wstring moduleCode;
    std::vector<EntityId> dependsOn;
    std::vector<EntityId> dependedBy;
    std::chrono::system_clock::time_point updatedAt = std::chrono::system_clock::now();
    UpdateStatus updateStatus = UpdateStatus::Clean;
    bool needsRecalculate = false;
    bool needsGraphicRefresh = false;
};

enum class UpdateEventType {
    SourceUpdated,
    SourceDeleted,
    ManualDirty
};

struct UpdateEvent {
    EntityId sourceEntityId;
    UpdateEventType type = UpdateEventType::SourceUpdated;
    std::wstring reason;
    std::chrono::system_clock::time_point occurredAt = std::chrono::system_clock::now();
};

struct RebuildRequest {
    EntityId sourceEntityId;
    EntityId targetEntityId;
    std::wstring targetModuleCode;
    bool requiresRecalculate = true;
    bool requiresGraphicRefresh = true;
    std::wstring reason;
};

std::wstring toWString(DesignEntityType type);
std::wstring toWString(UpdateStatus status);

} // namespace roadproto::domain
