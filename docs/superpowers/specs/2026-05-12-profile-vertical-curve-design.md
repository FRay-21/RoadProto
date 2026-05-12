# 纵断面竖曲线实体设计

## 背景

RoadProto 已有道路中线、地形 TIN 和纵断面拉坡图原型。纵断面拉坡图负责表达地面线和图面坐标映射，下一步需要建立独立的竖曲线设计对象，用于表达道路纵断面上的设计高程线。

本设计覆盖“创建竖曲线”对象、核心计算接口、CAD 绘制接口、夹点和右键编辑、双击 WPF 参数编辑，以及后续可扩展的业务层接口。实现基线为 `codex/profile-grade-graph` worktree 中已有的 `DnProfileGradeGraphEntity`、`ProfileGradeGraphLayout` 和 WPF Bridge 文件协议。

## 目标

- 新增独立 `DnProfileVerticalCurveEntity` 自定义实体，关联一个 `DnProfileGradeGraphEntity` 的 handle。
- 新增命令 `RD_PROFILE_VERTICAL_CURVE_CREATE`，运行后选择纵断面拉坡图，默认生成一条连接地面线起终点的设计直线。
- 支持选中竖曲线后通过右键新增变坡点、删除变坡点。
- 支持起点、终点、PVI 和半径夹点拖动；拖动后竖曲线桩号范围允许比关联拉坡图更长或更短。
- 支持双击竖曲线实体打开 WPF 编辑窗口，左右切换不同变坡点，编辑 PVI 高程、桩号和竖曲线半径。
- 在 domain 层沉淀对称二次抛物线竖曲线计算能力，支持任意桩号设计高程、瞬时坡度、高点/低点、BVC/PVC 和 EVC/PVT 计算。
- 预留非对称竖曲线、多条设计线、要素表、横断面设计高程、三维道路模型、土方和排水设计联动扩展。

## 非目标

- 第一版不实现完整道路规范参数推荐。
- 第一版不实现非对称竖曲线，只保留数据结构扩展点。
- 第一版不实现完整竖曲线要素表实体，只提供要素表 DTO 和绘制预留。
- 第一版不自动裁剪竖曲线到拉坡图桩号范围。
- 第一版不把竖曲线数据嵌入 `DnProfileGradeGraphEntity`，也不让拉坡图实体直接承担设计线编辑职责。

## 推荐架构

竖曲线采用独立实体方案：

```text
DnProfileGradeGraphEntity
  - 地面线样本
  - 图面插入点、局部坐标轴
  - 桩号/高程到图面坐标映射

DnProfileVerticalCurveEntity
  - profileGraphHandle
  - 起点、终点、PVI、半径和显示属性
  - 自身夹点、右键命令、双击编辑
```

独立实体的好处是选择、夹点、双击编辑和后续多设计线扩展都清晰。拉坡图继续作为底图和坐标映射来源，竖曲线作为设计高程对象，可以被横断面、三维模型、土方计算和排水设计独立引用。

## 分层落点

