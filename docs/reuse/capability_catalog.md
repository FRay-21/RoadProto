# 复用能力目录

## 通用 CAD 能力

| 能力 | 当前状态 | 源码 |
| --- | --- | --- |
| 编辑器消息输出 | V0.1 ObjectARX 适配器 | `src/cad_adapter/objectarx/ObjectArxEditor.*` |
| 命令注册 | V0.1 ObjectARX 适配器 | `src/cad_adapter/objectarx/ObjectArxCommandRegistrar.*` |
| 选择集释放 guard | 预留辅助类 | `src/cad_adapter/objectarx/ObjectArxSelectionSetGuard.h` |
| 事务作用域 | 接口占位 | `src/cad_adapter/transaction/TransactionScope.h` |
| 图层规格 | 接口占位 | `src/cad_adapter/layer/LayerService.h` |
| 文字标注规格 | 接口占位 | `src/cad_adapter/annotation/TextAnnotationService.h` |
| 块插入规格 | 接口占位 | `src/cad_adapter/block/BlockInsertService.h` |
| ObjectARX 地形对象提取 | V0.1.6 原型，支持连续点选样例、按同图层同类型扫描模型空间、状态栏进度和按类隐藏源对象 | `src/cad_adapter/objectarx/terrain/ObjectArxTerrainTinCommand.cpp` |
| 地形构网参数确认窗口 | V0.1.6 历史过渡实现，C++ Win32 对话框，当前原型阶段后续重做需迁移为 WPF | `src/cad_adapter/objectarx/terrain/TerrainTinCreateDialog.*` |
| 地形 TIN 自定义实体 | V0.1.6 原型，支持 TrueColor 渐变边线高程着色、边界显示、DWG 持久化、RMesh 流转和双击编辑 handle 入口 | `src/cad_adapter/objectarx/terrain/DnTerrainTinEntity.*` |
| AutoCAD 可见 Ribbon 托管插件 | V0.1.8 原型，创建带小图标且尺寸一致的 `RoadProto` / `数模` / `平面设计` Ribbon 入口，并转发数模和道路中线双击事件 | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs` |

## 通用道路设计能力

| 能力 | 当前状态 | 源码 |
| --- | --- | --- |
| 设计实体唯一 ID | V0.1 已实现 | `src/domain/common/EntityId.*` |
| 实体依赖关系 | V0.1 已实现 | `src/domain/relation/EntityRelationManager.*` |
| 脏标记与重建请求 | V0.1 已实现 | `src/domain/relation/DesignEntity.h` |
| 文字高程解析 | V0.1.3 已实现 | `src/domain/terrain/TerrainTextElevationParser.*` |
| 地形点清洗与文字-点关联 | V0.1.3 已实现 | `src/domain/terrain/TerrainPointNormalizer.*` |
| Delaunay / TIN 构建 | V0.1.3 已实现，基于 `delaunator-cpp` | `src/domain/terrain/TerrainTinBuilder.*` |
| 第三方 Delaunay 头文件 | V0.1.3 已引入，MIT License | `third_party/delaunator-cpp/` |
| TIN 高程查询 | V0.1.3 已实现 | `src/domain/terrain/TerrainSurfaceQuery.*` |
| TIN 三角形空间索引 | V0.1.18 已实现，生成期为横断面剖切建立临时 flat 网格索引，按剖切线段穿越格子筛选候选三角形，不写入 DWG，不形成长期内存缓存 | `src/domain/terrain/TerrainTriangleSpatialIndex.*` |
| RMesh 地形数模文件读写 | V0.1.6 已实现，支持 `.rmesh` 导入导出和跨 DWG 流转 | `src/domain/terrain/TerrainMeshFile.*` |
| 回旋线与平曲线五单元构建 | V0.1.8 原型，支持主点切线长、旧切线长保护、直线-缓和曲线-圆曲线-缓和曲线-直线构建和采样 | `src/domain/alignment/HorizontalAlignmentBuilder.*` |
| 平面线形元素链与不完整缓和曲线 | V0.1.8 扩展，支持圆曲线、有限半径到有限半径的不完整缓和曲线、正负曲率过渡和元素链采样 | `src/domain/alignment/AlignmentElementChainBuilder.*` |
| ICD 道路中线文件读写 | V0.1.8 扩展，支持积木法 `.icd` 直线、圆曲线、完整缓和曲线、不完整缓和曲线读写、工程坐标/CAD 坐标转换和元素链转换 | `src/domain/alignment/IcdAlignmentFile.*` |
| 道路中线夹点编辑 | V0.1.8 原型，按夹点索引移动控制点或调整曲线参数，并在领域层重建五单元平曲线 | `src/domain/alignment/AlignmentGripEditService.*` |
| 桩号格式化 | V0.1.7 原型，支持 `K0+000`、`K0+100` 等道路桩号文本 | `src/domain/alignment/StationFormatter.*` |
| 道路中线自定义实体 | V0.1.8 原型，支持 DWG 持久化、分元素着色、引线桩号、特征点标注、参数标注和参数夹点动态预览，特征点不额外绘制小十字 | `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.*` |
| 道路中线 WPF 参数窗口 | V0.1.8 原型，通过请求/响应文件 Bridge 编辑道路属性、数模关联、桩号间距和平曲线参数 | `src/ui/wpf/RoadProto.Terrain.UI/AlignmentCenterlineWindow.xaml` |
| DMX 纵地面线文件读取 | V0.1.9 原型，支持 `.dmx` 桩号/高程读取、注释跳过、断链写法兼容、重复桩号保留和非法行拒绝 | `src/domain/profile/ProfileDmxFile.*` |
| 纵断面拉坡图布局映射 | V0.1.9 原型，支持桩号 X 方向 1:1、纵向比例 1/10/100、网格范围和地面线图面坐标计算 | `src/domain/profile/ProfileGradeGraphLayout.*` |
| 纵断面拉坡图自定义实体 | V0.1.9 原型，支持标题、表头、网格线、地面线折线、DWG 持久化、几何范围和变换 | `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.*` |
| 纵断面拉坡图 WPF 属性 Bridge | V0.1.9 原型，通过请求/响应文件编辑显示属性，并支持 DMX 来源重新读取地面线 | `src/cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.*` |
| 纵断面竖曲线计算 | V0.1.9 follow-up 原型，支持起终点、PVI、半径、对称二次抛物线、BVC/EVC、高低点、任意桩号高程和瞬时坡度 | `src/domain/profile/ProfileVerticalCurveCalculator.*` |
| 纵断面竖曲线图形分段计划 | V0.1.9 follow-up 原型，输出直坡设计段、曲线设计段和 BVC/PVI/EVC 理论切线段，用于 CAD 分色绘制 | `src/domain/profile/ProfileVerticalCurveDisplayPlanner.*` |
| 纵断面竖曲线自定义实体 | V0.1.9 follow-up 原型，独立关联拉坡图，支持 DWG 持久化、拉坡图 frame 映射、直坡/曲线/理论切线分色绘制、起终点/PVI/半径夹点和 PVI 增删 | `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.*` |
| 纵断面竖曲线 WPF 编辑 Bridge | V0.1.9 follow-up 原型，通过请求/响应文件编辑名称、起终点、PVI 高程和半径，并由 C++ 严格解析后回写实体 | `src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.*` |
| 路基模板领域模型与默认值 | V0.1.19 原型，支持道路等级、左右侧部件、宽度、高度差、固定/变化坡度、变宽表、坡度变化表、颜色和路面结构层模板 handle 关联；二级、三级、四级道路已有默认组成 | `src/domain/cross_section/SubgradeTemplateModel.*` |
| 路基模板自定义实体 | V0.1.13 原型，独立绘制中线和左右侧部件，支持中文部件标注、DWG 持久化、几何范围、变换和插入点夹点移动 | `src/cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.*` |
| 路基模板 WPF 编辑 Bridge | V0.1.10 原型，通过请求/响应文件编辑模板参数、部件参数、变宽表和坡度变化表 | `src/cad_adapter/objectarx/cross_section/SubgradeTemplateDialogBridge.*` |
| 边坡模板领域模型与默认值 | V0.1.14 原型，支持填方/挖方预设、边坡/护坡道部件、坡率/坡高/宽度三选二、部件级搜索增值和交地控制条件 | `src/domain/cross_section/SlopeTemplateModel.*` |
| 边坡模板自定义实体 | V0.1.14 原型，独立线框显示边坡模板，支持 DWG 持久化、几何范围、变换和双击编辑入口 | `src/cad_adapter/objectarx/cross_section/DnSlopeTemplateEntity.*` |
| 边坡模板 WPF 编辑 Bridge | V0.1.14 原型，通过请求/响应文件编辑边坡模板参数和部件列表 | `src/cad_adapter/objectarx/cross_section/SlopeTemplateDialogBridge.*` |
| 路面结构层模板领域模型与默认值 | V0.1.23 原型，支持上面层/中面层/下面层/基层/底基层/垫层、每层 RGB 显示色、CAD 填充类型、填充角度、填充比例、显示方式、结构代号、路基干湿类型、路面类型、路基土组、设计弯沉、累计轴次、等厚/内外侧非等厚、内外侧正/负加宽和正/负坡度、顶边沿上一层底边所在直线延长或收回、坡度按 `1:n` 驱动顶边到底边的侧边水平移动、四边形/梯形横断面预览几何；新增通用设计字段当前仅作为数据保留 | `src/domain/cross_section/PavementLayerTemplateModel.*` |
| 路面结构层模板自定义实体 | V0.1.23 原型，独立显示路面结构层模板，按层保存 RGB 真彩色、填充类型、填充角度和填充比例，并持久化结构代号、路基干湿类型、路面类型、路基土组、设计弯沉和累计轴次；支持按颜色/按填充/按填充+颜色显示，DWG 中仅在模板上方居中显示模板名称，不显示尺寸标注，新增通用设计字段不绘制为文字，支持 DWG 持久化、几何范围、变换和双击编辑入口 | `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.*` |
| 路面结构层模板 WPF 编辑 Bridge | V0.1.23 原型，通过请求/响应文件编辑路面结构层模板、显示方式、每层 RGB、每层填充类型、填充角度、填充比例，以及可折叠显示的结构代号、路基干湿类型、路面类型、路基土组、设计弯沉和累计轴次；WPF 支持当前层编辑、预览点击选层、固定字号标注、索引颜色选择和 `.rpavement.xml` 导入导出 | `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.*` |
| 横断面道路模型 | V0.1.19 原型，按道路中线、竖曲线、路基模板范围、路面结构层模板引用和边坡模板组生成断面节点链、结构层边界线与弱化填充显示、地面剖面快照与三维网格线框，支持优先级解析、行内点选模板、模板组管理、生成进度回调、TIN 网格候选剖切、采样桩号保存、横断面预览、DWG 持久化和编辑回写 | `src/domain/cross_section/RoadModel.*` |
| 道路模型自定义实体 | V0.1.19 原型，保存 `RoadModelData`、采样桩号、断面节点链、结构层节点、地面剖面快照和网格线框，并绘制路基、结构层、边坡横向肋线、纵向连接线、最外侧边界线、端部封闭线和过渡线，支持 DWG 持久化、几何范围和变换 | `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.*` |
| 道路模型横断面查看 Bridge | V0.1.19 原型，通过请求文件把道路模型断面节点、结构层线和地面快照预览传给 WPF 只读查看窗口 | `src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.*` |

## 模块专用能力

| 能力 | 当前状态 | 源码 |
| --- | --- | --- |
| 地形更新后的联动标记 | V0.1.6 保留的历史框架示例，不代表当前 TIN 实体已自动监听源对象修改 | `src/application/terrain/TerrainUpdateSampleService.*` |
| 地形构网流程服务 | V0.1.6 原型 | `src/application/terrain/TerrainTinCreateService.*` |
| 平交口模块注册 | V0.1 示例 | `src/modules/intersection/IntersectionModule.*` |
| 平面设计模块注册 | V0.1.8 原型，注册平面布线、编辑平曲线参数、ICD 导入导出、双击 handle 编辑命令和 WPF 回写内部命令，并按功能文档分配业务文档路径 | `src/modules/alignment/AlignmentModule.*` |
| 纵断面设计模块注册 | V0.1.9 原型，注册纵断面拉坡图、竖曲线创建、双击 handle 编辑、WPF 回写、PVI 增删命令和 Ribbon 面板 | `src/modules/profile/ProfileModule.*` |

## 临时原型能力

| 能力 | 当前状态 | 后续处理 |
| --- | --- | --- |
| 固定示例实体 ID | `RD_TERRAIN_MARKDIRTY` 使用 | 后续替换为持久化 ID 和 DWG 元数据 |
| WPF 地形构网 UI Bridge 预留 | `RoadProto.Terrain.UI` 已编译，并提供可见 Ribbon 和双击事件转发；参数窗口仍由 C++ 对话框承接 | 后续用 C++/CLI 或 AutoCAD .NET Bridge 接入完整 WPF 参数窗口 |

## V0.1.8 平面布线最终复用边界

- `HorizontalAlignmentBuilder` 是道路中线几何核心能力，支持单交点和多交点控制点链、`LS1/LS2` 不等长、`T1/T2` 派生和无效平曲线参数拒绝。
- `AlignmentGripEditService` 是道路中线夹点编辑核心能力，封装控制点移动和特征点参数调整，不依赖 ObjectARX。
- `StationFormatter` 是桩号文本能力，当前用于 `K0+000`、`K0+100` 等道路桩号格式化。
- `DnRoadCenterlineEntity` 是 CAD 持久化和显示能力，负责分元素着色、引线桩号、曲线参数标注、特征点标注、DWG 保存重开和参数夹点动态预览。
- `AlignmentDialogBridge` 是原型阶段 UI 解耦能力，通过请求/响应文件在 WPF 与 C++ ObjectARX 之间传递道路属性、数模 handle、桩号间距和多组平曲线参数。
- 后续控制点实时预览、隐藏正式实体、防止旧实体短直线残留、处理 `Enter`/右键结束点取等行为属于 ObjectARX Adapter 层能力，不进入 WPF 业务逻辑。
- 数模关联当前沉淀为 handle 关联能力；WPF 不直接读取自定义实体类型，后续高程查询由 C++ Adapter/Service 接管。

## V0.1.9 纵断面拉坡图复用边界

- `ProfileDmxFile` 是 DMX 纵地面线读取能力，可复用于后续纵断面地面线、竖曲线设计底图和数据导入。
- `ProfileGradeGraphLayout` 是拉坡图图面映射能力，固定 X 方向桩号 1:1，Y 方向按纵向比例映射。
- `ProfileGradeGraphCreateService` 负责把已准备好的道路信息和地面线样本组装为拉坡图数据，不依赖 ObjectARX。
- `DnProfileGradeGraphEntity` 是 CAD 持久化和显示能力，负责标题、表头、网格线和地面线折线表达。
- `ProfileGradeGraphDialogBridge` 是原型阶段 UI 解耦能力，通过请求/响应文件在 WPF 与 C++ ObjectARX 之间传递属性和 DMX 更新请求。
- 数模来源的地面线当前在创建命令中按精度采样，属性窗口修改精度时可按保存 handle 重新采样；路线移动或数模更新后的自动重建需后续接入统一实体关系管理机制。

## V0.1.9 纵断面竖曲线复用边界

- `ProfileVerticalCurveModel` 和 `ProfileVerticalCurveCalculator` 是竖曲线几何核心能力，不依赖 ObjectARX，可复用于横断面设计高程、三维模型、土方和排水设计。
- `ProfileVerticalCurveDisplayPlanner` 是竖曲线图形表达计划能力，不依赖 ObjectARX，可复用于 CAD、出图和后续可替换 UI 的分色绘制。
- `ProfileVerticalCurveCreateService` 负责从拉坡图地面线首末点生成默认设计线，不做 CAD 选择。
- `ProfileVerticalCurveEditService` 统一承接 WPF 回写、夹点移动、PVI 新增、PVI 删除和半径更新。
- `DnProfileVerticalCurveEntity` 是 CAD 持久化和显示能力，负责通过关联拉坡图 frame 映射设计坐标、按直坡青色/曲线黄色/理论切线白色绘制图形，并提供起终点/PVI/半径夹点。
- `ProfileVerticalCurveDialogBridge` 是原型阶段 UI 解耦能力，通过请求/响应文件在 WPF 与 C++ ObjectARX 之间传递竖曲线参数。
- 第一版完整标注、要素表、非对称竖曲线和跨模块自动联动仍需后续扩展。

## V0.1.10 路基模板复用边界

- `SubgradeTemplateModel` 是路基模板参数核心能力，不依赖 ObjectARX，可复用于横断面设计、标准断面库、三维建模和出图。
- `SubgradeTemplateDefaults` 保存高速公路、城市快速路以及二级、三级、四级道路默认组成，后续新增道路等级默认值时应继续在 domain 层扩展。
- `SubgradeTemplateRules` 保存显示比例、宽度加宽、坡度查询、变化坡度固定值隔离和路面结构层模板引用归一化规则。
- `SubgradeTemplateCreateService` 只生成默认模板数据，不处理 CAD 点取和 WPF 交互。
- `DnSubgradeTemplateEntity` 是 CAD 持久化和显示能力，负责绘制中线、左右侧部件和 DWG 保存重开。
- `DnSubgradeTemplateEntity` 提供插入点夹点，支持模板实体在图中整体移动。
- `SubgradeTemplateDialogBridge` 是原型阶段 UI 解耦能力，通过请求/响应文件在 WPF 与 C++ ObjectARX 之间传递模板和部件参数。
- 路线桩号关联、按桩号应用变宽/变坡、路面结构层模板联动重建仍需后续扩展。

## V0.1.11 横断面道路模型复用边界

- `RoadModelTemplateResolver` 是模板优先级解析能力，按表格行顺序处理重叠桩号范围。
- `RoadModelStationSampler` 是道路模型采样点收集能力，合并道路端点、模板边界、竖曲线关键点和固定采样间距点。
- `RoadModelBuilder` 是三维道路线框生成能力，把道路中线平面采样、竖曲线设计高程、路基模板部件、路面结构层模板、边坡模板规则和 TIN 地面剖面组合为 `RoadModelData`，并生成断面节点链、结构层线和网格线框。
- `TerrainTriangleSpatialIndex` 是 TIN 剖切候选过滤能力，生成期为 TIN 建立临时 flat 网格索引，剖切时只遍历当前搜索线段穿越网格内的候选三角形；索引不写入实体，不形成长期内存缓存。
- `RoadModelSection` 保存生成时左右地面剖面快照，边坡放坡、道路模型持久化和查看横断面使用同一份剖面数据。
- `RoadModelSlopeTemplateGroupResolver` 是边坡模板组优先级解析能力，按左/右侧模板组和组内模板顺序搜索首个戴帽成功模板。
- `RoadModelBuildInput::progressCallback` 是不依赖 ObjectARX 的构建进度回调，可由 AutoCAD 状态栏、后续 Palette 或其他 UI 技术栈自行适配。
- `RoadModelSectionPreviewBuilder` 是已生成道路模型的横断面预览能力，按桩号把三维路基线、边坡线和地面快照转换为二维偏距-高程线段；没有快照的旧实体才读取 TIN 临时剖切。
- `RoadModelBuildService` 是应用层构建入口，负责校验 handle、配置、采样和模板来源后调用 domain builder。
- `DnRoadModelEntity` 是 CAD 持久化和显示能力，负责保存三维路基与边坡兼容部件线、生成时采样桩号、断面节点链、网格线框、DWG 保存重开、范围和变换。
- `RoadModelDialogBridge` 是原型阶段 UI 解耦能力，通过请求/响应文件在 WPF 与 C++ ObjectARX 之间传递模板范围表和行内点选模板动作。
- `RoadModelSectionViewerBridge` 是原型阶段 UI 解耦能力，通过请求文件在 C++ ObjectARX 与 WPF 之间传递多个桩号的只读断面预览。
- 当前道路模型只生成三维线框，自动联动重建、道路面模型、结构层实体面和算量仍需后续扩展。

## V0.1.21 路面结构层模板复用边界

- `PavementLayerTemplateModel` 是路面结构层模板参数核心能力，不依赖 ObjectARX，可复用于横断面设计、标准结构层库、三维建模和算量。
- `PavementLayerTemplateRules` 统一处理结构层类型编码、显示名称、填充类型、填充角度、填充比例、显示方式、结构代号、路基干湿类型、路面类型、路基土组、设计弯沉、累计轴次、等厚/非等厚厚度、内外侧加宽、内外侧坡度和横断面预览几何；新增通用设计字段先只做数据保存和枚举归一化。
- 内侧 = closer to road centerline for the subgrade component；外侧 = farther from road centerline。内侧加宽扩向道路中线。
- 加宽表示当前层顶边相对上一层底边所在直线的横向变化；正值沿该直线扩宽，负值沿该直线缩短。除第一层外，当前层顶边必须沿上一层底边所在直线平行/共线延长或收回，始终保持四边形/梯形。坡度只驱动当前层顶边到底边的内外侧边按 `厚度 * n` 水平移动，正坡度向外放，负坡度向层内收，不把加宽误作为坡率变化，也不生成六点台阶轮廓。
- `DnPavementLayerTemplateEntity` 是 CAD 持久化和显示能力，负责保存模板数据、绘制颜色/填充预览和模板名称居中标题；DWG 自定义实体不显示尺寸标注，支持 DWG 保存重开和双击编辑。
- `PavementLayerTemplateDialogBridge` 是原型阶段 UI 解耦能力，通过请求/响应文件在 WPF 与 C++ ObjectARX 之间传递模板参数。
- `.rpavement.xml` 是 WPF 侧模板流转格式，可用于导入导出路面结构层模板参数、显示方式、填充类型、填充角度、填充比例、结构代号、路基干湿类型、路面类型、路基土组、设计弯沉和累计轴次，不替代 DWG 自定义实体持久化。
- 路基模板部件通过 handle 引用路面结构层模板；所有部件类型均允许绑定，是否生成结构层由 handle 是否有效决定。
- 道路模型生成读取绑定模板并创建结构层三维边界线，`DnRoadModelEntity` 仍按层 RGB 显示为弱化填充面和层色边线，`RoadModelSectionPreviewBuilder` 在查看横断面中显示 `结构层`；模板显示方式不改变道路模型显示。
- 自动联动重建、结构层实体面、材料库、体积和算量仍需后续扩展。
