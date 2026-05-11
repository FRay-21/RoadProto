# 地形数模模块

## 模块编码

`TERRAIN`

## 模块目的

管理地形数模相关原型流程，并为后续依赖地形的联动更新打基础。

## V0.1.6 定版说明

当前地形主功能以 `DN_TERRAIN_TIN_CREATE`、`DN_TERRAIN_TIN_EDIT`、`DN_TERRAIN_TIN_EXPORT`、`DN_TERRAIN_TIN_IMPORT` 和自定义实体 `DnTerrainTinEntity` 为准。`RD_TERRAIN_MARKDIRTY` 仍保留为关系联动框架示例，不代表当前 TIN 实体已实现源对象 reactor 自动重建。

模块内职责按三层拆开：

- 业务逻辑：地形点提取规则、文字高程解析、块属性高程识别、文字-点近邻合并、重复点清洗、TIN 构网、三角形过滤、RMesh 文件格式和高程查询能力，沉淀在 `src/domain/terrain` 与 `src/application/terrain`。
- CAD 功能逻辑：Ribbon 命令触发、样例对象选择、按同图层同类型扫描、状态栏进度、临时隐藏源对象、自定义实体插入/绘制/DWG 持久化、双击编辑转发、文件对话框，沉淀在 `src/cad_adapter/objectarx/terrain` 和托管 Ribbon 插件。
- UI 逻辑：当前原型阶段由托管 WPF 工程负责 AutoCAD 可见 Ribbon、双击事件转发和后续用户参数窗口；历史 C++ `TerrainTinCreateDialog` 仅作为过渡实现保留，后续新增或重做窗口先统一迁移到 WPF，且不得移动领域算法和自定义实体能力。

## V0.1 命令

| 命令 | 说明 | 业务文档 |
| --- | --- | --- |
| `RD_TERRAIN_MARKDIRTY` | 历史框架示例：地形更新后，将示例纵断面和横断面实体标记为需要重建。 | `docs/business/terrain/地形更新联动示例.md` |
| `DN_TERRAIN_TIN_CREATE` | 从 CAD 地形相关对象提取高程点，清洗后构建 TIN 地形数模并生成 `DnTerrainTinEntity`。 | `docs/business/terrain/地形构网_TIN地形数模.md` |
| `DN_TERRAIN_TIN_EDIT` | 编辑已有 `DnTerrainTinEntity` 的显示模式和构网参数，可基于实体内点集重新生成 TIN。 | `docs/business/terrain/地形数模_编辑.md` |
| `DN_TERRAIN_TIN_EDIT_HANDLE` | 内部双击 Bridge 命令，按实体 handle 打开已有 `DnTerrainTinEntity` 编辑流程，不挂 Ribbon。 | `docs/business/terrain/地形数模_编辑.md` |
| `DN_TERRAIN_TIN_EXPORT` | 将已有 `DnTerrainTinEntity` 导出为 `.rmesh` 文件。 | `docs/business/terrain/地形数模_RMesh导入导出.md` |
| `DN_TERRAIN_TIN_IMPORT` | 从 `.rmesh` 文件导入并生成新的 `DnTerrainTinEntity`。 | `docs/business/terrain/地形数模_RMesh导入导出.md` |

模块文档只记录模块职责、代码落点和文档索引。每个独立功能的操作流程、输入输出和业务规则以对应业务文档为准。

## Ribbon

分组标题：`数模`

当前有两层 Ribbon 信息：

- C++ 框架层继续通过 `RibbonModel` 维护模块、面板和命令元数据。
- AutoCAD 可见 Ribbon 由托管插件 `RoadProto.Terrain.UI.dll` 创建，入口位于 `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`。ARX 加载时会自动尝试加载同配置的托管插件，若未出现 Ribbon，可手动执行 `NETLOAD`。

新增按钮：

- 显示名：`地形构网`
- 命令名：`DN_TERRAIN_TIN_CREATE`
- AutoCAD Ribbon 位置：`RoadProto` 选项卡 / `数模` 面板
- 图标：托管 Ribbon 插件内置小型地形图标，按钮尺寸与 `编辑数模` 一致。
- 显示名：`编辑数模`
- 命令名：`DN_TERRAIN_TIN_EDIT`
- AutoCAD Ribbon 位置：`RoadProto` 选项卡 / `数模` 面板
- 显示名称：`导出数模`
- 命令名：`DN_TERRAIN_TIN_EXPORT`
- AutoCAD Ribbon 位置：`RoadProto` 选项卡 / `数模` 面板
- 显示名称：`导入数模`
- 命令名：`DN_TERRAIN_TIN_IMPORT`
- AutoCAD Ribbon 位置：`RoadProto` 选项卡 / `数模` 面板

## 代码落点

| 层 | 路径 | 职责 |
| --- | --- | --- |
| 领域层 | `src/domain/terrain` | 文字高程解析、点清洗、TIN 构建、高程查询和 RMesh 文件读写；不得依赖 ObjectARX。 |
| 应用层 | `src/application/terrain` | 组织地形构网和历史联动示例流程。 |
| 模块层 | `src/modules/terrain` | 注册 `TERRAIN` 模块和地形数模命令元数据。 |
| CAD 适配层 | `src/cad_adapter/objectarx/terrain` | CAD 对象提取、进度显示、自定义实体、参数窗口、双击 handle 编辑和文件对话框。 |
| UI 层 | `src/ui/wpf/RoadProto.Terrain.UI` | AutoCAD 可见 Ribbon 和双击事件转发，保留后续 WPF 参数窗口接入口。 |
| 测试 | `tests/core_tests.cpp` | 不依赖 AutoCAD 的地形点解析、清洗、构网、查询和 RMesh 文件读写测试。 |

## 后续说明

- 当前命令已支持点、线、多段线、文字、块属性和有限嵌套块识别。
- WPF 工程 `RoadProto.Terrain.UI` 目前负责 AutoCAD 可见 Ribbon 和双击事件转发，并保留 `TerrainTinCreateWindow`、ViewModel、DTO 和 Bridge 接口。
- 当前构网参数窗口与编辑窗口仍有历史 C++ Win32 实现；当前原型阶段后续修改或重做时应通过 C++/CLI 或 AutoCAD .NET Bridge 接入 WPF 参数确认层，WPF 不直接接触 ObjectARX 类型。未来如整体 UI 技术栈调整，也应继续遵守 Bridge/Adapter 解耦边界。
- 在关系持久化机制完成后，再增加自动监听源对象修改和地面线提取能力。
- 具体操作流程、输入输出和业务规则不得写在模块文档中，应写入对应功能业务文档。
