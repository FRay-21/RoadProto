# 版本记录

## 未发布 - 2026-05-29

- 阶段：路面结构层创建向导内外侧参数与部件增删交互调整。
- 修改内容：
  - 新增 `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND` 路面结构图例命令，可选择道路模型或一个横断面图；选择横断面图时按道路模型 handle 搜索当前模型空间同路所有横断面图。
  - 路面结构图例从路面结构层模板读取路基土组、路基干湿类型、设计弯沉、累计当量轴次、结构代号、结构层厚度、层名、颜色和填充参数。
  - 图例按模板绘制等宽列，结构图示宽度固定 `20cm`，厚度按厘米 `1:1` 表达；底部填充样式图例逐项绘制，不按填充类型或层名合并。
  - 路面结构图例的结构图示内部不再绘制路面结构层类型文字，第一列表头列宽加宽，避免 `路基干湿类型`、`累计当量轴次`、`路面总厚度(cm)` 等标题越出表格线。
  - 图例绘制只使用 `AcDbLine`、`AcDbText`、`AcDbPolyline`、`AcDbHatch` 等普通 CAD 图元，不新增自定义实体。
  - 横断面图配置绘制结构层时，配置行优先级改为按桩号、侧别和路基部件类型逐项解析；同范围不同行车道和硬路肩不冲突，可同时绘制，同一路基部件重叠时仍按上方行优先。
  - 路面结构层创建向导的厚度、加宽和坡度改为内外侧字段，同一结构层尽量保持在一行内编辑。
  - 沥青路面向导移除主线路缘带适应路段类型。
  - 沥青路面主线行车道预设中，基层和底基层改为内侧加宽 `0.1`、内侧坡度 `1`、外侧加宽 `0`、外侧坡度 `0`。
  - 沥青路面主线硬路肩预设中，基层和底基层改为外侧加宽 `0.1`、外侧坡度 `1`、内侧加宽 `0`、内侧坡度 `0`，该值直接来自创建向导预设源头。
  - 路面结构层模板参数窗口新增“新增部件”和“删除部件”按钮；新增时选择在当前部件上方或下方插入，删除时确认删除选中部件。
  - 所有路面结构层类型的预览宽度初始默认值统一为 `3`，同步调整 WPF、Bridge DTO、领域默认值、创建服务默认值和 DWG 实体显示兜底值。
  - WPF 预览中的层名和厚度改为白色竖向引线式标注：一根引线从结构层顶边下引，每层一行显示名称和厚度，每行配白色下划线；文字、引线、下划线、加宽箭头和标注偏移均使用固定模型尺寸并随预览缩放，加宽文字缩小并放在尺寸箭头中部上方，坡度文字缩小并放在坡边中部侧边。
  - 缩短 WPF 预览中的加宽尺寸线和层名厚度下划线，并减小加宽尺寸箭头，保持宽度文字贴近尺寸箭头中部上方。
  - 横断面图配置窗口在 `路面结构层` 旁新增 `清表` tab，按起点桩号、终点桩号、左侧坡率、右侧坡率、厚度、作用范围和挖方是否清表维护清表范围。
  - `清表` 配置会在横断面地面线以下生成清表面域，作用范围支持左侧、右侧和两侧；挖方是否清表为否时，挖方侧不绘制清表层，厚度按配置行输入值生成。
  - 横断面图实体持久化版本升级到 `6`，保存清表配置和厚度；路面工程量统计读取横断面图面域时排除清表面域。
  - 新增 `ClearTableQuantityDrawingFaceSampler` 领域接口，预留清表面域独立算量入口并承接厚度字段；本轮不计算清表工程量，也不并入路面工程量统计表。
- 验证状态：已通过核心测试 Debug 构建与运行、WPF 托管插件 Debug 构建、`RoadProto.sln` Debug 全量构建和 `git diff --check`；本轮已重新生成 Debug ARX 与托管 DLL，Release 构建和 AutoCAD 图形界面仍待后续验证。

## v0.1.31 - 2026-05-27

- 版本标识：`v0.1.31_20260527_SectionDrawingConfig`。
- ARX 文件：`RoadProto_v0.1.31_20260527_SectionDrawingConfig.arx`。
- 阶段：横断面图配置与图上结构层手动编辑。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、托管 bridge 测试 Debug/Release、WPF Debug/Release 构建和 `RoadProto.sln` Debug/Release 全量构建已验证。

### 修改内容

- 新增 `RD_SECTION_DRAWING_CONFIG` 横断面图配置命令，选择 `查看横断面 / 绘制横断面` 生成的 `DnRoadModelSectionDrawingEntity` 后打开 WPF 配置窗口。
- 新增 `SectionDrawingConfigModel` 领域模型，支持 CSV 导入导出、起终点桩号范围、路基类型多选、表格行优先级解析和模板 handle 归一化。
- 横断面图配置窗口上方支持 CSV 路径、导入和导出；当前包含 `路面结构层` tab，表格字段为起点桩号、终点桩号、路基类型和模板。
- 路基类型从同一道路模型在当前 DWG 中已经绘制出的横断面图提取，按左/右侧和部件类型去重后作为多选项。
- 点击 `绘制` 后，按配置把路面结构层面域应用到同一道路模型下的所有横断面图；表格上方行优先级高。
- `DnRoadModelSectionDrawingEntity` 新增横断面图配置持久化、结构层面域来源字段、`faceId`、来源模板 handle、来源配置行号和 `manualEdited` 标记。
- 横断面图结构层面域新增顶点夹点；用户拖动四边形或梯形顶点后标记 `manualEdited=true`，后续重新绘制时保留手动修改。
- `RD_DRAWING_PAVEMENT_QUANTITY_TABLE` 优先通过 `PavementQuantityDrawingFaceSampler` 从横断面图实体当前面域采样，工程量统计以图上手动修改后的尺寸为准；缺少可用面域时再回退道路模型断面数据。
- 更新横断面图配置业务文档、查看横断面文档、路面工程量统计表文档、模块说明、复用说明、README 和测试说明。

### 验证状态

- 自动化验证：核心测试 Debug/Release 构建与运行通过；托管 bridge 测试 Debug/Release 通过；WPF Debug/Release 构建通过；`RoadProto.sln` Debug/Release 全量构建通过，生成 `RoadProto_v0.1.31_20260527_SectionDrawingConfig.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：待在 AutoCAD 2021 中加载 ARX 和托管 DLL 后验证 Ribbon 入口、横断面图选择、CSV 导入导出、模板点选、图上结构层绘制、顶点夹点编辑、双击横断面图二次编辑和工程量统计结果。

## v0.1.30 - 2026-05-27

- 版本标识：`v0.1.30_20260527_PavementQuantityComponentFormatFix`。
- ARX 文件：`RoadProto_v0.1.30_20260527_PavementQuantityComponentFormatFix.arx`。
- 阶段：路面工程量统计表部件名反推与表格格式修正。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、WPF Debug/Release 构建和 `RoadProto.sln` Debug/Release 全量构建已验证。

### 修改内容

- 路面工程量统计表从道路模型读取断面数据时，若旧模型结构层节点缺少部件名，不再直接输出 `未分部件`，而是优先从结构层纵向线 `componentIndex` 和路基部件线的部件类型反推 `行车道`、`硬路肩` 等部件名称。
- 反推失败时再退回路基部件边界范围匹配；仍无法识别时才输出 `未分部件`。
- `.xls` 写出新增统一表格样式：所有内容水平/垂直居中、自动换行；中文文字使用宋体 10 号；桩号、英文和数字使用 Times New Roman 10 号；面积和体积动态列加宽。
- 新增领域层 `RoadModelPavementQuantitySampler`，把道路模型断面采样和旧数据部件名反推从 ObjectARX 命令中沉淀为可测试能力。

### 验证状态

- 自动化验证：核心测试 Debug/Release 构建与运行通过；WPF Debug/Release 构建通过；`RoadProto.sln` Debug/Release 全量构建通过，生成 `RoadProto_v0.1.30_20260527_PavementQuantityComponentFormatFix.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：待在 AutoCAD 2021 中加载 ARX 和托管 DLL 后验证旧道路模型导出的部件拆列、保存对话框统计方式和 `.xls` 打开后的格式效果。

## v0.1.29 - 2026-05-27

- 版本标识：`v0.1.29_20260527_PavementQuantityComponentMode`。
- ARX 文件：`RoadProto_v0.1.29_20260527_PavementQuantityComponentMode.arx`。
- 阶段：路面工程量统计表部件聚合模式。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、WPF Debug/Release 构建和 `RoadProto.sln` Debug/Release 全量构建已验证。

### 修改内容

- 路面工程量统计表新增统计方式：`按部件和结构层`、`按结构层类型`。
- `按部件和结构层` 输出 `部件名称-结构层名称面积` 和 `部件名称-结构层名称体积` 动态列，例如 `行车道-上面层面积`。
- `按结构层类型` 保持上一版按结构层名称合并的输出方式。
- 道路模型断面节点、查看横断面预览和横断面落图面域新增部件名称传递与持久化；旧实体缺少部件名称时按 `未分部件` 处理。
- 输出路径选择优先使用带统计方式选项的 Windows 保存文件对话框；不可用时退回默认按部件和结构层统计。
- 更新业务文档、模块说明、复用说明、README 和测试说明。

### 验证状态

- 自动化验证：核心测试 Debug/Release 构建与运行通过；WPF Debug/Release 构建通过；`RoadProto.sln` Debug/Release 全量构建通过，生成 `RoadProto_v0.1.29_20260527_PavementQuantityComponentMode.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：待在 AutoCAD 2021 中加载 ARX 和托管 DLL 后验证保存对话框统计方式、按部件拆列、按类型合并和 `.xls` 打开效果。

## v0.1.28 - 2026-05-27

- 版本标识：`v0.1.28_20260527_PavementQuantityTable`。
- ARX 文件：`RoadProto_v0.1.28_20260527_PavementQuantityTable.arx`。
- 阶段：出图出表路面工程量统计表。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、WPF Debug/Release 构建和 `RoadProto.sln` Debug/Release 全量构建已验证。

### 修改内容

- 新增 `DRAWING_QUANTITY` 出图、出表、算量模块，并通过 `ModuleRegistry` 注册。
- Ribbon 新增 `出图出表` 面板和 `路面工程量统计表` 按钮，命令为 `RD_DRAWING_PAVEMENT_QUANTITY_TABLE`。
- 新增路面工程量统计领域能力：按道路模型构造物范围切分普通段、桥梁段和隧道段。
- 面积按结构层平面投影宽度沿桩号区间累计；体积按相邻断面结构层截面积平均值乘以桩号间距的平均断面法累计。
- 命令选择 `查看横断面` 绘制的 `DnRoadModelSectionDrawingEntity`，读取对应道路模型结构层断面数据，并提示输出 `.xls` 路径。
- `DnRoadModelSectionDrawingEntity` 结构层面域新增结构层名称字段，旧版横断面落图读取时名称为空并按通用结构层处理。
- 更新业务文档、模块说明、复用说明、README 和测试说明。

### 验证状态

- 自动化验证：核心测试 Debug/Release 构建与运行通过；WPF Debug/Release 构建通过；`RoadProto.sln` Debug/Release 全量构建通过，生成 `RoadProto_v0.1.28_20260527_PavementQuantityTable.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：本轮未在 AutoCAD 2021 图形界面完整点验；建议加载 Debug 或 Release 产物后验证 `出图出表` 面板、`路面工程量统计表` 命令、横断面图选择、输出路径提示、`.xls` 打开和构造物切段结果。

## v0.1.27 - 2026-05-27

- 版本标识：`v0.1.27_20260527_RoadModelStructures`。
- ARX 文件：`RoadProto_v0.1.27_20260527_RoadModelStructures.arx`。
- 阶段：横断面戴帽构造物范围与边坡跳过。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、托管 bridge 测试、WPF Debug/Release 构建和 `RoadProto.sln` Debug/Release 全量构建已验证。

### 修改内容

- 横断面戴帽新增构造物 tab，可维护起点桩号、终点桩号、构造物类型和影响范围。
- 构造物类型支持桥梁、隧道；影响范围支持左侧、右侧、两侧。
- 新增 `RoadModelStructureRange` 配置，并通过 WPF DTO、临时请求/响应文件、C++ `RoadModelDialogBridge` 和 `DnRoadModelEntity` DWG 持久化同步保存。
- 生成道路模型时，桥梁或隧道命中的桩号范围内，按左侧、右侧或两侧跳过对应边坡放坡，模型中不生成该侧边坡线。
- `RoadModelStationSampler` 将构造物起终点纳入采样桩号，确保构造物边界处能断开或恢复边坡。
- `DnRoadModelEntity` 数据版本升至 7；旧版本道路模型读取时构造物列表为空，新版本保存构造物范围配置。
- 更新横断面戴帽业务文档、道路模型复用说明、模块说明、README 和测试说明。

### 验证状态

- 自动化验证：托管 bridge 测试通过；核心测试 Debug/Release 构建与运行通过；WPF Debug 构建通过；`RoadProto.sln` Debug/Release 全量构建通过，生成 `RoadProto_v0.1.27_20260527_RoadModelStructures.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：本轮未在 AutoCAD 2021 图形界面完整点验；建议加载 Debug 或 Release 产物后验证 `构造物` tab 的桥梁/隧道、左侧/右侧/两侧范围、编辑回显、DWG 保存重开和边坡缺失效果。

## v0.1.26 - 2026-05-25

- 版本标识：`v0.1.26_20260525_CrossSectionFrameLabelWhite`。
- ARX 文件：`RoadProto_v0.1.26_20260525_CrossSectionFrameLabelWhite.arx`。
- 阶段：查看横断面落图外框与桩号颜色调整。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、托管 bridge 测试和 `RoadProto.sln` Debug/Release 构建已验证。

### 修改内容

- `DnRoadModelSectionDrawingEntity` 绘制横断面外框和下方桩号文字时统一使用 AutoCAD ACI 7 白色。
- 路面结构层面域、填充线和断面内部线段继续使用原有路面结构层模板颜色与填充参数，不随外框/桩号颜色变化。
- 更新查看横断面业务文档、复用说明、README、测试说明和核心 source-contract，确保新版本产物可追踪。

### 验证状态

- 自动化验证：托管 bridge 测试通过；核心测试 Debug/Release 构建与运行通过；`RoadProto.sln` Debug/Release 构建通过，生成 `RoadProto_v0.1.26_20260525_CrossSectionFrameLabelWhite.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：本轮未在 AutoCAD 2021 图形界面完整点验；建议加载 Release 产物后验证 `绘制横断面` 落图的外框和桩号文字显示为白色。

