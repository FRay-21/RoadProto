# RoadProto V0.1 框架

RoadProto 是一个面向 ObjectARX/C++ 的道路设计原型功能框架，用于后续持续开发类似 EICAD 的道路设计原型能力。V0.1 的重点是建立可维护的分层结构、统一命令/模块注册、业务文档沉淀机制，以及可扩展的实体关系与联动更新基础机制。本阶段不实现复杂道路设计算法。

AI 或 Codex 进入项目时，应先阅读根目录 `AGENTS.md`。`README.md` 负责说明项目，`AGENTS.md` 负责规定每次开发前必须读取哪些规则文档。

核心架构原则见 `AGENTS.md` 和 `docs/architecture/overview.md`：RoadProto 采用“C++ ObjectARX 核心 + 可替换 UI 层”。C++ ObjectARX 负责 CAD 核心能力、几何算法、自定义实体和 DWG 持久化；WPF 只负责界面展示与用户交互，并通过 Bridge / Adapter 调用核心能力。地形 TIN 当前代码结构见 `docs/architecture/terrain_tin_code_structure.md`。

## 目标环境

- C++17
- AutoCAD 2021 x64
- ObjectARX 2021
- 本机优先使用 Visual Studio 2026 Insiders：`D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE`
- 命令行构建优先使用 VS2026 MSBuild：`D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe`
- 仍需保持 AutoCAD 2021 / ObjectARX 2021 兼容；项目平台工具集按当前工程配置执行，不随意升级 CAD 版本
- .NET Framework 4.8，用于 AutoCAD 可见 Ribbon 托管插件
- 运行形式：ARX 插件

## 当前版本

- 版本：`v0.1.31`
- 构建日期：`20260527`
- 阶段：`SectionDrawingConfig`
- ARX 文件名：`RoadProto_v0.1.31_20260527_SectionDrawingConfig.arx`
- 托管 Ribbon 插件：`RoadProto.Terrain.UI.dll`
- 输出目录：
  - `artifacts/x64/Debug/`
  - `artifacts/x64/Release/`
  - `artifacts/managed/Debug/net48/`
  - `artifacts/managed/Release/net48/`

## 分层说明

- `src/app`：ARX 入口、启动、加载、卸载。
- `src/core`：命令注册、模块注册、日志、配置、错误、版本信息。
- `src/cad_adapter`：ObjectARX / AutoCAD API 适配层。
- `src/domain`：道路设计实体、关系模型、业务概念，尽量不依赖 ObjectARX。
- `src/application`：具体功能流程和 use case。
- `src/modules`：业务模块，负责注册命令和 Ribbon 分组。
- `src/ui`：Ribbon 模型、对话框、资源和 AutoCAD 托管 Ribbon 插件。
- `docs`：架构说明、业务文档、复用说明、版本记录、开发规则。

核心原则：

```text
命令 -> 应用服务 -> 领域层 / CAD 适配层
UI   -> 只负责参数收集和展示
领域 -> 沉淀业务规则，不依赖 ObjectARX
文档 -> 沉淀业务规则和复用能力
```

## V0.1 示例模块

- 地形数模历史联动示例：命令 `RD_TERRAIN_MARKDIRTY`
  - 示例演示地形数模更新后，将纵断面地面线和横断面地面线标记为 `NeedsRebuild`；当前仅作为关系联动框架示例保留，不代表 `DnTerrainTinEntity` 已实现源对象 reactor 自动重建。
  - 业务文档：`docs/business/terrain/地形更新联动示例.md`
- 地形构网命令：命令 `DN_TERRAIN_TIN_CREATE`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`数模` 面板、`地形构网` 按钮触发。
  - ARX 加载后会自动尝试加载同配置目录下的 `RoadProto.Terrain.UI.dll` 以创建可见 Ribbon；如果未出现，可手动 `NETLOAD` 托管插件。
  - 用户可连续点选多个样例对象；每次点选后，命令自动提取模型空间中同图层、同图元类型的数据。
  - 提取时显示 AutoCAD 状态栏进度条，提取完成后临时隐藏当前同类源对象；连续提取完成后按 Enter 打开参数窗口。
  - 从 CAD 点、线、多段线、文字和块中提取地形点，清洗后构建 TIN，并生成 `DnTerrainTinEntity` 自定义实体。
  - 参数窗口展示提取统计，开放 XY 合并容差、最大三角边长、退化三角形过滤和显示模式。
  - `生成地形数模` 会按当前参数重新生成预览实体并显示窗口进度；`预览` 可临时回到 DWG 查看，按 ESC 返回；`确认` 保留结果，`取消` 删除预览并恢复源对象。
  - 三角网显示采用 TrueColor 连续渐变边线高程着色，不再填充三角面。
  - Delaunay 三角剖分使用 `third_party/delaunator-cpp`，保留 MIT License。
  - 业务文档：`docs/business/terrain/地形构网_TIN地形数模.md`
