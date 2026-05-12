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

- 版本：`v0.1.9`
- 构建日期：`20260511`
- 阶段：`ProfileGradeGraph`
- ARX 文件名：`RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx`
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
  - 竖曲线实体通过关联拉坡图的图面坐标系绘制，支持 DWG 持久化、起终点/PVI/半径夹点、PVI 新增和删除。
  - 双击竖曲线可打开 WPF 编辑窗口，修改名称、起终点、PVI 桩号/高程和半径；回写由 C++ application/domain 层校验后刷新实体。
  - 业务文档：`docs/business/profile/竖曲线_创建.md`、`docs/business/profile/竖曲线_编辑.md`、`docs/business/profile/竖曲线_夹点与右键编辑.md`
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
ARXLOAD artifacts\x64\Release\RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx
NETLOAD artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll
```

加载后可在 Ribbon 中打开 `RoadProto` 选项卡，点击 `数模` 面板下的 `地形构网`、`编辑数模`、`导出数模` 或 `导入数模`；也可点击 `平面设计` 面板下的 `平面布线`、`编辑平曲线参数`、`导出中线 ICD` 和 `导入中线 ICD`；纵断面入口位于 `纵断面设计` 面板下的 `纵断面拉坡图` 和 `创建竖曲线`。命令行可直接运行 `DN_TERRAIN_TIN_CREATE`、`DN_TERRAIN_TIN_EDIT`、`DN_TERRAIN_TIN_EXPORT`、`DN_TERRAIN_TIN_IMPORT`、`RD_ALIGN_CENTERLINE_CREATE`、`RD_ALIGN_CURVE_PARAM_EDIT`、`RD_ALIGN_CENTERLINE_EXPORT_ICD`、`RD_ALIGN_CENTERLINE_IMPORT_ICD`、`RD_PROFILE_GRADE_GRAPH_CREATE`、`RD_PROFILE_VERTICAL_CURVE_CREATE`、`RD_PROFILE_VERTICAL_CURVE_ADD_PVI` 和 `RD_PROFILE_VERTICAL_CURVE_DELETE_PVI`。数模流转文件后缀固定为 `.rmesh`，道路中线流转文件后缀固定为 `.icd`，纵断面地面线文件后缀固定为 `.dmx`。

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
