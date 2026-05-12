# 纵断面设计模块

## 模块信息

- 模块名称：纵断面设计
- 模块编码：`PROFILE`
- 命令前缀：`RD_PROFILE_`
- 当前状态：纵断面拉坡图创建、实体显示、DMX 更新、WPF 属性编辑、竖曲线创建、竖曲线夹点/PVI 编辑、竖曲线 WPF 编辑和可见 Ribbon 入口已接入

## 命令清单

| 命令 | 显示名称 | 类型 | 文档 |
| --- | --- | --- | --- |
| `RD_PROFILE_GRADE_GRAPH_CREATE` | 纵断面拉坡图 | 用户命令 | `docs/business/profile/纵断面拉坡图_创建.md` |
| `RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE` | 按 handle 编辑纵断面拉坡图 | 内部桥接命令 | `docs/business/profile/纵断面拉坡图_属性编辑.md` |
| `RD_PROFILE_APPLY_DIALOG_FILE` | 应用纵断面拉坡图对话框结果 | 内部桥接命令 | `docs/business/profile/纵断面拉坡图_属性编辑.md` |
| `RD_PROFILE_VERTICAL_CURVE_CREATE` | 创建竖曲线 | 用户命令 | `docs/business/profile/竖曲线_创建.md` |
| `RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE` | 按 handle 编辑竖曲线 | 内部桥接命令 | `docs/business/profile/竖曲线_编辑.md` |
| `RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE` | 应用竖曲线对话框结果 | 内部桥接命令 | `docs/business/profile/竖曲线_编辑.md` |
| `RD_PROFILE_VERTICAL_CURVE_ADD_PVI` | 新增竖曲线变坡点 | 用户/上下文命令 | `docs/business/profile/竖曲线_夹点与右键编辑.md` |
| `RD_PROFILE_VERTICAL_CURVE_DELETE_PVI` | 删除竖曲线变坡点 | 用户/上下文命令 | `docs/business/profile/竖曲线_夹点与右键编辑.md` |

## Ribbon

- C++ Ribbon model：`RoadProto / 纵断面设计 / 纵断面拉坡图`
- 可见 AutoCAD WPF Ribbon：`RoadProto / 纵断面设计 / 纵断面拉坡图`、`创建竖曲线`
- 托管 Ribbon 插件文件：`src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`

## 代码落点

| 层 | 代码 | 职责 |
| --- | --- | --- |
| domain | `src/domain/profile/ProfileDmxFile.*` | DMX 文件读取和校验 |
| domain | `src/domain/profile/ProfileGradeGraphModel.h` | 拉坡图领域数据模型 |
| domain | `src/domain/profile/ProfileGradeGraphLayout.*` | 坐标映射和网格范围计算 |
| domain | `src/domain/profile/ProfileVerticalCurveModel.h` | 竖曲线控制点、PVI、半径和显示属性模型 |
| domain | `src/domain/profile/ProfileVerticalCurveCalculator.*` | 竖曲线重建、BVC/EVC、高低点、高程和坡度计算 |
| application | `src/application/profile/ProfileGradeGraphCreateService.*` | 创建流程服务 |
| application | `src/application/profile/ProfileVerticalCurveCreateService.*` | 从拉坡图首末地面线创建默认竖曲线 |
| application | `src/application/profile/ProfileVerticalCurveEditService.*` | WPF 回写、夹点移动、PVI 增删和半径更新服务 |
| modules | `src/modules/profile/ProfileModule.*` | 模块、命令和 C++ Ribbon 元数据注册 |
| startup | `src/app/startup/ProfileStartupRegistration.*` | 启动期注册 PROFILE 模块 |
| cad_adapter | `src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.*` | 命令交互、数模采样、DMX 选择、实体插入和回写 |
| cad_adapter | `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.*` | 自定义实体显示、DWG 持久化和变换 |
| cad_adapter | `src/cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.*` | WPF 请求/响应文件 Bridge |
| cad_adapter | `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.*` | 竖曲线创建、编辑、PVI 增删和回写命令 |
| cad_adapter | `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.*` | 竖曲线自定义实体、DWG 持久化、图面映射、夹点和绘制 |
| cad_adapter | `src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.*` | 竖曲线 WPF 请求/响应文件 Bridge |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/ProfileGradeGraphWindow.xaml` | 属性窗口 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/ProfileGradeGraphDialogCommands.cs` | WPF 弹窗命令 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/ProfileVerticalCurveWindow.xaml` | 竖曲线编辑窗口 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/ProfileVerticalCurveDialogCommands.cs` | 竖曲线 WPF 弹窗命令 |

## 功能文档

- 创建：`docs/business/profile/纵断面拉坡图_创建.md`
- 属性编辑和更新地面线：`docs/business/profile/纵断面拉坡图_属性编辑.md`
- 竖曲线创建：`docs/business/profile/竖曲线_创建.md`
- 竖曲线编辑：`docs/business/profile/竖曲线_编辑.md`
- 竖曲线夹点与右键编辑：`docs/business/profile/竖曲线_夹点与右键编辑.md`

## 边界

- `domain/profile` 不依赖 ObjectARX，负责文件格式、数据结构和布局规则。
- `application/profile` 只把已准备好的道路信息、来源信息和样本数据组装为拉坡图数据，不做 CAD 选择。
- `cad_adapter/objectarx/profile` 承担 AutoCAD 选择、文件对话框、数模高程查询、自定义实体和命令行输出。
- WPF 只展示和收集属性，不直接操作 `AcDbEntity`、`AcDbObjectId`、`ads_name`。
- 竖曲线与拉坡图通过 handle 建立第一版关联；路线、地形或拉坡图变化后的自动重建暂未实现，后续应通过统一关系管理机制表达依赖和重建请求。