- 地形数模编辑命令：命令 `DN_TERRAIN_TIN_EDIT`
  - 可通过 Ribbon 的 `编辑数模` 按钮或双击 `DnTerrainTinEntity` 触发。
  - 支持修改显示模式、最大三角边长、退化三角形过滤，并可使用实体内保存的点重新生成 TIN。
  - 业务文档：`docs/business/terrain/地形数模_编辑.md`
- 平面布线命令：命令 `RD_ALIGN_CENTERLINE_CREATE`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`平面设计` 面板、`平面布线` 按钮触发。
  - 用户依次点取起点、交点和第三点后，先生成可见 `DnRoadCenterlineEntity` 道路中线自定义实体；继续点取控制点时实体会实时刷新预览。
  - 道路中线按直线、第一缓和曲线、圆曲线、第二缓和曲线、直线五单元组合；交点按前后直线延长线交点参与主点计算，缓和曲线采用回旋线公式 `rho * l = A^2`、`A^2 = R * Ls`。
  - 实体显示中直线段为青色，第一缓和曲线为红色，圆曲线为黄色，第二缓和曲线为洋红色。
  - 默认道路名称按 `K1`、`K2`、`K3` 递增；起点桩号为 `K0+000`，默认每 100m 绘制桩号标注。
  - 道路中线夹点拖动时通过 ObjectARX 动态预览跟随鼠标，释放后正式重建写回实体。
  - 创建完成后打开 WPF 参数窗口，可编辑道路名称、道路等级、数模关联和桩号标注间距；双击道路中线也会打开同一 WPF 窗口。
  - 领域层已新增元素链表达，可生成圆曲线、不完整缓和曲线、圆曲线的连续内部线形，用于后续 ICD `5/6`、卵形曲线、S 型曲线和 C 型曲线扩展；当前只支持内部计算、采样、显示和 DWG 保存，不提供交互编辑按钮。
  - 业务文档：`docs/business/alignment/平面布线_道路中线创建.md`
  - 相关功能文档：`docs/business/alignment/道路中线_属性编辑.md`、`docs/business/alignment/道路中线_夹点动态编辑.md`、`docs/business/alignment/平面线形_元素链与不完整缓和曲线.md`
- 道路中线 ICD 导入导出命令：命令 `RD_ALIGN_CENTERLINE_EXPORT_ICD`、`RD_ALIGN_CENTERLINE_IMPORT_ICD`
  - 可通过 Ribbon `平面设计` 面板下的 `导出中线 ICD`、`导入中线 ICD` 按钮触发。
  - 导出选中的 `DnRoadCenterlineEntity` 为积木法线形单元 `.icd` 文件，支持直线、圆曲线、完整缓和曲线和不完整缓和曲线单元。
  - ICD 起点坐标采用工程坐标 `X/Y`，与 CAD `x/y` 相反；导入时转换为 CAD 坐标和 CAD 方位角，导出时反向写回工程坐标。
  - 导入 `.icd` 文件后创建新的道路中线自定义实体；`5/6` 不完整缓和曲线会映射为内部元素链，支持显示、桩号、DWG 保存和后续再次导出。
  - 业务文档：`docs/business/alignment/道路中线_ICD导入导出.md`
- 编辑平曲线参数命令：命令 `RD_ALIGN_CURVE_PARAM_EDIT`
  - 可通过 Ribbon 的 `编辑平曲线参数` 按钮触发。
  - 选择道路中线后，通过 WPF 参数窗口编辑 `T1`、`T2`、`LS1`、`R`、`LS2`，并刷新道路中线实体。
  - 业务文档：`docs/business/alignment/平曲线参数编辑.md`