## v0.1.25 - 2026-05-25

- 版本标识：`v0.1.25_20260525_CrossSectionViewDrawing`。
- ARX 文件：`RoadProto_v0.1.25_20260525_CrossSectionViewDrawing.arx`。
- 阶段：查看横断面预览交互与模型空间批量落图。
- 是否可作为稳定测试版本：是。核心测试 Debug、托管 bridge 测试和 `RoadProto.sln` Debug 构建已验证。

### 修改内容

- 优化 `查看横断面` WPF 预览图，参考路面结构层模板预览，支持鼠标拖动平移和滚轮缩放。
- 预览图先绘制结构层半透明填充面，再绘制结构层边线、路基线、边坡线和地面线。
- 在查看横断面对话框新增 `绘制横断面` 按钮，点击后通过响应文件触发 ObjectARX 内部命令回到模型空间点取插入基点。
- 新增内部命令 `RD_SECTION_ROAD_MODEL_VIEW_SECTION_APPLY_DIALOG_FILE`，负责读取 WPF 响应、重建所有桩号断面预览并按基点向上批量绘制。
- 新增 `DnRoadModelSectionDrawingEntity` 自定义实体，每个桩号生成一个实体，保存道路模型 handle、中线 handle、桩号、外框尺寸、线段、结构层面域和结构层模板填充信息。
- 横断面落图中的结构层颜色和填充优先按道路模型结构层线引用的路面结构层模板匹配；无法匹配时保留预览颜色并使用实体填充，避免落图失败。

### 验证状态

- 自动化验证：托管 bridge 测试通过；核心测试 Debug 构建与运行通过；`RoadProto.sln` Debug 构建通过，生成 `RoadProto_v0.1.25_20260525_CrossSectionViewDrawing.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：本轮未在 AutoCAD 2021 图形界面完整点验；建议加载 Debug 或 Release 产物后验证预览拖动缩放、绘制横断面基点点取、批量落图间距、桩号文字、外框实体和结构层填充效果。

## v0.1.24 - 2026-05-25

- 版本标识：`v0.1.24_20260525_PavementLayerTemplateWizard`。
- ARX 文件：`RoadProto_v0.1.24_20260525_PavementLayerTemplateWizard.arx`。
- 阶段：路面结构层模板创建向导与文档预设参数。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、托管 bridge 测试、WPF Release 构建和 ARX Release 构建已验证。

### 修改内容

- 新增路面结构层创建向导；运行创建命令后先选择路面类型和适应路段类型，再进入原有路面结构层模板参数窗口，既有 DWG 自定义实体双击编辑仍直接打开原有编辑窗口。
- 向导按 `参数示意.docx` 内置沥青路面主线行车道、主线硬路肩、主线路缘带、互通匝道、桥头过渡段、桥面铺装、互通被交路、隧道，以及混凝土路面收费站广场、通道连接线初始参数。
- 路面结构层类型扩展为上面层、中面层、下面层、沥青封层、基层、底基层、垫层和搭板层；桥接文件、WPF 枚举、XML 流转和 DWG 持久化均使用稳定编码保存。
- 创建向导取消时不提前生成默认模板实体；确认旧参数窗口后再点取插入点并创建 `DnPavementLayerTemplateEntity`，避免取消向导后模型空间残留默认实体。

### 验证状态

- 自动化验证：核心测试 Debug/Release 构建与运行、托管 bridge 测试、WPF Release 构建和 `RoadProto.sln` Release 构建通过；Release 产物生成 `RoadProto_v0.1.24_20260525_PavementLayerTemplateWizard.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：本轮未在 AutoCAD 2021 图形界面完整点验；建议加载 Release ARX 和托管 DLL 后验证创建向导、向导确认后旧参数窗口、双击编辑不显示向导、`.rpavement.xml` 导入导出和道路模型结构层显示。

## v0.1.23 - 2026-05-23

- 版本标识：`v0.1.23_20260523_PavementLayerTemplateGeneralParams`。
- ARX 文件：`RoadProto_v0.1.23_20260523_PavementLayerTemplateGeneralParams.arx`。
- 阶段：路面结构层模板高级通用参数数据保留与默认折叠显示。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、托管 bridge 测试、WPF Release 构建和 ARX Release 构建已验证。

### 修改内容

- 路面结构层模板通用参数新增 `showAllGeneralParameters`、`structureCode`、`subgradeMoistureTypes`、`pavementType`、`subgradeSoilGroups`、`designDeflection` 和 `cumulativeAxleLoads`；领域模型、C++ Bridge、DWG 自定义实体、WPF 请求/响应文件和 `.rpavement.xml` 均同步读写。
- WPF 通用参数区默认只显示预览宽度、结构层数量、显示方式和当前编辑层；勾选“显示全部通用参数”后才显示结构代号、路基干湿类型、路面类型、路基土组、设计弯沉和累计轴次。
- 路基干湿类型和路基土组使用多选下拉保存稳定枚举编码；路面类型使用单选下拉，默认沥青路面。
- 新增通用设计字段当前仅作为模板数据保留，不参与 WPF 预览标注、`DnPavementLayerTemplateEntity` 模板标题/尺寸文字或道路模型结构层图形显示。

### 验证状态

- 自动化验证：核心测试 Debug/Release 构建与运行、托管 bridge 测试、WPF Release 构建和 `RoadProto.sln` Release 构建通过；Release 产物生成 `RoadProto_v0.1.23_20260523_PavementLayerTemplateGeneralParams.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：本轮未新增 AutoCAD 2021 图形界面手工验证；建议加载 Release ARX 和托管 DLL 后点验高级通用参数折叠显示、XML 导入导出和旧模板回写兼容。

## v0.1.22 - 2026-05-23

- 版本标识：`v0.1.22_20260523_PavementLayerTemplateHatchParams`。
- ARX 文件：`RoadProto_v0.1.22_20260523_PavementLayerTemplateHatchParams.arx`。
- 阶段：路面结构层模板填充角度/比例、DWG 标题显示和预览固定标注优化。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、托管 bridge 测试、WPF Release 构建和 ARX Release 构建已验证。

### 修改内容

- 路面结构层模板每层新增 `hatchAngle` 和 `hatchScale`，请求/响应文件、`.rpavement.xml`、C++ Bridge、领域归一化和 DWG 自定义实体持久化同步读写。
- 扩充填充类型列表，覆盖 AutoCAD 常用 ANSI、AR、BRICK、STEEL、GRAVEL、EARTH 等内置填充名；非法填充仍归一化为 `SOLID`，非法角度归零，非法比例归一。
- WPF 预览中的层名+厚度标注改为固定字号，不再随结构层厚度变化；填充预览按当前层填充角度和填充比例绘制。
- `DnPavementLayerTemplateEntity` 不再显示层名、厚度、加宽和坡度尺寸标注，只在模板上方显示模板名称；标题绘制前估算文字总长度，再按文字中点与模板内容中心对齐。
- 道路模型结构层显示策略保持不变，仍只按层 RGB 颜色显示弱化填充面和边线，不引入模板填充模式。

### 验证状态

- 自动化验证：核心测试 Debug/Release 构建与运行、托管 bridge 测试、WPF Release 构建和 `RoadProto.sln` Release 构建已通过；Release 产物已生成 `RoadProto_v0.1.22_20260523_PavementLayerTemplateHatchParams.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：本轮仍建议在 AutoCAD 2021 中加载 Release ARX 和托管 DLL 后，点验 DWG 模板标题居中、WPF 固定字号标注、填充角度/比例和道路模型颜色显示；历史回归记录中已在 AutoCAD 2021 图形界面加载 Debug ARX 完成模板实体显示验证。

## v0.1.21 - 2026-05-23

- 版本标识：`v0.1.21_20260523_PavementLayerTemplateAnnotation`。
- ARX 文件：`RoadProto_v0.1.21_20260523_PavementLayerTemplateAnnotation.arx`。
- 阶段：路面结构层模板标注、当前层编辑和填充显示优化。

### 修改内容

- 优化路面结构层模板 WPF 预览标注：层名与厚度合并为一行，整体标注缩小；加宽处改为 CAD 式尺寸标注；坡度在侧边中心以 `1:n` 显示。
- 将 WPF 参数区改为当前层编辑：预览图点击选择结构层，右侧仅显示当前层参数和通用参数，并提供当前层输入框和上/下切换按钮。
- 新增每层 `hatchPattern` 填充类型和通用 `displayMode` 显示方式，支持按颜色、按填充、按填充+颜色显示；WPF 预览和 `DnPavementLayerTemplateEntity` 按选择方式显示。
- 新增颜色预览块点击选择索引颜色；请求/响应文件、`.rpavement.xml` 和 DWG 自定义实体持久化同步保存显示方式和填充类型。
- 保持道路模型中的路面结构层模型显示策略不变：仍按层 RGB 颜色绘制弱化填充面和边线，不引入模板显示方式中的填充预览模式。

### 验证状态

- 自动化验证：核心测试 Debug/Release 构建与运行、托管 bridge 测试、WPF Release 构建和 `RoadProto.sln` Release 构建已通过；Release 产物已生成 `RoadProto_v0.1.21_20260523_PavementLayerTemplateAnnotation.arx` 和 `RoadProto.Terrain.UI.dll`。
- 图形界面验证：仍建议在 AutoCAD 2021 中加载 Release ARX 和托管 DLL 后，点验模板创建、双击编辑、预览点击选层、索引颜色选择、填充显示方式和道路模型颜色显示。

## v0.1.20 - 2026-05-22

- 版本标识：`v0.1.20_20260522_PavementLayerTemplateDisplay`。
- ARX 文件：`RoadProto_v0.1.20_20260522_PavementLayerTemplateDisplay.arx`。
- 阶段：路面结构层模板 DWG 实体显示一致性与独立加载文件名。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、托管 bridge 测试、WPF Release 构建和 ARX Release 构建已验证；AutoCAD 2021 图形界面已使用新文件名 ARX 对正常创建路面结构层模板实体做回归截图验证。

### 修改内容

- 更新构建版本信息为 `v0.1.20_20260522_PavementLayerTemplateDisplay`，让本轮 ARX 输出为新的独立文件名，避免 AutoCAD 当前进程继续复用已加载的同名 `v0.1.19` 旧模块。
- 保持路面结构层模板几何规则不变：`DnPavementLayerTemplateEntity` 仍使用 `PavementLayerTemplateRules::buildSection` 的四点轮廓、预览式弱化填充色、层保存 RGB 边线和中文标注。
- 更新 README、测试说明和路面结构层模板业务文档中的当前版本记录与加载路径。

### 验证状态

- 回归测试：核心测试 Debug/Release、托管 bridge 测试、WPF Release 构建、ARX Debug/Release 构建通过。
- 图形界面验证：已在 AutoCAD 2021 图形界面加载新文件名 ARX，执行 `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE` 正常创建实体，截图确认模型空间自定义实体使用预览式弱化填充、层 RGB 边线和四边形/梯形轮廓。

## v0.1.0

- 日期：2026-05-08
- ARX 文件：`RoadProto_v0.1.0_20260508_Framework.arx`
- 阶段：框架搭建
- 是否可作为稳定测试版本：是。需要在配置 ObjectARX 2021 SDK 后完成本地 ARX 构建和 AutoCAD 加载验证。

### 新增内容

- Visual Studio 解决方案和 ARX 项目骨架。
- 统一 `CommandRegistry`。
- 统一 `ModuleRegistry`。
- 运行期 `ApplicationContext` 与启动/卸载流程。
- ObjectARX 入口函数与集中式命令注册适配器。
- Ribbon 模型和按模块分组的菜单框架。
- 地形数模示例模块与命令 `RD_TERRAIN_MARKDIRTY`。
- 平交口示例模块与命令 `RD_INTERSECTION_INFO`。
- domain 层实体关系、脏标记和重建请求机制。
- 不依赖 ObjectARX 的核心测试项目。
- 业务文档模板和两个示例命令文档。
- 复用能力模板和能力目录。
- 架构说明和 AI 后续开发规则。

### 修改内容

- 初始化仓库结构。
- 将 ObjectARX 2021 SDK 默认路径配置为 `C:\Autodesk\ObjectARX_for_AutoCAD_2021_Win_64bit_dlm\CDROM1`。
- 将 ObjectARX 2021 链接库调整为 `acdb24.lib`、`acge24.lib`、`ac1st24.lib` 等 AutoCAD 2021 对应库名。
- 移除 `.def` 中固定 `LIBRARY` 名称，避免 ARX 输出名与导出文件名不一致的链接警告。
- 补充导出 `acrxGetApiVersion`，满足 AutoCAD 2021 加载 ARX 的必要入口符号。
- Debug ARX 构建改用 `/MD` 运行库，并忽略 ObjectARX SDK 自带库缺 PDB 的链接噪声。

### 构建验证

- Debug：`artifacts/x64/Debug/RoadProto_v0.1.0_20260508_Framework.arx` 构建通过，0 警告，0 错误。
- Release：`artifacts/x64/Release/RoadProto_v0.1.0_20260508_Framework.arx` 构建通过，0 警告，0 错误。
- Release 导出表已验证包含 `acrxEntryPoint` 和 `acrxGetApiVersion`。

### 已知问题

- V0.1 只建立 Ribbon 模型和适配器占位，暂未实现完整 AutoCAD 原生 Ribbon UI 创建。
- ARX 构建需要配置 ObjectARX 2021 SDK 路径，并使用 AutoCAD 2021 兼容的 Visual Studio 工具集。
- V0.1 不实现复杂道路设计算法。

## v0.1.1

- 日期：2026-05-08
- ARX 文件：`RoadProto_v0.1.1_20260508_TerrainTin.arx`
- 阶段：地形构网原型
- 是否可作为稳定测试版本：是。核心测试、WPF 工程、ARX Debug/Release 构建和 Core Console 加载执行均已验证；完整 WPF 接入仍属后续项。

### 新增内容

