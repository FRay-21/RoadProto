# Codex 项目工作入口

本文件是 RoadProto 项目的 AI 开发入口文件。每次使用 Codex 进入本项目时，先读本文件，再按下面规则继续读取相关文档。

## 每次必读的三个文件

每次开始修改前，必须先阅读这三个文件：

1. `README.md`
   - 用途：了解项目目标、技术环境、目录分层、构建方式和当前 V0.1 范围。

2. `docs/dev/ai_development_rules.md`
   - 用途：了解 AI 后续开发规则，包括分层边界、文档同步、版本记录和 ObjectARX 注意事项。

3. `docs/coding_rules.md`
   - 用途：了解编码规则，包括命令规则、文档规则、测试规则和各层职责。

这三个文件是固定入口。不要只看当前要改的文件就直接动手。

## 修改前先判断修改类型

开始修改前，先判断本次工作属于哪一类，然后再阅读对应文档。

| 修改内容 | 继续阅读的文档 |
| --- | --- |
| 架构、分层、职责边界 | `docs/architecture/overview.md` |
| 地形 TIN 数模代码结构和业务/CAD/UI 解耦边界 | `docs/architecture/terrain_tin_code_structure.md` |
| 目录结构、模块位置、文件放置 | `docs/architecture/project_structure.md` |
| 命令注册、模块注册、命令命名 | `docs/rules/command_and_module_rules.md` |
| 实体依赖、联动更新、脏标记、重建请求 | `docs/architecture/entity_relation_update.md` |
| 新增或修改业务命令 | `docs/business/业务文档模板.md`，以及对应模块下的业务文档 |
| 新增或沉淀复用能力 | `docs/reuse/_template.md` 和 `docs/reuse/capability_catalog.md` |
| 模块说明、模块清单、模块职责 | `docs/modules/module_index.md` 和对应模块文档 |
| 版本号、ARX 命名、输出目录 | `docs/dev/build_and_versioning.md` |
| 版本记录 | `docs/dev/version_log.md` |
| 核心测试、测试运行方式 | `tests/README.md` |
| 图标资源 | `assets/icons/README.md` |
| 示例数据 | `samples/README.md` |

## 开发流程

1. 先读本文件。
2. 再读三个必读文件。
3. 判断本次要改什么。
4. 按修改类型阅读对应规则文档。
5. 再开始改代码或文档。
6. 改完后检查是否需要同步更新：
   - 业务文档：`docs/business/`，必须按独立功能拆分，一功能一文档
   - 复用说明：`docs/reuse/`
   - 模块说明：`docs/modules/`
   - 版本记录：`docs/dev/version_log.md`
   - README 或本文件

## 项目硬性规则

- 不要把所有逻辑堆在 ARX 入口文件。
- 不要把业务逻辑堆在命令回调。
- 不要把复杂业务规则写在 UI 事件中。
- 命令只能通过 `CommandRegistry` 注册。
- 模块只能通过 `ModuleRegistry` 注册。
- `domain` 层不得依赖 ObjectARX。
- ObjectARX API 调用应放在 `cad_adapter`。
- 模块之间的联动更新必须通过统一关系管理机制表达。
- 每个独立功能必须有且只有一份对应业务文档，不得把多个功能长期混写在同一份业务文档中；模块总览文档只能做索引和边界说明。
- 每个原型命令必须指向对应功能的业务文档；内部 Bridge / handle 命令也要有对应功能或桥接说明文档。
- 每个可复用能力都要沉淀到复用说明。
- 生成或调整 ARX 版本时，必须更新版本记录。

## 核心架构原则

RoadProto 采用“C++ ObjectARX 核心 + 可替换 UI 层”的架构原则。

- C++ ObjectARX 负责 CAD 核心能力，包括命令注册、Ribbon 入口、选择集、AcDb 图元读取、自定义实体、DWG 持久化、几何算法、三角网构建、CAD 内显示、数据查询接口和后续道路设计对象联动接口。
- WPF 只负责界面展示和用户交互，包括参数展示、统计信息展示、按钮交互、状态提示、错误提示和演示型 UI 美化。
- 禁止把 CAD 核心业务逻辑写入 WPF。WPF 不应直接操作 `AcDbEntity`、`AcDbObjectId`、`ads_name` 等 ObjectARX 类型。
- WPF 与 C++ ObjectARX 之间必须通过 Bridge / Adapter 层交互。Bridge 层只做数据转换和调用转发，不写业务算法。
- 当前 RoadProto 原型阶段，用户参数窗口、编辑窗口、设置窗口和二级配置窗口先统一使用 WPF 实现。不得新增 C++ Win32/MFC 参数对话框作为当前阶段的正式交互；已有 C++ 临时对话框只能作为历史兼容，后续修改或重做时应优先迁移到 WPF。
- 所有可复用能力必须沉淀在 C++ Service / Entity / Data 层，确保后续 UI 从 WPF 替换为 MFC、Qt、AutoCAD Palette、其他 UI 技术栈或 EICAD 正式 UI 时，核心能力仍可复用。

## Git 仓库与本机 Git

本项目 Git 仓库 URL 固定为：

```text
https://github.com/FrFrFr123/RoadProto.git
```

当前 PowerShell 环境如果找不到 `git` 命令，优先使用 Visual Studio 2026 Insiders 自带 Git：

```text
D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe
```

例如：

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" status --short
```

不要因为 `git` 未加入 PATH 就要求用户重复提供仓库 URL；先按上面的本机 Git 路径执行。

## 本机编译与运行排查规则

本项目在本机优先使用 Visual Studio 2026 Insiders 编译：

- IDE 路径优先使用 `D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE`。
- 命令行构建优先使用 `D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe`。
- 目标仍保持 AutoCAD 2021 / ObjectARX 2021 兼容，不因为使用 VS2026 就随意升级 ObjectARX、AutoCAD 或项目平台工具集。
- 只有在 VS2026 路径不存在、缺少兼容工具集，或 ObjectARX 2021 明确要求旧工具链时，才退回其他 Visual Studio / MSBuild。

本项目运行或测试 AutoCAD 插件时，如出现内存异常，先做证据采集再判断原因：

- 优先检查 `acad.exe`、`accoreconsole.exe`、`taskhostw.exe`、`Codex.exe`、`devenv.exe` 和 MSBuild 相关进程的工作集、私有内存与命令行。
- 如果 `taskhostw.exe` 占用异常，需要记录 PID、命令行、内存占用和时间点；不要在没有进程证据时直接断定由 RoadProto 引起。
- AutoCAD 图形界面测试结束后，应确认是否残留无窗口 `acad.exe` 或后台 Core Console 进程，避免重复加载 ARX / NETLOAD 后持续占用内存。
- 后续需要复现内存问题时，先使用当前稳定 ARX / DLL，再逐步打开 Ribbon、地形构网、导入导出和双击编辑路径，定位触发点。

## 当前 V0.1 重点

当前阶段只搭框架，不实现复杂道路设计算法。

已经具备：

- ObjectARX 入口框架。
- 统一命令注册机制。
- 统一模块注册机制。
- Ribbon 菜单模型。
- 实体关系与联动更新基础机制。
- 地形数模示例模块。
- 平交口示例模块。
- 核心测试项目。

后续新增功能时，应优先保持框架边界清晰，让后续 AI 和人工开发都能继续扩展。
