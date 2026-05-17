# 版本记录

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
