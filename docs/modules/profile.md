# 纵断面设计模块

## 模块信息

- 模块名称：纵断面设计
- 模块编码：`PROFILE`
- 命令前缀：`RD_PROFILE_`
- 当前状态：纵断面拉坡图创建、实体显示、DMX 更新、WPF 属性编辑和可见 Ribbon 入口已接入

## 命令清单

| 命令 | 显示名称 | 类型 | 文档 |
| --- | --- | --- | --- |
| `RD_PROFILE_GRADE_GRAPH_CREATE` | 纵断面拉坡图 | 用户命令 | `docs/business/profile/纵断面拉坡图_创建.md` |
| `RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE` | 按 handle 编辑纵断面拉坡图 | 内部桥接命令 | `docs/business/profile/纵断面拉坡图_属性编辑.md` |
| `RD_PROFILE_APPLY_DIALOG_FILE` | 应用纵断面拉坡图对话框结果 | 内部桥接命令 | `docs/business/profile/纵断面拉坡图_属性编辑.md` |

## Ribbon

- C++ Ribbon model：`RoadProto / 纵断面设计 / 纵断面拉坡图`
- 可见 AutoCAD WPF Ribbon：`RoadProto / 纵断面设计 / 纵断面拉坡图`
- 托管 Ribbon 插件文件：`src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`

## 代码落点

| 层 | 代码 | 职责 |
| --- | --- | --- |
| domain | `src/domain/profile/ProfileDmxFile.*` | DMX 文件读取和校验 |
| domain | `src/domain/profile/ProfileGradeGraphModel.h` | 拉坡图领域数据模型 |
| domain | `src/domain/profile/ProfileGradeGraphLayout.*` | 坐标映射和网格范围计算 |
| application | `src/application/profile/ProfileGradeGraphCreateService.*` | 创建流程服务 |
| modules | `src/modules/profile/ProfileModule.*` | 模块、命令和 C++ Ribbon 元数据注册 |
| startup | `src/app/startup/ProfileStartupRegistration.*` | 启动期注册 PROFILE 模块 |
| cad_adapter | `src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.*` | 命令交互、数模采样、DMX 选择、实体插入和回写 |
| cad_adapter | `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.*` | 自定义实体显示、DWG 持久化和变换 |
| cad_adapter | `src/cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.*` | WPF 请求/响应文件 Bridge |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/ProfileGradeGraphWindow.xaml` | 属性窗口 |
| WPF | `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/ProfileGradeGraphDialogCommands.cs` | WPF 弹窗命令 |

## 功能文档

- 创建：`docs/business/profile/纵断面拉坡图_创建.md`
- 属性编辑和更新地面线：`docs/business/profile/纵断面拉坡图_属性编辑.md`

## 边界

- `domain/profile` 不依赖 ObjectARX，负责文件格式、数据结构和布局规则。
- `application/profile` 只把已准备好的道路信息、来源信息和样本数据组装为拉坡图数据，不做 CAD 选择。
- `cad_adapter/objectarx/profile` 承担 AutoCAD 选择、文件对话框、数模高程查询、自定义实体和命令行输出。
- WPF 只展示和收集属性，不直接操作 `AcDbEntity`、`AcDbObjectId`、`ads_name`。
- 路线移动后的自动重建暂未实现，后续应通过统一关系管理机制表达依赖和重建请求。
