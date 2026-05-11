# 实体关系与联动更新机制

## 目的

道路设计实体之间天然存在依赖关系。地形数模变化后，可能影响纵断面地面线、横断面地面线、算量、出图和后续三维模型。V0.1 提供最小关系管理器，让依赖关系在统一位置表达，而不是写死在某个模块内部。

## 核心类型

- `EntityId`：领域实体唯一 ID。
- `DesignEntityDescriptor`：统一设计实体描述。
- `DesignEntityType`：地形、路线、纵断面、横断面、平交口、出图算量等实体类型。
- `UpdateStatus`：`Clean`、`Dirty`、`NeedsRebuild`、`Rebuilding`、`Failed`。
- `UpdateEvent`：源实体更新通知。
- `RebuildRequest`：面向被影响实体的重建请求。
- `EntityRelationManager`：保存实体、依赖关系，并负责脏标记传播。

## 依赖方向

如果纵断面地面线依赖地形数模：

```text
ProfileGroundLine --depends on--> TerrainModel
TerrainModel --depended by--> ProfileGroundLine
```

当地形数模更新时，`EntityRelationManager::markUpdated` 会把被依赖实体标记为：

- `updateStatus = NeedsRebuild`
- `needsRecalculate = true`
- `needsGraphicRefresh = true`

## V0.1 示例

命令 `RD_TERRAIN_MARKDIRTY` 创建以下示例关系：

```text
terrain.sample.tin.001
  -> profile.sample.groundline.001
  -> cross_section.sample.groundline.001
```

该命令将地形数模标记为已更新，并为纵断面和横断面示例实体生成重建请求。

## 后续扩展方向

后续版本可以增加：

- 基于 DWG 字典或扩展字典的关系持久化。
- 与 ObjectARX 实体 handle 的绑定。
- 重建队列。
- 局部重算策略。
- 更新事务分组。
- 在 UI 中展示待更新实体并让用户确认。
