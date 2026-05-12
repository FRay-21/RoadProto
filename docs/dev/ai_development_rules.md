# AI 后续开发规则

## 通用规则

- 遵守 `docs/architecture/overview.md` 中定义的分层边界。
- 命令只能通过 `CommandRegistry` 注册。
- 模块只能通过 `ModuleRegistry` 注册。
- 命令回调必须保持简短，只触发 application service 并输出结果。
- UI 事件只负责参数收集和展示。
- 当前 RoadProto 原型阶段，用户参数窗口、编辑窗口、设置窗口和二级配置窗口先统一使用 WPF 实现，并通过 Bridge / Adapter 与 C++ ObjectARX 核心交互；不得新增 C++ Win32/MFC 参数对话框作为当前阶段的正式 UI。未来如整体更换为 MFC、Qt、AutoCAD Palette 或其他 UI 技术栈，也必须保持 Bridge / Adapter 解耦边界。
- domain 代码不得依赖 ObjectARX 头文件和 AutoCAD 运行环境。
- 所有 AutoCAD API 调用都必须放在 `cad_adapter` 后面。
- 每个独立功能都必须在 `docs/business/<module>/` 中建立单独业务文档，文档粒度按功能边界划分，不按模块大文档混写。
- 每个原型命令都必须在命令元数据中指向对应功能的业务文档；内部 Bridge / handle 命令也必须指向对应功能或桥接说明文档。
- 模块文档只能做模块职责、命令清单、代码落点和功能文档索引，不承载多个功能的详细业务规则。
- 可复用能力必须记录到 `docs/reuse`。
- 每次形成可构建版本后都要更新 `docs/dev/version_log.md`。
- 本机编译优先使用 VS2026 Insiders：`D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE`；命令行优先使用 `D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe`。
- Git 仓库 URL 固定为 `https://github.com/FrFrFr123/RoadProto.git`；当前 PowerShell 若找不到 `git`，优先使用 VS2026 Insiders 自带 Git：`D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe`。
- 编译工具链可以更新，AutoCAD 2021 / ObjectARX 2021 目标不能随意升级。
- 如果当前开发目录位于 `.worktrees/<分支名>`，新增或更新的文档必须在收尾前按相同相对路径同步一份到主项目目录 `F:\0_GPT_道路设计原型功能项目`。同步范围包括 `AGENTS.md`、`README.md`、`docs/business/`、`docs/modules/`、`docs/reuse/`、`docs/dev/`、`docs/architecture/`、`docs/rules/`、`tests/README.md`、`assets/icons/README.md` 和 `samples/README.md`；源代码默认不做主目录双写，除非用户明确要求。
- Worktree 内的文档版本必须提交并推送到 Git，作为正式留档；主项目目录必须保留同路径、内容一致的可见副本，方便用户直接查看。

## 新增功能流程

新增功能时：

1. 先编写或更新对应功能的单独业务文档；如功能边界改变，先拆分旧文档再继续开发。
2. 对不依赖 ObjectARX 的 core、domain、application 行为增加聚焦测试。
3. 在 domain 层实现业务规则，不引入 ObjectARX 依赖。
4. 在 application 层组织功能流程。
5. 在模块层增加命令注册。
6. CAD 操作只能通过 adapter service 完成。
7. 更新复用说明和版本记录。

## ObjectARX 规则

- 选择集必须通过 guard 或明确释放路径释放。
- 命令组名称必须集中管理。
- 不要在事务作用域之外保存原始 AutoCAD object 指针。
- 面向用户的错误应通过 editor adapter 输出。
- domain/application 层应使用实体 ID 和 handle，不应持有 ObjectARX object 指针。
- 图形界面测试结束后检查是否残留无窗口 `acad.exe`、`accoreconsole.exe` 或异常 `taskhostw.exe`，记录 PID、命令行和内存占用后再判断是否与本项目相关。

## 版本规则

- V0.1.x：框架搭建。
- V0.2.x：基础 CAD adapter 能力。
- V0.3.x：道路通用能力。
- V1.0.0：稳定可复用框架。
