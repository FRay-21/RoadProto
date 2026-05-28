# 路面结构图例规划与普通图元绘制

## 能力分类

- 通用道路设计能力
- 通用 CAD 能力

## 能力说明

该能力把路面结构层模板转换为出图可用的图例规划，并在 ObjectARX 侧用普通 CAD 图元绘制。它沉淀了结构代号、路基土组、路基干湿类型、设计弯沉、累计当量轴次、结构层厚度、填充样式和底部图例项的组织方式，避免把这些规则散落在命令回调中。

## 当前实现

- 源码路径：
  - `src/domain/quantity/PavementStructureLegend.*`
  - `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.*`
- 对外类型/函数：
  - `PavementStructureLegendPlanner::build`
  - `PavementStructureLegendPlan`
  - `pavementStructureLegendCommandProcedure`
- 当前使用该能力的命令：`RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND`

## 可复用内容

- 从 `PavementLayerTemplateData` 生成图例列数据。
- 将米制厚度转换为厘米图示高度。
- 等厚和内外侧非等厚结构层的厚度文本格式化。
- 结构代号、路基土组、路基干湿类型、设计弯沉和累计当量轴次的列头规划。
- 第一列表头列宽规划，保证较长标题不越出表格线。
- 结构图示内部只表达填充样式和厚度，不绘制路面结构层类型文字。
- 底部填充样式图例按出现顺序逐项保留，不做合并。
- ObjectARX 侧使用 `AcDbLine`、`AcDbText`、`AcDbPolyline`、`AcDbHatch` 绘制普通实体。

## 不可复用或临时内容

- 当前表格行高、模板列宽、文字高度和图例项宽度为原型固定值。
- 当前没有图层服务、文字样式服务和图框比例适配。
- 当前绘制结果为一次性普通实体，不建立与模板或道路模型的自动联动。

## 依赖关系

- domain 依赖：`domain/cross_section/PavementLayerTemplateModel.h`
- cad_adapter 依赖：ObjectARX 选择集、模型空间遍历、普通实体创建、道路模型和横断面图自定义实体读取。
- 模块依赖：`DRAWING_QUANTITY` 出图、出表、算量模块。

## 扩展说明

- 后续可将固定排版参数提升为出图样式配置。
- 后续可接入统一图层、文字样式和比例服务。
- 后续若需要自动更新图例，应通过统一实体关系管理表达模板、道路模型和图例之间的依赖。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- 自动化验证：核心测试覆盖图例规划、模板列去重、表头列宽、厚度格式化、结构图示不写层类型文字、底部图例不合并、命令注册、Ribbon 入口和普通实体源码契约。
- AutoCAD 手工验证：加载 ARX 和托管 DLL 后，选择道路模型或横断面图，点取插入位置，检查表格行、结构图示、厚度、总厚度和底部图例项。