- 纵断面拉坡图命令：命令 `RD_PROFILE_GRADE_GRAPH_CREATE`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`纵断面设计` 面板、`纵断面拉坡图` 按钮触发。
  - 用户选择道路中线后，命令优先读取道路中线关联的 `DnTerrainTinEntity` 并沿中线采样地面高程；没有可用数模时，选择 `.dmx` 纵地面线文件读取 `桩号 高程` 数据。
  - 创建时由用户点取图纸插入位置，生成 `DnProfileGradeGraphEntity` 自定义实体；X 方向按桩号 1:1 增长，Y 方向按纵向比例 1:1、1:10 或 1:100 显示。
  - 实体绘制拉坡图名称、表头、网格线和地面线折线；默认地面线为青绿色、线宽 1、采样精度 10、网格间距 10。
  - 双击拉坡图可打开 WPF 属性窗口，修改地面线颜色、线宽、精度、纵向比例和网格间距；DMX 来源实体可使用“更新地面线”重新读取保存的 DMX 文件路径，数模来源实体该按钮置灰。
  - 业务文档：`docs/business/profile/纵断面拉坡图_创建.md`、`docs/business/profile/纵断面拉坡图_属性编辑.md`
- 竖曲线命令：命令 `RD_PROFILE_VERTICAL_CURVE_CREATE`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`纵断面设计` 面板、`创建竖曲线` 按钮触发。
  - 用户选择纵断面拉坡图后，系统创建独立 `DnProfileVerticalCurveEntity`，默认连接地面线起终点，作为设计高程线。
  - 竖曲线实体通过关联拉坡图的图面坐标系绘制，直坡段为青色、曲线段为黄色，并绘制 BVC/PVI/EVC 理论切线；支持 DWG 持久化、起终点/PVI/半径夹点、PVI 新增和删除。
  - 双击竖曲线可打开 WPF 编辑窗口，修改名称、起终点、PVI 桩号/高程和半径；回写由 C++ application/domain 层校验后刷新实体。
  - 业务文档：`docs/business/profile/竖曲线_创建.md`、`docs/business/profile/竖曲线_编辑.md`、`docs/business/profile/竖曲线_夹点与右键编辑.md`
- 路基模板命令：命令 `RD_SECTION_SUBGRADE_TEMPLATE_CREATE`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`横断面设计` 面板、`创建路基模板` 按钮触发。
  - 用户只点取插入点，不选择、不绑定现有道路中线；确认参数后生成独立 `DnSubgradeTemplateEntity`。
  - WPF 参数窗口支持模板名称、显示比例、道路等级、二维预览、部件点选、左右切换、新增/删除部件、宽度、高度差、颜色、固定/变化坡度、变宽表、坡度变化数据表和路面结构层模板引用；二级、三级、四级道路默认生成两条行车道、硬路肩和土路肩的左右对称模板。
  - 任意部件类型均可通过点选 DWG 中的 `DnPavementLayerTemplateEntity` 绑定路面结构层模板，也可清除绑定；WPF 不直接读取 CAD 实体，由 C++ ObjectARX 负责点选和类型校验。
  - 实体自身绘制道路中线标记和中文部件标注；支持插入点夹点拖动移动模板图面位置；双击编辑时应读取上次保存的实体参数，后续再扩展与道路中线的自动联动。
  - 业务文档：`docs/business/cross_section/路基模板_创建.md`、`docs/business/cross_section/路基模板_编辑.md`、`docs/business/cross_section/路基模板_WPF桥接回写.md`
