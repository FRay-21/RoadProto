# 测试说明

`RoadProtoCoreTests.vcxproj` 用于验证 core、domain、application 和 Ribbon 模型行为，不需要加载 AutoCAD 或 ObjectARX。

运行方式：

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

当前测试覆盖：

- 命令注册元数据和重复注册检查。
- 模块注册、命令注册、Ribbon 分组注册。
- 实体依赖关系与脏标记/重建请求传播。
- 地形更新示例 use case。
- 文字高程解析。
- 地形点重复合并、Z 冲突统计、文字-点近邻赋高程。
- Delaunay / TIN 构网的正常三角化和共线失败。
- TIN 三角形查找与重心插值高程查询。
- RMesh `.rmesh` 地形数模文件的写入、读取、Unicode 元数据回读和非法文件拒绝。
- 回旋线公式、道路主点切线长、旧切线长保护、单交点五单元构建、多交点连续平曲线链构建、桩号格式化和无效平曲线参数拒绝。
- 纵断面拉坡图领域规则：DMX 文件读取、断链桩号兼容、重复桩号保留、布局范围、纵向比例校验和创建服务默认属性。
- 纵断面竖曲线领域规则：默认设计线创建、PVI 对称二次抛物线、BVC/EVC、高低点、任意桩号高程和坡度、PVI 增删、半径更新和命令元数据。
- 横断面边坡模板领域规则：填方/挖方默认预设、坡率/坡高/宽度三选二约束、重复最后一组识别、编码转换和模板组优先级解析。
- 横断面路面结构层模板规则：上面层/中面层/下面层/基层/底基层/垫层编码、等厚/内外侧非等厚、内外侧加宽、`.rpavement.xml` 流转、路基部件点选绑定和模板实体源码契约。
- 横断面道路模型边坡和线框规则：从路基模板最外侧生成边坡线、读取部件绑定的路面结构层模板并生成结构层线框、TIN 地面剖切交地、断面地面快照、边坡模板戴帽结果、生成进度回调、采样桩号保存、断面节点链、三维网格线框和查看横断面预览。
- 文档和版本 source-contract：检查 `build/RoadProto.Build.props`、README、版本记录和 `docs/reuse/pavement_layer_template.md` 的 v0.1.19 路面结构层模板发布信息。

V0.1.6 继续保留 `TerrainMeshFile` 领域层测试，用于保证 `DN_TERRAIN_TIN_EXPORT` / `DN_TERRAIN_TIN_IMPORT` 依赖的跨 DWG 数模文件数据不会在读写中丢失。

AutoCAD / ObjectARX 命令回归需要在本机 AutoCAD 环境中手动执行。当前 `v0.1.6` 已用 Core Console 验证 `DN_TERRAIN_TIN_CREATE` 的样例对象选择、同图层同类型提取、源对象隐藏、TIN 生成、`DN_TERRAIN_TIN_EDIT` 非 UI 编辑路径、`DN_TERRAIN_TIN_EDIT_HANDLE` 按 handle 编辑路径、`DN_TERRAIN_TIN_IMPORT` 的 `.rmesh` 导入、DWG 保存后重新打开和 `REGEN`；托管 Ribbon 插件已验证 Release 构建。完整 Ribbon 点击、导出文件对话框和真实鼠标双击弹窗需要在 AutoCAD 图形界面中人工确认。

## V0.1.8 平面布线验证范围

核心测试覆盖 `HorizontalAlignmentBuilder` 的五单元构建、连续多交点控制点链、`AlignmentGripEditService` 的夹点移动重建、交点 shared grip 去重、`LS1/LS2` 不等长时的 `T1/T2` 派生、旧切线长保护、无效参数拒绝和命令元数据注册。

核心测试同时覆盖 `AlignmentElementChainBuilder` 的圆曲线-不完整缓和曲线-圆曲线元素链、卵形同向曲率过渡、S 型正负曲率过渡和无效不完整缓和曲线拒绝。

