# RoadProto V0.1 框架实施计划

> 本文件为历史实施计划的中文归档版。以后 `docs/superpowers/plans/` 下的计划文档统一使用中文编写；代码标识、命令名、路径和构建命令保持原文。

## 目标

搭建 RoadProto V0.1 ObjectARX/C++ 原型框架，形成可持续扩展的模块、命令、Ribbon、文档和测试骨架。

## 主要任务

- 建立 `app`、`core`、`domain`、`application`、`modules`、`cad_adapter`、`ui`、`docs`、`tests` 等目录分层。
- 实现 `CommandRegistry`、`ModuleRegistry` 和 Ribbon 模型。
- 创建基础 ARX 入口、启动注册流程和示例模块。
- 建立业务文档、复用说明、版本记录和开发规则的初始结构。
- 增加核心测试项目，覆盖命令注册、模块注册和基础关系模型。

## 验证

- 使用 MSBuild 构建 `RoadProto.sln`。
- 运行核心测试项目。
- 在 AutoCAD 2021 中加载生成的 ARX，验证基础命令和 Ribbon 注册。