- 路面结构层模板命令：命令 `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`横断面设计` 面板、`创建路面结构层模板` 按钮触发。
  - 运行创建命令后先打开路面结构层创建向导，选择路面类型和适应路段类型，并可在同一层行中快速调整内外侧厚度、内外侧加宽和内外侧坡度；确认向导后再打开原有 WPF 参数窗口继续细化模板名称、显示比例、预览宽度、显示方式、结构层列表、每层 RGB 颜色、填充类型、填充角度和填充比例；旧实体双击编辑仍直接打开原有 WPF 参数窗口，不显示创建向导。
  - 向导内置沥青路面主线行车道、主线硬路肩、互通匝道、桥头过渡段、桥面铺装、互通被交路、隧道，以及混凝土路面收费站广场、通道连接线初始参数；空白或 0 的文档项不生成有效结构层参数。
  - 所有路面结构层类型的预览宽度初始默认值统一为 `3`；通用参数区默认只显示预览宽度、结构层数量、显示方式和当前编辑层，勾选“显示全部通用参数”后才显示结构代号、路基干湿类型、路面类型、路基土组、设计弯沉和累计轴次，这些新增字段先作为模板数据保存，不参与 DWG 标注或道路模型结构层显示。
  - 结构层类型下拉值固定为上面层、中面层、下面层、沥青封层、基层、底基层、垫层、搭板层。
  - 参数窗口厚度、加宽和坡度均支持“内外是否一致”复选框；一致时使用单一值，不一致时分别使用内侧和外侧值；当前编辑层始终有选中项，可通过“新增部件”在当前部件上方或下方插入结构层，也可通过“删除部件”确认删除选中结构层。
  - 内侧表示更靠近道路中线的一侧，外侧表示更远离道路中线的一侧；内外侧加宽支持正负值，正值扩宽、负值缩短。除第一层外，当前层顶边以上一层底边所在直线为基准，按本层加宽沿该直线向两端延长或收回，始终保持四边形/梯形，不生成台阶式六边形。坡度按 `1:n` 计算当前层顶边到底边的侧边水平位移，正坡度向外放，负坡度向内收。
  - WPF 预览使用白色竖向引线汇总层名和厚度标注，引线起点恰好落在结构层顶边，每层一行并带白色下划线；文字、引线、下划线、加宽箭头和标注偏移均按固定模型尺寸换算到当前预览缩放，随滚轮缩放一起变大变小。加宽文字缩小并放在尺寸箭头中部上方，坡度文字缩小并放在坡边中部侧边。DWG 模板实体不显示尺寸标注，只在模板上方按文字总长居中显示模板名称。道路模型结构层仍只按每层 RGB 颜色显示弱化填充面和层色边线。
  - WPF 窗口支持 `.rpavement.xml` 导入导出，用于跨图纸流转结构层模板参数。
  - 业务文档：`docs/business/cross_section/路面结构层模板_创建.md`、`docs/business/cross_section/路面结构层模板_编辑.md`、`docs/business/cross_section/路面结构层模板_WPF桥接回写.md`
- 边坡模板命令：命令 `RD_SECTION_SLOPE_TEMPLATE_CREATE`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`横断面设计` 面板、`创建边坡模板` 按钮触发。
  - 用户点取插入点后打开 WPF 参数窗口，支持模板名称、显示比例、填方/挖方初始预设、二维线框预览、部件点选、新增/删除/移动部件、坡率/坡高/宽度三选二约束、每部件搜索地面线范围坡高增值，以及“交地则结束放坡”“重复最后一组参数直至交地”控制条件。
  - 确认后生成独立 `DnSlopeTemplateEntity`，实体只以线表达边坡模板，不生成厚度；双击实体可重新打开同一窗口编辑。
  - 业务文档：`docs/business/cross_section/边坡模板_创建.md`、`docs/business/cross_section/边坡模板_编辑.md`、`docs/business/cross_section/边坡模板_WPF桥接回写.md`
- 横断面戴帽道路模型命令：命令 `RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_EDIT`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`横断面设计` 面板、`横断面戴帽` 和 `编辑道路模型` 按钮触发。
  - 创建时选择道路中线，系统自动匹配同一中线关联的唯一竖曲线；不唯一时提示选择竖曲线，并校验竖曲线所属拉坡图与当前中线一致。
  - WPF `横断面戴帽` 窗口提供 `路基模板` tab，以表格配置起终点桩号、模板 handle 和模板名称；每行可从 CAD 图中点选路基模板实体回填，行越靠上优先级越高。
  - WPF `边坡模板` tab 按左侧/右侧二级 tab 独立配置搜索宽度和模板组；每个模板组可选择多个边坡模板，程序按组优先级和组内模板顺序搜索，先戴帽成功的模板生效。
  - 每行模板组提供 `管理模板组` 入口，选中后可在下方管理组内模板的点选、删除、上移、下移和清空。
  - WPF `构造物` tab 可按桩号范围配置桥梁或隧道，并选择影响范围为左侧、右侧或两侧；生成道路模型时，对命中范围和侧别的边坡不再放坡，也不生成对应边坡线。
  - 生成时只取道路中线、竖曲线和模板范围的有效交集，读取路基模板部件绑定的路面结构层模板并生成结构层三维边界线和弱化填充面；边坡从路基模板最外侧开始，按左右搜索宽度从道路中线向外搜索 TIN 地面线；TIN 剖切使用网格候选过滤，并把生成时地面剖面快照保存到道路模型断面。
  - `生成模型` 过程会在 AutoCAD 状态栏显示进度，便于大数据量图纸下观察当前生成状态。
  - 系统生成 `DnRoadModelEntity` 三维道路模型线框实体，按采样桩号保存断面节点链，绘制路基、结构层和边坡的横断面肋线、纵向连接线、最外侧闭合线、端部封闭线和节点数量不一致时的过渡线；边坡颜色来自模板部件颜色，路基/道路主体颜色保持现有部件颜色。
  - 双击或编辑命令可重新打开同一窗口调整模板范围、边坡模板组并刷新实体。
  - 业务文档：`docs/business/cross_section/横断面戴帽_道路模型创建.md`、`docs/business/cross_section/横断面戴帽_边坡模板.md`、`docs/business/cross_section/横断面戴帽_构造物.md`、`docs/business/cross_section/道路模型_编辑.md`、`docs/business/cross_section/道路模型_WPF桥接回写.md`
- 查看横断面命令：命令 `RD_SECTION_ROAD_MODEL_VIEW_SECTION`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`横断面设计` 面板、`查看横断面` 按钮触发。
  - 用户选择已生成的 `DnRoadModelEntity` 后，系统按道路模型保存的采样桩号打开 WPF 只读预览窗口。
  - 预览图优先使用道路模型实体内保存的断面节点链和地面剖面快照显示路基模板线、结构层面、结构层线、边坡模板线和地面线，支持鼠标拖动平移和滚轮缩放；旧模型缺少快照时才回退读取 TIN 剖切。
  - 窗口内可点击 `绘制横断面`，回到模型空间选取基点后按桩号依次向上生成 `DnRoadModelSectionDrawingEntity` 横断面图，结构层颜色和填充参数沿用路面结构层模板，横断面外框和桩号文字使用白色。
  - 横断面图实体后续可通过 `RD_SECTION_DRAWING_CONFIG` 配置图上路面结构层，并支持结构层面域顶点夹点手动调整。
  - 业务文档：`docs/business/cross_section/查看横断面.md`
- 横断面图配置命令：命令 `RD_SECTION_DRAWING_CONFIG`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`横断面设计` 面板、`横断面图配置` 按钮触发，也可双击 `DnRoadModelSectionDrawingEntity` 横断面图继续编辑。
  - 用户选择 `查看横断面 / 绘制横断面` 生成的横断面图后，打开 WPF `横断面图配置` 窗口。
  - 窗口上方支持 CSV 配置路径、导入和导出；当前包含一个 `路面结构层` tab，表格字段为起点桩号、终点桩号、路基类型多选和模板。
  - 路基类型从所选横断面图对应道路模型在当前 DWG 中已绘制出的所有横断面图提取，按左/右侧和部件类型去重后提供多选，例如 `左侧行车道`、`右侧硬路肩`。
  - 模板列用于点选 `DnPavementLayerTemplateEntity`；点击 `绘制` 后，按桩号、侧别和路基部件类型逐项解析配置，为同一道路模型的所有横断面图生成路面结构层面域；同一路基部件重叠时表格上方行优先，不同部件同桩号段可同时生效。
  - 横断面图结构层面域保存 `faceId`、来源模板 handle、来源配置行号和 `manualEdited` 标记；面域顶点提供夹点，用户拖动后会标记为手动编辑并在后续重新绘制时保留。
  - CSV 表头固定为 `起点桩号,终点桩号,路基类型,模板Handle,模板名称`，多选路基类型使用分号分隔。
  - 业务文档：`docs/business/cross_section/横断面图配置.md`
- 路面工程量统计表命令：命令 `RD_DRAWING_PAVEMENT_QUANTITY_TABLE`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`出图出表` 面板、`路面工程量统计表` 按钮触发。
  - 用户选择 `查看横断面` 绘制出的 `DnRoadModelSectionDrawingEntity` 横断面图后，系统优先读取横断面图实体当前保存的结构层面域尺寸；若图上结构层已经通过夹点手动修改，面积和体积以修改后的面域为准。
  - 找不到可用横断面图面域时，系统再读取对应道路模型的结构层断面数据和构造物范围作为兼容回退。
  - 输出路径选择界面提供统计方式选择：可按 `部件名称-结构层名称` 拆分面积/体积列，也可按上一版结构层类型直接汇总。
  - 旧道路模型结构层节点缺少部件名时，会从关联路基模板部件线和结构层纵向线反推 `行车道`、`硬路肩` 等部件名。
  - 统计按构造物范围切分普通段、桥梁段和隧道段；面积为结构层平面投影面积，体积使用相邻断面截面积平均值乘以桩号间距的平均断面法。
  - 输出 Excel 可打开的 `.xls` 路面工程量统计表；单元格居中、自动换行，中文为宋体 10 号，桩号/英文/数字为 Times New Roman 10 号，面积和体积列加宽。
  - 业务文档：`docs/business/drawing_quantity/路面工程量统计表.md`
- 路面结构图例命令：命令 `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND`
  - 可通过 AutoCAD Ribbon 的 `RoadProto` 选项卡、`出图出表` 面板、`路面结构图例` 按钮触发。
  - 用户选择道路模型或一个横断面图；选择横断面图时，系统按该图保存的道路模型 handle 搜索当前模型空间同一道路的所有横断面图。
  - 系统读取当前道路使用到的 `DnPavementLayerTemplateEntity`，生成路基土组、路基干湿类型、设计弯沉、累计当量轴次、结构代号、结构图示、总厚度和底部填充样式图例。
  - 结构图示宽度固定 `20cm`，厚度按厘米 `1:1` 绘制；底部图例按结构层出现顺序逐项绘制，不合并同填充类型或同层名。
  - 命令只绘制普通 CAD 图元，不生成自定义实体。
  - 业务文档：`docs/business/drawing_quantity/路面结构图例.md`
- 平交口模块：命令 `RD_INTERSECTION_INFO`
  - 示例演示新增模块可以通过框架完成注册、命令暴露和 Ribbon 元数据挂接。
  - 业务文档：`docs/business/intersection/平交口模块框架说明.md`

## 构建方式

优先使用本机 Visual Studio 2026 Insiders 打开 `RoadProto.sln`：

```text
D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\devenv.exe
```

命令行构建优先使用 VS2026 MSBuild：

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

构建 ARX 项目前，请确认以下 MSBuild 属性指向有效安装目录：

```text
ObjectARX2021Dir=C:\Autodesk\ObjectARX_for_AutoCAD_2021_Win_64bit_dlm\CDROM1
AutoCAD2021Dir=C:\Program Files\Autodesk\AutoCAD 2021
```

也可以在命令行覆盖：

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" RoadProto.sln /p:Configuration=Release /p:Platform=x64 /p:ObjectARX2021Dir="D:\SDK\ObjectARX 2021"
```

