# 纵断面拉坡图实体创建设计

## 背景

RoadProto 已有地形 TIN 数模和道路中线初版。纵断面第一步需要建立“纵断面拉坡图”对象：选择道路中线后，根据关联数模或外部纵地面线文件得到桩号-地面高程数据，并在图纸中生成可保存、可双击编辑的自定义实体。

本设计只覆盖纵断面拉坡图实体创建和属性编辑，不包含纵断面竖曲线设计。

## 目标

- 新增 `PROFILE` 模块和命令 `RD_PROFILE_GRADE_GRAPH_CREATE`。
- 在 AutoCAD 可见 Ribbon 的 `RoadProto` 选项卡下新增 `纵断面设计` 面板，并把 `纵断面拉坡图` 命令按钮放入该面板，按钮样式和注册方式参考已有 `数模`、`平面设计` 面板。
- 支持从道路中线关联的 `DnTerrainTinEntity` 探测地面高程。
- 支持道路中线没有关联数模时，从 `.dmx` 纵地面线文件读取桩号-高程数据。
- 生成 `DnProfileGradeGraphEntity` 自定义实体，绘制表头、网格线和地面线折线。
- 双击实体打开 WPF 属性窗口，支持修改地面线颜色、线宽、取样精度、纵向比例和网格间距。
- DMX 来源的实体保存 DMX 文件路径，并可通过“更新地面线”重新读取文件刷新数据。
- 数模来源的实体将“更新地面线”交互置灰，避免与后续路线/数模联动重建混淆。

## 非目标

- 不实现纵断面竖曲线设计。
- 不实现坡度线、变坡点、竖曲线半径和设计高程。
- 不实现路线或数模变化后的自动 reactor 重建。
- 不实现复杂断链坐标展开；第一版只保存断链号并按桩号数值绘图。
- 不把地面线拆成普通 AutoCAD 子实体；第一版由自定义实体统一绘制。

## 架构

采用完整分层的 `PROFILE` 模块：

| 层 | 路径 | 职责 |
| --- | --- | --- |
| 领域层 | `src/domain/profile` | DMX 解析、地面线点数据、纵向比例、网格范围和坐标映射规则。 |
| 应用层 | `src/application/profile` | 组织从数模取高或从 DMX 读入的拉坡图数据创建流程。 |
| 模块层 | `src/modules/profile` | 注册 `PROFILE` 模块、命令元数据和 Ribbon 面板。 |
| CAD 适配层 | `src/cad_adapter/objectarx/profile` | 选择道路中线、解析数模 handle、选择 DMX 文件、点选插入位置、实体绘制和 DWG 持久化。 |
| WPF/UI 层 | `src/ui/wpf/RoadProto.Terrain.UI` | Ribbon 按钮、双击转发、属性窗口、Bridge DTO 和回写命令。 |
| 测试 | `tests/core_tests.cpp`、`tests/RoadProtoManagedBridgeTests` | 不依赖 AutoCAD 的 DMX、坐标映射、命令元数据和 WPF Bridge 文件协议测试。 |

命令命名：

- `RD_PROFILE_GRADE_GRAPH_CREATE`：创建纵断面拉坡图。
- `RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE`：内部双击命令，按 handle 打开属性窗口。
- `RD_PROFILE_APPLY_DIALOG_FILE`：内部 WPF Bridge 回写命令。

Ribbon 入口：

- 选项卡：`RoadProto`。
- 新增面板：`纵断面设计`。
- 按钮：`纵断面拉坡图`。
- 命令：`RD_PROFILE_GRADE_GRAPH_CREATE`。
- 样式：沿用已有托管 Ribbon 按钮的 `RibbonItemSize.Standard`、`Orientation.Horizontal` 和内置小图标策略。

## 数据模型

领域层新增地面线数据结构：

- `ProfileGroundSourceType`
  - `TerrainTin`
  - `DmxFile`