- 新增命令 `DN_TERRAIN_TIN_CREATE`，Ribbon 注册在地形数模模块的 `数模` 面板。
- 新增 C++ terrain data/service：`TinPoint`、`TinTriangle`、`TinExtractOptions`、`TinBuildOptions`、`TinExtractSummary`、`TinBuildResult`、`TerrainTextElevationParser`、`TerrainPointNormalizer`、`TerrainTinBuilder`、`TerrainSurfaceQuery`、`TerrainTinCreateService`。
- 新增 ObjectARX 适配能力：从选择集中读取点、线、多段线、文字、多行文字、块参照和带属性块，解析文字高程和块属性高程，支持有限嵌套块读取且不 explode 图纸。
- 新增 `DnTerrainTinEntity` 自定义实体，支持 DWG 持久化、三角网显示、边界显示、几何范围、变换和基础高程查询接口。
- 新增 WPF 工程 `RoadProto.Terrain.UI`，包含 `TerrainTinCreateWindow`、`TerrainTinCreateViewModel`、`ITerrainTinBridge` 和 DTO。
- 新增第三方头文件 `third_party/delaunator-cpp`，MIT License，用于更高性能的二维 Delaunay 三角剖分。

### 修改内容

- 将核心架构原则“C++ ObjectARX 核心 + 可替换 UI 层”写入 `AGENTS.md` 和 `docs/architecture/overview.md`，并在 `README.md` 增加引用。
- 更新 `README.md` 当前版本和新增命令说明。
- 更新 `docs/modules/terrain.md`、`docs/modules/module_index.md`、`docs/reuse/capability_catalog.md` 和 `tests/README.md`。
- 公共 C++ 构建参数增加 `/utf-8`，避免中文宽字符串在 MSVC 默认代码页下被误读。
- `TerrainTinBuilder` 从项目内简单增量式三角剖分替换为 `delaunator-cpp`，保留退化三角形、最小面积和最大边长过滤。

### 构建验证

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- 核心测试 Release：`artifacts\x64\Release\RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- WPF：`RoadProto.Terrain.UI.csproj` Release 构建通过，0 警告，0 错误。
- Debug ARX：`artifacts/x64/Debug/RoadProto_v0.1.1_20260508_TerrainTin.arx` 构建通过，0 警告，0 错误。
- Release ARX：`artifacts/x64/Release/RoadProto_v0.1.1_20260508_TerrainTin.arx` 构建通过，0 警告，0 错误。
- Core Console：将同一二进制复制为 `.crx` 后加载执行，通过 `C:\Users\admin\Desktop\test\050802\地形.dwg` 副本全选构网验证，选中 29989 个对象，提取 947937 个原始点，清洗后 942424 个有效点，生成 1884708 个三角形并插入 `DnTerrainTinEntity`，随后执行 `QSAVE`。
- Core Console：重新打开已保存副本、加载 `.crx` 并执行 `REGEN`，未出现加载或显示崩溃。

### 已知问题

- WPF 工程已可单独编译，但当前未通过 C++/CLI 或 AutoCAD .NET Bridge 接入 unmanaged ARX 命令流程。
- `DN_TERRAIN_TIN_CREATE` 当前使用默认参数，暂未弹出 WPF 参数确认窗口。
- `FindTriangle` V1 使用简单遍历，后续大数据场景需要空间索引。
- 暂不支持约束 Delaunay、断裂线、洞口边界、等高线自动采样和源对象 reactor 自动重建。

## v0.1.2

- 日期：2026-05-08
- ARX 文件：`RoadProto_v0.1.2_20260508_TerrainTinUi.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 阶段：地形构网 UI 与 Ribbon 优化
- 是否可作为稳定测试版本：是。ARX Debug/Release、托管 Ribbon Debug/Release、核心测试和 Core Console 命令回归均已验证；完整 WPF 参数窗口 Bridge 仍属后续项。

### 新增内容

- `RoadProto.Terrain.UI` 改为 AutoCAD 2021 可加载的 .NET Framework 4.8 插件，新增 `RoadProto` 选项卡、`数模` 面板和 `地形构网` 按钮。
- 新增 `DN_ROADPROTO_SHOW_RIBBON` 辅助命令，用于托管插件加载后重新创建可见 Ribbon。
- `DN_TERRAIN_TIN_CREATE` 改为点选一个样例对象后，按样例对象的图层和精确图元类型扫描模型空间同类数据。
- 新增 C++ `TerrainTinCreateDialog` 参数确认窗口，显示提取统计，开放 XY 合并容差、最大三角边长、退化三角形过滤和显示模式。
- 命令生成 `DnTerrainTinEntity` 时可直接应用 `ColoredTriangles` 或 `BoundaryOnly` 显示模式。

### 修改内容

- 更新 ARX 命名阶段为 `TerrainTinUi`。
- 更新 `ObjectArxTerrainTinCommand` 流程，命令层只组织选择、预览、参数确认、构网和插入实体，提取、清洗、构网仍由 service / domain 层承担。
- 更新 WPF 工程输出目录到 `artifacts/managed/<Configuration>/net48/`。
- 更新 `README.md`、模块文档、业务文档、复用能力目录和架构说明。

### 构建验证

- Debug ARX：`artifacts/x64/Debug/RoadProto_v0.1.2_20260508_TerrainTinUi.arx` 构建通过，0 警告，0 错误。
- Release ARX：`artifacts/x64/Release/RoadProto_v0.1.2_20260508_TerrainTinUi.arx` 构建通过，0 警告，0 错误。
- Debug 托管 Ribbon：`artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。
- Release 托管 Ribbon：`artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。
- 核心测试：`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- Core Console：将 Release ARX 复制为 `.crx` 后，通过 `(arxload "...")` 加载，基于 `C:\Users\admin\Desktop\test\050802\地形.dwg` 副本执行 `DN_TERRAIN_TIN_CREATE`；命令使用首个样例对象按同图层同类型提取，得到 30499 个原始点、30145 个有效点、60251 个三角形并成功插入实体。
- Core Console：重新打开已保存副本、加载 `.crx` 并执行 `REGEN`，未出现加载或显示崩溃。
- Core Console：从 ASCII 临时路径 `NETLOAD` 托管 Ribbon 插件并运行 `DN_ROADPROTO_SHOW_RIBBON`，命令注册和插件加载无错误。

### 已知问题

- 可见 Ribbon 需要在完整 AutoCAD 中 `NETLOAD RoadProto.Terrain.UI.dll`；Core Console 不显示 Ribbon，只用于命令回归。
- 当前参数确认窗口为 C++ Win32 对话框，不是完整 WPF 参数窗口；WPF 窗口、ViewModel、DTO 和 Bridge 仍保留为后续接入点。
- 点选样例对象后按精确图元类型过滤，暂不支持一次样例自动合并同图层的多种不同类型。
- `FindTriangle` V1 使用简单遍历，后续大数据场景需要空间索引。
- 暂不支持约束 Delaunay、断裂线、洞口边界、等高线自动采样和源对象 reactor 自动重建。

## v0.1.3

- 日期：2026-05-08
- ARX 文件：`RoadProto_v0.1.3_20260508_TerrainTinEdit.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 阶段：地形构网体验与可编辑数模
- 是否可作为稳定测试版本：是。ARX Debug/Release、托管 Ribbon Debug/Release、核心测试、Core Console 创建/编辑/保存/REGEN 回归均已验证；完整 AutoCAD 可见 Ribbon 和双击弹窗仍需要在图形界面中做人工确认。

### 新增内容

- ARX 加载时自动尝试从 `artifacts/managed/<Configuration>/net48/RoadProto.Terrain.UI.dll` 加载托管 Ribbon 插件。
- Ribbon 新增 `编辑数模` 按钮，命令为 `DN_TERRAIN_TIN_EDIT`。
- 新增 `DN_TERRAIN_TIN_EDIT` 命令，可编辑已有 `DnTerrainTinEntity` 的显示模式和构网参数，并可基于实体内保存的点重新生成 TIN。
- `DnTerrainTinEntity` 增加 `AcDbDoubleClickEdit` 协议扩展，支持双击实体打开编辑窗口。
- 地形对象提取、点清洗、TIN 构建、源对象隐藏和重新构建过程增加 AutoCAD 状态栏进度条。
- 按样例对象提取完成后临时隐藏同图层同类型源对象，取消或失败时恢复，构网成功后保留隐藏状态。

### 修改内容

- `ColoredTriangles` 显示模式由三角面填充改为三角网边线按高程着色，降低大三角网显示和选中时的卡顿。
- `TerrainTinCreateDialog` 改为浅色、分组式 Win32 UI，并复用为创建窗口和编辑窗口。
- `RoadProto.Terrain.UI` 托管 Ribbon 插件保持 AutoCAD 2021 可加载的 .NET Framework 4.8 目标，并在创建后激活 `RoadProto` 选项卡。
- 更新 `README.md`、模块文档、业务文档、复用能力目录和架构说明。

### 构建验证

- Debug ARX：`artifacts/x64/Debug/RoadProto_v0.1.3_20260508_TerrainTinEdit.arx` 构建通过，0 警告，0 错误。
- Release ARX：`artifacts/x64/Release/RoadProto_v0.1.3_20260508_TerrainTinEdit.arx` 构建通过，0 警告，0 错误。
- Debug 托管 Ribbon：`artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。
- Release 托管 Ribbon：`artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。
- 核心测试：`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- Core Console：将 Release ARX 复制为 `.crx` 后，通过 `(arxload "...")` 加载，基于 `C:\Users\admin\Desktop\test\050802\地形.dwg` 副本执行 `DN_TERRAIN_TIN_CREATE`；命令使用首个样例对象按同图层同类型提取，得到 30499 个原始点、30145 个有效点、60251 个三角形并成功插入实体。
- Core Console：在同一 DWG 副本上执行 `DN_TERRAIN_TIN_EDIT`，选择集中过滤到 `DnTerrainTinEntity`，并在 Core Console 非 UI 路径下切换为边界显示。
- Core Console：重新打开已保存副本、加载 `.crx` 并执行 `REGEN`，未出现加载或显示崩溃。
- Core Console：从 ASCII 临时路径 `NETLOAD` 托管 Ribbon 插件并运行 `DN_ROADPROTO_SHOW_RIBBON`，命令注册和插件加载无错误。

### 已知问题

- Core Console 不显示 Ribbon，也无法验证双击 UI；双击弹出编辑窗口需在完整 AutoCAD 图形界面中确认。
- 当前编辑窗口的“重新生成”基于实体内已保存的地形点重建 TIN，尚未从原始源对象重新提取。
- 点选样例对象后按精确图元类型过滤，暂不支持一次样例自动合并同图层的多种不同类型。
- `FindTriangle` V1 使用简单遍历，后续大数据场景需要空间索引。
- 暂不支持约束 Delaunay、断裂线、洞口边界、等高线自动采样和源对象 reactor 自动重建。

## v0.1.4

- 日期：2026-05-08
- ARX 文件：`RoadProto_v0.1.4_20260508_TerrainTinWorkflow.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 阶段：地形构网交互流程优化
- 是否可作为稳定测试版本：是。Release ARX、托管 Ribbon Release、核心测试、Core Console 创建/编辑/保存/REGEN 回归均已验证；完整 AutoCAD 图形界面的 Ribbon 点击和双击弹窗仍需人工点验。

### 新增内容

- Ribbon `地形构网` 和 `编辑数模` 按钮统一为小按钮尺寸，并增加内置地形图标。
- Ribbon 按钮命令发送逻辑兼容 AutoCAD 传入 `RibbonButton` 作为命令参数的事件路径，点击后发送 `DN_TERRAIN_TIN_CREATE` 或 `DN_TERRAIN_TIN_EDIT`。
- `DN_TERRAIN_TIN_CREATE` 改为连续样例提取流程：每次点选一个样例，提取同图层同类型对象，显示进度，提取后隐藏当前类；按 Enter 后打开参数窗口。
- 创建窗口新增 `生成地形数模` 窗口进度、`预览`、`确认`、`取消` 交互。生成按钮每次按当前参数重建最新预览实体，确认保留，取消删除预览并恢复源对象。
- `预览` 会临时隐藏参数窗口并回到 DWG，按 ESC 返回继续调参。

### 修改内容

- `DnTerrainTinEntity` 的 `ColoredTriangles` 模式继续只绘制边线，但由 ACI 分段色改为 TrueColor 连续渐变色。
- 创建窗口底部按钮文字调整为 `确认` / `取消`，并保留创建和编辑两种模式复用。
- Core Console 路径下保持非 UI 自动构网，用于命令回归和 DWG 持久化验证。
- 更新 `README.md`、模块文档、业务文档、复用能力目录、架构说明、测试说明和版本信息。

### 构建验证

- Release ARX：`artifacts/x64/Release/RoadProto_v0.1.4_20260508_TerrainTinWorkflow.arx` 构建通过，0 警告，0 错误。
- Release 托管 Ribbon：`artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。
- 核心测试：`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- Core Console：将 Release ARX 复制为 `.crx` 后，通过 `(arxload "...")` 加载，基于 `C:\Users\admin\Desktop\test\050802\地形.dwg` 副本执行 `DN_TERRAIN_TIN_CREATE`；命令使用首个样例对象按同图层同类型提取，得到 30499 个原始点、30145 个有效点、60251 个三角形并成功插入实体。
- Core Console：在同一 DWG 副本上执行 `DN_TERRAIN_TIN_EDIT`，选择集中过滤到 `DnTerrainTinEntity`，并在 Core Console 非 UI 路径下切换为边界显示。
- Core Console：重新打开已保存副本、加载 `.crx` 并执行 `REGEN`，未出现加载或显示崩溃。
- Core Console：从 ASCII 临时路径 `NETLOAD` 托管 Ribbon 插件并运行 `DN_ROADPROTO_SHOW_RIBBON`，命令注册和插件加载无错误。

### 已知问题

- Core Console 不显示 Ribbon，也无法模拟鼠标点击 Ribbon 或双击实体；这两项仍需在完整 AutoCAD 图形界面中人工确认。
- 当前 `预览` 是隐藏参数窗口并等待 ESC 返回，不是 AutoCAD Palette 式非模态窗口。
- 当前编辑窗口的“重新生成”基于实体内已保存的地形点重建 TIN，尚未从原始源对象重新提取。
- 点选样例对象后按精确图元类型过滤，暂不支持一次样例自动合并同图层的多种不同类型。
- `FindTriangle` V1 使用简单遍历，后续大数据场景需要空间索引。