ARX 项目的输出命名规则来自 `build/RoadProto.Build.props`。

托管 Ribbon 插件使用 .NET Framework 4.8：

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

在 AutoCAD 2021 中手动验证当前 RoadProto 流程：

```text
ARXLOAD artifacts\x64\Release\RoadProto_v0.1.31_20260527_SectionDrawingConfig.arx
NETLOAD artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll
```

加载后可在 Ribbon 中打开 `RoadProto` 选项卡，点击 `数模` 面板下的 `地形构网`、`编辑数模`、`导出数模` 或 `导入数模`；也可点击 `平面设计` 面板下的 `平面布线`、`编辑平曲线参数`、`导出中线 ICD` 和 `导入中线 ICD`；纵断面入口位于 `纵断面设计` 面板下的 `纵断面拉坡图` 和 `创建竖曲线`；横断面入口位于 `横断面设计` 面板下的 `创建路基模板`、`创建边坡模板`、`创建路面结构层模板`、`横断面戴帽`、`编辑道路模型`、`查看横断面` 和 `横断面图配置`；出图出表入口位于 `出图出表` 面板下的 `路面工程量统计表` 和 `路面结构图例`。命令行可直接运行 `DN_TERRAIN_TIN_CREATE`、`DN_TERRAIN_TIN_EDIT`、`DN_TERRAIN_TIN_EXPORT`、`DN_TERRAIN_TIN_IMPORT`、`RD_ALIGN_CENTERLINE_CREATE`、`RD_ALIGN_CURVE_PARAM_EDIT`、`RD_ALIGN_CENTERLINE_EXPORT_ICD`、`RD_ALIGN_CENTERLINE_IMPORT_ICD`、`RD_PROFILE_GRADE_GRAPH_CREATE`、`RD_PROFILE_VERTICAL_CURVE_CREATE`、`RD_PROFILE_VERTICAL_CURVE_ADD_PVI`、`RD_PROFILE_VERTICAL_CURVE_DELETE_PVI`、`RD_SECTION_SUBGRADE_TEMPLATE_CREATE`、`RD_SECTION_SLOPE_TEMPLATE_CREATE`、`RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE`、`RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_EDIT`、`RD_SECTION_ROAD_MODEL_VIEW_SECTION`、`RD_SECTION_DRAWING_CONFIG`、`RD_DRAWING_PAVEMENT_QUANTITY_TABLE` 和 `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND`。数模流转文件后缀固定为 `.rmesh`，道路中线流转文件后缀固定为 `.icd`，纵断面地面线文件后缀固定为 `.dmx`，路面结构层模板流转文件后缀固定为 `.rpavement.xml`，横断面图配置流转文件使用 `.csv`。