- `ProfileGroundSample`
  - `double station`
  - `double elevation`
  - `std::wstring rawStationText`
  - `int breakChainIndex`
- `ProfileGradeGraphProperties`
  - `std::wstring graphName`
  - `int groundLineColorIndex`
  - `double groundLineWidth`
  - `double sampleInterval`
  - `double verticalScale`
  - `double gridSpacing`
- `ProfileGradeGraphData`
  - 来源类型、路线 handle、数模 handle、DMX 文件路径、地面线点列和显示属性。

`DnProfileGradeGraphEntity` 保存：

- `graphName`：默认 `道路名称 + 拉坡图`。
- `roadCenterlineHandle`：来源道路中线 handle。
- `groundSourceType`：`TerrainTin` 或 `DmxFile`。
- `terrainTinHandle`：数模来源时保存。
- `dmxFilePath`：DMX 来源时保存。
- `insertionPoint`：用户点选的拉坡图左下基准点。
- `groundSamples`：桩号-高程数据。
- 显示属性：地面线颜色、线宽、地面线精度、纵向比例、网格线间距。

## DMX 文件规则

DMX 是纵地面线文件，扩展名为 `.dmx`。每个有效数据行格式为：

```text
ZH H
```

- `ZH`：桩号。
- `H`：地面高程。
- 断链桩号写法为 `桩号_断链号`，例如 `37123.456_2`。
- 空行和以 `//` 开头的说明行忽略。
- 同一桩号多条记录按文件顺序保留，不合并。
- 解析失败的行跳过并计数。
- 有效点少于 2 个时创建失败。
- 第一版绘图 x 坐标使用 `ZH` 中下划线前的桩号数值，断链号保存到 `breakChainIndex`，原始桩号保存到 `rawStationText`。

示例：

```text
0.00000000 21.25100000
2.70000000 19.95400000
2.70000000 19.93400000
37123.456_2 36.12000000
```

## 绘图规则

坐标映射：

- x 方向固定按桩号 1:1 绘制：`x = insertionX + station - minStation`。
- y 方向按高程差乘纵向比例：`y = insertionY + (elevation - baseElevation) * verticalScale`。
- `verticalScale` 下拉值：
  - `1:1` -> `1`
  - `1:10` -> `10`
  - `1:100` -> `100`
- `baseElevation = floor(minElevation / gridSpacing) * gridSpacing`。
- 当地面线高程刚好落在同一网格线上导致图形高度差为 `0` 时，图框高度至少保留一个高程网格间距，保证平地面线也有可见网格。

默认值：

- 地面线颜色：青绿色。
- 地面线宽度：`1`。
- 地面线精度：`10m`。
- 纵向比例：`1:10`。
- 网格线间距：`10m`。

图形内容：

- 顶部表头显示 `graphName`。
- 绘制外框。
- x 方向按桩号递增绘制网格，主桩线每 `100m` 标注 `K0`、`K1`、`K2`。
- y 方向按高程网格间距绘制并标注高程。
- 地面线按 `groundSamples` 顺序绘制折线；同一桩号不同高程会形成竖向折线段。

## 创建流程

1. 用户运行 `RD_PROFILE_GRADE_GRAPH_CREATE` 或点击 Ribbon `纵断面拉坡图`。
2. 命令提示选择 `DnRoadCenterlineEntity`。
3. 读取道路中线属性和 handle。
4. 如果道路中线已启用数模关联且 `linkedTerrainHandle` 有效：
   - 打开对应 `DnTerrainTinEntity`。
   - 按道路中线采样点和默认精度 `10m` 得到桩号和 XY。
   - 对每个 XY 调用 `SampleElevation(x, y)`。
   - TIN 外取不到高程的采样点跳过，命令行提示跳过数量。
   - 来源类型设为 `TerrainTin`，保存数模 handle。
