# 边坡模板复用说明

## 能力分类

通用道路设计能力

## 能力说明

边坡模板能力把填方边坡、挖方边坡和护坡道抽象为可持久化、可编辑、可组合的模板参数。它可复用于横断面戴帽、三维边坡建模、标准断面库、出图和后续算量。

## 当前实现

- 源码路径：`src/domain/cross_section/SlopeTemplateModel.*`
- 源码路径：`src/application/cross_section/SlopeTemplateCreateService.*`
- 源码路径：`src/cad_adapter/objectarx/cross_section/DnSlopeTemplateEntity.*`
- 源码路径：`src/cad_adapter/objectarx/cross_section/SlopeTemplateDialogBridge.*`
- 源码路径：`src/ui/wpf/RoadProto.Terrain.UI/SlopeTemplateWindow.xaml`
- 当前使用该能力的命令：`RD_SECTION_SLOPE_TEMPLATE_CREATE`、`RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE`、`RD_SECTION_ROAD_MODEL_CREATE`

## 可复用内容

- 填方/挖方默认模板预设。
- 坡率、坡高、宽度三选二几何约束。
- 部件级搜索地面线范围坡高增值。
- `交地则结束放坡` 和 `重复最后一组参数直至交地` 控制条件。
- 边坡模板 DWG 持久化和线框显示。
- 横断面戴帽中模板组优先级和组内模板优先级解析。

## 不可复用或临时内容

- WPF 与 C++ 之间的临时请求/响应文件 Bridge 属于原型接入方式。
- 当前只生成边坡三维线框，不生成实体面、材质或算量。
- 当前模板来源是图中自定义实体 handle，后续正式化应增加模板库。

## 依赖关系

- domain 依赖：`domain/cross_section`
- cad_adapter 依赖：`DnSlopeTemplateEntity`、`DnRoadModelEntity`
- WPF 依赖：`SlopeTemplateWindow`、`RoadModelWindow`

## 扩展说明

- 后续可扩展为标准边坡模板库，并支持按道路等级、区域和填挖状态自动筛选。
- 后续生成实体面时，应继续复用当前领域层模板规则，只替换下游几何表达。
- RoadModel 生成期已在 TIN 剖切处加入临时网格候选过滤，并把生成时地面剖面作为断面快照保存；后续仍可在统计明确后继续优化索引参数或大图纸分块策略。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- 构建路径：`src/app/RoadProtoArx.vcxproj`
- 手工验证：创建边坡模板后，在横断面戴帽 `边坡模板` tab 中为左侧或右侧模板组点选该模板，生成道路模型并检查 `DnRoadModelEntity` 是否包含边坡三维线框。
