# 纵断面竖曲线实施计划

> 本文件为历史实施计划的中文归档版。以后 `docs/superpowers/plans/` 下的计划文档统一使用中文编写；代码标识、命令名、路径和构建命令保持原文。

## 目标

实现纵断面竖曲线原型，基于纵断面拉坡图创建独立 `DnProfileVerticalCurveEntity`，支持计算、显示、夹点编辑、PVI 增删、WPF 编辑和右键菜单入口。

## 主要任务

- 在 `domain/profile` 中实现竖曲线模型、计算器、显示计划和编辑服务。
- 在 `application/profile` 中实现默认竖曲线创建服务。
- 在 `cad_adapter/objectarx/profile` 中实现竖曲线实体、创建命令、编辑命令、PVI 增删命令和 WPF Bridge。
- 在托管 WPF 中新增竖曲线编辑窗口、文件 Bridge、Ribbon 按钮、双击编辑和右键菜单转发。
- 更新 `PROFILE` 模块命令元数据、业务文档、复用说明、版本记录和测试说明。

## 测试与验证

- 核心测试覆盖 BVC/PVI/EVC、凸凹曲线、高低点、任意桩号高程和坡度、半径更新、PVI 增删和命令元数据。
- 托管 Bridge 测试覆盖请求/响应文件读写。
- AutoCAD 手工验证创建、编辑、夹点、右键新增/删除 PVI、保存重开和图形刷新。
