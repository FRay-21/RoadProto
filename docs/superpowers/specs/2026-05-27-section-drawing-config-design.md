# 横断面图配置设计

## 背景

用户需要在横断面模块新增 `横断面图配置` 功能。该功能不同于已有的路面结构层模板创建，也不同于横断面戴帽道路模型配置：它面向 `查看横断面` 已绘制到模型空间的横断面图成果。用户选择一张横断面图后打开配置窗口，按桩号范围、路基部件类型和路面结构层模板批量给同一道路模型下所有已绘制横断面图生成或更新结构层面域。

横断面图中的图框仍是 `DnRoadModelSectionDrawingEntity` 自定义实体，但图框内的路面结构层必须允许用户二次手动修改。用户可以拖动结构层四边形或梯形的顶点来修正局部形状和面积，后续路面工程量统计表必须以这些修改后的断面几何为准。

## 目标

- 新增用户命令 `RD_SECTION_DRAWING_CONFIG`，显示名为 `横断面图配置`。
- 命令选择一张 `DnRoadModelSectionDrawingEntity` 横断面图，打开 WPF 配置窗口。
- 配置窗口支持导入、导出 CSV 配置文件，并显示配置文件路径。
- 当前窗口包含一个 tab：`路面结构层`。
- tab 内表格包含起点桩号、终点桩号、路基类型和模板四列。
- 路基类型为多选下拉，来源于当前横断面图对应道路模型在当前 DWG 中已绘制过的所有横断面部件类型，按侧别和部件类型去重。
- 模板列通过 CAD 点选 `DnPavementLayerTemplateEntity` 回填模板 handle 和模板名称。
- 点击 `绘制` 后，把当前配置同步到同一道路模型的所有已绘制横断面图，并按配置生成或更新结构层面域。
- 点击 `取消` 后不写回、不绘制。
- 双击横断面图框实体时打开同一个配置窗口进行二次编辑。
- 表格行越靠上优先级越高，桩号范围命中规则参考横断面道路模型戴帽逻辑。
- 横断面图中的结构层面域提供顶点夹点，允许用户手动拖动顶点修改面积。
- 路面工程量统计表优先使用横断面图实体当前保存的结构层面域，不再优先回到道路模型原始断面数据。

## 非目标

- 不新增新的路面结构层模板编辑器；模板本体仍由 `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE` 和既有编辑流程维护。
- 不把横断面图配置写回 `DnRoadModelEntity` 作为道路模型戴帽配置。
- 不让 WPF 直接读取或修改 CAD 实体；点选、批量查找、实体读写仍由 ObjectARX adapter 完成。
- 第一版不提供“覆盖手动修改”选项。若结构层面域已被用户拖动修改，批量绘制默认保留该手改面域。
- CSV 只保存配置参数，不保存用户拖动后的结构层几何。手动几何属于 DWG 横断面图实体快照。

## 推荐方案

采用“横断面图实体快照为主”的方案。`DnRoadModelSectionDrawingEntity` 从只保存落图线段和结构层面域，升级为保存：

- 横断面图配置数据。
- 当前结构层面域。
- 结构层面域来源信息。
- 结构层面域是否已手动修改的标记。

道路模型仍作为横断面图来源和构造物范围来源；横断面图配置属于成果图阶段，不反向改变道路模型。这样可以保证用户在图面修正结构层后，算量直接按成果图当前几何计算。

## 数据模型

新增 `SectionDrawingConfigData`，建议放在 `src/domain/cross_section`：

- `configPath`：最近一次导入或导出的 CSV 路径。
- `pavementRows`：路面结构层配置行。
- `version`：配置数据版本。

新增 `SectionPavementLayerConfigRow`：

- `startStation`
- `endStation`
- `componentTypes`：多选路基类型，保存稳定编码，如 `Left:TravelLane`、`Right:HardShoulder`。
- `templateHandle`
- `templateName`

扩展 `RoadModelSectionDrawingFace`：

- `faceId`：稳定面域 ID，用于夹点和后续识别。
- `sourceTemplateHandle`：生成该面域的路面结构层模板 handle。
- `sourceConfigRowIndex`：生成该面域的配置行索引。
- `manualEdited`：用户拖动顶点后为 true。

旧 DWG 读取时：

- 没有配置数据则配置为空。
- 旧结构层面域保留现有点、颜色、填充、部件名和结构层名。
- 缺少 `manualEdited` 时按 false 处理。
- 缺少 `faceId` 时按读取顺序生成稳定兜底 ID。