核心测试还覆盖 `IcdAlignmentFile` 的五单元道路中线导出再导入、ICD `5` 不完整缓和曲线导入、工程坐标到 CAD 坐标和方位角的转换、导入后再次导出保持 `3/4` 完整缓和曲线类型。

AutoCAD 图形界面需要手工验证 `RD_ALIGN_CENTERLINE_CREATE`、`RD_ALIGN_CURVE_PARAM_EDIT`、`RD_ALIGN_CENTERLINE_EDIT_HANDLE`、`RD_ALIGN_APPLY_DIALOG_FILE` 和 `DnRoadCenterlineEntity`：

- 创建命令第三点后立即显示道路中线自定义实体预览，后续点取阶段无橡皮筋短直线残留。
- 后续点取阶段 `Enter`、鼠标右键和 `ESC` 均结束点取并打开 WPF 道路中线窗口。
- WPF 道路中线窗口可编辑道路属性、数模 handle 和桩号间距；平曲线参数页可按“第 x 个平曲线”翻页编辑多组参数。
- 双击道路中线实体可进入同一个 WPF 编辑窗口。
- 保存 DWG 后重开并 `REGEN`，道路中线、桩号、曲线参数标注和实体属性保持正常。
- 夹点拖动起终点、交点、直缓点、缓圆点、圆曲线中点、圆缓点和缓直点时，道路中线应跟随鼠标连续预览；释放后五单元平曲线基本样式保持稳定。
- `RD_ALIGN_CENTERLINE_EXPORT_ICD` 可将选中道路中线导出为 `.icd`。
- `RD_ALIGN_CENTERLINE_IMPORT_ICD` 可导入包含 `1/2/3/4/5/6` 单元的 `.icd` 并生成道路中线实体；ICD 起点工程坐标应正确转换为 CAD 坐标，含 `5/6` 的导入实体应显示、保存、重开和再次导出正常。

## V0.1.9 纵断面拉坡图验证范围

核心测试覆盖 `ProfileDmxFile` 的 `.dmx` 注释跳过、桩号/高程读取、断链写法兼容、重复桩号保留和非法输入拒绝。

核心测试覆盖 `ProfileGradeGraphLayout` 的桩号 X 方向 1:1 映射、纵向比例 1/10/100 映射、网格范围计算、非有限数值拒绝、无效网格间距拒绝和全样本桩号跨度为 0 时拒绝。

核心测试覆盖 `ProfileGradeGraphCreateService` 的默认图名、默认属性、来源信息保存和无效样本拒绝。

AutoCAD 图形界面需要手工验证 `RD_PROFILE_GRADE_GRAPH_CREATE`、`RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE`、`RD_PROFILE_APPLY_DIALOG_FILE` 和 `DnProfileGradeGraphEntity`：

- 选择有关联数模的道路中线后，可沿中线采样并创建纵断面拉坡图。
- 道路中线没有可用数模时，可选择 `.dmx` 文件并创建纵断面拉坡图。
- 创建后图中显示拉坡图名称、表头、网格线和青绿色地面线折线。
- 双击拉坡图可打开 WPF 属性窗口，修改颜色、线宽、精度、纵向比例和网格间距后刷新实体。
- DMX 来源实体点击“更新地面线”会重新读取保存的 DMX 文件路径；数模来源实体该按钮置灰。
- 保存 DWG 后重开并 `REGEN`，拉坡图实体和属性保持正常。

## V0.1.9 纵断面竖曲线验证范围

核心测试覆盖 `ProfileVerticalCurveCreateService` 从拉坡图地面线首末点创建默认设计线。

核心测试覆盖 `ProfileVerticalCurveCalculator` 的前后坡、坡度差、凸/凹曲线类型、`L/T/BVC/EVC`、任意桩号设计高程、瞬时坡度、高点或低点计算，以及曲线越过相邻坡段时拒绝。

核心测试覆盖 `ProfileVerticalCurveEditService` 的 PVI 新增、删除、夹点移动和半径更新。

核心测试覆盖 `ProfileVerticalCurveDisplayPlanner` 的图形分段规则：直坡设计段为青色、曲线设计段为黄色，并为每个曲线元素输出两段理论切线。