| 层 | 新增或修改路径 | 职责 |
| --- | --- | --- |
| domain | `src/domain/profile/ProfileVerticalCurveModel.h` | 竖曲线控制点、PVI、段落、显示属性和数据模型 |
| domain | `src/domain/profile/ProfileVerticalCurveCalculator.*` | 坡度、BVC/EVC、抛物线、高低点、任意桩号高程和坡度计算 |
| application | `src/application/profile/ProfileVerticalCurveCreateService.*` | 从拉坡图地面线首末点创建默认竖曲线 |
| application | `src/application/profile/ProfileVerticalCurveEditService.*` | 对话框回写、夹点移动、新增/删除 PVI 和半径编辑 |
| modules | `src/modules/profile/ProfileModule.*` | 注册竖曲线创建、编辑、回写和右键辅助命令 |
| cad_adapter | `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.*` | 自定义实体绘制、持久化、夹点和变换 |
| cad_adapter | `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.*` | CAD 选择、实体插入、右键命令、回写命令 |
| cad_adapter | `src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.*` | C++ 与 WPF 编辑窗口的请求/响应文件桥接 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/ProfileVerticalCurveWindow.xaml(.cs)` | 竖曲线编辑窗口 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileVerticalCurveDialogDtos.cs` | WPF Bridge DTO |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileVerticalCurveDialogFile.cs` | 请求/响应文件读写 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/ProfileVerticalCurveDialogCommands.cs` | 打开竖曲线编辑窗口的托管命令 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs` | Ribbon 按钮、双击识别和右键菜单入口 |

## 命令

| 命令 | 显示名 | 类型 | Ribbon | 文档 |
| --- | --- | --- | --- | --- |
| `RD_PROFILE_VERTICAL_CURVE_CREATE` | 创建竖曲线 | 用户命令 | 是 | `docs/business/profile/竖曲线_创建.md` |
| `RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE` | 按 handle 编辑竖曲线 | 内部桥接命令 | 否 | `docs/business/profile/竖曲线_编辑.md` |
| `RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE` | 应用竖曲线对话框结果 | 内部桥接命令 | 否 | `docs/business/profile/竖曲线_编辑.md` |
| `RD_PROFILE_VERTICAL_CURVE_ADD_PVI` | 新增竖曲线变坡点 | 用户/右键命令 | 否 | `docs/business/profile/竖曲线_夹点与右键编辑.md` |
| `RD_PROFILE_VERTICAL_CURVE_DELETE_PVI` | 删除竖曲线变坡点 | 用户/右键命令 | 否 | `docs/business/profile/竖曲线_夹点与右键编辑.md` |

## 领域数据结构

新增 `ProfileVerticalCurveModel.h`：

```cpp
namespace roadproto::domain::profile {

enum class VerticalCurvePointRole {
    Start,
    Pvi,
    End,
};

enum class VerticalCurveType {
    TangentOnly,
    Crest,
    Sag,
};

enum class VerticalCurveGripRole {
    StartPoint,
    EndPoint,
    PviPoint,
    RadiusPoint,
};

struct VerticalCurveControlPoint {
    VerticalCurvePointRole role = VerticalCurvePointRole::Pvi;
    double station = 0.0;
    double elevation = 0.0;
};

struct VerticalCurvePvi {
    double station = 0.0;
    double elevation = 0.0;
    double radius = 1000.0;
    bool radiusLocked = false;
};

struct VerticalCurveProperties {
    std::wstring name = L"竖曲线";
    int designLineColorIndex = 1;
    int tangentLineColorIndex = 8;
    int keyPointColorIndex = 2;
    double designLineWidth = 0.35;
    double sampleInterval = 5.0;
    bool showLabels = true;
    bool showTangentLines = true;
};

struct VerticalCurveKeyPoint {
    double station = 0.0;
    double elevation = 0.0;
    bool isHighPoint = false;
};

struct VerticalCurveElement {
    std::size_t pviIndex = 0;
    VerticalCurveType type = VerticalCurveType::TangentOnly;
    double pviStation = 0.0;
    double pviElevation = 0.0;
    double i1 = 0.0;
    double i2 = 0.0;
    double gradeDifference = 0.0;
    double radius = 0.0;
    double length = 0.0;
    double tangentLength = 0.0;
    double bvcStation = 0.0;
    double bvcElevation = 0.0;
    double evcStation = 0.0;
    double evcElevation = 0.0;
    std::optional<VerticalCurveKeyPoint> highLowPoint;
};

struct ProfileVerticalCurveData {
    std::wstring profileGraphHandle;
    std::vector<VerticalCurveControlPoint> controlPoints;
    std::vector<VerticalCurvePvi> pvis;
    VerticalCurveProperties properties;
    int version = 1;
};

} // namespace roadproto::domain::profile
```

控制点按桩号升序维护。起点和终点必须存在且不可删除。PVI 可新增、拖动和删除。半径保存在 PVI 上，第一版每个 PVI 使用对称二次抛物线。

## 核心计算接口

新增 `ProfileVerticalCurveCalculator.*`：

```cpp
class ProfileVerticalCurveCalculator final {
public:
    static ProfileVerticalCurveBuildResult rebuild(const ProfileVerticalCurveData& data);
    static ProfileVerticalCurveQueryResult elevationAt(const ProfileVerticalCurveBuildResult& built, double station);
    static ProfileVerticalCurveQueryResult gradeAt(const ProfileVerticalCurveBuildResult& built, double station);
    static ProfileVerticalCurveSampleResult sample(const ProfileVerticalCurveData& data, double interval);
    static ProfileVerticalCurveEditResult moveControlPoint(ProfileVerticalCurveData& data, std::size_t pointIndex, double station, double elevation);
    static ProfileVerticalCurveEditResult movePvi(ProfileVerticalCurveData& data, std::size_t pviIndex, double station, double elevation);
    static ProfileVerticalCurveEditResult updateRadius(ProfileVerticalCurveData& data, std::size_t pviIndex, double radius);
    static ProfileVerticalCurveEditResult insertPvi(ProfileVerticalCurveData& data, double station, double elevation, double radius);
    static ProfileVerticalCurveEditResult removePvi(ProfileVerticalCurveData& data, std::size_t pviIndex);
};
```

计算规则：

- 坡度使用小数表达，`0.03` 表示 `3%`。
- 相邻控制点之间的直线坡度为 `i = deltaElevation / deltaStation`。
- 每个 PVI 由前后相邻控制点形成前坡 `i1` 和后坡 `i2`。
- 坡度差 `A = i2 - i1`。
- 竖曲线类型：
  - `i2 < i1` 为凸形竖曲线。
  - `i2 > i1` 为凹形竖曲线。
  - `abs(A)` 小于容差时退化为直坡段。
- 第一版长度使用 `L = abs(A) * R`。
- 对称竖曲线切线长 `T = L / 2`。
- `BVC/PVC` 桩号为 `PVI - T`，高程按前坡线计算。
- `EVC/PVT` 桩号为 `PVI + T`，高程按后坡线计算。
- 曲线段按二次抛物线计算，任意点的高程和瞬时坡度由 `x = station - BVC` 推导。
- 当曲线范围内瞬时坡度为 0 时，自动计算最高点或最低点。

当 PVI 太接近起点、终点或相邻 PVI，导致 `BVC/EVC` 超出相邻坡段时，第一版先允许几何计算继续，但在 `warnings` 中返回“曲线段与相邻曲线可能重叠”。后续可增加约束求解器。

## 默认创建流程

1. 用户运行 `RD_PROFILE_VERTICAL_CURVE_CREATE` 或点击 Ribbon “创建竖曲线”。
2. 命令提示选择 `DnProfileGradeGraphEntity`。
3. CAD 适配层读取拉坡图 handle、地面线样本、插入点、局部坐标轴和图面属性。
4. `ProfileVerticalCurveCreateService::buildDefaultFromGraph()` 使用地面线首末样本创建默认数据：
   - 起点桩号 = 首个地面线样本桩号。
   - 起点高程 = 首个地面线样本高程。
   - 终点桩号 = 最后一个地面线样本桩号。
   - 终点高程 = 最后一个地面线样本高程。
   - PVI 列为空。
   - 名称默认“竖曲线”。
5. 插入 `DnProfileVerticalCurveEntity`，保存关联拉坡图 handle。
6. 实体以设计直线形式显示。

## CAD 绘制接口

`DnProfileVerticalCurveEntity` 绘制时打开关联拉坡图实体，并复用其图面映射规则：

- `x = station - profileLayout.minStation`。
- `y = (elevation - profileLayout.baseElevation) * profileGraph.verticalScale`。
- 图面点 = `profileInsertion + profileXAxis * x + profileYAxis * y`。

为了避免重复逻辑，建议从 `DnProfileGradeGraphEntity` 暴露只读查询：

```cpp
ProfileGradeGraphData graphData() const;
AcGePoint3d insertionPoint() const;
ProfileGraphDrawingFrame drawingFrame() const;
```

`ProfileGraphDrawingFrame` 包含 `minStation`、`baseElevation`、`verticalScale`、`xAxis`、`yAxis` 等绘制所需值。若不希望扩大实体公开接口，可在 `cad_adapter/objectarx/profile` 新增局部工具函数从拉坡图实体组装 frame。

绘制内容：

- 设计直坡线：连接起点、PVI 和终点的切线辅助线。
- 竖曲线曲线段：按 `sampleInterval` 采样绘制平顺曲线。
- 关键点：起点、终点、PVI、BVC/PVC、EVC/PVT、高点或低点。
- 标注：PVI 桩号和高程、前后坡度、`R/L/T`、BVC/EVC 桩号和高程、高点/低点信息。

竖曲线桩号范围允许超出拉坡图地面线范围。实体 extents 必须包含超出图框的设计线和标注。

## 夹点设计

竖曲线夹点分四类：

- `StartPoint`：拖动起点桩号和设计高程。
- `EndPoint`：拖动终点桩号和设计高程。
- `PviPoint`：拖动 PVI 桩号和设计高程。
- `RadiusPoint`：拖动 PVI 对应半径。

夹点坐标从图面坐标反算到桩号和高程：

```text
station = profileLayout.minStation + localX
elevation = profileLayout.baseElevation + localY / verticalScale
```

半径夹点默认放在 PVI 附近的竖曲线法向方向。拖动半径夹点时不改变 PVI 桩号和高程，只根据拖动距离更新 `radius`，再调用 `ProfileVerticalCurveCalculator::updateRadius()` 重算 `L/BVC/EVC` 和采样点。

夹点拖动规则：

- 起点和终点可拖动，不强制限制在拉坡图范围内。
- PVI 可拖动，拖动后按桩号重新排序。
- 起点必须小于终点；若拖动导致顺序无效，操作返回 `eInvalidInput` 并保持原数据。
- 半径必须大于最小值，例如 `1.0`。
- 拖动阶段使用 drag cache 预览，释放后写回实体正式数据。

## 右键新增和删除 PVI

托管插件在选中 `DNPROFILEVERTICALCURVEENTITY` 时增加右键菜单项：

- 新增变坡点。
- 删除当前变坡点。

右键菜单只发送命令，不写业务逻辑。C++ 命令负责选择实体、提示点取或命中最近 PVI、调用 application/domain 服务并刷新实体。

新增 PVI：

1. 运行 `RD_PROFILE_VERTICAL_CURVE_ADD_PVI`。
2. 用户选择竖曲线实体，或使用当前 implied selection。
3. 用户在图面点取 PVI 位置。
4. CAD 适配层把点反算为桩号和高程。
5. 默认半径取 `1000.0`，或取相邻 PVI 的半径。
6. 插入 PVI 后重算实体。

删除 PVI：

1. 运行 `RD_PROFILE_VERTICAL_CURVE_DELETE_PVI`。
2. 用户选择竖曲线实体。
3. 用户点取要删除的 PVI，或删除最近 PVI 夹点命中的 PVI。
4. 起点和终点不可删除。
5. 删除到无 PVI 时，实体退回起终点设计直线。

## 双击 WPF 编辑

托管插件识别 `DNPROFILEVERTICALCURVEENTITY` 双击后发送：

```text
RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE <handle>
```

C++ Bridge 写出 request 文件，WPF 打开 `ProfileVerticalCurveWindow`。窗口内容：

- 竖曲线名称。
- 关联拉坡图 handle。
- 当前 PVI 序号和 `上一个 / 下一个` 按钮。
- 当前 PVI 的桩号、高程、半径 `R` 输入框。
- 起点和终点桩号/高程编辑入口。
- 只读结果：`i1`、`i2`、`A`、`L`、`T`、曲线类型、BVC/PVC、EVC/PVT、高点/低点。

点击确定后，WPF 写 response 文件。C++ 读取 response，调用 `ProfileVerticalCurveEditService::applyDialogEdit()`，校验通过后写回实体并刷新显示。

WPF 不直接操作 `AcDbEntity`、`AcDbObjectId` 或 ObjectARX 类型。

## 业务层接口

`ProfileVerticalCurveCreateService`：

```cpp
struct ProfileVerticalCurveCreateInput {
    std::wstring profileGraphHandle;
    std::vector<domain::profile::ProfileGroundSample> groundSamples;
};

struct ProfileVerticalCurveCreateResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::profile::ProfileVerticalCurveData data;
};

class ProfileVerticalCurveCreateService {
public:
    ProfileVerticalCurveCreateResult buildDefaultFromGraph(const ProfileVerticalCurveCreateInput& input) const;
};
```

`ProfileVerticalCurveEditService`：

```cpp
class ProfileVerticalCurveEditService {
public:
    ProfileVerticalCurveEditResult applyDialogEdit(ProfileVerticalCurveData& data, const ProfileVerticalCurveDialogEdit& edit) const;
    ProfileVerticalCurveEditResult applyGripMove(ProfileVerticalCurveData& data, const ProfileVerticalCurveGripEdit& edit) const;
    ProfileVerticalCurveEditResult addPvi(ProfileVerticalCurveData& data, double station, double elevation, double radius) const;
    ProfileVerticalCurveEditResult deletePvi(ProfileVerticalCurveData& data, std::size_t pviIndex) const;
};
```

这些服务不读取 CAD 实体，不弹窗口，不调用 ObjectARX。CAD 选择、点取、handle 解析和实体打开都在 `cad_adapter/objectarx/profile`。

## 联动关系

第一版实体保存关联拉坡图 handle。创建时应在关系管理机制中表达：

```text
ProfileVerticalCurve --depends on--> ProfileGradeGraph
```

当拉坡图地面线、图面比例或插入位置变化时，后续应标记竖曲线需要图形刷新或重建。V0.1 可先在文档中记录关系，待关系持久化机制完善后接入 DWG 字典或扩展字典。

后续下游依赖：

- 横断面设计高程查询。
- 道路三维模型。
- 土方计算。
- 排水设计低点识别。
- 纵断面出图和要素表。

## 持久化

`DnProfileVerticalCurveEntity` DWG 持久化字段：

- 实体版本号。
- 关联拉坡图 handle。
- 实体名称和显示属性。
- 起点、终点控制点。
- PVI 数组：桩号、高程、半径、半径锁定标记。
- 可选扩展字段：非对称曲线参数、标注样式 ID、要素表显示开关。

读入旧版本时应兜底：

- 缺少 PVI 时显示起终点直线。
- 缺少半径时使用默认半径 `1000.0`。
- 关联拉坡图找不到时实体仍保存数据，但绘制阶段输出最小可见提示或不绘制设计线。

## 错误处理

- 未选择拉坡图：命令取消并提示。
- 拉坡图地面线少于两个有效样本：创建失败。
- 关联拉坡图 handle 失效：竖曲线实体保留数据，但无法按图面坐标绘制。
- 起终点桩号相同或顺序无效：拒绝编辑。
- PVI 桩号与起终点关系无效：拒绝编辑或返回警告。
- 半径非有限或小于最小值：拒绝编辑。
- WPF response 文件读取失败：不修改实体。

## 文档更新

实现时新增：

- `docs/business/profile/竖曲线_创建.md`
- `docs/business/profile/竖曲线_编辑.md`
- `docs/business/profile/竖曲线_夹点与右键编辑.md`
- `docs/reuse/profile_vertical_curve.md`

实现时更新：

- `docs/modules/profile.md`
- `docs/modules/module_index.md`
- `docs/reuse/capability_catalog.md`
- `docs/dev/version_log.md`
- `tests/README.md`
- `README.md`

## 测试计划

核心测试：

- 默认创建时，起点和终点来自拉坡图地面线首末样本。
- 无 PVI 时，任意桩号设计高程按直线插值。
- 单 PVI 时能计算 `i1`、`i2`、`A`、`L`、`T`、BVC/PVC 和 EVC/PVT。
- `i2 < i1` 判为凸形，`i2 > i1` 判为凹形。
- 任意桩号设计高程和瞬时坡度符合对称二次抛物线公式。
- 曲线范围内坡度为 0 时返回高点或低点。
- 移动起点、终点和 PVI 后重算数据。
- 半径夹点更新半径后重算 `L/T/BVC/EVC`。
- 新增 PVI 后按桩号排序并保持起终点。
- 删除 PVI 后相邻坡段重连；删除到无 PVI 时退回直线。
- 竖曲线桩号范围可超出拉坡图地面线范围。
- PROFILE 模块注册新增命令且业务文档路径正确。

WPF Bridge 测试：

- request 文件能表达多个 PVI、当前索引、只读计算结果和 response 路径。
- response 文件能回写名称、起终点、PVI 桩号、高程和半径。
- 取消窗口时不修改实体。

AutoCAD 手工验证：

- 运行 `RD_PROFILE_VERTICAL_CURVE_CREATE` 选择拉坡图后生成设计直线。
- 新增 PVI 后显示前坡、后坡、竖曲线曲线段和关键点标注。
- 拖动起点、终点、PVI 和半径夹点时图形动态预览，释放后实体刷新。
- 右键新增和删除 PVI 可用。
- 双击竖曲线打开 WPF 编辑窗口，左右切换 PVI 并修改高程/半径。
- 保存 DWG 后重开并 `REGEN`，竖曲线数据和显示保持正常。

## 后续扩展

- 增加非对称竖曲线参数，如前切线长、后切线长分别控制。
- 增加竖曲线要素表实体和导出能力。
- 支持多条设计线和不同方案比选。
- 支持与横断面设计高程、道路三维模型、土方计算和排水设计联动。
- 支持规范参数推荐和最小半径校核。
- 支持从外部纵断面设计文件导入 PVI 和半径。
