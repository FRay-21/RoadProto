# 地形数模 / RMesh 导入导出

## 基本信息

- 功能名称：地形数模 RMesh 导入导出
- 所属模块：地形数模
- 命令名称：`DN_TERRAIN_TIN_EXPORT`、`DN_TERRAIN_TIN_IMPORT`
- 对应代码入口：`TerrainMeshFile.*`、`ObjectArxTerrainTinCommand.cpp`
- 文件后缀：`.rmesh`
- 业务文档维护人：RoadProto
- 原型版本：v0.1.6
- 是否可复用：是

## 功能背景

地形数模需要在不同 DWG 或不同测试场景之间流转。RMesh 是 RoadProto 当前用于保存 TIN 数据的轻量文件格式。

## 业务目标

- 将 DWG 中已有的 `DnTerrainTinEntity` 导出为独立 `.rmesh` 文件。
- 在另一个 DWG 中导入 `.rmesh`，恢复可显示、可保存、可查询、可编辑的 `DnTerrainTinEntity`。
- 保留 TIN 点、三角形、边界边、显示模式、构网参数、提取参数和点来源信息。

## 适用场景

- 地形数模跨 DWG 流转。
- 保存测试样例，复现构网和显示问题。
- 后续纵断面、横断面取高测试需要稳定地形输入。

## 输入条件

- CAD 选择对象：导出时选择 `DnTerrainTinEntity`；导入时无需选择实体。
- 用户输入参数：导出文件路径或导入文件路径。
- 已有设计实体：导出时需要已有地形数模实体。
- 外部数据：`.rmesh` 文件。

## 输出结果

- CAD 图形实体：导入时新增 `DnTerrainTinEntity`。
- 领域实体：`TerrainMeshFile` 中保存的 TIN 点、三角形、边界边和参数。
- 外部文件：导出时生成 `.rmesh` 文件。
- 更新通知或重建请求：无。

## 操作流程

1. 用户运行 `DN_TERRAIN_TIN_EXPORT`，选择一个 `DnTerrainTinEntity`。
2. 系统弹出文件选择框，默认后缀为 `.rmesh`。
3. `TerrainMeshFile` 将数模数据写入 RoadProto RMesh 二进制文件。
4. 用户在目标 DWG 中运行 `DN_TERRAIN_TIN_IMPORT`，选择 `.rmesh` 文件。
5. 系统读取并校验文件头、版本号、点索引和边界边索引。
6. 导入成功后，ObjectARX 适配层创建新的 `DnTerrainTinEntity` 并插入模型空间。
7. 导入实体与普通创建实体一致，支持 `REGEN`、DWG 持久化和编辑。

## 关键业务规则

- RMesh 文件读写在 `domain` 层实现，不依赖 ObjectARX 类型。
- ObjectARX 命令层只负责选择实体、选择文件、调用 `TerrainMeshFile` 和插入实体。
- `.rmesh` V1 格式包含固定 magic、版本号、提取参数、构网参数、统计摘要、点、三角形和边界边。
- 读取时必须拒绝非法 magic、不支持版本、异常 EOF、非法布尔值、过大数量和越界三角形索引。
- 导出不修改当前 DWG。
- 导入只新增一个 `DnTerrainTinEntity`，不修改外部来源对象。

## 可复用性说明

- 可复用内容：RMesh V1 文件格式、二进制读写、数据校验、地形数模跨 DWG 流转。
- 临时原型内容：文件路径选择和导入实体插入由 ObjectARX 命令直接承接。
- 正式复用前需要改造的内容：多版本升级迁移、文件热更新、外部来源映射和更完整的错误定位。

## 与其他模块的依赖关系

- 上游模块：地形构网、地形数模编辑。
- 下游模块：纵断面、横断面、道路模型、土方计算。
- 实体联动行为：导入导出不触发其他实体自动重建。

## 当前限制

- `.rmesh` V1 不保存 DWG 内原始对象本体，只保存来源 handle、块名、属性 Tag 等追踪信息。
- 导入到其他 DWG 后，来源 handle 仅用于追溯，不保证能映射到目标 DWG 中的原对象。
- V0.1.6 暂不实现与外部文件的自动热更新，也不实现多版本 RMesh 升级迁移。

## 后续对接正式 EICAD 功能的注意事项

- RMesh 可以继续作为 RoadProto 内部交换格式，但正式交付前需要明确版本兼容策略。
- 文件格式读写不得依赖 CAD 对话框或实体选择。