核心测试覆盖 PROFILE 模块中 `RD_PROFILE_VERTICAL_CURVE_CREATE`、`RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE`、`RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE`、`RD_PROFILE_VERTICAL_CURVE_ADD_PVI` 和 `RD_PROFILE_VERTICAL_CURVE_DELETE_PVI` 的命令元数据。

核心测试覆盖托管 Ribbon 扩展源码中的竖曲线对象右键菜单注册约定：必须注册 AutoCAD 对象快捷菜单，并包含新增/删除 PVI 的上下文转发命令。

AutoCAD 图形界面需要手工验证 `RD_PROFILE_VERTICAL_CURVE_CREATE`、`RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE`、`RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE`、`RD_PROFILE_VERTICAL_CURVE_ADD_PVI`、`RD_PROFILE_VERTICAL_CURVE_DELETE_PVI` 和 `DnProfileVerticalCurveEntity`：

- 选择纵断面拉坡图后，可创建默认连接地面线起终点的竖曲线实体。
- 竖曲线在拉坡图坐标系中显示，保存 DWG 后重开并 `REGEN` 仍保持可见。
- 竖曲线直坡设计段为青色，曲线段为黄色，BVC/PVI/EVC 理论切线可见。
- 双击竖曲线可打开 WPF 编辑窗口，修改起终点、PVI 高程和半径后刷新实体。
- 起点、终点、PVI 和半径夹点可拖动，拖动后图形刷新且无索引错位。
- 选中竖曲线实体后右键，可看到 `新增竖曲线变坡点` 和 `删除竖曲线变坡点` 菜单项。
- 通过右键菜单或命令行执行新增 PVI 和删除最近 PVI，可正确更新实体。

## V0.1.10 路基模板验证范围

核心测试覆盖 `SubgradeTemplateDefaults` 的高速公路、城市快速路以及二级、三级、四级道路默认部件、颜色约定、显示比例、变宽表宽度计算、坡度变化表取值、路面结构层模板引用归一化和 `SubgradeTemplateCreateService` 默认创建结果。

核心测试覆盖 `CROSS_SECTION` 模块中 `RD_SECTION_SUBGRADE_TEMPLATE_CREATE`、`RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE` 和 `RD_SECTION_SUBGRADE_TEMPLATE_APPLY_DIALOG_FILE` 的命令元数据、模块启动注册和托管 Ribbon 源码中的 `DNSUBGRADETEMPLATEENTITY` 双击编辑入口。

AutoCAD 图形界面需要手工验证 `RD_SECTION_SUBGRADE_TEMPLATE_CREATE`、`RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE`、`RD_SECTION_SUBGRADE_TEMPLATE_APPLY_DIALOG_FILE` 和 `DnSubgradeTemplateEntity`：

- 点击 `RoadProto / 横断面设计 / 创建路基模板` 后，命令只要求点取插入点，不要求选择道路中线。
- 参数窗口可修改模板名称、显示比例、道路等级和左右侧部件参数。
- 预览图中线清晰可见，部件宽度和坡度文字不遮挡右侧 UI，点选部件、左右按钮、新增和删除部件可正常工作。
- 变宽表和坡度变化数据表二级窗口可新增、删除和保存桩号数据；坡度选择变化值时固定坡度输入置灰。
- 任意部件类型均可勾选启用路面结构层模板，并通过“选择结构层模板”点选 DWG 中的 `DnPavementLayerTemplateEntity` 回填模板 handle 和名称；“清除结构层模板”会清空当前部件绑定。
- 确认后图中生成 `DnSubgradeTemplateEntity`，显示中线和左右侧路基部件，部件标注使用中文。
- 双击实体可重新打开同一参数窗口，并显示上次保存的配置；修改后实体刷新。
- 保存 DWG 后重开并 `REGEN`，路基模板实体和参数保持正常。

## V0.1.11 横断面戴帽道路模型验证范围

核心测试覆盖 `RoadModelTemplateResolver` 的模板优先级解析、`RoadModelStationSampler` 的采样点收集、`RoadModelBuilder` 的三维部件线、断面节点链、网格线框生成和 `RoadModelBuildService` 的配置校验。

