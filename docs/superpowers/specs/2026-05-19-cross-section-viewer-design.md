# 查看横断面设计

## 目标

在横断面设计模块中新增“查看横断面”功能，用于检查已生成道路模型在不同采样桩号处的横断面样式。第一版只读现有 `DnRoadModelEntity` 的线框结果和配置，不重新执行横断面戴帽算法。

## 用户流程

1. 用户运行 `RD_SECTION_ROAD_MODEL_VIEW_SECTION`，或点击 Ribbon `RoadProto / 横断面设计 / 查看横断面`。
2. 程序提示用户选择一个 `DnRoadModelEntity`。
3. 系统读取道路模型保存的采样桩号、路基部件线、边坡部件线、道路中线 handle、竖曲线 handle 和搜索宽度。
4. 系统尽量从竖曲线所属拉坡图关联的 TIN 数模剖切当前桩号地面线。
5. 弹出 WPF 只读窗口，窗口主体为横断面预览图。
6. 用户在桩号列表或下拉框中切换桩号，预览图同步显示当前桩号的路基模板线、边坡模板线和地面线。

## 架构

- `domain/cross_section` 新增只读横断面预览模型和生成器，输入为道路模型数据、道路中线采样、可选 TIN 地面和目标桩号列表，输出为二维预览段。
- `cad_adapter/objectarx/cross_section` 负责选择 `DnRoadModelEntity`、读取关联中线/地形、组装预览请求并启动 WPF。
- `ui/wpf/RoadProto.Terrain.UI` 新增查看窗口和 Bridge DTO/文件读写。WPF 只做展示和桩号切换，不读取 CAD 实体。
- `modules/cross_section` 只注册命令元数据和 Ribbon 入口。

## 数据规则

- 新道路模型生成后保存 `sampledStations`，用于后续查看横断面。
- 旧道路模型没有保存 `sampledStations` 时，查看命令按道路模型配置、竖曲线和模板范围重新收集桩号。
- 横断面预览采用局部二维坐标：横向偏距为 X，设计高程或地面高程为 Y。
- 路基线和边坡线从道路模型三维线框中按当前桩号附近截取。若某条线在当前桩号无法插值，则跳过该线。
- 地面线从关联 TIN 沿当前桩号横断面剖切，剖切宽度使用道路模型保存的左、右边坡搜索宽度。
- 若没有可用 TIN，窗口仍打开，只显示路基和边坡，并在状态区提示“未找到地面线”。

## UI

窗口标题为 `查看横断面`。

主要区域：

- 顶部显示道路模型 handle、道路中线 handle、采样桩号数量。
- 左侧显示桩号列表，按工程桩号格式显示。
- 右侧为 Canvas 预览区。
- 预览区包含三类图层：
  - 路基模板：蓝色线。
  - 边坡模板：绿色线。
  - 地面线：棕色线。
- 底部显示当前桩号、线段数量和状态文本。

## 命令

- 用户命令：`RD_SECTION_ROAD_MODEL_VIEW_SECTION`
- 显示名：`查看横断面`
- 是否挂 Ribbon：是
- 业务文档：`docs/business/cross_section/查看横断面.md`

内部桥接命令：

- `RD_SECTION_ROAD_MODEL_VIEW_SECTION_SHOW_WPF_DIALOG`
- 该命令只由 C++ Bridge 发送给托管 WPF 插件，不作为用户命令注册到 C++ `CommandRegistry`。

## 验收

自动测试：

- 核心测试覆盖道路模型保存采样桩号。
- 核心测试覆盖 `RoadModelSectionPreviewBuilder` 能按桩号从三维线框生成二维横断面段。
- 核心测试覆盖命令注册、Ribbon 元数据和 ObjectARX 源码契约。
- 托管桥接测试覆盖查看横断面请求文件读写和 WPF 窗口关键控件。

手工验证：

- 先生成道路模型，再运行 `RD_SECTION_ROAD_MODEL_VIEW_SECTION`。
- 选择道路模型后窗口打开。
- 切换桩号时预览图变化。
- 有 TIN 时显示地面线，无 TIN 时仍显示路基和边坡线。
