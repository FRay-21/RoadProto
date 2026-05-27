# 查看横断面预览与绘制优化设计

## 背景

`RD_SECTION_ROAD_MODEL_VIEW_SECTION` 当前已经能选择 `DnRoadModelEntity` 并打开 WPF 只读预览窗口，按道路模型保存的采样桩号查看路基模板线、结构层线、边坡模板线和地面线快照。现有预览图只能自适应显示，不能像路面结构层模板预览一样拖动和滚轮缩放；也没有把所有桩号横断面批量绘制到模型空间的入口。

本次优化在保留“WPF 只负责展示和触发、C++ ObjectARX 负责 CAD 选择和绘图”的边界下，补齐两个能力：

- 预览图支持鼠标拖动平移和滚轮缩放。
- 在查看窗口中增加 `绘制横断面` 按钮，用户点取模型空间基点后，将所有采样桩号的横断面图依次向上绘制。

## 用户流程

1. 用户运行 `RD_SECTION_ROAD_MODEL_VIEW_SECTION` 或点击 Ribbon `横断面设计 / 查看横断面`。
2. 选择已生成的 `DnRoadModelEntity`。
3. WPF 打开 `查看横断面` 窗口，左侧按桩号切换，右侧预览当前断面。
4. 用户可在预览图中滚轮缩放、鼠标拖动平移，切换桩号时保留当前视图交互状态。
5. 用户点击 `绘制横断面`。
6. WPF 关闭窗口并通过 Bridge 发送内部动作到 ObjectARX。
7. ObjectARX 提示用户在模型空间点取插入基点。
8. 选择基点后，系统把所有可预览桩号的横断面从基点开始按 Y 方向依次向上排列绘制，不重叠。
9. 每个横断面下方显示桩号文字，每个横断面生成一个外围框自定义实体，保存道路模型 handle、桩号和断面绘图信息，供后续面积标注等功能扩展。

## 方案

### WPF 预览交互

`RoadModelSectionViewerWindow` 复用路面结构层模板预览的交互模式：

- 使用 `_previewZoom` 和 `_previewPan` 保存当前预览视图。
- 鼠标滚轮以鼠标位置为锚点缩放。
- 鼠标左键拖动平移；中键也可平移，兼容路面结构层模板预览习惯。
- 画布 `ClipToBounds=true`，避免缩放和平移后图形溢出。
- 预览图按“填充在前、线条在后”的顺序绘制结构层，普通线段保持原有线条表达。

为了支持结构层填充，WPF 预览 DTO 继续读取现有 `PavementLayer` 线段。若同一结构层的四条边能组成闭合四边形，则先绘制弱化填充面，再绘制边线；无法识别闭合面时降级为线段显示。

### 绘制横断面动作

查看窗口增加 `绘制横断面` 按钮。Bridge 请求文件新增：

- `responsePath`：WPF 回写动作文件。
- `action`：`none` 或 `drawSections`。
- `accepted`：是否执行动作。
- `handle`：道路模型 handle。

WPF 点击 `绘制横断面` 时写出响应文件，并发送内部命令：

`RD_SECTION_ROAD_MODEL_VIEW_SECTION_APPLY_DIALOG_FILE`

ObjectARX 读取响应后：

- 校验 handle 对应 `DnRoadModelEntity`。
- 重新读取道路模型、道路中线和必要的地面快照/TIN 回退数据。
- 使用 `RoadModelSectionPreviewBuilder` 生成所有桩号预览。
- 提示用户点取模型空间基点。
- 将每个桩号转换为一个独立横断面绘图实体。

### CAD 横断面绘图实体

新增 `DnRoadModelSectionDrawingEntity`，一桩号一个实体。实体职责：

