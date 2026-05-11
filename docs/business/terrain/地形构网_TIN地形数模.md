# 地形构网 / TIN 地形数模

## 基本信息

- 功能名称：地形构网 / TIN 地形数模
- 所属模块：地形数模
- 命令名称：`DN_TERRAIN_TIN_CREATE`
- 对应代码入口：`TerrainTinCreateService.*`、`TerrainTinBuilder.*`、`ObjectArxTerrainTinCommand.cpp`
- 业务文档维护人：RoadProto
- 原型版本：`v0.1.6`
- 是否可复用：是

## 功能背景

道路设计中的纵断面地面线、横断面地面线、土方计算、贴地建模等能力都依赖可查询的地形数模。该原型先完成从 CAD 图纸对象中提取高程点、构建 TIN、自定义实体显示和持久化的基础链路。

## 业务目标

- 从 CAD 中常见地形对象提取 X/Y/Z 点。
- 自动清洗重复点、近重复点、文字与点近邻表达的同一地形点。
- 使用二维 Delaunay / TIN 构网生成可显示、可保存、可查询的地形实体。
- 为后续道路设计对象联动更新预留源对象记录和重建接口。

## V0.1.6 定版范围与解耦边界

本功能在 V0.1.6 作为当前地形数模原型定版。后续不在本版本继续扩大地形算法范围，新增能力应在新版本中独立记录。

业务逻辑关注“地形点如何成为一个可查询 TIN”：

- 从点、线、多段线、文字、块属性和有限嵌套块中识别 X/Y/Z 地形点。
- 按 XY 容差合并重复点，按 Z 容差处理同点高程，记录 Z 冲突和无效对象。
- 将文字标高与附近几何点关联，必要时用文字高程给无高程几何点赋 Z。
- 用二维 X/Y 执行 Delaunay 构网，Z 只作为高程属性参与显示和插值查询。
- 过滤退化三角形、最小面积三角形和超过最大边长的三角形，形成可保存、可显示、可查询的数模结果。

CAD 功能逻辑关注“如何从 AutoCAD 图纸进入和呈现业务结果”：

- Ribbon 入口、命令触发、用户点选样例对象、按同图层同类型扫描模型空间、状态栏进度和临时隐藏源对象均属于 ObjectARX / AutoCAD 适配层职责。
- `DnTerrainTinEntity` 的插入、DWG 持久化、`subWorldDraw` 边线高程着色、边界显示、`subGetGeomExtents`、`subTransformBy` 和双击编辑入口属于 CAD 实体职责。
- `.rmesh` 导入导出的文件对话框和实体选择属于 CAD 命令职责；文件格式读写本身属于领域层能力。
- WPF / 托管插件只创建 AutoCAD 可见 Ribbon，并转发双击事件到 C++ 命令；不直接操作 `AcDbEntity`、`AcDbObjectId`、`ads_name`。

代码逻辑按项目分层沉淀：

- `src/domain/terrain`：`TinPoint`、`TinTriangle`、`TinBuildOptions`、`TinExtractSummary`、`TerrainTextElevationParser`、`TerrainPointNormalizer`、`TerrainTinBuilder`、`TerrainSurfaceQuery`、`TerrainMeshFile`，不依赖 ObjectARX。
- `src/application/terrain`：地形构网和示例联动的流程服务，组织领域能力，不持有 AutoCAD 指针。
- `src/cad_adapter/objectarx/terrain`：`ObjectArxTerrainTinCommand`、`TerrainTinCreateDialog`、`DnTerrainTinEntity`，负责 AutoCAD 选择、进度、对话框、实体读写和命令交互。
- `src/ui/wpf/RoadProto.Terrain.UI`：`RoadProtoRibbonExtension` 创建可见 Ribbon，并通过 AutoCAD .NET 事件把双击实体转发给 `DN_TERRAIN_TIN_EDIT_HANDLE`；`TerrainTinCreateWindow`、ViewModel 和 DTO 保留为后续替换 UI 的接入点。
- `third_party/delaunator-cpp`：Delaunay 三角剖分第三方头文件，保留 MIT License。

完整代码结构索引见 `docs/architecture/terrain_tin_code_structure.md`。

## 适用场景

- CAD 地形图中包含点、线、多段线、文字标高或标高块。
- 产品经理演示地形构网基础流程。
- 后续纵断面、横断面和土方计算能力接入前的地形数模验证。

## 输入条件

- CAD 选择对象：用户可连续点选样例对象；系统每次按样例对象的图层和精确图元类型，在模型空间中自动提取同图层、同类型对象并隐藏当前类。支持样例类型包括 `AcDbPoint`、`AcDbLine`、`AcDbPolyline`、`AcDb2dPolyline`、`AcDb3dPolyline`、`AcDbText`、`AcDbMText`、`AcDbBlockReference` 和带属性块。用户按 Enter 后结束提取并打开参数窗口。
- 用户输入参数：参数窗口展示统计信息，开放 XY 合并容差、最大三角边长、是否过滤退化三角形和显示模式；其余高级参数使用稳定默认值。提取时使用 AutoCAD 状态栏进度条，窗口内生成时使用窗口进度条。
- 已有设计实体：V1 不要求已有道路设计实体。
- 外部数据：暂无。

## 输出结果

