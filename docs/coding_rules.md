# 编码规则

## 分层规则

- `domain` 不得包含 ObjectARX 头文件。
- `application` 负责组织流程，可调用 domain service 或 cad_adapter 接口。
- `modules` 负责注册命令、Ribbon 分组和模块元数据。
- `app/arx_entry` 只负责 ARX 生命周期。
- `cad_adapter/objectarx` 是直接调用 ObjectARX API 的唯一位置。
- 当前 RoadProto 原型阶段，用户对话框统一先放在 WPF UI 层实现。新增或重做参数窗口、编辑窗口、设置窗口、二级配置窗口时，不得在 `cad_adapter/objectarx` 中新增 C++ Win32/MFC 参数对话框作为当前阶段正式 UI；ObjectARX 层只负责 CAD 选择、实体打开、预览、绘制、持久化和 Bridge 调用。未来如整体更换 UI 技术栈，仍必须保持该边界。
- Agent 后端统一放在 `src/agent/RoadProto.Agent.Host`，只做本地 HTTP 服务、模型 Provider、管理控制台、skill/知识库读取和工具调用候选生成；不得引入 ObjectARX 或直接读写 DWG。
- Agent 面板代码统一放在 `src/ui/wpf/RoadProto.Terrain.UI`，只做展示、确认、HTTP 调用和受控请求文件生成；不得直接创建、修改或删除 CAD 实体。
- Agent 工具 JSON 解析、schema 校验和参数映射放在 `src/application/agent`；实际触达 AutoCAD 的白名单执行器放在 `src/cad_adapter/objectarx/agent`。新增 Agent 能力的完整落点见 `docs/architecture/agent_code_structure.md`。

## 命令规则

每个命令必须包含：

- 内部命令名。
- 显示名。
- 模块编码。
- 功能说明。
- 命令入口函数。
- 是否为原型功能。
- 是否具备复用价值。
- 对应业务文档路径。
- 是否可挂接 Ribbon。

## 文档规则

每个独立功能都需要在 `docs/business/<module>/` 下建立单独业务文档；一份业务文档只描述一个功能，不把创建、编辑、导入导出、内部桥接等不同功能混写。

每个原型命令的 `businessDocPath` 必须指向该命令所属功能的业务文档；内部命令如果承载 Bridge、handle 编辑或回写流程，也要指向对应功能或桥接说明文档。

模块文档和 README 只做索引、范围和入口说明，不替代功能业务文档。

每个可复用能力都应记录在 `docs/reuse/` 下。

每个生成的 ARX 版本都要记录到 `docs/dev/version_log.md`。

Agent 文档职责固定如下：`docs/architecture/agent_code_structure.md` 描述代码和文档结构，`docs/agent/` 描述 Agent 总览、工具协议和 skill 规则，`docs/business/agent/` 描述用户流程和业务边界，`docs/modules/agent.md` 只做模块索引，`docs/reuse/agent_tool_gateway.md` 只做复用说明。

使用 `.worktrees/<分支名>` 开发时，主项目目录必须同步保留最新文档和所有代码：在 worktree 内保存正式代码或文档后，收尾前必须按相同相对路径同步到主项目目录 `F:\0_GPT_道路设计原型功能项目`。这条规则适用于 `AGENTS.md`、`README.md`、`.gitignore`、`RoadProto.sln`、`build/`、`src/`、`tests/`、`third_party/`、`assets/`、`docs/`、`samples/` 和用户明确要求可见的说明文档或代码文件。

不同步 `.git/`、`.vs/`、`.worktrees/`、`bin/`、`obj/`、构建缓存和本机私有配置；`artifacts/` 只按构建产物同步规则复制可加载 ARX / DLL / PDB。

Worktree 内提交并推送的版本仍作为正式 Git 留档；主项目目录必须保留同路径、内容一致的最新副本，方便用户直接查看、加载和继续开发。若用户要求后续基于主目录更新，必须先把 worktree 最新文档和所有代码同步回主目录，再继续开发。

## 测试规则

- core、domain、application 逻辑应尽量提供不依赖 AutoCAD 的测试。
- ObjectARX 集成能力需要在 AutoCAD 2021 中加载 ARX 后验证。
- 测试应聚焦行为和扩展契约，不要只验证实现细节。