## v0.1.5

- 日期：2026-05-08
- ARX 文件：`RoadProto_v0.1.5_20260508_TerrainTinRmesh.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 阶段：地形数模双击编辑兜底与 RMesh 流转
- 是否可作为稳定测试版本：是。Release ARX、核心测试、Release 托管 Ribbon 构建和 Core Console RMesh 导入/REGEN 回归均已验证；完整 AutoCAD 图形界面的 Ribbon 点击、导出文件对话框和双击弹窗仍需人工点验。

### 新增内容

- 新增命令 `DN_TERRAIN_TIN_EXPORT`，可将选中的 `DnTerrainTinEntity` 导出为 `.rmesh` 文件。
- 新增命令 `DN_TERRAIN_TIN_IMPORT`，可从 `.rmesh` 文件导入并插入新的 `DnTerrainTinEntity`。
- Ribbon `数模` 面板新增 `导出数模` 和 `导入数模` 按钮，保持与已有小按钮尺寸一致。
- 新增领域层 `TerrainMeshFile`，以 RoadProto RMesh V1 二进制格式保存 TIN 点、三角形、边界边、参数、统计和来源追踪信息。
- 新增 `TerrainMeshFile` 核心测试，覆盖 `.rmesh` 往返读写、Unicode 元数据回读和非法文件拒绝。

### 修改内容

- `DnTerrainTinEntity` 在原有 `AcDbDoubleClickEdit` 协议基础上增加 `AcEditorReactor::beginDoubleClick` 兜底入口，双击实体时会优先按隐含选择集或点击点查找数模实体并打开编辑窗口。
- `DN_TERRAIN_TIN_EDIT`、导出命令的选择逻辑优先识别当前隐含选择，减少双击或预选后还要重复点选的问题。
- README、地形模块文档、地形构网业务文档、RMesh 业务文档、复用说明和测试说明已同步更新到 v0.1.5。

### 构建验证

- Release ARX：`artifacts/x64/Release/RoadProto_v0.1.5_20260508_TerrainTinRmesh.arx` 构建通过，0 警告，0 错误。
- 核心测试：`RoadProtoCoreTests.vcxproj` Release 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- Release 托管 Ribbon：`artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。
- Core Console：加载 `RoadProto_v0.1.5_20260508_TerrainTinRmesh.crx` 后执行 `DN_TERRAIN_TIN_IMPORT`，从 `.rmesh` 小样导入 3 个点、1 个三角形并插入 `DnTerrainTinEntity`；随后重新打开导入后的 DWG 执行 `REGEN`，未出现加载或显示崩溃。

### 已知问题

- Core Console 无法验证完整 AutoCAD 图形界面中的 Ribbon 点击和实体双击；双击 UI 仍需在完整 AutoCAD 界面中人工点验。
- `.rmesh` V1 只保存数模结果和来源追踪信息，不保存原 DWG 对象本体；跨 DWG 导入后可编辑显示和参数，但不会自动重连源对象。
- Core Console 无法稳定模拟 `DN_TERRAIN_TIN_EXPORT` 的图形选择和文件对话框；导出命令的文件读写核心由 `TerrainMeshFile` 核心测试覆盖，完整导出交互仍需在 AutoCAD 图形界面中点验。

## v0.1.6

- 日期：2026-05-08
- ARX 文件：`RoadProto_v0.1.6_20260508_TerrainTinDblClick.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 阶段：地形数模双击编辑修复
- 是否可作为稳定测试版本：是。Release ARX、Release 托管 Ribbon、核心测试和 Core Console handle 编辑链路均已验证；完整鼠标双击弹窗仍需在 AutoCAD 图形界面中点验。

### 新增内容

- 新增内部命令 `DN_TERRAIN_TIN_EDIT_HANDLE`，按 `DnTerrainTinEntity` 的 handle 打开编辑流程，不挂 Ribbon。
- 托管 Ribbon 插件监听 AutoCAD `BeginDoubleClick` 事件，双击 `DNTERRAINTINENTITY` 时提取实体 handle，并转发到 `DN_TERRAIN_TIN_EDIT_HANDLE <handle>`。

### 修改内容

- 双击编辑不再依赖 AutoCAD 双击后的隐含选择集是否仍然有效，降低“已选中数模但双击无法弹窗”的概率。
- `DN_TERRAIN_TIN_EDIT` 复用同一套实体编辑函数，命令选择和 handle 编辑最终进入同一条 C++ ObjectARX 编辑流程。
- README、地形构网业务文档、地形模块文档、测试说明和版本记录已同步更新到 v0.1.6。
- 补充地形 V0.1.6 定版文档，明确业务逻辑、CAD 功能逻辑和代码分层逻辑的边界。
- 将 `地形更新联动示例.md` 标记为历史框架示例，说明当前 TIN 实体仅预留来源追踪和重建接口，尚未实现源对象 reactor 自动重建。
- 全局开发文档加入本机 VS2026 Insiders 优先编译路径，以及 `taskhostw.exe`、AutoCAD 残留进程和内存异常的排查规则。

### 构建验证

- Release ARX：`artifacts/x64/Release/RoadProto_v0.1.6_20260508_TerrainTinDblClick.arx` 构建通过，0 警告，0 错误。
- Release 托管 Ribbon：`artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。
- 核心测试：`RoadProtoCoreTests.vcxproj` Release 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- Core Console：导入 `.rmesh` 小样生成 `DnTerrainTinEntity`，再通过 `(command "DN_TERRAIN_TIN_EDIT_HANDLE" (cdr (assoc 5 (entget (entlast)))))` 验证 handle 编辑入口可找到实体并执行非 UI 编辑路径；随后 `REGEN`、`QSAVE` 正常。
- Core Console：从 ASCII 临时路径 `NETLOAD RoadProto.Terrain.UI.dll` 并运行 `DN_ROADPROTO_SHOW_RIBBON`，托管插件加载和命令注册无报错。

### 已知问题

- Core Console 无法模拟真实鼠标双击和弹窗交互；本次自动验证覆盖的是双击事件最终转发到的 handle 编辑入口。

## v0.1.7

- 日期：2026-05-09
- ARX 文件：`RoadProto_v0.1.7_20260509_AlignmentCenterline.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 阶段：平面布线与道路中线实体原型
- 是否可作为稳定测试版本：是。核心测试、ARX Debug/Release 构建和托管 Ribbon Release 构建已验证；完整 AutoCAD 图形界面的 Ribbon 点击、道路中线创建、双击编辑、夹点拖动和保存重开仍需人工点验。

### 新增内容

- 新增 `ALIGNMENT` 平面设计模块。
- 新增命令 `RD_ALIGN_CENTERLINE_CREATE`，用于点取控制点并创建 `DnRoadCenterlineEntity` 道路中线自定义实体。
- 新增命令 `RD_ALIGN_CURVE_PARAM_EDIT`，用于编辑道路中线的 `T1`、`T2`、`LS1`、`R`、`LS2`。
- 新增内部命令 `RD_ALIGN_CENTERLINE_EDIT_HANDLE`，供托管双击 Bridge 按 handle 打开道路中线属性编辑。
- 新增领域层 `src/domain/alignment`，包含回旋线公式、道路等级、桩号格式化、单交点五单元构建和多交点连续平曲线链构建。
- 新增 `DnRoadCenterlineEntity`，支持 DWG 持久化、图中绘制、百米桩号、特征点标注、曲线参数标注和基础夹点。
- 托管 Ribbon 插件新增 `平面设计` 面板、`平面布线` 和 `编辑平曲线参数` 按钮，并监听 `DNROADCENTERLINEENTITY` 双击后转发到 handle 编辑命令。

### 修改内容

- 更新 `README.md` 当前版本、命令说明和 Ribbon 说明。
- 更新命令与模块规则，明确新增 Ribbon 命令时必须同时更新 C++ Ribbon 元数据和托管可见 Ribbon。
- 更新模块索引、平面设计模块文档、平面布线业务文档、复用能力目录、复用说明和测试说明。
- 更新构建版本信息为 `v0.1.7_20260509_AlignmentCenterline`。

### 构建验证

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- Debug ARX：`artifacts/x64/Debug/RoadProto_v0.1.7_20260509_AlignmentCenterline.arx` 构建通过，0 警告，0 错误。
- Release ARX：`artifacts/x64/Release/RoadProto_v0.1.7_20260509_AlignmentCenterline.arx` 构建通过，0 警告，0 错误。
- Release 托管 Ribbon：`artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。

### 已知问题

- 第一版属性与参数编辑采用 AutoCAD 命令交互承接，后续必须按全局 UI 规则迁移为 WPF 对话框。
- 第一版夹点拖动以拖动结束后刷新为主，暂未实现拖动过程连续动态预览。
- 道路等级第一版仅保存属性，不驱动规范参数推荐。
- 数模关联第一版保存 handle 和业务入口，暂不执行纵断面取高。

## v0.1.8

- 日期：2026-05-09
- ARX 文件：`RoadProto_v0.1.8_20260509_AlignmentWpfPreview.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 阶段：平面布线 WPF 编辑与图面交互优化
- 是否可作为稳定测试版本：是。核心测试、ARX Debug/Release 构建和托管 Ribbon Debug/Release 构建已验证；完整 AutoCAD 图形界面的真实鼠标拖拽、WPF 弹窗交互和保存重开仍需人工点验。

### 新增内容

- 新增道路中线 WPF 参数窗口，支持编辑道路名称、道路等级、是否关联数模、关联数模 handle 和桩号标注间距。
- 新增 WPF 桩号标注设置二级窗口，当前支持配置桩号标注间距，默认 `100m`。
- 新增 WPF 平曲线参数窗口入口，`RD_ALIGN_CURVE_PARAM_EDIT` 选择道路中线后编辑 `T1`、`T2`、`LS1`、`R`、`LS2`。
- 新增 `AlignmentDialogBridge`，通过请求/响应文件把 WPF DTO 和 C++ ObjectARX 实体重建解耦。
- 新增内部命令 `RD_ALIGN_APPLY_DIALOG_FILE`，用于应用 WPF 回写结果。

### 修改内容

- `RD_ALIGN_CENTERLINE_CREATE` 在点取第三个控制点和后续控制点时使用 `AcEdJig` 显示道路中线自定义实体动态预览，确认后插入或实时刷新正式实体。
- 平曲线构建改为按前后切线延长线交点 PI 和回旋线主点公式计算，核心测试补充 `m/p/T` 与 PI 主点断言。
- 平曲线构建不再信任旧 `T1/T2` 自由长度，切线长由 `R`、`LS1`、`LS2` 和交角重新派生，避免夹点拖动后出现折线、穿插或反弯的破坏形态。
- 道路中线实体按元素着色：直线青色、第一缓和曲线红色、圆曲线黄色、第二缓和曲线洋红色。
- 桩号标注改为带引线形式；曲线参数标注按曲线段显示圆曲线半径、圆弧长度和两段缓和曲线长。
- 特征点夹点第一版支持起终点、交点、直缓/缓圆/圆缓/缓直和圆曲线中点拖动后重建；直缓/缓直夹点按等比缩放方式调整当前平曲线。
- 去除特征点图面小十字，仅保留选中状态下的 AutoCAD 夹点。
- 双击道路中线从 handle 编辑命令进入 WPF 道路中线窗口，不再使用命令行属性输入。
- WPF 桥接命令改为自动提交请求/响应文件参数，避免弹出窗口前停在命令行提示。
- 更新 README、平面设计模块文档、平面布线业务文档、复用说明、复用目录、测试说明和构建版本信息。

### 构建验证

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- Debug ARX：`artifacts/x64/Debug/RoadProto_v0.1.8_20260509_AlignmentWpfPreview.arx` 构建通过，0 警告，0 错误。
- Release ARX：`artifacts/x64/Release/RoadProto_v0.1.8_20260509_AlignmentWpfPreview.arx` 构建通过，0 警告，0 错误。
- Debug 托管 Ribbon：`artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。
- Release 托管 Ribbon：`artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。

### 已知问题

- 第一版实时预览已覆盖第三点和后续点取过程；更复杂的多控制点预览样式、临时标注抑制和动态性能优化后续继续增强。
- `T1/T2` 当前作为派生值显示以保护五单元形态，后续需要增加类似 EICAD 的编辑控制模式后再开放为可控参数。
- WPF Bridge 当前采用临时请求/响应文件，后续可替换为更正式的进程内 Bridge。
- 道路等级第一版仅保存属性，不驱动规范参数推荐；数模关联仅保存 handle，暂不执行纵断面取高。

## v0.1.9

- 日期：2026-05-11
- ARX 文件：`RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 阶段：纵断面拉坡图原型
- 是否可作为稳定测试版本：是。核心测试、ARX Debug/Release 构建和托管 Ribbon Debug/Release 构建已验证；完整 AutoCAD 图形界面的 Ribbon 点击、文件对话框、双击 WPF 弹窗、数模取高和 DWG 保存重开仍需人工点验。

### 新增内容

- 新增 `PROFILE` 纵断面设计模块，注册 `RD_PROFILE_GRADE_GRAPH_CREATE`、`RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE` 和 `RD_PROFILE_APPLY_DIALOG_FILE`。
- 新增 `ProfileDmxFile`，支持 `.dmx` 纵地面线文件读取、注释跳过、断链写法兼容、重复桩号保留和非法输入拒绝。
- 新增 `ProfileGradeGraphModel`、`ProfileGradeGraphLayout` 和 `ProfileGradeGraphCreateService`，沉淀拉坡图数据模型、默认属性、纵向比例、网格范围和图面坐标映射。
- 新增 `DnProfileGradeGraphEntity` 自定义实体，支持标题、表头、网格线、高程/桩号标注、地面线折线、DWG 持久化、几何范围和变换。
- 新增 `RD_PROFILE_GRADE_GRAPH_CREATE` 流程：选择道路中线，优先使用关联数模采样高程；无可用数模时选择 `.dmx` 文件；点取插入点后创建纵断面拉坡图实体。
- 新增 WPF 纵断面拉坡图属性窗口，支持修改名称、地面线颜色、线宽、精度、纵向比例和网格间距。
- 新增 DMX 来源“更新地面线”能力，实体保存 DMX 文件路径并可在属性窗口中重新读取；数模来源更新按钮置灰。
- 托管 Ribbon 新增 `纵断面设计` 面板和 `纵断面拉坡图` 按钮，并监听 `DNPROFILEGRADEGRAPHENTITY` 双击事件转发到 handle 编辑命令。

