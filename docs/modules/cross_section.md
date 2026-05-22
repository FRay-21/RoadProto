# 横断面设计模块

## 模块信息

- 模块名称：横断面设计
- 模块编码：`CROSS_SECTION`
- 命令前缀：`RD_SECTION_`
- 当前状态：已实现路基模板独立实体创建、边坡模板独立实体创建、路面结构层模板独立实体创建、每层 RGB 颜色、WPF 参数窗口、`.rpavement.xml` 导入导出、二维预览、双击编辑入口、桥接回写、路基部件点选绑定结构层模板和插入点夹点移动；已实现横断面戴帽道路模型创建、编辑、WPF 路基模板范围表、左右边坡模板组、模板组管理入口、生成进度反馈、`DnRoadModelEntity` 三维道路模型网格线框实体、路面结构层弱化填充面和层色边线、断面地面快照、按采样桩号查看横断面预览和 ObjectARX 回写流程。

## 命令清单

| 命令 | 显示名称 | 类型 | 文档 |
| --- | --- | --- | --- |
| `RD_SECTION_SUBGRADE_TEMPLATE_CREATE` | 创建路基模板 | 用户命令 | `docs/business/cross_section/路基模板_创建.md` |
| `RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE` | 按句柄编辑路基模板 | 内部桥接命令 | `docs/business/cross_section/路基模板_编辑.md` |
| `RD_SECTION_SUBGRADE_TEMPLATE_APPLY_DIALOG_FILE` | 应用路基模板对话框结果 | 内部桥接命令 | `docs/business/cross_section/路基模板_WPF桥接回写.md` |
| `RD_SECTION_SLOPE_TEMPLATE_CREATE` | 创建边坡模板 | 用户命令 | `docs/business/cross_section/边坡模板_创建.md` |
| `RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE` | 按 handle 编辑边坡模板 | 内部桥接命令 | `docs/business/cross_section/边坡模板_编辑.md` |
| `RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE` | 应用边坡模板对话框结果 | 内部桥接命令 | `docs/business/cross_section/边坡模板_WPF桥接回写.md` |
| `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE` | 创建路面结构层模板 | 用户命令 | `docs/business/cross_section/路面结构层模板_创建.md` |
| `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_EDIT_HANDLE` | 按 handle 编辑路面结构层模板 | 内部桥接命令 | `docs/business/cross_section/路面结构层模板_编辑.md` |
| `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_APPLY_DIALOG_FILE` | 应用路面结构层模板对话框结果 | 内部桥接命令 | `docs/business/cross_section/路面结构层模板_WPF桥接回写.md` |
| `RD_SECTION_ROAD_MODEL_CREATE` | 横断面戴帽 | 用户命令 | `docs/business/cross_section/横断面戴帽_道路模型创建.md` |
| `RD_SECTION_ROAD_MODEL_EDIT` | 编辑道路模型 | 用户命令 | `docs/business/cross_section/道路模型_编辑.md` |
| `RD_SECTION_ROAD_MODEL_VIEW_SECTION` | 查看横断面 | 用户命令 | `docs/business/cross_section/查看横断面.md` |
| `RD_SECTION_ROAD_MODEL_EDIT_HANDLE` | 按 handle 编辑道路模型 | 内部桥接命令 | `docs/business/cross_section/道路模型_编辑.md` |
| `RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE` | 应用道路模型对话框结果 | 内部桥接命令 | `docs/business/cross_section/道路模型_WPF桥接回写.md` |

## Ribbon

- C++ Ribbon model：`RoadProto / 横断面设计 / 创建路基模板`、`RoadProto / 横断面设计 / 创建边坡模板`、`RoadProto / 横断面设计 / 创建路面结构层模板`、`RoadProto / 横断面设计 / 横断面戴帽`、`RoadProto / 横断面设计 / 编辑道路模型`、`RoadProto / 横断面设计 / 查看横断面`
- 可见 AutoCAD WPF Ribbon：`RoadProto / 横断面设计 / 创建路基模板`、`RoadProto / 横断面设计 / 创建边坡模板`、`RoadProto / 横断面设计 / 创建路面结构层模板`、`RoadProto / 横断面设计 / 横断面戴帽`、`RoadProto / 横断面设计 / 编辑道路模型`、`RoadProto / 横断面设计 / 查看横断面`
- 托管 Ribbon 插件文件：`src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`

## 代码落点

