# 纵断面竖曲线复用说明

## 能力范围

纵断面竖曲线能力用于在拉坡图上表达道路设计高程线。当前沉淀的复用能力包括领域模型、对称二次抛物线计算、编辑服务、ObjectARX 自定义实体、夹点编辑和 WPF 参数 Bridge。

## 可复用能力

### 领域模型

- 源码：`src/domain/profile/ProfileVerticalCurveModel.h`
- 数据：起点、终点、PVI、半径、显示属性、BVC/EVC、高低点、采样点。
- 边界：领域模型不依赖 ObjectARX，不保存 CAD object id，只保存拉坡图 handle 等可序列化数据。

### 竖曲线计算

- 源码：`src/domain/profile/ProfileVerticalCurveCalculator.*`
- 能力：重建控制点、计算前后坡、坡度差、竖曲线类型、长度、切线长、BVC/EVC、高低点、任意桩号高程和瞬时坡度。
- 曲线规则：第一版使用对称二次抛物线，长度 `L = abs(i2 - i1) * R`，切线长 `T = L / 2`。
- 约束：PVI 必须位于起终点之间；曲线范围不能越过相邻坡段；采样间距必须为正。

### 创建和编辑服务

- 源码：`src/application/profile/ProfileVerticalCurveCreateService.*`、`src/application/profile/ProfileVerticalCurveEditService.*`
- 创建服务：从拉坡图地面线首末样本生成默认设计直线。
- 编辑服务：统一承接 WPF 回写、夹点移动、PVI 新增、PVI 删除和半径更新。
- 复用边界：服务不做 CAD 选择，不打开 DWG，不依赖 UI。

### ObjectARX 自定义实体

- 源码：`src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.*`
- 能力：DWG 持久化、关联拉坡图 frame 映射、世界绘制、几何范围、起终点/PVI/半径夹点。
- 坐标映射：正向使用拉坡图 `insertionPoint + xAxis * stationOffset + yAxis * elevationOffset`；反向按 `xAxis/yAxis` 二维基向量求逆，支持拉坡图实体被旋转或缩放后的点位反算。
- 边界：实体只保存竖曲线自身数据，不把设计线嵌入拉坡图实体。

### WPF 编辑 Bridge

- 源码：`src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.*`、`src/ui/wpf/RoadProto.Terrain.UI/ProfileVerticalCurveWindow.xaml`
- 能力：C++ 请求文件、pending 文件、WPF 参数窗口、响应文件和 C++ 回写命令。
- 可编辑字段：名称、起终点桩号/高程、PVI 桩号/高程、半径。
- 防护：C++ 响应解析采用严格数值解析和 PVI 数量上限；坏响应不写回实体。

## 适用场景

- 纵断面设计高程线绘制。
- 任意桩号设计高程查询。
- 横断面提取设计高程。
- 三维道路模型和土方计算前置设计线。
- 排水设计中的凹曲线低点查询。

## 当前限制

- 第一版只支持对称二次抛物线。
- 当前图形表达以曲线采样线和控制点标记为主，完整坡度、半径、BVC/EVC 和高低点标注后续补充。
- 右键菜单第一版提供托管转发命令，完整 AutoCAD 原生上下文菜单后续接入。
- 竖曲线与横断面、三维模型、土方和排水设计的自动联动尚未接入。

## 后续扩展

- 非对称竖曲线。
- 竖曲线要素表。
- 多设计线。
- 标注样式与图层标准。
- 与实体关系管理的自动重建和脏标记联动。