### 修改内容

- 更新 README 当前版本、命令说明、Ribbon 说明、加载示例和测试覆盖说明。
- 更新纵断面设计模块文档、纵断面拉坡图创建业务文档、属性编辑业务文档、复用能力目录和测试说明。
- 更新构建版本信息为 `v0.1.9_20260511_ProfileGradeGraph`。
- 数模来源地面线改为按“地面线精度”沿道路中线重新取样；属性窗口修改精度后会按保存的道路中线和数模 handle 重新采样。
- 地面线宽度校验与 AutoCAD lineweight 显示范围对齐，WPF 与 C++ 均限制为 `(0, 2.11]mm`。
- 修正道路中线 WPF 属性窗口“选择数模”流程：WPF 不再直接调用 AutoCAD `Editor.GetEntity`，改为写出 `PickTerrain` 动作并关闭窗口，由 C++ ObjectARX Adapter 执行数模选择、类型校验、清空隐含选择集，再重新打开 WPF 窗口，避免在 WPF 模态窗口内重入 CAD Editor 导致崩溃。
- 修正 WPF Bridge request 路径传递：`RD_ALIGN_SHOW_WPF_DIALOG` 和 `RD_PROFILE_SHOW_WPF_DIALOG` 不再通过命令行 `GetString` 读取 request 文件路径，改为读取 C++ 写出的 pending 文件，避免后续 `RD_*_APPLY_DIALOG_FILE` 命令被误读为 request 路径并报“路径中具有非法字符”。
- 修正 WPF Bridge response 路径解析：`RD_ALIGN_APPLY_DIALOG_FILE` 和 `RD_PROFILE_APPLY_DIALOG_FILE` 在 C++ 端会去除命令参数首尾空白与包裹引号，避免临时响应文件路径被当作含引号文件名而读取失败。

### 构建验证

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- Debug ARX：`artifacts/x64/Debug/RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx` 构建通过，0 警告，0 错误。
- Debug 托管 Ribbon：`artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。
- Release ARX：`artifacts/x64/Release/RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx` 构建通过，0 警告，0 错误。
- Release 托管 Ribbon：`artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll` 构建通过，0 警告，0 错误。

### 已知问题

- 数模来源的更新按钮已置灰，路线移动或数模更新后的自动重建尚未接入统一实体关系管理机制。
- `.dmx` 断链号当前只用于兼容读取，布局计算仍使用桩号数值部分。
- WPF Bridge 当前采用临时请求/响应文件，后续可替换为更正式的进程内 Bridge。
- Core Console 无法覆盖完整鼠标双击、文件对话框和 WPF 弹窗；这些路径需要在 AutoCAD 图形界面中人工验证。

## v0.1.9 profile vertical curve follow-up - 2026-05-12

- 新增竖曲线领域模型、默认创建服务、对称二次抛物线计算器和编辑服务。
- 新增 `DnProfileVerticalCurveEntity`，独立关联纵断面拉坡图 handle，支持 DWG 持久化、拉坡图 frame 坐标映射、曲线绘制、几何范围、起终点/PVI/半径夹点。
- 新增 `RD_PROFILE_VERTICAL_CURVE_CREATE`、`RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE`、`RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE`、`RD_PROFILE_VERTICAL_CURVE_ADD_PVI` 和 `RD_PROFILE_VERTICAL_CURVE_DELETE_PVI`。
- 新增 WPF 竖曲线编辑窗口和 `ProfileVerticalCurveDialogBridge`，支持编辑名称、起终点桩号/高程、PVI 桩号/高程和半径。
- 托管 Ribbon `纵断面设计` 面板新增 `创建竖曲线` 按钮；双击 `DNPROFILEVERTICALCURVEENTITY` 可进入 WPF 编辑窗口。
- 增强竖曲线实体图面反算：按拉坡图 `xAxis/yAxis` 二维基向量求逆，支持拉坡图被旋转或缩放后的夹点和点取反算。
- 响应文件解析改为严格数值解析，并限制 PVI 数量，避免坏响应文件写坏竖曲线实体。
- 更新竖曲线创建、编辑、夹点与右键编辑业务文档，新增竖曲线复用说明，并同步模块、复用目录、测试说明和 README。
- 构建验证：核心测试 Debug 构建通过并输出 `All RoadProto core tests passed.`；Debug ARX 构建通过，0 警告，0 错误；Debug 托管 Ribbon 构建通过，0 警告，0 错误。
- 当前仍沿用 `RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx` 输出名；后续正式 release 再统一更新 ARX 版本号和阶段名。

## v0.1.9 vertical curve context menu follow-up - 2026-05-12

- 修正竖曲线实体右键无法新增或删除变坡点的问题：托管 Ribbon 插件现在注册 AutoCAD 对象快捷菜单，并在当前隐含选择包含 `DNPROFILEVERTICALCURVEENTITY` 时显示 `新增竖曲线变坡点` 和 `删除竖曲线变坡点`。
- 右键菜单项通过 `RD_PROFILE_VERTICAL_CURVE_CONTEXT_ADD_PVI` 和 `RD_PROFILE_VERTICAL_CURVE_CONTEXT_DELETE_PVI` 转发到 C++ 侧 `RD_PROFILE_VERTICAL_CURVE_ADD_PVI` / `RD_PROFILE_VERTICAL_CURVE_DELETE_PVI`，继续由 application/domain 层完成校验和回写。
- 核心测试新增托管 Ribbon 扩展右键菜单注册约定检查，防止后续只保留命令而漏挂菜单。
- 更新竖曲线夹点与右键编辑业务文档、纵断面模块文档和测试说明。

## v0.1.9 vertical curve display follow-up - 2026-05-12

- 新增 `ProfileVerticalCurveDisplayPlanner`，在 domain 层生成竖曲线图形分段计划，区分直坡设计段、曲线设计段和曲线理论切线。
- `DnProfileVerticalCurveEntity` 改为按分段绘制：直坡设计段使用青色，曲线设计段使用黄色，BVC/PVC - PVI - EVC/PVT 理论切线使用白色细线。
- 竖曲线显示默认色调整为直坡青色、理论切线白色，关键点标记仍沿用黄色。
- 核心测试新增竖曲线显示分段规则用例，覆盖 BVC/EVC 边界拆分、直坡/曲线颜色和两段理论切线。
- 更新竖曲线创建业务文档、复用说明、模块文档、README 和测试说明。

## 记录模板

```markdown
## vX.Y.Z

- 日期：YYYY-MM-DD
- ARX 文件：`RoadProto_vX.Y.Z_YYYYMMDD_Stage.arx`
- 阶段：框架 | CAD 适配 | 领域规则 | 原型 | 稳定
- 是否可作为稳定测试版本：是 | 否

### 新增内容

- 

### 修改内容

- 

### 已知问题