- 保存道路模型 handle、道路中线 handle、桩号、桩号文字、绘图比例、基点、外框范围和预览线段。
- 绘制外围框、结构层填充面、结构层边线、路基线、边坡线、地面线和桩号文字。
- 结构层填充和边线颜色优先来自路面结构层模板层定义；填充类型、填充角度和填充比例也随模板定义写入实体绘图数据。
- 没有可用模板显示字段的旧数据，至少使用道路模型中保存的结构层 RGB 弱化填充和边线。
- 预留字段或数据结构用于后续保存断面面积、面积标注位置等派生信息。

横断面实体只负责绘图表达和信息保存，不重新执行横断面戴帽，不修改源 `DnRoadModelEntity`。

### 排列规则

绘图命令以用户点取的基点作为第一张横断面外框左下角：

- 每个横断面按当前预览点集计算偏距/高程边界。
- 每张图设置固定内边距和桩号文字高度。
- 当前横断面实体的外框高度 = 断面几何高度 + 内边距 + 桩号文字区。
- 下一张横断面的基点 Y = 上一张外框顶边 + 固定间距。
- X 方向统一从基点开始，不按不同断面宽度左右漂移，避免横断面图互相重叠。

### 结构层模板显示信息

绘制到模型空间时需要结构层填充参数。`RoadModelData` 已保存结构层线 key 和层 RGB，但没有保存每层填充模式。ObjectARX 绘图前会根据 `RoadModelPavementLayerLineKey::pavementLayerTemplateHandle` 读取 `DnPavementLayerTemplateEntity`，按 `layerIndex` 找到对应层：

- `color`：使用层保存 RGB。
- `hatchPattern`：使用层填充类型。
- `hatchAngle`：使用层填充角度。
- `hatchScale`：使用层填充比例。

如果模板实体不存在、层索引越界或旧模型缺少连续结构层线 key，则降级使用预览 segment 的 RGB 和简化弱化填充。

## 文件与边界

- `domain/cross_section/RoadModel.*`：可新增纯领域绘图 DTO 或闭合结构层面识别帮助函数，不依赖 ObjectARX。
- `cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.*`：处理内部回写命令、选点、读取道路模型并创建绘图实体。
- `cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.*`：增加响应文件和绘制动作桥接。
- `cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.*`：新增模型空间横断面绘图自定义实体。
- `ui/wpf/RoadProto.Terrain.UI/RoadModelSectionViewerWindow.*`：增加预览平移缩放和绘制按钮。
- `ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewer*.cs`：增加 action/responsePath 读写。
- `ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadModelSectionViewerCommands.cs`：读取响应并发送内部回写命令。

WPF 不直接选择 CAD 对象、不直接创建实体、不直接读取 ObjectARX 类型。所有模型空间绘图和自定义实体创建都在 C++ ObjectARX adapter 层完成。

## 错误处理

- 用户取消点取基点：不绘制，命令提示取消。
- 道路模型 handle 无效：提示找不到道路模型。
- 没有任何可生成预览的桩号：提示无可绘制横断面。
- 结构层模板无法读取：不中断绘制，结构层降级为 RGB 弱化填充和线框，并在命令行提示。
- 某个桩号预览失败：跳过该桩号，最终提示成功绘制数量和跳过数量。

## 测试

- 核心测试覆盖新增命令元数据、Bridge 源码契约、`DnRoadModelSectionDrawingEntity` 源码契约、ARX 工程包含新实体文件、初始化/卸载注册新实体类。
- 领域测试覆盖从结构层预览线段识别闭合面、横断面绘图布局不重叠、桩号顺序向上排列。
- 托管 Bridge 测试覆盖 `responsePath`、`action=drawSections`、绘制按钮、鼠标滚轮缩放、鼠标拖动平移和发送内部回写命令。
- 构建验证运行核心测试、托管 bridge 测试、WPF Release 构建和 `RoadProto.sln` Release 构建。

## 文档同步

需要同步更新：

- `docs/business/cross_section/查看横断面.md`
- `docs/modules/cross_section.md`
- `docs/modules/module_index.md`
- `docs/reuse/road_model.md`
- `docs/reuse/capability_catalog.md`
- `docs/dev/version_log.md`
- `tests/README.md`
- `README.md`