核心测试覆盖 `CROSS_SECTION` 模块中 `RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_EDIT`、`RD_SECTION_ROAD_MODEL_EDIT_HANDLE` 和 `RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE` 的命令元数据、业务文档路径和 Ribbon 可见入口。

核心测试通过源码契约覆盖 `RoadModelDialogBridge`、`DnRoadModelEntity`、`DnSubgradeTemplateEntity` 和 `ObjectArxRoadModelCommand` 的关键 ObjectARX 接入点：请求/响应文件字段、道路模型实体 DWG 持久化、三维网格线框绘制、路基模板移动夹点、初始化卸载、创建/编辑/回写命令流程、行内点选模板和竖曲线归属校验。

托管桥接测试覆盖道路模型 WPF 请求/响应文件读写、点选模板动作字段和行号字段，并检查 `RoadModelWindow.xaml` 中只读道路中线 handle 文本框必须使用 OneWay 绑定，避免打开横断面戴帽窗口时触发 WPF TwoWay 绑定只读属性异常。

托管 bridge 测试覆盖道路模型 WPF 请求/响应文件的 UTF-8 读写、转义、`assignmentCount`、InvariantCulture 数值解析和缺失 `responsePath` 拒绝。

核心测试覆盖 `RoadModelSectionPreviewBuilder` 的路基模板线、边坡模板线、TIN 地面线预览构建和实体内地面快照优先读取，并覆盖 `RD_SECTION_ROAD_MODEL_VIEW_SECTION` 命令元数据、业务文档路径、Ribbon 入口、ObjectARX 查看命令流程和查看横断面 WPF Bridge 源码契约。

托管 bridge 测试覆盖查看横断面请求文件的 UTF-8 读写、转义、InvariantCulture 数值解析、预览/线段/点数量字段，以及 `RoadModelSectionViewerWindow.xaml` 的桩号列表、预览画布和图例源码契约。

AutoCAD 图形界面需要手工验证 `RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_EDIT`、`RD_SECTION_ROAD_MODEL_EDIT_HANDLE`、`RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE` 和 `DnRoadModelEntity`：

- 点击 `RoadProto / 横断面设计 / 横断面戴帽` 后，命令要求选择道路中线。
- 同一中线只有一条关联竖曲线时可自动匹配；没有唯一竖曲线时提示选择竖曲线。
- 选择不属于当前中线的竖曲线时应拒绝生成模型。
- WPF `路基模板` tab 可编辑起终点桩号、模板 handle、模板名称和行优先级，并可在某一行点选图中路基模板实体回填 handle 和名称。
- 点击 `生成模型` 后图中生成 `DnRoadModelEntity`，并绘制由横断面肋线、纵向连接线、最外侧边界线、端部封闭线和过渡线组成的三维道路模型线框。
- 双击道路模型或运行 `RD_SECTION_ROAD_MODEL_EDIT` 可重新打开同一窗口，保留并调整上次保存的模板范围。
- 运行 `RD_SECTION_ROAD_MODEL_VIEW_SECTION` 并选择道路模型后，应打开 `查看横断面` 窗口；切换桩号时，预览图显示当前桩号的路基模板线、边坡模板线和生成时地面线快照。
- 保存 DWG 后重开并 `REGEN`，道路模型实体和三维网格线框保持正常。
- 选择路基模板实体时应出现插入点夹点，拖动后模板整体位置随夹点移动。

## V0.1.19 路面结构层模板验证范围

核心测试覆盖 `PavementLayerTemplateDefaults`、`PavementLayerTemplateRules` 和 `PavementLayerTemplateCreateService`，包括结构层类型编码、中文显示名、等厚/非等厚厚度归一化、内外侧加宽、显示比例校验和横断面预览几何。

核心测试覆盖 `RoadModelBuilder` 读取路基模板部件绑定的路面结构层模板并生成 `RoadModelWireLineKind::PavementLayer`，并验证左侧部件仍保持“内侧靠近道路中线、外侧远离道路中线”的语义。