| 层 | 代码 | 职责 |
| --- | --- | --- |
| domain | `src/domain/cross_section/SubgradeTemplateModel.*` | 路基模板枚举、数据模型、默认值、颜色、显示比例和基础规则 |
| domain | `src/domain/cross_section/SlopeTemplateModel.*` | 边坡模板枚举、默认值、坡率/坡高/宽度约束、控制条件和重复最后一组规则 |
| domain | `src/domain/cross_section/PavementLayerTemplateModel.*` | 路面结构层模板枚举、默认值、每层 RGB、等厚/非等厚、内外侧加宽/坡度规则和横断面预览几何构建 |
| domain | `src/domain/cross_section/RoadModel.*` | 道路模型配置、模板范围、路面结构层模板来源、边坡模板组、采样、TIN 地面剖切、断面节点链、结构层边界线、三维网格线框和横断面预览领域模型 |
| application | `src/application/cross_section/SubgradeTemplateCreateService.*` | 创建命令默认模板数据生成 |
| application | `src/application/cross_section/SlopeTemplateCreateService.*` | 创建命令默认边坡模板数据生成 |
| application | `src/application/cross_section/PavementLayerTemplateCreateService.*` | 创建命令默认路面结构层模板数据生成 |
| application | `src/application/cross_section/RoadModelBuildService.*` | 道路模型构建流程服务 |
| modules | `src/modules/cross_section/CrossSectionModule.*` | 模块、命令和 C++ Ribbon 元数据注册 |
| startup | `src/app/startup/CrossSectionStartupRegistration.*` | 启动期注册 `CROSS_SECTION` 模块 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.*` | 自定义实体显示、DWG 持久化、几何范围和变换 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/DnSlopeTemplateEntity.*` | 边坡模板自定义实体线框显示、DWG 持久化、几何范围和变换 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.*` | 路面结构层模板自定义实体预览显示、DWG 持久化、几何范围和变换 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.*` | 道路模型三维网格线框、结构层弱化填充面显示、DWG 持久化、几何范围和变换 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/ObjectArxSubgradeTemplateCommand.*` | 插入点点取、弹窗、实体创建和回写命令 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/ObjectArxSlopeTemplateCommand.*` | 边坡模板插入点点取、弹窗、实体创建和回写命令 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/ObjectArxPavementLayerTemplateCommand.*` | 路面结构层模板插入点点取、弹窗、实体创建和回写命令 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.*` | 道路模型创建、编辑、查看横断面和 WPF 回写命令入口 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/SubgradeTemplateDialogBridge.*` | WPF 请求/响应文件桥接 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/SlopeTemplateDialogBridge.*` | 边坡模板 WPF 请求/响应文件桥接 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.*` | 路面结构层模板 WPF 请求/响应文件桥接 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/RoadModelDialogBridge.*` | 道路模型 WPF 请求/响应文件桥接 |
| cad_adapter | `src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.*` | 查看横断面 WPF 请求文件桥接 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/SubgradeTemplateWindow.xaml` | 参数窗口和二维预览 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/SlopeTemplateWindow.xaml` | 边坡模板参数窗口和二维线框预览 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml` | 路面结构层模板参数窗口、二维预览和 `.rpavement.xml` 导入导出 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/StationValueTableWindow.xaml` | 变宽/变坡二级表格 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/RoadModelWindow.xaml` | 横断面戴帽窗口、路基模板范围表、左右边坡模板组、组内模板管理和生成入口 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/RoadModelSectionViewerWindow.xaml` | 查看横断面窗口、桩号列表、预览图和图例 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SubgradeTemplateDialogCommands.cs` | WPF 弹窗命令和响应转发 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SlopeTemplateDialogCommands.cs` | 边坡模板 WPF 弹窗命令和响应转发 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/PavementLayerTemplateDialogCommands.cs` | 路面结构层模板 WPF 弹窗命令和响应转发 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadModelDialogCommands.cs` | 道路模型 WPF 弹窗命令和响应转发 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadModelSectionViewerCommands.cs` | 查看横断面 WPF 弹窗命令 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs` | AutoCAD WPF Ribbon 横断面入口扩展 |

## 边界

- `domain/cross_section` 不依赖 ObjectARX。
- WPF 不直接读写 CAD 自定义实体。
- 路基模板、边坡模板和路面结构层模板当前是独立实体，不绑定道路中线。
- 路基模板部件可通过 handle 绑定路面结构层模板；所有部件类型均允许绑定。
- 道路模型通过 handle 关联道路中线、竖曲线、路基模板、路面结构层模板和边坡模板；当前版本不自动监听上游实体变更。
- 统一关系管理机制的自动重建在后续功能中扩展。
## 2026-05-13 更新

- 路基模板默认值已覆盖一级道路、二级道路、三级道路、四级道路、城市主干道、城市次干道和城市支路。
- WPF 路基模板编辑模式保留实体已保存参数，仅在空白新建请求时回退到高速公路默认值。
- 预览图支持直接点选部件，左、右按钮按横断面几何顺序移动。

## 2026-05-18 更新

- 新增横断面戴帽道路模型创建和编辑流程。
- 新增 `DnRoadModelEntity`，支持三维道路模型网格线框 DWG 持久化、显示、范围和变换。
- 新增道路模型 WPF 桥接和 `RoadModelBuildService` 应用服务接入。
- 道路模型创建和回写会校验竖曲线所属拉坡图与当前道路中线一致。

