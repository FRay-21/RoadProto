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
| 路基模板领域模型与默认值 | V0.1.10 原型，支持道路等级、左右侧部件、宽度、高度差、固定/变化坡度、变宽表、坡度变化表、颜色和路面结构层关联预留字段；二级、三级、四级道路已有默认组成 | `src/domain/cross_section/SubgradeTemplateModel.*` |
| 路基模板自定义实体 | V0.1.13 原型，独立绘制中线和左右侧部件，支持中文部件标注、DWG 持久化、几何范围、变换和插入点夹点移动 | `src/cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.*` |
| 路基模板 WPF 编辑 Bridge | V0.1.10 原型，通过请求/响应文件编辑模板参数、部件参数、变宽表和坡度变化表 | `src/cad_adapter/objectarx/cross_section/SubgradeTemplateDialogBridge.*` |
| 横断面道路模型 | V0.1.13 原型，按道路中线、竖曲线和路基模板范围生成三维部件线，支持优先级解析、行内点选模板、采样、DWG 持久化和编辑回写 | `src/domain/cross_section/RoadModel.*` |
| 道路模型自定义实体 | V0.1.11 原型，保存 `RoadModelData` 并绘制三维部件线，支持 DWG 持久化、几何范围和变换 | `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.*` |

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
- `SubgradeTemplateRules` 保存显示比例、宽度加宽、坡度查询、变化坡度固定值隔离和路面结构层厚度启用规则。
- `SubgradeTemplateCreateService` 只生成默认模板数据，不处理 CAD 点取和 WPF 交互。
- `DnSubgradeTemplateEntity` 是 CAD 持久化和显示能力，负责绘制中线、左右侧部件和 DWG 保存重开。
- `DnSubgradeTemplateEntity` 提供插入点夹点，支持模板实体在图中整体移动。
- `SubgradeTemplateDialogBridge` 是原型阶段 UI 解耦能力，通过请求/响应文件在 WPF 与 C++ ObjectARX 之间传递模板和部件参数。
- 路线桩号关联、按桩号应用变宽/变坡、路面结构层实体关联和联动重建仍需后续扩展。

## V0.1.11 横断面道路模型复用边界

- `RoadModelTemplateResolver` 是模板优先级解析能力，按表格行顺序处理重叠桩号范围。
- `RoadModelStationSampler` 是道路模型采样点收集能力，合并道路端点、模板边界、竖曲线关键点和固定采样间距点。
- `RoadModelBuilder` 是三维道路部件线生成能力，把道路中线平面采样、竖曲线设计高程和路基模板部件组合为 `RoadModelData`。
- `RoadModelBuildService` 是应用层构建入口，负责校验 handle、配置、采样和模板来源后调用 domain builder。
- `DnRoadModelEntity` 是 CAD 持久化和显示能力，负责绘制三维部件线、DWG 保存重开、范围和变换。
- `RoadModelDialogBridge` 是原型阶段 UI 解耦能力，通过请求/响应文件在 WPF 与 C++ ObjectARX 之间传递模板范围表和行内点选模板动作。
- 当前道路模型只生成三维线框，自动联动重建、道路面模型、结构层实体和算量仍需后续扩展。