## CSV 格式

配置文件使用 UTF-8 with BOM CSV，方便 Excel 直接打开。第一行固定为表头：

```csv
起点桩号,终点桩号,路基类型,模板Handle,模板名称
```

规则：

- 起点桩号、终点桩号支持十进制米值，也支持项目已有桩号格式解析能力时的 `K0+000` 类格式。
- 路基类型为分号分隔的多选编码或显示名。导出优先写显示名，导入时兼容稳定编码。
- 模板 Handle 为空的行无效，不参与绘制。
- 模板名称用于显示和人工核对，绘制时以 handle 为准。

## 部件类型来源

命令选择一张横断面图后，通过其 `roadModelHandle` 读取对应 `DnRoadModelEntity`。系统从道路模型保存的断面节点、结构层节点和部件线中收集当前道路已经生成过的部件类型：

- 侧别保留为类型的一部分，如 `左侧行车道` 和 `右侧行车道` 是两个选项。
- 同一侧别和同一部件类型在多个路基模板中出现时只显示一个选项。
- 如果旧模型缺少部件名，则沿用路面工程量统计表已有的部件反推能力，从结构层线或路基部件跨度推断。

## 命令流程

1. 用户运行 `RD_SECTION_DRAWING_CONFIG` 或点击 Ribbon `横断面设计 / 横断面图配置`。
2. 命令选择一个 `DnRoadModelSectionDrawingEntity`。
3. ObjectARX 读取所选横断面图的 `roadModelHandle`。
4. ObjectARX 收集当前 DWG 中同一 `roadModelHandle` 的全部横断面图实体。
5. ObjectARX 读取对应 `DnRoadModelEntity`，生成路基类型候选列表。
6. ObjectARX 将所选横断面图当前配置、配置路径和候选列表写入 WPF 请求文件。
7. WPF 打开 `横断面图配置` 窗口。
8. 用户导入 CSV、编辑表格或点选模板。
9. 点选模板时 WPF 写出 `PickTemplate` 动作和当前行号，ObjectARX 回到 CAD 选择 `DnPavementLayerTemplateEntity`，再重新打开窗口并回填该行。
10. 用户点击 `绘制`。
11. WPF 写出响应文件，ObjectARX 读取并校验配置。
12. ObjectARX 将配置写入同一道路模型的所有横断面图实体。
13. ObjectARX 按每张横断面图桩号解析配置行，批量更新结构层面域。

## 绘制规则

- 配置行按表格顺序解析，上方优先级高。
- 每张横断面图按自身 `station` 命中第一条有效配置行。
- 起终点桩号为闭区间，允许起终点相等但该行只命中该桩号。
- 行内路基类型为多选；只给命中类型的部件绘制结构层。
- 模板 handle 必须指向 `DnPavementLayerTemplateEntity`，否则该行跳过并输出警告。
- 结构层几何复用 `PavementLayerTemplateRules::buildSection` 的四边形或梯形规则，保持与模板预览、道路模型结构层和查看横断面预览一致。
- 新生成面域保存结构层名、部件名、颜色、填充参数、模板 handle 和配置行索引。
- 已有 `manualEdited=true` 的面域不被批量绘制覆盖。
- 未命中任何配置的横断面图保留现有面域。
- 未命中配置但已有手改面域的横断面图仍参与后续算量。

## 图面夹点编辑

`DnRoadModelSectionDrawingEntity` 增加结构层面域顶点夹点：

- 第一个夹点为图框插入点，用于整体移动横断面图。
- 后续夹点按 face 顺序和点顺序对应结构层面域顶点。
- 拖动结构层顶点时只修改对应 face 的一个点。
- 拖动后该 face 标记 `manualEdited=true`。
- 第一版允许四点 polygon 自由变形，不强制保持梯形或四边形平行关系。
- 普通 `MOVE` 或插入点夹点移动仍只改变图框位置和坐标轴，不改变面域局部坐标。

## 双击编辑

托管 Ribbon 插件的双击钩子新增 `DNROADMODELSECTIONDRAWINGENTITY` 识别：

- 双击横断面图实体时发送内部命令 `RD_SECTION_DRAWING_CONFIG_EDIT_HANDLE {handle}`。
- 内部命令按 handle 打开同一个 WPF 配置窗口。
- 如果同一道路模型下多张横断面图已有不同配置，窗口以双击那张的配置为准。
- 用户点击 `绘制` 后，该配置同步到同一道路模型的所有横断面图。