- CAD 图形实体：`DnTerrainTinEntity` 自定义实体。
- 领域数据：`TinPoint`、`TinTriangle`、`TinBuildResult`。
- 查询接口：`SampleElevation`、`FindTriangle`、`GetPoints`、`GetTriangles`。
- 更新预留：点来源 handle、文字关联 handle、块名、属性 Tag、`RebuildFromSourceObjects` 和 `m_isDirty`。

## 操作流程

1. 用户在 Ribbon 的 `RoadProto` / `数模` / `地形构网` 按钮触发命令，或直接运行 `DN_TERRAIN_TIN_CREATE`。
2. 命令提示用户点选地形样例对象；如果选择集中有多个对象，只使用第一个对象作为当前样例。
3. ObjectARX 适配层读取样例对象图层和精确图元类型，并扫描模型空间中同图层、同类型对象。
4. 提取过程显示状态栏进度条；提取完成后临时隐藏当前同类源对象，用户可继续点选下一类对象。
5. 用户按 Enter 结束连续提取后，C++ terrain service 执行点清洗、文字-点近邻合并和重复点处理。
6. 命令弹出 `TerrainTinCreateDialog`，展示累计提取统计和少量构网参数。
7. 用户点击 `生成地形数模` 后，窗口显示生成进度，TIN builder 使用 X/Y 执行 Delaunay 三角剖分，Z 作为高程属性保存；V0.1.6 基于 `third_party/delaunator-cpp` 的 MIT License 头文件实现三角剖分。
8. 每次点击 `生成地形数模` 都按当前参数重新生成最新预览实体，并替换上一版预览。
9. 用户点击 `预览` 可隐藏窗口查看 DWG，按 ESC 返回继续修改；点击 `确认` 保留最新预览并保留源对象隐藏状态，点击 `取消` 删除预览并恢复源对象。
10. 命令行输出提取统计、三角形数量和实体 handle。
11. 地形数模创建完成后，编辑功能见 `docs/business/terrain/地形数模_编辑.md`。
12. `.rmesh` 导入导出功能见 `docs/business/terrain/地形数模_RMesh导入导出.md`。

## 关键业务规则

- `AcDbPoint` 提取点坐标。
- `AcDbLine` 提取起点和终点。
- `AcDbPolyline`、`AcDb2dPolyline`、`AcDb3dPolyline` 提取顶点；2D 多段线 elevation 为 0 时标记为无有效高程。
- `AcDbText`、`AcDbMText` 支持 `12.35`、`+12.35`、`-3.20`、`H=12.35`、`EL=12.35`、`ELEV=12.35`、`Z=12.35`、`高程12.35`、`标高12.35`、`设计高程12.35`。
- 块属性 Tag 支持 `H`、`h`、`ELEV`、`Elev`、`EL`、`el`、`Z`、`z`、`高程`、`标高`、`设计高程`。
- 块处理优先级为：块属性高程点、块内文字高程点、块内几何点。
- 默认文字-点关联距离为 `0.5`，XY 合并容差为 `0.001`，Z 相等容差为 `0.01`。
- 有效点少于 3 个、点集共线、三角形全部被过滤时返回明确失败原因。
- 显示模式支持 `ColoredTriangles` 和 `BoundaryOnly`；`ColoredTriangles` 在 V0.1.6 中不再填充三角面，而是按三角网边线平均高程做 TrueColor 连续渐变映射，`BoundaryOnly` 只绘制边界边。

## 可复用性说明

- 可复用内容：
  - `TerrainTextElevationParser`
  - `TerrainPointNormalizer`
  - `TerrainTinBuilder`
  - `TerrainSurfaceQuery`
  - `DnTerrainTinEntity`
  - `TerrainTinCreateDialog`
  - 创建流程所需的预览实体插入、确认和取消策略
- 临时原型内容：
  - WPF UI 已编译为独立工程，当前负责 AutoCAD 可见 Ribbon 和双击事件转发；历史参数窗口与编辑窗口曾由 C++ Win32 `TerrainTinCreateDialog` 承接，当前原型阶段后续新增或重做窗口统一通过 Bridge 接入 WPF。
  - `FindTriangle` V1 使用简单遍历，后续应增加空间索引。
  - 当前参数窗口和编辑窗口仍有历史 C++ Win32 实现；当前原型阶段后续修改或重做时必须迁移为 WPF。未来如整体 UI 技术栈调整，可替换为 MFC、Qt、AutoCAD Palette 或其他正式 UI，但不能移动 C++ 核心算法和实体能力。
- 正式复用前需要改造的内容：
  - 接入托管 Bridge。
  - 完成源对象 reactor 和自动重建。
  - 补充约束 Delaunay、断裂线、洞口边界和大数据性能策略。

## 与其他模块的依赖关系

- 上游模块：CAD 图元读取适配层。
- 下游模块：纵断面、横断面、道路模型、土方计算。
- 实体联动行为：V1 记录来源对象并预留 `RebuildFromSourceObjects`，暂不自动监听源对象修改。

## 后续对接正式 EICAD 功能的注意事项

- 将 `DnTerrainTinEntity` 与统一设计实体 ID 和关系持久化机制绑定。
- 补充地形标准图块模板、测点点号规则和等高线采样。
- 将 WPF Bridge 替换或适配为正式 UI 时，不能移动 C++ 核心算法和实体能力。
