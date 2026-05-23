# 路面结构层模板复用说明

## 能力分类

通用道路设计能力

## 能力说明

路面结构层模板能力用于独立表达横断面部件上的铺装结构层参数，包括层类型、RGB 显示颜色、厚度、内外侧加宽和坡度。该能力与路基模板部件通过 handle 关联，后续可复用于横断面建模、三维道路结构层、出图、材料统计和算量。

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
- 每层 RGB 显示色：默认按层号给出蓝、绿、黄、橙、紫、灰初始色，用户可独立修改；WPF 预览、DWG 模板实体、道路模型结构层填充面/边线和查看横断面预览都使用层数据中的 RGB。
- 每层填充类型、角度和比例：`PavementLayerTemplateLayer::hatchPattern` 保存 CAD 常用填充名，`hatchAngle` 保存填充角度，`hatchScale` 保存填充比例；`PavementLayerTemplateProperties::displayMode` 支持按颜色、按填充、按填充+颜色显示。该显示策略用于 WPF 预览和 `DnPavementLayerTemplateEntity`，道路模型结构层保持颜色显示。
- 通用设计数据：`PavementLayerTemplateProperties::showAllGeneralParameters` 控制 WPF 是否展开全部通用参数；`structureCode`、`subgradeMoistureTypes`、`pavementType`、`subgradeSoilGroups`、`designDeflection` 和 `cumulativeAxleLoads` 仅作为模板数据、Bridge 字段和 `.rpavement.xml` 属性保留，暂不参与结构层几何、预览标注、DWG 模板实体标题或道路模型结构层显示。
- 当前层编辑交互：WPF 只展示当前选中结构层参数，预览图点击、当前层输入框和上/下按钮都可改变当前编辑层，减少多层模板参数滚动量。
- 等厚/非等厚厚度模型：勾选“内外厚度是否一致”时使用单一厚度，未勾选时使用内侧厚度和外侧厚度。
- 加宽和坡度编辑模型：WPF 默认勾选“内外加宽是否一致”和“内外坡度是否一致”，需要差异化时再分别填写内侧和外侧。
- 内外侧语义：内侧 = closer to road centerline for the subgrade component；外侧 = farther from road centerline。内侧加宽向道路中线方向扩展。
- 结构层横断面预览几何构建：每一层只保留顶边、底边、内侧边和外侧边四条边。除第一层外，当前层顶边以上一层底边所在直线为基准；加宽沿该直线平行/共线延长或收回，正加宽让当前层顶边更宽，负加宽让顶边缩短。任何情况下都不生成“接触边 + 外轮廓”的六点台阶形。
- 坡度输入支持 `1:n` 或数字 `n`，`n` 表示每 1 个竖向厚度对应的水平位移量，侧边水平位移量 = `厚度 * n`。正坡度表示从顶边向下到底边时侧边向外放，负坡度表示向内收。`1:1` 时侧边与底边约为 45°，`1:2` 时约为 30°，`1:0.5` 时约为 60°；坡度允许为负，`1:-0.5` 时侧边向内收且与底边约为 120°。加宽只改变顶边端点，不参与坡度计算，不改变坡率。
- 结构层默认显示配色由 `PavementLayerTemplateRules::displayColorForLayerIndex` 统一提供；归一化后颜色存入 `PavementLayerTemplateLayer::color`，后续绘制和模型生成只读取层保存色。
- WPF 预览可按颜色、填充、填充+颜色三种方式显示，并使用固定字号的层名+厚度单行标注、CAD 式加宽尺寸线和侧边中心 `1:n` 坡度标注；填充预览读取每层 `hatchAngle` 和 `hatchScale`。`DnPavementLayerTemplateEntity` 使用同一轮廓和显示方式，但不显示尺寸标注，只在模板上方按文字总长度居中绘制模板名称。`DnRoadModelEntity` 不跟随模板显示方式切换，仍按层 RGB 颜色绘制结构层弱化填充面和边线。
- WPF 预览图提供居中初始视图、鼠标位置基点滚轮缩放和中键平移。
- 独立 DWG 模板实体 `DnPavementLayerTemplateEntity`，支持图面显示、DWG 持久化、几何范围和变换。
- `.rpavement.xml` 导入导出格式，可在 WPF 侧保存和读取模板参数，用于跨图纸流转。
- 路基模板部件通过 handle 绑定路面结构层模板，道路模型生成时读取绑定模板并生成结构层三维边界线，`DnRoadModelEntity` 再按连续边界线绘制结构层弱化填充面。

## 不可复用或临时内容

- WPF 与 C++ 之间的请求/响应文件 Bridge 属于原型接入方式。
- `.rpavement.xml` 当前保存几何参数、每层 RGB 显示色、填充类型、填充角度、填充比例，以及结构代号、路基干湿类型、路面类型、路基土组、设计弯沉和累计轴次等通用设计数据；这些通用设计数据先作为数据保留，不保存正式材料库、造价、压实度或规范编号。
- 路基模板部件当前保存模板 handle 和名称；模板变更后不会自动通知已生成道路模型。
- 当前道路模型结构层弱化填充面属于 `DnRoadModelEntity` 的显示表达，不是可单独选择、编辑、赋材质或算量的实体体积。

## 依赖关系

- domain 依赖：`domain/cross_section`
- cad_adapter 依赖：`DnPavementLayerTemplateEntity`、`DnSubgradeTemplateEntity`、`DnRoadModelEntity`
- 模块依赖：`CROSS_SECTION`

## 扩展说明

- 后续应把路面结构层模板接入统一实体关系管理，使模板修改后能标记引用它的路基模板和道路模型需要重建。
- 后续可引入标准结构层库、材料库和按道路等级自动选择模板能力。
- 后续可将道路模型中的结构层显示面扩展为正式三维实体、体积计算和出图表达。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- 测试路径：`tests/RoadProtoManagedBridgeTests/`
- AutoCAD 手工验证：运行 `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE` 创建模板，编辑上面层/中面层/下面层/基层/底基层/垫层和等厚/非等厚参数，导出再导入 `.rpavement.xml`；在路基模板中点选该模板绑定到任意部件，运行 `RD_SECTION_ROAD_MODEL_CREATE` 后确认道路模型结构层与模板预览保持同一四边形/梯形弱化填充和 RGB 边线，`查看横断面` 也显示结构层。
