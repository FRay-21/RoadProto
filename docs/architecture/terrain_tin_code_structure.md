# 地形 TIN 数模代码结构

## 当前版本

- 原型版本：`v0.1.6`
- ARX：`RoadProto_v0.1.6_20260508_TerrainTinDblClick.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 自定义实体：`DnTerrainTinEntity`
- 业务文档：`docs/business/terrain/地形构网_TIN地形数模.md`、`docs/business/terrain/地形数模_编辑.md`、`docs/business/terrain/地形数模_RMesh导入导出.md`

## 分层原则

地形 TIN 功能遵守“C++ ObjectARX 核心 + 可替换 UI 层”：

- 业务逻辑不直接写在 UI 事件里。
- CAD 选择、绘制、DWG 持久化和 ObjectARX 对象访问不进入 `domain`。
- WPF / 托管插件不直接操作 `AcDbEntity`、`AcDbObjectId`、`ads_name`。
- Bridge / Adapter 只做数据转换和调用转发，不写构网、清洗、解析等业务算法。

## 业务逻辑层

位置：`src/domain/terrain`

职责：

- 定义 TIN 数据结构：`TinPoint`、`TinTriangle`、`TinBuildOptions`、`TinExtractOptions`、`TinExtractSummary`、`TinBuildResult`。
- 解析文字高程：`TerrainTextElevationParser`。
- 清洗点集、合并重复点、处理文字-点近邻关系：`TerrainPointNormalizer`。
- 构建 Delaunay / TIN：`TerrainTinBuilder`。
- 查询任意 XY 位置高程：`TerrainSurfaceQuery`。
- 读写 `.rmesh` 文件：`TerrainMeshFile`。

规则：

- 只处理普通 C++ 数据和业务规则，不包含 ObjectARX 头文件。
- Delaunay 使用 X/Y；Z 作为高程属性保存和插值。
- `.rmesh` 文件格式读写属于领域能力，不能依赖 AutoCAD 文件对话框或实体选择。

## 应用流程层

位置：`src/application/terrain`

职责：

- 组织地形构网流程服务和历史联动示例服务。
- 连接领域能力，形成可由 CAD adapter 调用的 use case。
- 保持流程可测试，不持有 AutoCAD 原始指针。

说明：

- `TerrainUpdateSampleService` 是历史关系联动示例，用于说明 `EntityRelationManager` 的重建请求机制。
- 当前 TIN 主线的自动源对象监听尚未实现；`DnTerrainTinEntity` 只预留来源记录、`m_isDirty` 和 `RebuildFromSourceObjects`。

## CAD 功能层

位置：`src/cad_adapter/objectarx/terrain`

职责：

- `ObjectArxTerrainTinCommand`：注册和执行 `DN_TERRAIN_TIN_CREATE`、`DN_TERRAIN_TIN_EDIT`、`DN_TERRAIN_TIN_EDIT_HANDLE`、`DN_TERRAIN_TIN_EXPORT`、`DN_TERRAIN_TIN_IMPORT`。
- `TerrainTinCreateDialog`：历史过渡创建/编辑参数窗口。当前原型阶段后续新增或重做参数窗口时应迁移到 WPF。
- `DnTerrainTinEntity`：保存点、三角形、边界、参数、统计和来源追踪；实现 DWG 持久化、显示、范围、变换和查询接口。
- 负责读取 CAD 图元、选择集、模型空间扫描、状态栏进度、临时隐藏源对象、文件对话框和实体插入。

规则：

- 命令层只组织流程，不堆业务算法。
- 读取 `AcDbPoint`、`AcDbLine`、多段线、文字、块和属性块后，应转换为领域数据再交给领域服务处理。
- 自定义实体可调用领域查询能力，但不能把 UI 事件逻辑写入实体。

## UI / Bridge 层

位置：`src/ui/wpf/RoadProto.Terrain.UI`

职责：

- `RoadProtoRibbonExtension` 创建 AutoCAD 可见 `RoadProto` 选项卡和 `数模` 面板。
- Ribbon 按钮触发 C++ 命令：`DN_TERRAIN_TIN_CREATE`、`DN_TERRAIN_TIN_EDIT`、`DN_TERRAIN_TIN_EXPORT`、`DN_TERRAIN_TIN_IMPORT`。
- 监听 AutoCAD `.NET` 的 `BeginDoubleClick`，识别 `DNTERRAINTINENTITY` 后转发到 `DN_TERRAIN_TIN_EDIT_HANDLE <handle>`。
- `TerrainTinCreateWindow`、ViewModel、DTO 和 Bridge 接口保留为后续正式 WPF 参数窗口接入点。

当前 V0.1.6 中，参数窗口和编辑窗口曾由 C++ Win32 `TerrainTinCreateDialog` 承接，这是历史过渡实现。全局规则已明确当前原型阶段后续新增或重做用户对话框统一使用 WPF；WPF 工程不承载地形业务算法，也不直接访问 ObjectARX 类型。

## 第三方算法

位置：`third_party/delaunator-cpp`

- 用途：二维 Delaunay 三角剖分。
- License：MIT，需保留第三方 license。
- 封装位置：`TerrainTinBuilder`。

## 已知限制

- `FindTriangle` V1 使用简单遍历，后续需要空间索引。
- 暂不支持约束 Delaunay、断裂线、洞口边界和等高线自动采样。
- 暂不自动监听源对象修改；跨 DWG 导入 `.rmesh` 后可编辑数模本体，但不会自动重连原图源对象。
- 当前地形 UI 仍包含历史 C++ 参数窗口。当前阶段后续替换为 WPF 时，领域算法、自定义实体和 CAD adapter 入口应保持稳定；未来如整体 UI 技术栈更换，也应复用同样的核心边界。