5. 如果道路中线没有有效关联数模：
   - 弹出 `.dmx` 文件选择。
   - 解析 DMX 文件得到桩号-高程数据。
   - 来源类型设为 `DmxFile`，保存文件路径。
6. 命令提示用户点选拉坡图放置位置。
7. 创建 `DnProfileGradeGraphEntity`，写入来源、样本、插入点和默认属性。
8. 插入模型空间，命令行输出实体 handle 和有效地面线点数量。

## 双击编辑和更新

托管 Ribbon 插件监听 AutoCAD 双击事件，识别 `DNPROFILEGRADEGRAPHENTITY` 后发送：

```text
RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE <handle>
```

WPF 属性窗口显示：

- 拉坡图名称。
- 地面线颜色。
- 地面线宽度。
- 地面线精度。
- 纵向比例下拉：`1:1`、`1:10`、`1:100`。
- 网格线间距。
- DMX 文件路径。
- “更新地面线”按钮。

交互规则：

- DMX 来源：DMX 文件路径显示为可读字段，“更新地面线”按钮可用。点击后 C++ 侧重新读取实体保存的 `dmxFilePath`，刷新地面线样本。
- 数模来源：DMX 文件路径为空或只读，“更新地面线”按钮置灰。后续数模/路线更新应走统一关系重建机制，不复用该按钮。
- 修改显示属性后只刷新实体图形，不改变来源数据。
- 重新读取 DMX 失败时不覆盖实体原有地面线数据，并向命令行和窗口返回错误。

## 错误处理

- 未选择道路中线：命令取消并提示。
- 道路中线关联数模 handle 无效：按“无有效关联数模”处理，转入 DMX 文件选择。
- 用户取消 DMX 文件选择：命令取消并提示。
- DMX 有效点少于 2 个：命令失败并提示。
- 数模取高有效点少于 2 个：命令失败并提示。
- 用户取消插入点选择：命令取消。
- WPF 回写数据中线宽、精度、比例或网格间距非法时，C++ 侧使用实体原值或默认值兜底。

## 文档更新

实现时同步新增或更新：

- `docs/business/profile/纵断面拉坡图_创建.md`
- `docs/business/profile/纵断面拉坡图_属性编辑.md`
- `docs/modules/profile.md`
- `docs/modules/module_index.md`
- `docs/reuse/capability_catalog.md`
- `docs/dev/version_log.md`

业务文档粒度保持“创建”和“属性编辑/更新”分离。

## 测试计划

核心测试：

- DMX 解析支持普通桩号和断链桩号。
- DMX 忽略注释和空行。
- DMX 同桩号多高程按顺序保留。
- DMX 非法行计数且不阻断有效数据。
- 有效点少于 2 个时失败。
- 坐标映射满足 x 桩号 1:1，y 按纵向比例放大。
- `baseElevation` 按网格间距向下取整。
- `PROFILE` 模块和命令元数据注册正确，业务文档路径有效。

托管 Bridge 测试：

- WPF 请求文件能读取来源类型、DMX 路径和属性默认值。
- WPF 响应文件能写出显示属性和更新动作。

AutoCAD 手工验证：

- 选择有关联数模的道路中线生成拉坡图。
- 选择无关联数模的道路中线后选择 DMX 文件生成拉坡图。
- 点选放置位置后表头、网格和地面线显示正常。
- 双击拉坡图打开属性窗口。
- 修改颜色、线宽、比例、网格间距后实体刷新。
- DMX 来源点击“更新地面线”后重新读取文件并刷新。
- 数模来源“更新地面线”置灰。
- 保存 DWG、重开并 `REGEN` 后实体数据和显示保持正常。

## 后续扩展

- 接入统一实体关系管理，路线或数模变化后标记纵断面拉坡图需要重建。
- 支持数模来源手动重新取样。
- 支持断链展开规则和断链桩号标注。
- 增加坡度线、变坡点、竖曲线和设计高程。
- 增加纵断面出图格式、栏表和标注样式。