- 
```

## v0.1.8 follow-up - 2026-05-09

- 修正不等长缓和曲线的 `T1/T2` 派生：`LS1` 与 `LS2` 不等长时分别计算前后切线长，避免拖动圆缓点后破坏五单元平曲线形态。
- 平曲线参数 WPF 页改为“第 x 个平曲线”左右翻页，Bridge 请求/响应文件支持回写所有交点对应的平曲线参数。
- 平面布线 Jig 预览取消 AutoCAD 自带橡皮筋基准线，后续点取阶段允许 Enter/右键结束并继续弹出 WPF 参数窗口。
- 数模关联按钮改为只保存选中对象 handle，不在 WPF 侧读取自定义实体 ObjectClass，降低 `.rmesh` 导入数模关联时崩溃风险。
- 后续点取阶段的 Jig 预览会临时隐藏已经插入 DWG 的道路中线正式实体，避免旧实体末端短直线与新预览实体重叠显示。
- WPF 回写只修改道路名称、等级或数模关联 handle 时，不再触发道路中线几何重建；仅桩号标注间距变化或曲线参数变化时才重建。
- WPF 选择数模后清空 AutoCAD 隐含选择集，避免被选中的大体量 `.rmesh` 数模实体在确认回写时继续参与高亮/选择状态。
- 同步运行时 `VersionInfo` 到 `v0.1.8_20260509_AlignmentWpfPreview`，避免 Core Console / AutoCAD 初始化日志继续显示旧的 `v0.1.6`。
- 修正后续点取阶段 Jig 空输入处理：取消 `kAcceptOtherInputString`，并在每次 `sampler()` 前重新应用 Jig 输入控制，让 Enter/右键走结束点取并弹出 WPF 参数窗口的路径。
- 优化后续点取阶段实时预览刷新：Jig 初始化时先装载上一轮路线基线，并取消隐藏正式实体后的强制 `acedUpdateDisplay()`，避免进入下一点时画面先黑一下、需要移动鼠标才恢复预览。
- 验证：核心测试 Debug 通过；Release ARX 构建通过；Release WPF 构建通过；Core Console 已验证桌面 `terrain.rmesh` 可导入并 `REGEN`。

## v0.1.8 documentation final - 2026-05-09

- 收口平面布线最终文档，不再把业务交互写成修订过程，改为直接描述最终交互逻辑。
- 补齐 `docs/business/alignment/平面布线_道路中线.md` 中的创建流程、WPF 道路中线窗口、平曲线参数编辑、图面表达、夹点和数模 handle 关联边界。
- 补齐 `docs/modules/alignment.md` 中的代码落点表，明确 domain、application、modules、cad_adapter、WPF UI 和 tests 的职责边界。
- 补齐 `docs/reuse/capability_catalog.md` 与 `docs/reuse/alignment_centerline.md` 中的平面布线最终复用边界，说明哪些能力可复用、哪些仍是原型 Bridge。
- 补齐 `tests/README.md` 中的 V0.1.8 平面布线自动测试与 AutoCAD 图形界面手工验证范围。

## v0.1.8 grip drag preview follow-up - 2026-05-09

- 新增 `AlignmentGripEditService`，把道路中线夹点索引、控制点移动、特征点参数调整和五单元重建逻辑沉淀到领域层，供 ObjectARX 实体拖动和核心测试复用。
- `DnRoadCenterlineEntity` 新增 `AcDbGripData` 夹点、`subMoveGripPointsAt` 新式回调、拖动缓存和 `subCloneMeForDragging` 无克隆预览路径；夹点拖动过程中道路中线图形、桩号和曲线参数标注随鼠标连续预览，释放后正式写回实体。
- 同步 README、平面布线业务文档、平面设计模块文档、复用目录、复用说明和测试说明，移除“完整 grip 动态预览仍需增强”的当前限制描述。
- 构建验证：核心测试 Debug 构建通过并输出 `All RoadProto core tests passed.`；Debug ARX `artifacts/x64/Debug/RoadProto_v0.1.8_20260509_AlignmentWpfPreview.arx` 构建通过，0 警告，0 错误；Release ARX `artifacts/x64/Release/RoadProto_v0.1.8_20260509_AlignmentWpfPreview.arx` 构建通过，0 警告，0 错误。

## v0.1.8 shared PI grip follow-up - 2026-05-09

- 修正交点夹点拖动仍不跟手的问题：AutoCAD 会把同坐标的控制点夹点和 PI 特征点夹点作为 shared hot grip 同时传入，原逻辑会把同一交点移动两次，可能导致动态重建失败而不显示预览。
- `AlignmentGripEditService` 现在按语义目标去重 shared grip，控制点夹点、PI 特征点夹点、起终点特征点夹点指向同一控制点时只应用一次偏移。
- 核心测试新增交点 shared grip 用例，覆盖控制点索引和 PI 特征点索引同时传入时只移动一次。
- 构建验证：核心测试 Debug 构建通过并输出 `All RoadProto core tests passed.`；Debug ARX 构建通过，0 警告，0 错误；Release ARX 构建通过，0 警告，0 错误。

## v0.1.8 incomplete spiral chain follow-up - 2026-05-09

- 新增 `AlignmentElementChainBuilder`，在五单元控制点模型之外提供平面线形元素链表达。
- 新增 `AlignmentElementType::PartialSpiral` 和 `AlignmentCurveCombinationType`，预留 C 型曲线、卵形曲线、S 型曲线和通用元素链类型。
- 支持圆曲线、不完整缓和曲线、圆曲线的内部计算和采样；卵形曲线可表达同向有限半径到有限半径过渡，S 型曲线可表达正负曲率过渡。
- `DnRoadCenterlineEntity` DWG 持久化升级为 V2，可保存元素链单元、采样点、特征点和桩号标注；元素链第一版不开放夹点交互编辑。
- 核心测试新增不完整缓和曲线元素链、S 型曲率过渡和非法不完整缓和曲线拒绝用例。
- 构建验证：核心测试 Debug 构建通过并输出 `All RoadProto core tests passed.`；Debug ARX 构建通过，0 警告，0 错误；Release ARX 构建通过，0 警告，0 错误。

## v0.1.8 alignment ICD follow-up - 2026-05-09

- 构建验证：核心测试 Debug 构建通过并输出 `All RoadProto core tests passed.`；Debug ARX 构建通过，0 警告，0 错误；Release ARX 构建通过，0 警告，0 错误；Release 托管 Ribbon 构建通过，0 警告，0 错误。
- 新增 `IcdAlignmentFile`，支持积木法线形单元 `.icd` 文件读写，覆盖 `1` 直线、`2` 圆曲线、`3/4` 完整缓和曲线和 `5/6` 不完整缓和曲线。
- 新增命令 `RD_ALIGN_CENTERLINE_EXPORT_ICD` 和 `RD_ALIGN_CENTERLINE_IMPORT_ICD`，可选择道路中线实体导出 `.icd`，或从 `.icd` 导入为新的 `DnRoadCenterlineEntity`。
- 托管 Ribbon `平面设计` 面板新增 `导出中线 ICD`、`导入中线 ICD` 按钮。
- ICD `5/6` 导入复用元素链不完整缓和曲线能力；导入实体第一版支持显示、桩号、DWG 持久化和再次导出，暂不接夹点和 WPF 参数编辑。
- 修正 ICD 起点工程坐标与 CAD 坐标口径差异：导入时将 ICD `x/y` 转为 CAD `x = 工程 y`、`y = 工程 x`，并同步换算方位角；导出时执行反向转换。
- 核心测试新增五单元道路中线 ICD 往返、`5` 不完整缓和曲线导入和导入后再次导出类型保持用例。

## v0.1.8 documentation split follow-up - 2026-05-09

- 全局规则新增“一功能一业务文档”要求：独立功能必须在 `docs/business/<module>/` 下单独成文，模块总览只做索引和边界说明，命令元数据必须指向对应功能文档。
- 拆分平面设计业务文档：道路中线创建、属性编辑、平曲线参数编辑、WPF 桥接回写、夹点动态编辑、元素链与不完整缓和曲线、ICD 导入导出均已独立成文。
- 拆分地形数模编辑业务文档，并将 `DN_TERRAIN_TIN_EDIT` 与 `DN_TERRAIN_TIN_EDIT_HANDLE` 的业务文档路径指向 `docs/business/terrain/地形数模_编辑.md`。
- 收敛模块文档职责：`docs/modules/alignment.md` 与 `docs/modules/terrain.md` 只保留模块职责、命令清单、代码落点和功能文档索引，具体操作流程和业务规则迁回功能业务文档。
- 更新 `docs/modules/alignment.md`、`docs/modules/terrain.md`、`docs/modules/module_index.md`、README、复用说明、业务文档模板和开发规则，使业务文档索引与命令注册保持一致。
- 验证：业务文档结构检查通过，核心测试 Debug 构建通过并输出 `All RoadProto core tests passed.`；Debug ARX 构建通过，0 警告，0 错误；Release ARX 构建通过，0 警告，0 错误。

## v0.1.10 - 2026-05-12

- 版本标识：`v0.1.10_20260512_SubgradeTemplate`。
- ARX 文件：`RoadProto_v0.1.10_20260512_SubgradeTemplate.arx`。
- 新增横断面设计模块 `CROSS_SECTION`，在 Ribbon 中提供 `横断面设计` 面板和 `创建路基模板` 入口。
- 新增 `RD_SECTION_SUBGRADE_TEMPLATE_CREATE` 命令：运行后只要求用户点取图纸插入点，创建独立路基模板实体；暂不选择或绑定既有道路中线。
- 新增 `RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE` 与 `RD_SECTION_SUBGRADE_TEMPLATE_APPLY_DIALOG_FILE` 内部桥接命令，支持双击实体后重新打开 WPF 对话框编辑参数并回写图面实体。
- 新增 `SubgradeTemplateModel` 与 `SubgradeTemplateCreateService`，沉淀道路等级、左右侧部件、宽度/加宽表、高差、坡度模式、颜色、显示比例、路面结构层预留字段和默认值规则。
- 新增 `DnSubgradeTemplateEntity` 自定义实体，支持 DWG 持久化、图面绘制中线和左右侧横断面部件、实体变换、范围计算和后续关联字段预留。
- 新增 WPF 路基模板参数窗口、预览图、部件选择/新增/删除、左右切换按钮、宽度加宽表与变化坡度二级表格窗口。
- 新增业务文档 `docs/business/cross_section/路基模板_创建.md`、`路基模板_编辑.md`、`路基模板_WPF桥接回写.md`，新增模块文档 `docs/modules/cross_section.md`，新增复用说明 `docs/reuse/subgrade_template.md`。
- 更新 README、模块索引、复用能力目录、核心测试说明和构建版本信息，使主目录代码、文档与构建产物保持同步。
- 验证：核心测试 Debug 构建通过，核心测试可执行文件输出 `All RoadProto core tests passed.`；WPF Debug/Release 构建通过，0 警告，0 错误；ARX Debug/Release 构建通过，0 警告，0 错误。

## v0.1.10 subgrade template polish - 2026-05-13

- 修正创建路基模板参数窗口的默认体验，保持新建命令初始道路等级为高速公路。
- 为二级、三级、四级道路补充默认模板：左右对称，两条 `3.75` 行车道、`3` 硬路肩和 `0.75` 土路肩。
- WPF 预览区域启用裁剪，避免部件数量增多或标注变长时遮挡右侧参数 UI；预览中增加部件宽度和坡度显示。
- 坡度方式为变化值时，固定坡度输入置灰并不再作为规则兜底；二级窗口名称统一为“坡度变化数据表”。
- `DnSubgradeTemplateEntity` 图面部件标注改为中文，并继续通过实体持久化数据驱动双击编辑窗口，保证再次打开显示上次保存的配置。
- 核心测试新增二级/三级/四级道路默认组成、中文部件名、变化坡度规则和 WPF 源码可读性检查。
## 2026-05-13 开发更新

- 路基模板创建和编辑体验优化：空白新建请求默认高速公路，已有实体编辑请求保留实体已保存参数，双击编辑命令发送带换行的实体句柄。
- 为一级道路、二级道路、三级道路、四级道路、城市主干道、城市次干道和城市支路补充初始部件配置。
- WPF 预览图增加透明命中区域支持直接点选部件，左、右按钮改为按横断面几何顺序移动。
- 增加默认部件、WPF 源码行为和托管桥接请求往返读取的验证。

## 2026-05-13 路基模板双击编辑桥接修正

- 修正路基模板 C++ 桥接文件写入枚举字段时的重载解析问题：`roadGradeCode`、`subgradeSideCode`、`subgradeComponentTypeCode` 和 `subgradeSlopeModeCode` 返回的字符串指针现在按文本写入，不再被误选为布尔重载写成 `1`。
- 该问题会导致 WPF 双击编辑窗口把已有实体参数误解析为一级道路、右侧、行车道和变化坡度，出现预览与 DWG 实体不一致。
- 更新路基模板创建、编辑和 WPF 桥接业务文档，移除说明段落中的英文表述。
- 增加核心测试检查桥接写入重载，防止枚举文本再次被误写为布尔值。

## v0.1.11 - 2026-05-18

- 版本标识：`v0.1.11_20260518_RoadModel`。
- ARX 文件：`RoadProto_v0.1.11_20260518_RoadModel.arx`。
- 阶段：横断面戴帽道路模型。
- 是否可作为稳定测试版本：是。核心测试、托管桥接测试、WPF Debug 构建和 ARX Debug 构建已验证；完整 AutoCAD 图形界面的 Ribbon 点击、WPF 弹窗、双击编辑和 DWG 保存重开仍需人工点验。

### 新增内容

- 新增横断面戴帽道路模型领域能力：`RoadModelTemplateResolver`、`RoadModelStationSampler` 和 `RoadModelBuilder`，支持模板优先级、采样点收集、竖曲线高程和三维部件线生成。
- 新增 `RoadModelBuildService`，在 application 层组织道路模型配置、道路中线采样、竖曲线和路基模板来源。
- 新增 `DnRoadModelEntity` 自定义实体，支持 `RoadModelData` DWG 持久化、三维部件线绘制、几何范围和变换。
- 新增道路模型 WPF 窗口，提供 `路基模板` tab 表格，支持起终点桩号、模板 handle、模板名称和行优先级调整。
- 新增道路模型 C++ / WPF 请求响应桥接，覆盖 UTF-8、转义、`assignmentCount` 和 InvariantCulture 数值读写。
- 新增 `RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_EDIT`、`RD_SECTION_ROAD_MODEL_EDIT_HANDLE` 和 `RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE` 命令。
- 托管 Ribbon `横断面设计` 面板新增 `横断面戴帽` 和 `编辑道路模型` 按钮；双击 `DNROADMODELENTITY` 可进入同一编辑窗口。

### 修改内容

- 更新横断面设计模块文档、道路模型创建/编辑/WPF 桥接业务文档、复用能力目录、道路模型复用说明、README 和测试说明。
- 更新构建版本信息为 `v0.1.11_20260518_RoadModel`。
- 道路模型创建和回写会校验竖曲线所属拉坡图与当前道路中线一致，避免混用不同道路的平面和纵断面数据。
- ARX 初始化失败路径会清理已注册的自定义实体类，降低重复加载风险。

### 构建验证

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- 托管桥接测试：`tests/RoadProtoManagedBridgeTests` 运行通过。
- WPF：`RoadProto.Terrain.UI.csproj` Debug 构建通过。
- Debug ARX：`artifacts/x64/Debug/RoadProto_v0.1.11_20260518_RoadModel.arx` 构建通过。

### 已知问题

- 当前道路模型只生成三维部件线框，不生成道路面、结构层体积、材质或算量。
- 模板表第一版使用模板 handle 输入，后续应提供 CAD 选择或模板库选择器。
- 道路模型保存上游对象 handle，但尚未接入统一实体关系管理机制自动标脏或重建。
- Core Console 和自动测试无法覆盖完整 AutoCAD 图形界面的 Ribbon 点击、WPF 弹窗、双击编辑和 DWG 保存重开，需要人工验证。

## v0.1.12 - 2026-05-18

- 版本标识：`v0.1.12_20260518_RoadModelWpfFix`。
- ARX 文件：`RoadProto_v0.1.12_20260518_RoadModelWpfFix.arx`。
- 阶段：横断面戴帽道路模型 WPF 热修。
- 修正 `RoadModelWindow.xaml` 中道路中线 handle 只读文本框的绑定模式：显式使用 `Mode=OneWay`，避免 `TextBox.Text` 默认 TwoWay 绑定只读属性时在打开横断面戴帽窗口时抛出 WPF 异常。
- 托管桥接测试新增道路模型窗口源码契约，防止只读 handle 绑定再次退回 TwoWay。
- 验证：托管桥接测试通过；Release WPF 构建通过；Release ARX 构建通过；使用 `C:\Users\admin\Desktop\test\MY01.dwg` 在 AutoCAD 2022 中加载热修 ARX/DLL，并以 handle `243` 的道路中线执行 `RD_SECTION_ROAD_MODEL_CREATE`，已弹出 `横断面戴帽` WPF 窗口，未出现 AutoCAD 错误中断。

## v0.1.13 - 2026-05-19

- 版本标识：`v0.1.13_20260519_RoadModelPickGrip`。
- ARX 文件：`RoadProto_v0.1.13_20260519_RoadModelPickGrip.arx`。
- 阶段：横断面戴帽行内点选模板与路基模板夹点优化。
- 是否可作为稳定测试版本：是。托管桥接测试、核心测试、Release WPF 构建、Release ARX 构建和 `MY01.dwg` 测试副本生成模型均已验证。

### 新增内容

- 横断面戴帽 `路基模板` 表格新增行内 `点选` 按钮，用户可从 CAD 图中选择 `DnSubgradeTemplateEntity` 并回填当前行模板 handle 和模板名称。
- 道路模型 WPF 请求/响应 DTO 新增 `action`、`pickAssignmentIndex` 和 `selectedAssignmentIndex` 字段，用于保持表格状态并返回 CAD 选择模板。
- C++ ObjectARX 回写命令新增 `pickTemplate` 动作处理，点选模板后重新打开同一个横断面戴帽窗口继续编辑或生成。
- `DnSubgradeTemplateEntity` 新增插入点夹点，支持在 CAD 图中拖动移动模板实体位置。

### 修改内容

- 更新 README、横断面戴帽道路模型创建/编辑/WPF 桥接业务文档、路基模板编辑文档、横断面模块文档、复用能力目录、道路模型复用说明、路基模板复用说明和测试说明。
- 更新构建版本信息为 `v0.1.13_20260519_RoadModelPickGrip`。

### 构建验证

- 托管桥接测试：`dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj` 输出 `All RoadProto managed bridge tests passed.`。
- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- 行内点选模板验证：托管桥接测试覆盖 `action=pickTemplate`、`pickAssignmentIndex` 和 `selectedAssignmentIndex` 文件读写；核心测试覆盖 WPF 行内 `点选` 按钮、C++ `pickTemplate` 动作和 `selectTypedEntity<DnSubgradeTemplateEntity>` 选择入口。
- WPF：`RoadProto.Terrain.UI.csproj` Release 构建通过，0 警告，0 错误。
- Release ARX：`RoadProtoArx.vcxproj` Release 构建通过，生成 `artifacts/x64/Release/RoadProto_v0.1.13_20260519_RoadModelPickGrip.arx`，0 警告，0 错误。

### AutoCAD 图形验证

- 使用 `C:\Users\admin\Desktop\test\MY01.dwg` 的测试副本 `C:\Users\admin\Desktop\test\RoadProtoFeatureLoad\MY01_RoadModelPickGripTest.dwg` 验证，未修改原始 `MY01.dwg`。
- 实体探测结果：handle `243` 为 `DNROADCENTERLINEENTITY`，handle `289` 为 `DNPROFILEVERTICALCURVEENTITY`，handle `50C` 为 `DNSUBGRADETEMPLATEENTITY`。
- 生成模型验证：以道路中线 `243`、竖曲线 `289`、路基模板 `50C` 和桩号范围 `0` 到 `100000` 执行回写生成，测试副本中 `DNROADMODELENTITY` 数量从 `0` 增加到 `1`，新增道路模型 handle 为 `557`。

### 已知问题

- 当前道路模型仍只生成三维部件线框，不生成道路面、结构层体积、材质或算量。
- 行内点选模板当前使用临时文件 Bridge 和 AutoCAD 选择流程，后续正式产品化时仍应补充模板库选择和更细的行级错误定位。
- 路基模板夹点移动已完成 ObjectARX 实体能力和编译验证，自动化脚本未覆盖鼠标拖拽交互本身，仍建议后续人工点验一次 CAD 图形交互手感。

## v0.1.14 - 2026-05-19

- 版本标识：`v0.1.14_20260519_SlopeTemplateRoadModel`。
- ARX 文件：`RoadProto_v0.1.14_20260519_SlopeTemplateRoadModel.arx`。
- 阶段：边坡模板与横断面戴帽边坡线框。
- 是否可作为稳定测试版本：是。核心测试、托管桥接测试、WPF Debug/Release 构建和 ARX Debug/Release 构建已验证；完整 AutoCAD 图形界面的真实点选、双击编辑和边坡戴帽效果仍需人工点验。

### 新增内容

- 新增 `SlopeTemplateModel` 和 `SlopeTemplateCreateService`，支持填方/挖方初始预设、边坡/护坡道部件、坡率/坡高/宽度三选二约束、部件级搜索地面线范围坡高增值、`交地则结束放坡` 和 `重复最后一组参数直至交地`。
- 新增 `DnSlopeTemplateEntity` 自定义实体，支持边坡模板 DWG 持久化、线框显示、几何范围、变换和 WPF 双击编辑入口。
- 新增 `RD_SECTION_SLOPE_TEMPLATE_CREATE`、`RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE` 和 `RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE` 命令。
- 新增 WPF `SlopeTemplateWindow`，左侧实时线框预览，右侧编辑模板名称、显示比例、模板类型、控制条件和部件参数。
- 横断面戴帽 WPF 窗口新增 `边坡模板` tab，并按左侧/右侧二级 tab 独立维护搜索宽度、模板组和组内多模板优先级。
- 道路模型构建新增边坡模板组解析、TIN 横断面剖切、从路基模板最外侧放坡和三维边坡线生成。
- `DnRoadModelEntity` 持久化版本升级，保存并绘制边坡三维线框。
- AutoCAD 可见 Ribbon 新增 `创建边坡模板` 按钮，并支持双击 `DNSLOPETEMPLATEENTITY` 转发到边坡模板编辑命令。

### 修改内容

- 更新 README 当前版本、加载路径、横断面入口说明和测试覆盖说明。
- 更新横断面设计模块文档、模块索引、边坡模板业务文档、横断面戴帽边坡模板业务文档、道路模型创建/编辑/WPF 桥接业务文档、复用能力目录、道路模型复用说明、边坡模板复用说明和测试说明。
- 更新构建版本信息为 `v0.1.14_20260519_SlopeTemplateRoadModel`。

### 构建验证

- 托管桥接测试：`dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj` 输出 `All RoadProto managed bridge tests passed.`。
- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- WPF Debug：`RoadProto.Terrain.UI.csproj` Debug 构建通过，生成 `artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll`，0 警告，0 错误。
- WPF Release：`RoadProto.Terrain.UI.csproj` Release 构建通过，生成 `artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll`，0 警告，0 错误。
- ARX Debug：`RoadProtoArx.vcxproj` Debug 构建通过，生成 `artifacts/x64/Debug/RoadProto_v0.1.14_20260519_SlopeTemplateRoadModel.arx`，0 警告，0 错误。
- ARX Release：`RoadProtoArx.vcxproj` Release 构建通过，生成 `artifacts/x64/Release/RoadProto_v0.1.14_20260519_SlopeTemplateRoadModel.arx`，0 警告，0 错误。

### 已知问题

- 当前边坡模型第一版本只生成三维线框，不生成边坡面、结构层体积、材质或算量。
- 当前 TIN 横断面剖切先以准确性为主，尚未引入空间索引；如果大数据量测试明显卡顿，再优化候选三角形搜索和断面缓存。
- 自动化测试覆盖核心规则、桥接文件、编译和源码契约；AutoCAD 图形界面的真实鼠标点选、双击和 DWG 保存重开仍建议人工验证。

## v0.1.15 - 2026-05-19

- 版本标识：`v0.1.15_20260519_RoadModelProgressGroupUi`。
- ARX 文件：`RoadProto_v0.1.15_20260519_RoadModelProgressGroupUi.arx`。
- 阶段：横断面戴帽生成进度与边坡模板组管理 UI。
- 是否可作为稳定测试版本：是。核心测试、托管桥接测试、WPF Debug/Release 构建和 ARX Debug/Release 构建均需要通过；完整 AutoCAD 图形界面仍建议人工点验。

### 新增内容

- `RoadModelBuildInput` 新增 `progressCallback`，由领域层在校验、竖曲线重建、桩号采样、断面生成、线框整理和完成阶段上报进度。
- `RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE` 在生成道路模型时接入 AutoCAD 状态栏进度条，长时间生成时不再无反馈。
- WPF `横断面戴帽 / 边坡模板` tab 的左侧、右侧模板组表新增行内 `管理模板组` 按钮。
- WPF 增加当前模板组管理区，支持组内模板点选、删除、上移、下移和清空。
- HTML 原型 `docs/prototypes/slope_template_ui_prototype.html` 增加生成模型进度条示意，并强化 `管理模板组` 按钮视觉层级。

### 修改内容

- 更新 README、横断面戴帽业务文档、道路模型 WPF 桥接文档、横断面模块说明、复用能力目录和测试说明。
- 更新构建版本信息为 `v0.1.15_20260519_RoadModelProgressGroupUi`。

### 构建验证

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- 托管桥接测试：`dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj` 输出 `All RoadProto managed bridge tests passed.`。
- WPF Debug：`RoadProto.Terrain.UI.csproj` Debug 构建通过，生成 `artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll`，0 警告，0 错误。
- WPF Release：`RoadProto.Terrain.UI.csproj` Release 构建通过，生成 `artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll`，0 警告，0 错误。
- ARX Debug：`RoadProtoArx.vcxproj` Debug 构建通过，生成 `artifacts/x64/Debug/RoadProto_v0.1.15_20260519_RoadModelProgressGroupUi.arx`，0 警告，0 错误。
- ARX Release：`RoadProtoArx.vcxproj` Release 构建通过，生成 `artifacts/x64/Release/RoadProto_v0.1.15_20260519_RoadModelProgressGroupUi.arx`，0 警告，0 错误。

### 已知问题

- 当前进度条按领域层构建阶段和断面区间推进，暂不显示精确 TIN 三角形搜索耗时明细。
- 模板组内新增模板仍通过 CAD 图中点选 `DnSlopeTemplateEntity` 完成，后续正式产品化时仍需模板库选择器。

## v0.1.16 - 2026-05-19

- 版本标识：`v0.1.16_20260519_RoadModelSectionViewer`。
- ARX 文件：`RoadProto_v0.1.16_20260519_RoadModelSectionViewer.arx`。
- 阶段：道路模型横断面查看。
- 是否可作为稳定测试版本：是。核心测试、托管桥接测试、WPF Debug/Release 构建和 ARX Debug/Release 构建均需要通过；完整 AutoCAD 图形界面仍建议人工点验。

### 新增内容

- 新增 `RD_SECTION_ROAD_MODEL_VIEW_SECTION` 命令和 Ribbon `查看横断面` 入口，选择 `DnRoadModelEntity` 后打开 WPF 只读横断面预览窗口。
- 新增 `RoadModelSectionPreviewBuilder`，按桩号把已生成道路模型的三维路基线、边坡线和可选 TIN 地面线转换为二维偏距-高程预览线段。
- 新增 `RoadModelSectionViewerBridge`，由 ObjectARX 写出查看横断面请求文件并调起托管 WPF 弹窗命令。
- 新增 WPF `RoadModelSectionViewerWindow`，包含桩号列表、预览画布、状态文字和路基模板/边坡模板/地面线图例。
- `DnRoadModelEntity` 持久化数据新增采样桩号，旧版道路模型没有采样桩号时仍按保存配置尝试回退采样。

### 修改内容

- 更新 README 当前版本、ARX 加载路径、横断面入口说明和测试覆盖说明。
- 更新查看横断面业务文档、横断面戴帽道路模型创建/编辑/WPF 桥接业务文档、横断面模块说明、模块索引、道路模型复用说明、复用能力目录和测试说明。
- 更新构建版本信息为 `v0.1.16_20260519_RoadModelSectionViewer`。

### 构建验证

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- 托管桥接测试：`dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj` 输出 `All RoadProto managed bridge tests passed.`。
- WPF Debug：`RoadProto.Terrain.UI.csproj` Debug 构建通过，生成 `artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll`，0 警告，0 错误。
- WPF Release：`RoadProto.Terrain.UI.csproj` Release 构建通过，生成 `artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll`，0 警告，0 错误。
- ARX Debug：`RoadProtoArx.vcxproj` Debug 构建通过，生成 `artifacts/x64/Debug/RoadProto_v0.1.16_20260519_RoadModelSectionViewer.arx`，0 警告，0 错误。
- ARX Release：`RoadProtoArx.vcxproj` Release 构建通过，生成 `artifacts/x64/Release/RoadProto_v0.1.16_20260519_RoadModelSectionViewer.arx`，0 警告，0 错误。

### 已知问题

- 查看横断面第一版是只读检查窗口，不重新执行模板优先级判断或道路模型戴帽生成。
- 预览地面线依赖道路模型能追溯到竖曲线所属拉坡图和关联 TIN；没有可用 TIN 时只显示路基和边坡线。
- 自动化测试覆盖核心预览规则、桥接文件、编译和源码契约；AutoCAD 图形界面的真实 Ribbon 点击、实体选择和窗口视觉效果仍建议人工验证。

## v0.1.17 - 2026-05-19

- 版本标识：`v0.1.17_20260519_RoadModelMeshWireframe`。
- ARX 文件：`RoadProto_v0.1.17_20260519_RoadModelMeshWireframe.arx`。
- 阶段：道路模型三维网格线框重定义。
- 是否可作为稳定测试版本：是。核心测试、托管桥接测试、WPF Debug/Release 构建和 ARX Debug/Release 构建均需要通过；完整 AutoCAD 图形界面仍建议人工点验。

### 新增内容

- `RoadModelData` 新增采样断面节点链 `sections` 和三维网格线框 `wireLines`，用于表达每个采样桩号的路基边界、边坡边界和交地点。
- 新增 `RoadModelSectionNodeKind`、`RoadModelWireLineKind`、`RoadModelSectionNode`、`RoadModelSection` 和 `RoadModelWireLine` 领域数据结构。
- 道路模型线框新增横断面肋线、纵向连接线、最外侧边界线、起终点端部封闭线和节点数量不一致时的过渡线。
- `DnRoadModelEntity` 持久化版本升级到 4，支持保存和读取断面节点链与网格线框。

### 修改内容

- `RoadModelBuilder` 在生成路基和边坡部件线的同时构造断面节点链，并基于相邻断面生成三维网格线框。
- `RoadModelBuilder` 按连续有效模板区间分段生成网格线框，避免跨无模板空档生成纵向连接线；连续模板切换处仍可生成过渡线。
- `RoadModelBuilder` 调整断面节点数量不一致时的过渡线规则：公共路基节点按对应位置纵向连接，过渡线只作用于公共外缘后的变化尾部，避免边坡颜色和边坡线进入路基模板内部。
- `DnRoadModelEntity` 绘制逻辑优先使用 `wireLines`；旧数据没有 `wireLines` 时继续回退绘制原 `componentLines` 和 `slopeLines`。
- `DnRoadModelEntity` 的几何范围、变换和持久化校验同步覆盖断面节点链与网格线框。
- `RoadModelSectionPreviewBuilder` 优先使用已保存断面节点链生成横断面预览，减少从三维线反推断面的误差。
- 路基/道路主体颜色保持现有模板部件颜色；边坡线颜色来自边坡模板部件自带颜色。
- 更新 README 当前版本、ARX 加载路径、横断面戴帽业务文档、道路模型编辑文档、查看横断面文档、横断面模块说明、模块索引、道路模型复用说明、复用能力目录和测试说明。

### 构建验证

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- 托管桥接测试：`dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj` 输出 `All RoadProto managed bridge tests passed.`。
- WPF Debug：`RoadProto.Terrain.UI.csproj` Debug 构建通过，生成 `artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll`，3 个既有 AutoCAD 托管引用架构警告，0 错误。
- WPF Release：`RoadProto.Terrain.UI.csproj` Release 构建通过，生成 `artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll`，3 个既有 AutoCAD 托管引用架构警告，0 错误。
- ARX Debug：`RoadProtoArx.vcxproj` Debug 构建通过，生成 `artifacts/x64/Debug/RoadProto_v0.1.17_20260519_RoadModelMeshWireframe.arx`，0 警告，0 错误。
- ARX Release：`RoadProtoArx.vcxproj` Release 构建通过，生成 `artifacts/x64/Release/RoadProto_v0.1.17_20260519_RoadModelMeshWireframe.arx`，0 警告，0 错误。

### 已知问题

- 当前仍生成三维线框，不生成道路实体面、材质、结构层体积或算量结果。
- 节点数量不一致时的过渡规则仍是线框拓扑近似，后续如需更贴近正式三维建模，可扩展为真实三角面拓扑和可见边控制。
- AutoCAD 图形界面的真实 Ribbon 点击、实体选择、线框视觉效果和大图纸性能仍建议人工验证。

## v0.1.18 - 2026-05-20

- 版本标识：`v0.1.18_20260520_RoadModelSectionSnapshot`。
- ARX 文件：`RoadProto_v0.1.18_20260520_RoadModelSectionSnapshot.arx`。
- 阶段：道路模型地面快照与横断面查看性能优化。
- 是否可作为稳定测试版本：是。核心测试、ARX Debug 构建和 WPF Debug 构建已验证；完整 AutoCAD 图形界面和大 TIN 性能仍建议人工点验。

### 新增内容

- `RoadModelSection` 新增左右侧地面剖面快照，生成道路模型时随断面节点链一起写入 `RoadModelData`。
- `DnRoadModelEntity` 持久化版本升级到 5，支持保存和读取断面地面剖面快照；版本 4 及更早数据仍可读取，只是没有快照。
- `RoadModelBuilder` 生成期新增 `TerrainTriangleSpatialIndex` TIN 三角形网格候选索引，横断面剖切时按搜索线段穿越网格筛选候选三角形，再沿用原有精确交点计算。

### 修改内容

- 边坡放坡不再在同一采样断面左右侧重复剖切 TIN；先生成断面地面剖面，再供边坡模板交地和道路模型快照共同使用。
- `RoadModelSectionPreviewBuilder` 优先使用道路模型实体内保存的地面快照构建查看横断面地面线；旧模型没有快照时才回退临时读取 TIN。
- `RD_SECTION_ROAD_MODEL_VIEW_SECTION` 查看横断面命令在所有目标桩号都有可用地面快照时不再读取 TIN，并将道路模型数据、道路中线采样和地形数据只复制到预览请求一次。
- 更新 README、横断面戴帽道路模型创建文档、边坡模板文档、查看横断面文档、道路模型复用说明、复用能力目录和测试说明。

### 构建验证

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，`RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`。
- 托管桥接测试：`dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj` 输出 `All RoadProto managed bridge tests passed.`。
- ARX / WPF Debug：`RoadProto.sln` Debug 构建通过，生成 `artifacts/x64/Debug/RoadProto_v0.1.18_20260520_RoadModelSectionSnapshot.arx` 和 `artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll`，0 警告，0 错误。
- ARX / WPF Release：`RoadProto.sln` Release 构建通过，生成 `artifacts/x64/Release/RoadProto_v0.1.18_20260520_RoadModelSectionSnapshot.arx` 和 `artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll`，0 警告，0 错误。
- Core Console 性能复测：使用 `C:\Users\admin\Desktop\test\MY01.dwg` 测试副本，Release ARX 生成带边坡道路模型；基准旧实现 full 为 38.619s、扣除 cancel 后约 32.522s，本次优化后 cancel 为 6.089s、full 为 6.280s、扣除启动加载后约 0.191s。诊断输出显示索引启用，网格 `174x256`，TIN 三角形 `1600847`，索引引用 `2136124`，410 次地面剖切平均候选约 `997`、最大 `2068`。

### 已知问题

- 地面剖面快照表达的是道路模型生成时的 TIN 状态；TIN、中线、竖曲线或模板变化后，需要重新生成道路模型以刷新快照。
- TIN 网格索引是生成过程内的临时索引，不写入 DWG，也不作为长期内存缓存；查看横断面主要依赖实体内快照提速。
- 当前优化仅在 AutoCAD 命令行输出 TIN 索引和候选数量诊断，未加入正式耗时统计 UI；大图纸下如需进一步定位，可后续记录 TIN 索引构建、剖切求交和 WPF 请求文件大小。

## v0.1.19 - 2026-05-20

- 版本标识：`v0.1.19_20260520_PavementLayerTemplate`。
- ARX 文件：`RoadProto_v0.1.19_20260520_PavementLayerTemplate.arx`。
- 阶段：完整路面结构层模板工作流。
- 是否可作为稳定测试版本：是。核心测试 Debug/Release、托管 bridge 测试、WPF Release 构建和 ARX Release 构建已验证；完整 AutoCAD 图形界面的 Ribbon 点击、`.rpavement.xml` 文件对话框、双击编辑、路基模板绑定和道路模型线框效果仍建议人工点验。

### 新增内容

- 新增完整路面结构层模板文档闭环：创建、编辑、WPF 桥接回写三份独立业务文档。
- 新增复用说明 `docs/reuse/pavement_layer_template.md`，沉淀结构层类型、等厚/非等厚、内外侧语义、`.rpavement.xml` 和道路模型结构层边界线/弱化填充面复用边界。
- 路基模板文档更新为“部件绑定独立路面结构层模板实体”，所有部件类型均允许点选 `DnPavementLayerTemplateEntity`。
- 横断面道路模型文档更新为读取绑定的路面结构层模板，并生成结构层三维边界线和弱化填充面显示。
- 查看横断面文档更新为显示 `结构层`，与路基模板线、边坡模板线和地面线同屏预览。

### 修改内容

- 更新 `README.md` 当前版本、ARX 加载路径、横断面入口、命令清单、路面结构层模板 `.rpavement.xml` 后缀和测试覆盖说明。
- 更新 `docs/modules/cross_section.md`，补充路面结构层模板 Ribbon 入口、代码落点、WPF 窗口、绑定关系和 V0.1.19 更新记录。
- 更新 `docs/reuse/road_model.md` 和 `docs/reuse/capability_catalog.md`，补充道路模型读取结构层模板、生成 `PavementLayer` 线框和查看横断面结构层预览。
- 更新 `tests/README.md`，补充路面结构层模板核心测试、托管 bridge 测试和 AutoCAD 手工验证范围。
- 更新构建版本信息为 `v0.1.19_20260520_PavementLayerTemplate`。
- 更新 `tests/core_tests.cpp` 文档/版本 source-contract，检查新版本元数据、复用文档、README 和版本记录。
- 2026-05-21 补充修复路面结构层模板窗口预览：初始居中、滚轮缩放以鼠标位置为基点；加宽和坡度编辑改为默认内外侧一致，取消勾选后分侧配置。
- 2026-05-21 补充修复路面结构层几何：加宽只表达当前层相对上一层扩宽，内外侧坡度参与对应侧边界高程计算。
- 2026-05-22 补充修复路面结构层几何表达：加宽先作用于当前层顶边，让当前层相对上一层底边直接变宽；内外侧坡度只作用于当前层自身侧边，底边按厚度和坡度向内收；同步 WPF 预览、CAD 模板实体和道路模型结构层显示。
- 2026-05-22 补充修复道路模型结构层拓扑：道路模型改为与路面结构层模板预览使用同一组四边形/梯形轮廓，当前层顶边、外侧边、底边和内侧边共同表达当前层，避免模型、模板实体和 WPF 预览样式分叉。
- 2026-05-22 补充修复结构层显示样式：`PavementLayerTemplateRules` 统一提供与 WPF 预览一致的按层号循环配色；`DnPavementLayerTemplateEntity` 改用真彩色和半透明填充绘制，`RoadModelBuilder` 生成的结构层节点、纵向线和三维线框改用同一套结构层配色，不再继承路基部件颜色。
- 2026-05-22 补充修复结构层坡度口径：每一层改为顶边、底边、内侧边、外侧边四边形表达；加宽先确定当前层顶边，坡度再让当前层侧边向内收并决定底边宽度，WPF 预览、CAD 模板实体、道路模型和查看横断面统一使用四边形/梯形轮廓，不再沿用六边形过渡表达。
- 2026-05-22 补充修复结构层预览共用边显示：WPF 预览和 `DnPavementLayerTemplateEntity` 改为先绘制全部填充、再绘制边线；相邻层边界共线重叠时沿同一几何线表达，避免左右厚度不一致时斜向边界重复描边造成层间相交的视觉误判。
- 2026-05-22 补充修复结构层左右非等厚后的层间连续性：除第一层外，当前层顶边改为以上一层底边所在直线为基准；加宽沿该直线平行/共线延长或收回，坡度再作用于顶边到底边的侧边，避免某一层左右厚度不同后下方各层看起来错位或相交。
- 2026-05-22 补充新增结构层每层 RGB 颜色：颜色从 `PavementLayerTemplateLayer` 进入领域数据、WPF 请求/响应、`.rpavement.xml`、DWG 模板实体持久化和道路模型结构层边界线/填充面；WPF 预览、`DnPavementLayerTemplateEntity`、道路模型和查看横断面统一使用层保存色，默认层号色仅作为新建和旧数据补齐。
- 2026-05-22 补充修复 DWG 模板实体显示差异：`DnPavementLayerTemplateEntity` 改为预览式弱化填充色、层保存 RGB 边线和层名/厚度/加宽/坡度文字标注，减少 AutoCAD 透明显示开关和旧高亮层号色造成的视觉差异。
- 2026-05-22 重新评估并修复 DWG 模板实体形状显示差异：确认 WPF 使用 2D Canvas 四边形预览，`DnPavementLayerTemplateEntity` 的横向线填充无法与对话框预览保持同一视觉样式；实体改为按同一套四点轮廓绘制预混合弱化色 `polygon` 填充、层色边线和支持中文的标注。
- 2026-05-22 补充修复道路模型结构层显示差异：`DnRoadModelEntity` 改为优先从连续 `pavementLayerLines` 组合四点结构层轮廓，按同一套预混合弱化色绘制顶面、底面、内外侧面和端面 `polygon` 填充，再叠加层 RGB 线框；没有结构层边界线的旧数据才回退到采样断面节点。

### 验证状态

- 核心测试：`RoadProtoCoreTests.vcxproj` Debug 构建通过，Debug `RoadProtoCoreTests.exe` 输出 `All RoadProto core tests passed.`；Release `RoadProtoCoreTests.exe` 同样通过。
- 文档/版本契约：核心测试检查 `RoadProto.Build.props` 中 `RoadProtoVersion=v0.1.19`、`RoadProtoBuildDate=20260520`、`RoadProtoStage=PavementLayerTemplate`，并检查 `docs/reuse/pavement_layer_template.md`、README 和版本记录包含路面结构层模板发布信息。
- 托管 bridge 测试：`dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug` 通过，覆盖 `.rpavement.xml` 往返、非法 XML 拒绝、WPF `SaveXml` / `ImportXml` 和托管命令注册。
- 构建验证：`dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release` 通过；`RoadProto.sln /p:Configuration=Release /p:Platform=x64` 通过，生成 `artifacts\x64\Release\RoadProto_v0.1.19_20260520_PavementLayerTemplate.arx` 和 `artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll`。
- 2026-05-21 回归验证：核心测试 Debug/Release 构建和运行通过；托管 bridge 测试通过；WPF Release 构建通过；`RoadProto.sln` Release 构建通过，0 警告，0 错误。
- 2026-05-22 回归验证：核心测试 Debug/Release 构建和运行通过；托管 bridge 测试通过；WPF Release 构建通过；`RoadProto.sln` Release 标准输出构建通过，生成 `artifacts\x64\Release\RoadProto_v0.1.19_20260520_PavementLayerTemplate.arx` 和 `artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll`。验证过程中曾遇到 AutoCAD 残留进程占用旧 ARX，进程释放后已重新生成标准产物。
- 2026-05-22 结构层线框拓扑和显示样式回归验证：新增核心测试覆盖“第二层加宽的模型轮廓必须与模板预览四边形/梯形一致”以及“道路模型结构层配色必须与模板预览一致”，Debug/Release 核心测试通过；托管 bridge 测试、WPF Release 构建和 `RoadProto.sln` Release 构建通过，0 警告，0 错误。
- 2026-05-22 结构层坡度四边形回归验证：新增核心测试和托管 bridge 源码契约，覆盖当前层顶边先按加宽确定、侧边按 `1:n` 坡度向内收、底边按坡度形成梯形，且 WPF 预览、CAD 模板实体、道路模型和查看横断面共用同一四边形轮廓规则。
- 2026-05-22 结构层坡度和加宽口径二次修正：坡度 `1:n` 改为按每 1 个竖向厚度产生 `n` 个水平位移，`1:1` 侧边与底边约 45°、`1:2` 约 30°、`1:0.5` 约 60°；加宽允许负值，用于表达当前层顶边相对上一层底边缩短。
- 2026-05-22 结构层坡度和负加宽回归验证：新增核心测试覆盖负加宽和负坡度；新增托管 bridge 测试覆盖 `.rpavement.xml` 负加宽往返、WPF 预览允许负 `1:n` 和负加宽输入。Debug/Release 核心测试通过；托管 bridge 测试通过；WPF Release 构建和 `RoadProto.sln` Release 构建通过，0 警告，0 错误。
- 2026-05-22 结构层坡度符号修正回归验证：明确顶边为图形上方边、底边为图形下方边；坡度 `1:n` 的正值表示从顶边向下到底边时侧边向外放，负值表示向内收。新增/调整核心测试和托管 bridge 源码契约后，Debug/Release 核心测试通过；托管 bridge 测试通过；WPF Release 构建和 `RoadProto.sln` Release 构建通过，0 警告，0 错误。
- 2026-05-22 结构层共用边显示回归验证：新增核心源码契约覆盖 `DnPavementLayerTemplateEntity` 使用单独边线绘制和首层/非重合顶边条件，新增托管 bridge 源码契约覆盖 WPF 预览 `drawTopEdge` 策略；Debug 核心测试和托管 bridge 测试通过。
- 2026-05-22 结构层层间连续性回归验证：新增核心测试覆盖左右非等厚后相邻层边界必须共线连续，并同步更新道路模型结构层节点为顶边/底边/内侧边/外侧边四点表达；Debug 核心测试和托管 bridge 测试通过。
- 2026-05-22 结构层加宽四边形修正：加宽后的边沿上一层底边所在直线平行/共线延长或收回，当前层始终保持四边形/梯形；移除 WPF 预览、DWG 模板实体和道路模型中的六点台阶轮廓，新增核心回归测试覆盖斜顶边加宽外推。
- 2026-05-22 结构层每层 RGB 回归验证：新增核心测试覆盖默认层色、自定义层色进入 `PavementLayerTemplateSection` 和道路模型结构层边界线；新增托管 bridge 测试覆盖请求/响应、`.rpavement.xml` 和 WPF 预览读取层 RGB；Debug/Release 核心测试通过，托管 bridge 测试通过，WPF Release 构建和 `RoadProto.sln` Release 构建通过，0 警告，0 错误。
- 2026-05-22 DWG 模板实体显示一致性回归验证：新增核心源码契约覆盖 `DnPavementLayerTemplateEntity` 的预览式填充色、层 RGB 边线和层名/厚度/加宽/坡度文字标注；Debug/Release 核心测试通过，托管 bridge 测试通过，WPF Release 构建通过，Debug ARX 和 `RoadProto.sln` Release 构建通过，0 警告，0 错误。
- 2026-05-22 DWG 模板实体形状一致性回归验证：新增核心源码契约要求 `DnPavementLayerTemplateEntity` 使用四点 `worldDraw->geometry().polygon(4, fillPoints)`、预混合弱化填充色、层 RGB 边线和 `SimSun` 简体中文文字样式；契约先失败于横向线填充实现，修复后 Debug 核心测试通过并完成 Debug ARX 构建。
- 2026-05-22 道路模型结构层显示一致性回归验证：新增核心源码契约要求 `DnRoadModelEntity` 在 `wireLines` 前调用 `drawPavementLayerFacesFromLines`，使用四点 `worldDraw->geometry().polygon(4, facePoints)`、预混合弱化填充色和层 RGB 线框；契约先失败于只绘制线框的实现，修复后 Debug/Release 核心测试通过，托管 bridge 测试通过，WPF Release 构建、Debug ARX 构建和 Release ARX 构建通过，0 警告，0 错误。
- 图形界面验证：已在 AutoCAD 2021 图形界面加载 Debug ARX，使用脚本创建不等厚模板（第二层内侧厚 0.35、外侧厚 0.9，内外坡度 1，含加宽和每层 RGB），截图确认模型空间自定义实体为四边形/梯形整块弱化填充、层色边线和中文标注，不再显示为横向线填充或问号文字。

### 已知问题

- 路面结构层模板修改后，当前不会自动标记已引用它的路基模板或道路模型需要重建，需要用户重新生成道路模型。
- 当前道路模型结构层弱化填充面属于 `DnRoadModelEntity` 的显示表达，不是可单独选择、赋材质、编辑或算量的实体体积。
- `.rpavement.xml` 当前是 WPF 模板参数流转格式，不包含正式材料库和规范参数。