## 工程量统计

`RD_DRAWING_PAVEMENT_QUANTITY_TABLE` 的取样顺序调整为：

1. 优先从所选 `DnRoadModelSectionDrawingEntity.faces` 当前几何取样。
2. 使用每个 face 当前 polygon 计算投影宽度和断面面积。
3. 使用 face 的 `componentName` 和 `layerName` 作为统计项。
4. 仅当横断面图没有可用结构层面域时，才回退到 `DnRoadModelEntity.sections` 原始结构层节点。
5. 道路模型仍用于读取构造物范围，并用于旧图纸缺少部件名时的反推兜底。

这样用户拖动结构层顶点后，工程量统计表的面积和体积都以修改后的断面尺寸为准。

## 文件与职责

预计新增或修改：

- `src/domain/cross_section/SectionDrawingConfigModel.*`：配置模型、CSV 行归一化、桩号命中和路基类型匹配。
- `src/cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.*`：WPF 请求/响应文件桥接。
- `src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.*`：用户命令、按 handle 编辑命令、回写命令、模板点选、批量横断面图收集。
- `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.*`：持久化配置、新 face 字段、夹点编辑。
- `src/ui/wpf/RoadProto.Terrain.UI/SectionDrawingConfigWindow.xaml*`：配置窗口。
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogDtos.cs`：WPF DTO。
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogFile.cs`：请求/响应文件读写。
- `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SectionDrawingConfigDialogCommands.cs`：托管 WPF 弹窗命令。
- `src/modules/cross_section/CrossSectionModule.cpp`：命令元数据。
- `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`：Ribbon 按钮和双击入口。
- `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.cpp`：算量优先读取横断面图当前 face。
- `docs/business/cross_section/横断面图配置.md`：新增业务文档。
- `docs/business/cross_section/查看横断面.md`：说明横断面图可作为配置与手改载体。
- `docs/business/drawing_quantity/路面工程量统计表.md`：说明按横断面图当前面域统计。
- `docs/modules/cross_section.md`、`docs/modules/module_index.md`、`tests/README.md`、`README.md`、`docs/dev/version_log.md`：同步命令和版本信息。

## 测试策略

核心测试：

- 配置行桩号闭区间命中。
- 表格行优先级。
- 多选路基类型匹配。
- CSV 导入导出 UTF-8 表头和字段往返。
- 手动修改后的 face polygon 面积和投影宽度计算。
- 工程量统计优先使用横断面图当前 face，而不是道路模型原始断面。

源码契约测试：

- `CROSS_SECTION` 模块包含 `RD_SECTION_DRAWING_CONFIG`、`RD_SECTION_DRAWING_CONFIG_EDIT_HANDLE` 和回写命令。
- 命令 businessDocPath 指向 `docs/business/cross_section/横断面图配置.md`。
- 托管 Ribbon 包含 `横断面图配置` 按钮。
- 双击钩子识别 `DNROADMODELSECTIONDRAWINGENTITY` 并发送编辑命令。
- `DnRoadModelSectionDrawingEntity` 包含配置持久化字段、face 来源字段和 `manualEdited`。
- `DnRoadModelSectionDrawingEntity` 实现结构层顶点夹点。

手工验证：

- 选择一张横断面图后打开配置窗口。
- 导入、导出 CSV 并用 Excel 打开检查列。
- 路基类型下拉只显示当前道路模型实际出现过的部件类型。
- 点选路面结构层模板后回填模板名称和 handle。
- 点击 `绘制` 后同一道路模型的所有横断面图更新。
- 双击横断面图能再次打开配置窗口。
- 拖动结构层顶点后图形刷新，保存 DWG 后重开仍保留修改。
- 生成路面工程量统计表，面积和体积按拖动后的结构层面域变化。

## 风险与约束

- 横断面图配置和道路模型配置必须保持分离，避免用户误以为修改成果图会改变道路模型。
- 保留手改面域会让“重新绘制”看起来没有覆盖所有结构层，需要在命令行或窗口状态里提示已跳过手改面域。
- CSV 中模板 handle 跨 DWG 可能失效，导入后应提示无效行，而不是静默绘制错误结构层。
- 第一版自由拖动 polygon 顶点会允许非梯形形状，算量按实际 polygon 面积计算。