## 2026-05-19 更新

- 横断面戴帽 `路基模板` 表格新增行内 `点选` 入口，可回到 CAD 图中选择 `DnSubgradeTemplateEntity` 并回填当前行模板 handle 和名称。
- 道路模型 WPF 桥接新增 `pickTemplate` 动作和行号字段，点选模板后保持当前表格内容并重新打开窗口继续编辑或生成。
- 路基模板实体新增插入点夹点，可在 CAD 图中拖动移动模板位置。
- 新增 `DnSlopeTemplateEntity`、边坡模板 WPF 编辑窗口和 `RD_SECTION_SLOPE_TEMPLATE_CREATE` / `EDIT_HANDLE` / `APPLY_DIALOG_FILE` 命令。
- 横断面戴帽新增 `边坡模板` tab，按左侧/右侧独立维护模板组、搜索宽度和组内多模板优先级。
- `DnRoadModelEntity` 持久化和绘制范围扩展到边坡三维线框，道路模型构建可从路基模板最外侧向外按 TIN 地面线搜索交地。
- 横断面戴帽 `边坡模板` tab 新增行内 `管理模板组` 入口和下方组内模板管理区，生成模型过程接入 AutoCAD 状态栏进度。
- `RD_SECTION_ROAD_MODEL_VIEW_SECTION` 查看横断面命令可选择 `DnRoadModelEntity` 后按采样桩号预览路基模板线、边坡模板线和生成时地面线快照。
- `DnRoadModelEntity` 持久化数据包含采样桩号、断面节点链和地面剖面快照，供查看横断面窗口按生成时采样精度切换断面。

## 2026-05-20 更新

- 新增路面结构层模板完整工作流：独立实体创建、双击编辑、WPF 桥接回写和 `.rpavement.xml` 导入导出。
- 路面结构层类型固定为上面层、中面层、下面层、基层、底基层和垫层；厚度支持等厚和内外侧非等厚。
- 路基模板部件可点选 DWG 中的路面结构层模板实体并保存 handle；所有部件类型均可绑定。
- 横断面道路模型生成时读取绑定的结构层模板，生成结构层三维边界线并由 `DnRoadModelEntity` 显示为弱化填充面和层色边线。
- 查看横断面窗口在路基模板线、边坡模板线和地面线之外显示 `结构层`。

## 2026-05-21 更新

- 路面结构层模板 WPF 预览初始居中显示，滚轮缩放以鼠标当前位置为基点，中键平移保持原有交互。
- 路面结构层模板的加宽和坡度编辑改为与厚度一致的交互：默认内外侧一致，取消勾选后分别配置内侧和外侧。
- 结构层领域几何明确为四边形/梯形：除第一层外，当前层顶边以上一层底边所在直线为基准；加宽沿该直线平行/共线延长或收回，支持正值扩宽和负值缩短；内外侧坡度再按 `1:n` 让当前层顶边到底边的侧边水平移动，正坡度向外放，负坡度向内收。
- 道路模型结构层显示同步使用与路面结构层模板预览一致的四边形/梯形轮廓，避免模型与预览图/模板实体样式分叉。
- 路面结构层模板实体和道路模型结构层填充面/边线同步使用层保存 RGB；模板实体以预览式弱化填充、层色边线和层名/厚度/加宽/坡度标注表达，结构层线框不再继承路基部件颜色。
- 路面结构层模板 WPF 预览和 DWG 模板实体统一为“先填充、后描边”，相邻层共线重叠的边界按同一几何线表达，减少非等厚结构层斜边的重复描边误读。
- 路面结构层模板修正左右非等厚后的层间连续性：下一层顶边沿上一层底边所在直线按本层加宽延长或收回，仍保持四边形/梯形，避免后续层看起来相交或产生台阶。
- 路面结构层模板新增每层 RGB 颜色编辑和持久化；WPF 预览、`DnPavementLayerTemplateEntity`、道路模型结构层填充面/边线和查看横断面预览统一使用层保存色。

## 2026-05-22 更新

- `DnPavementLayerTemplateEntity` 的填充表达改为与 WPF 预览一致的四点 `polygon` 填充，填充色使用预览背景和层 RGB 透明度预混合后的弱化色，再叠加层 RGB 边线和支持中文的文字标注，避免模型空间模板实体与对话框预览在形状、颜色和标签上分叉。
- `DnRoadModelEntity` 的路面结构层显示同步改为先按连续 `pavementLayerLines` 组合四点 `polygon` 弱化填充面，再叠加层 RGB 线框；没有结构层边界线的旧数据才回退到采样断面节点，避免道路模型中的结构层样式与模板预览继续分叉。
