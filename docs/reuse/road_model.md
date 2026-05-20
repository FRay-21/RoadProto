# 道路模型复用说明

## 能力分类

通用道路设计能力

## 能力说明

道路模型能力把道路中线、纵断面竖曲线设计高程、横断面路基模板和边坡模板组合为三维道路模型线框。它可复用于后续三维建模、出图、算量、检查和道路对象联动。

## 当前实现

- 源码路径：`src/domain/cross_section/RoadModel.*`
- 源码路径：`src/application/cross_section/RoadModelBuildService.*`
- 源码路径：`src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.*`
- 源码路径：`src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.*`
- 对外类型/函数：`RoadModelBuilder`、`RoadModelStationSampler`、`RoadModelTemplateResolver`、`RoadModelSlopeTemplateGroupResolver`、`RoadModelSectionPreviewBuilder`、`RoadModelBuildService`、`RoadModelDialogBridge`、`RoadModelSectionViewerBridge`
- 当前使用该能力的命令：`RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_EDIT`、`RD_SECTION_ROAD_MODEL_VIEW_SECTION`

## 可复用内容

- 模板范围优先级解析。
- 道路模型采样点收集。
- 竖曲线高程与道路中线平面点组合。
- 路基模板部件边界转三维部件线。
- 边坡模板组优先级解析和组内多模板搜索。
- 以道路中线左右搜索宽度剖切 TIN 地面线，并通过 TIN 网格候选索引减少每个断面需要遍历的三角形数量。
- 从路基模板最外侧按边坡模板生成三维边坡线。
- 按采样桩号构建断面节点链和左右地面剖面快照，并由节点链生成横向肋线、纵向连接线、最外侧边界线、端部封闭线和节点数量不一致时的过渡线。
- 构建进度回调，供 AutoCAD 状态栏或后续替换 UI 展示生成进度。
- `RoadModelData` 的 CAD 实体持久化和显示表达，其中包含生成时采样桩号、断面节点链、地面剖面快照和三维网格线框。
- 已生成道路模型的横断面预览构建，按桩号输出路基模板线、边坡模板线和生成时地面线快照；旧模型没有快照时可回退读取 TIN。
- WPF 表格行与 CAD 模板实体点选之间的桥接动作。

## 不可复用或临时内容

- WPF 与 C++ 之间的临时请求/响应文件 Bridge 属于原型接入方式。
- 模板 handle 当前可手工输入或从 CAD 图中点选实体回填，后续仍应升级为正式模板库。
- TIN 剖切索引当前按构建时地形面生成，不作为长期缓存持有；生成后的地面剖面快照随道路模型持久化，避免查看横断面时重复探测地形。
- 当前只生成三维线框，不生成道路实体面、材质、结构层体积或算量结果；路基/道路主体颜色沿用部件颜色，边坡线颜色来自边坡模板部件颜色。

## 依赖关系

- domain 依赖：`domain/alignment`、`domain/profile`、`domain/cross_section`
- cad_adapter 依赖：`DnRoadCenterlineEntity`、`DnProfileGradeGraphEntity`、`DnProfileVerticalCurveEntity`、`DnSubgradeTemplateEntity`、`DnSlopeTemplateEntity`、`DnTerrainTinEntity`、`DnRoadModelEntity`
- 模块依赖：`ALIGNMENT`、`PROFILE`、`CROSS_SECTION`

## 扩展说明

- `RoadModelBuilder` 在生成期为 TIN 建立临时网格索引，剖切时只遍历当前搜索线段覆盖网格内的候选三角形；索引不写入实体，不形成长期内存缓存。
- `RoadModelSection` 保存生成时左右地面剖面快照，边坡放坡、道路模型持久化和查看横断面使用同一份剖面数据。
- `RoadModelSectionPreviewBuilder` 优先按桩号把断面节点和地面快照转换为二维偏距-高程线段；没有快照的旧实体才读取 TIN 临时剖切。
- 后续应接入统一实体关系管理，保存道路模型对中线、竖曲线和模板实体的依赖。
- 可扩展为道路面、边坡面、结构层、排水构造和算量对象。
- 可增加模板库、模板组和按道路等级自动选择模板能力。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- 测试路径：`tests/RoadProtoManagedBridgeTests/`
- AutoCAD 手工验证：创建道路中线、竖曲线和路基模板后运行 `RD_SECTION_ROAD_MODEL_CREATE`，在表格行内点选图中模板、在边坡模板组内管理多个模板，并确认生成时状态栏显示进度且最终创建 `DnRoadModelEntity`；双击道路模型或运行 `RD_SECTION_ROAD_MODEL_EDIT` 后可调整模板范围并刷新实体；运行 `RD_SECTION_ROAD_MODEL_VIEW_SECTION` 后选择道路模型，应能按采样桩号切换并预览路基模板线、边坡模板线和生成时地面线快照。