核心测试覆盖 `CROSS_SECTION` 模块中 `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE`、`RD_SECTION_PAVEMENT_LAYER_TEMPLATE_EDIT_HANDLE` 和 `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_APPLY_DIALOG_FILE` 的命令元数据和业务文档路径，并通过源码契约检查 `DnPavementLayerTemplateEntity`、`PavementLayerTemplateDialogBridge` 和 `ObjectArxPavementLayerTemplateCommand`。

托管 bridge 测试覆盖路面结构层模板 WPF 请求/响应文件、`.rpavement.xml` 导入导出、非法 XML 拒绝、窗口 `SaveXml` / `ImportXml` 动作和 AutoCAD Ribbon/命令注册源码契约。

AutoCAD 图形界面需要手工验证 `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE`、`RD_SECTION_PAVEMENT_LAYER_TEMPLATE_EDIT_HANDLE`、`RD_SECTION_PAVEMENT_LAYER_TEMPLATE_APPLY_DIALOG_FILE`、`RD_SECTION_SUBGRADE_TEMPLATE_CREATE`、`RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_VIEW_SECTION` 和相关实体：

- 点击 `RoadProto / 横断面设计 / 创建路面结构层模板` 后，命令要求点取插入点并打开 WPF 路面结构层模板窗口。
- WPF 窗口可选择上面层、中面层、下面层、基层、底基层、垫层，并可切换“内外厚度是否一致”。
- 导出 `.rpavement.xml` 后再次导入，模板名称、显示比例、预览宽度、层类型、等厚/非等厚厚度、内外侧加宽和坡度保持一致。
- 双击 `DnPavementLayerTemplateEntity` 可重新打开同一窗口编辑并回写原实体。
- 在路基模板窗口中，任意部件类型均可点选该路面结构层模板实体并保存 handle 与名称。
- 横断面戴帽生成道路模型后，`DnRoadModelEntity` 中应显示结构层三维线框。
- 运行 `查看横断面` 后，预览图应显示路基模板线、结构层、边坡模板线和生成时地面线快照。

## V0.1.15 边坡模板与道路模型边坡验证范围

核心测试覆盖 `SlopeTemplateDefaults` 的填方/挖方默认部件、`SlopeTemplateRules` 的坡率/坡高/宽度三选二求解、搜索增值字段保留、最后一组护坡道+边坡识别和模板类型/部件类型编码转换。

核心测试覆盖 `RoadModelSlopeTemplateGroupResolver` 的模板组优先级和组内模板优先级，覆盖 `RoadModelBuilder` 从路基模板最外侧生成边坡线，以及用 TIN 地面剖面搜索交地后提前结束的结果。

核心测试覆盖 `CROSS_SECTION` 模块中 `RD_SECTION_SLOPE_TEMPLATE_CREATE`、`RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE` 和 `RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE` 的命令元数据和业务文档路径。

AutoCAD 图形界面需要手工验证 `RD_SECTION_SLOPE_TEMPLATE_CREATE`、`RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE`、`RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE`、`RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_EDIT` 和 `DnRoadModelEntity`：

- 点击 `RoadProto / 横断面设计 / 创建边坡模板` 后，命令要求点取插入点并打开 WPF 边坡模板窗口。
- WPF 边坡模板窗口可切换填方/挖方预设，新增、删除、移动部件，并在坡率、坡高、宽度三种编辑方式之间切换。
- 修改部件参数后，左侧线框预览实时更新。
- 确认后图中生成 `DnSlopeTemplateEntity`，双击可再次打开窗口编辑。
- 横断面戴帽 `边坡模板` tab 可分别配置左侧和右侧搜索宽度、模板组和组内多个模板。
- 模板组行内 `管理模板组` 按钮应能选中当前组，下方组内模板区应支持点选、删除、上移、下移和清空。
- 点击 `生成模型` 后，AutoCAD 状态栏应显示道路模型生成进度。
- 从图中点选边坡模板后，模板追加到当前侧当前模板组；生成道路模型后应在 `DnRoadModelEntity` 中看到三维边坡线框。
