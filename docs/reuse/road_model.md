# 道路模型复用说明

## 能力分类

通用道路设计能力

## 能力说明

道路模型能力把道路中线、纵断面竖曲线设计高程和横断面路基模板组合为三维部件线。它可复用于后续三维建模、出图、算量、检查和道路对象联动。

## 当前实现

- 源码路径：`src/domain/cross_section/RoadModel.*`
- 源码路径：`src/application/cross_section/RoadModelBuildService.*`
- 源码路径：`src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.*`
- 对外类型/函数：`RoadModelBuilder`、`RoadModelStationSampler`、`RoadModelTemplateResolver`、`RoadModelBuildService`、`RoadModelDialogBridge`
- 当前使用该能力的命令：`RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_EDIT`

## 可复用内容

- 模板范围优先级解析。
- 道路模型采样点收集。
- 竖曲线高程与道路中线平面点组合。
- 路基模板部件边界转三维部件线。
- `RoadModelData` 的 CAD 实体持久化和显示表达。
- WPF 表格行与 CAD 模板实体点选之间的桥接动作。

## 不可复用或临时内容

- WPF 与 C++ 之间的临时请求/响应文件 Bridge 属于原型接入方式。
- 模板 handle 当前可手工输入或从 CAD 图中点选实体回填，后续仍应升级为正式模板库。
- 当前只生成三维线框，不生成道路实体面、材质、结构层体积或算量结果。

## 依赖关系

- domain 依赖：`domain/alignment`、`domain/profile`、`domain/cross_section`
- cad_adapter 依赖：`DnRoadCenterlineEntity`、`DnProfileGradeGraphEntity`、`DnProfileVerticalCurveEntity`、`DnSubgradeTemplateEntity`、`DnRoadModelEntity`
- 模块依赖：`ALIGNMENT`、`PROFILE`、`CROSS_SECTION`

## 扩展说明

- 后续应接入统一实体关系管理，保存道路模型对中线、竖曲线和模板实体的依赖。
- 可扩展为道路面、边坡面、结构层、排水构造和算量对象。
- 可增加模板库、模板组和按道路等级自动选择模板能力。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- 测试路径：`tests/RoadProtoManagedBridgeTests/`
- AutoCAD 手工验证：创建道路中线、竖曲线和路基模板后运行 `RD_SECTION_ROAD_MODEL_CREATE`，在表格行内点选图中模板并确认生成 `DnRoadModelEntity`；双击道路模型或运行 `RD_SECTION_ROAD_MODEL_EDIT` 后可调整模板范围并刷新实体。
