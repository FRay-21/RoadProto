# RoadProto V0.1 架构说明

## 目标

V0.1 建立一个稳定的 ObjectARX/C++ 工程骨架，用于后续持续扩展道路设计原型功能。框架需要支持按模块、按命令、按领域对象、按业务文档逐步增长。

## 项目分层

| 分层 | 职责 | 是否依赖 ObjectARX |
| --- | --- | --- |
| `app` | ARX 入口、启动、卸载、运行上下文 | 仅 `arx_entry` 依赖 |
| `core` | 命令注册、模块注册、日志、配置、错误、版本 | 否 |
| `cad_adapter` | AutoCAD / ObjectARX 封装 | 是，仅 `objectarx` 子目录直接依赖 |
| `domain` | 道路设计对象、业务规则、关系模型 | 否 |
| `application` | 功能流程组织和 use case | 不直接依赖 ObjectARX |
| `modules` | 模块元数据、命令注册、Ribbon 注册 | 不直接依赖 ObjectARX |
| `ui` | Ribbon 模型、对话框、资源、AutoCAD 托管 Ribbon 插件 | 核心 UI 模型不依赖；AutoCAD 托管入口可依赖 AutoCAD .NET API |
| `docs` | 业务规则、复用说明、版本记录 | 否 |

## C++ ObjectARX 核心 + 可替换 UI 层

RoadProto 的道路设计原型功能采用“C++ ObjectARX 核心 + 可替换 UI 层”的架构原则。

- C++ ObjectARX 负责 CAD 核心能力：命令注册、Ribbon 入口、选择集、AcDb 图元读取、自定义实体、DWG 持久化、几何算法、三角网构建、CAD 内显示、数据查询接口和后续道路设计对象联动接口。
- WPF 只负责界面展示和用户交互：参数展示、统计信息展示、按钮交互、状态提示、错误提示和演示型 UI 美化。
- WPF 不得直接操作 `AcDbEntity`、`AcDbObjectId`、`ads_name` 等 ObjectARX 类型，也不得承载 CAD 核心业务逻辑。
- WPF 与 C++ ObjectARX 之间必须通过 Bridge / Adapter 层交互。Bridge 层只做 DTO 转换和调用转发，不写业务算法。
- 当前 RoadProto 原型阶段，用户对话框先统一由 WPF 实现，包括创建参数、编辑参数、属性设置和二级配置窗口。C++ ObjectARX 不新增当前阶段的正式参数对话框，只提供 CAD 交互、实体访问、预览绘制和 Bridge 调用能力。
- 可复用能力必须沉淀在 C++ Service / Entity / Data 层，保证后续 UI 从 WPF 替换为 MFC、Qt、AutoCAD Palette、其他 UI 技术栈或 EICAD 正式 UI 时，核心能力仍可复用。

当前 `DN_TERRAIN_TIN_CREATE` 已将点清洗、文字高程解析、Delaunay/TIN 构建、高程查询和自定义实体持久化放在 C++ 层；AutoCAD 可见 Ribbon 由 `RoadProto.Terrain.UI.dll` 创建，按钮只负责发送命令，ARX 会自动尝试加载同配置的托管 Ribbon 插件。历史版本中的 C++ 参数窗口属于过渡实现，不作为后续新增功能的模式；当前阶段后续新增或重做窗口应通过 WPF + Bridge 接入。若未来整体 UI 技术栈更换，也必须保持同样的 Bridge/Adapter 边界，核心算法和实体不需要迁移。

地形 TIN 的业务逻辑、CAD 功能逻辑和代码结构索引见 `docs/architecture/terrain_tin_code_structure.md`。后续维护地形模块时，应优先保持该文档中的边界：`domain` 沉淀算法和数据，`cad_adapter` 处理 ObjectARX 交互，`ui` 只做可替换入口和交互转发。

## 运行流程

1. AutoCAD 加载 `RoadProto_v0.1.6_20260508_TerrainTinDblClick.arx`。
2. `acrxEntryPoint` 调用 `app::initialize`。
3. 启动流程通过 `ModuleRegistry` 注册内置模块。
4. 模块通过 `CommandRegistry` 注册命令。
5. `RibbonRegistrationService` 根据模块和命令生成 Ribbon 模型。
6. ObjectARX 适配器把所有命令统一注册到 AutoCAD 命令组。
7. 命令回调只触发 application service，不直接堆业务逻辑。
8. application service 调用 domain 或 cad_adapter 完成流程。

## 扩展约定

新增模块通常只需要：

- 在 `src/modules/<module>` 增加模块定义。
- 在 `src/application/<module>` 增加流程服务。
- 必要时在 `src/domain/<module>` 增加领域对象或规则。
- 通过 `src/cad_adapter` 调用 CAD 能力。
- 补充业务文档和复用说明。

除非要修改插件生命周期，否则新增模块不应修改 `RoadProtoArxEntry.cpp`。
