# 路面结构层模板复用说明

## 能力分类

通用道路设计能力

## 能力说明

路面结构层模板能力用于独立表达横断面部件上的铺装结构层参数，包括层类型、厚度、内外侧加宽和坡度。该能力与路基模板部件通过 handle 关联，后续可复用于横断面建模、三维道路结构层、出图、材料统计和算量。

## 当前实现

- 源码路径：`src/domain/cross_section/PavementLayerTemplateModel.*`
- 源码路径：`src/application/cross_section/PavementLayerTemplateCreateService.*`
- 源码路径：`src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.*`
- 源码路径：`src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.*`
- 源码路径：`src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml`
- 对外类型/函数：`PavementLayerTemplateData`、`PavementLayerTemplateLayer`、`PavementLayerTemplateRules`、`PavementLayerTemplateDefaults`、`PavementLayerTemplateCreateService`、`DnPavementLayerTemplateEntity`
- 当前使用该能力的命令：`RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE`、`RD_SECTION_PAVEMENT_LAYER_TEMPLATE_EDIT_HANDLE`、`RD_SECTION_PAVEMENT_LAYER_TEMPLATE_APPLY_DIALOG_FILE`、`RD_SECTION_SUBGRADE_TEMPLATE_APPLY_DIALOG_FILE`、`RD_SECTION_ROAD_MODEL_CREATE`

## 可复用内容

- 结构层类型枚举和显示名称：上面层、中面层、下面层、基层、底基层、垫层。
- 等厚/非等厚厚度模型：勾选“内外厚度是否一致”时使用单一厚度，未勾选时使用内侧厚度和外侧厚度。
- 内外侧语义：内侧 = closer to road centerline for the subgrade component；外侧 = farther from road centerline。内侧加宽向道路中线方向扩展。
- 结构层横断面预览几何构建，可根据路基部件侧别生成左/右侧一致语义的矩形结构层线。
- 独立 DWG 模板实体 `DnPavementLayerTemplateEntity`，支持图面显示、DWG 持久化、几何范围和变换。
- `.rpavement.xml` 导入导出格式，可在 WPF 侧保存和读取模板参数，用于跨图纸流转。
- 路基模板部件通过 handle 绑定路面结构层模板，道路模型生成时读取绑定模板并生成结构层三维线框。

## 不可复用或临时内容

- WPF 与 C++ 之间的请求/响应文件 Bridge 属于原型接入方式。
- `.rpavement.xml` 当前只保存几何和显示参数，不保存正式材料库、造价、压实度或规范编号。
- 路基模板部件当前保存模板 handle 和名称；模板变更后不会自动通知已生成道路模型。
- 当前道路模型生成结构层三维线框，不生成结构层实体面、体积或算量。

## 依赖关系

- domain 依赖：`domain/cross_section`
- cad_adapter 依赖：`DnPavementLayerTemplateEntity`、`DnSubgradeTemplateEntity`、`DnRoadModelEntity`
- 模块依赖：`CROSS_SECTION`

## 扩展说明

- 后续应把路面结构层模板接入统一实体关系管理，使模板修改后能标记引用它的路基模板和道路模型需要重建。
- 后续可引入标准结构层库、材料库和按道路等级自动选择模板能力。
- 后续可将道路模型中的结构层线框扩展为三维实体面、体积计算和出图表达。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- 测试路径：`tests/RoadProtoManagedBridgeTests/`
- AutoCAD 手工验证：运行 `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE` 创建模板，编辑上面层/中面层/下面层/基层/底基层/垫层和等厚/非等厚参数，导出再导入 `.rpavement.xml`；在路基模板中点选该模板绑定到任意部件，运行 `RD_SECTION_ROAD_MODEL_CREATE` 后确认道路模型和 `查看横断面` 均显示结构层。