## 本机运行与内存排查

如运行项目或 AutoCAD 测试时出现内存异常，先采集进程证据再判断来源。重点检查 `acad.exe`、`accoreconsole.exe`、`taskhostw.exe`、`Codex.exe`、`devenv.exe` 和 MSBuild 进程的 PID、工作集、私有内存与命令行。`taskhostw.exe` 是 Windows 宿主进程，只有在确认其内存持续异常且与 RoadProto 操作时间线吻合时，才将其作为本项目问题继续排查。

AutoCAD 图形界面测试结束后，应确认没有残留无窗口 `acad.exe` 或后台 Core Console 进程，避免重复加载 ARX / NETLOAD 后累积占用内存。

## 测试

核心测试项目不依赖 AutoCAD 或 ObjectARX：

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

当前测试覆盖：

- 命令注册元数据和重复注册检查。
- 模块注册、命令注册、Ribbon 分组注册。
- 实体依赖关系与脏标记/重建请求传播。
- 地形更新示例 use case。
- 平面布线领域规则：回旋线公式、五单元平曲线构建、连续多交点控制点链、夹点编辑重建、`LS1/LS2` 不等长参数保护、桩号格式化和无效参数拒绝。
- 平面布线元素链规则：圆曲线、有限半径到有限半径的不完整缓和曲线、反向曲率过渡的计算和采样。
- ICD 路线文件规则：积木法线形单元 `.icd` 的读取、写入、注释清理、工程坐标与 CAD 坐标转换、五单元导出再导入和 `5/6` 不完整缓和曲线导入。
- 纵断面拉坡图规则：DMX 纵地面线读取、桩号/高程布局映射、纵向比例校验、网格范围计算和创建服务默认属性。
- 纵断面竖曲线规则：默认创建、对称二次抛物线计算、BVC/EVC、高低点、任意桩号高程和坡度、PVI 增删、半径更新和命令元数据。
- 横断面道路模型规则：模板优先级解析、采样点收集、竖曲线高程、路基模板三维部件线生成、路面结构层模板内外侧语义和线框生成、边坡模板默认值和几何约束、边坡模板组优先级、构造物范围按左侧/右侧/两侧跳过边坡放坡、TIN 地面剖切交地、断面地面快照、断面节点链、三维网格线框生成、生成进度回调、采样桩号持久化、横断面查看预览、横断面图配置 CSV、同一路基部件配置行优先级、路基类型多选、结构层面域夹点手动编辑、命令元数据、WPF Bridge 和 ObjectARX 实体源码契约。
- 出图出表规则：路面工程量统计表命令元数据、横断面图实体当前面域优先采样、构造物切段、旧道路模型部件名反推、部件/结构层双模式聚合、结构层平面投影面积、平均断面法体积和带格式 `.xls` 写出；路面结构图例命令元数据、模板列规划、厚度厘米表达、底部图例不合并和普通 CAD 图元绘制契约。

## 新增命令流程

1. 如果涉及业务概念，先在 `src/domain` 增加领域对象或规则。
2. 在 `src/application/<module>` 增加流程服务。
3. 在 `src/modules/<module>` 增加命令入口。
4. 只能通过 `CommandRegistry` 注册命令。
5. 通过 `RibbonModel` 增加或更新模块 Ribbon 分组。
6. 在 `docs/business/<module>/` 为每个独立功能创建单独业务文档，并让命令元数据指向对应文档；模块总览不能替代功能文档。
7. 在 `docs/reuse/` 记录可复用能力。
8. 在 `docs/dev/version_log.md` 更新版本记录。

不要把业务逻辑直接写在 ARX 入口函数、命令回调或 UI 事件中。
