# 纵断面设计模块

## 模块信息

- 模块名称：纵断面设计
- 模块编码：`PROFILE`
- 命令前缀：`RD_PROFILE_`
- 当前状态：增量原型实现

## 命令清单

| 命令 | 显示名称 | 类型 | 文档 |
| --- | --- | --- | --- |
| `RD_PROFILE_GRADE_GRAPH_CREATE` | 纵断面拉坡图 | 用户命令 | `docs/business/profile/纵断面拉坡图_创建.md` |
| `RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE` | 按 handle 编辑纵断面拉坡图 | 内部桥接命令 | `docs/business/profile/纵断面拉坡图_属性编辑.md` |
| `RD_PROFILE_APPLY_DIALOG_FILE` | 应用纵断面拉坡图对话框结果 | 内部桥接命令 | `docs/business/profile/纵断面拉坡图_属性编辑.md` |

## Ribbon

- C++ Ribbon model：`RoadProto / 纵断面设计 / 纵断面拉坡图`
- 可见 AutoCAD WPF Ribbon 扩展留待后续任务实现。

## 边界

当前模块只负责命令和 Ribbon 元数据注册。实体、CAD 选择/采样、WPF UI、可见 Ribbon C# 扩展和完整业务流程由后续任务补充。
