# RoadProto V0.1 设计说明

## 目标

建立一个面向 ObjectARX/C++ 的道路设计原型功能框架，为后续持续开发类似 EICAD 的能力打基础。本阶段只做框架，不实现复杂道路算法。

## 方案选择

采用分层 C++ 工程结构。core、domain、application 不依赖 ObjectARX；ARX 生命周期、编辑器输出和命令注册集中放在 ObjectARX 适配层。模块通过 `ModuleRegistry` 注册，命令只通过 `CommandRegistry` 注册，实体联动通过 `EntityRelationManager` 表达。

## 组成部分

- `app`：ARX 入口和启动上下文。
- `core`：命令注册、模块注册、配置、日志、版本、错误。
- `cad_adapter`：ObjectARX 封装边界。
- `domain`：实体 ID、实体描述、更新状态、依赖图。
- `application`：命令对应的功能流程。
- `modules`：地形数模和平交口示例模块。
- `ui`：按模块分组的 Ribbon 模型。
- `docs`：架构、业务、复用、版本、开发规则。

## V0.1 范围

- 包含地形数模和平交口两个示例模块。
- 至少包含一个可运行的统一注册命令。
- 包含地形更新影响纵断面、横断面的脏标记示例。
- 包含 Visual Studio 工程和版本化 ARX 输出命名。
- 包含不依赖 AutoCAD 的核心测试。

## 暂不包含

- 真实地形、路线、纵断面、横断面、算量或出图算法。
- DWG 中的实体关系持久化。
- 完整 AutoCAD 原生 Ribbon 创建。
