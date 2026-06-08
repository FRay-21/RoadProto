# RoadProto Agent 总览

## 目标

RoadProto Agent 是 CAD 内嵌设计软件原型助手。当前原型已经接通右侧 WPF 对话面板、本地 Agent 后端、模型 Provider 配置、Agent skill 文档读取、受控工具调用和首个自动化工具：创建路基模板。

当前状态：代码链路已实现并通过自动化构建与测试；AutoCAD 2021 图形界面的完整端到端点验仍待手工执行，因此暂不标记为稳定版本。

Agent 代码、文档和运行期目录的主结构契约见 `docs/architecture/agent_code_structure.md`。本文档只说明 Agent 能力总览，不替代代码落点和新增工具流程约定。

## 边界

- WPF 只负责输入、展示、确认和结果反馈。
- Agent 后端不依赖 ObjectARX，不读写 CAD 对象。
- C++ 工具网关只执行白名单工具。
- domain/application 继续承载业务规则。
- cad_adapter/objectarx 继续承载 AutoCAD API 调用。

## 首版命令

- `RD_AI_ASSISTANT_OPEN`：打开或激活右侧 WPF Agent 面板。
- `RD_AI_EXECUTE_TOOL_FILE`：执行 `%TEMP%\RoadProtoAgent\` 下的受控 Agent 工具 JSON 文件。

## 首版工具

- `cross_section.subgrade_template.create`：自动化创建 `DnSubgradeTemplateEntity` 路基模板实体。

## 后端能力

- `.NET 8` 本地 sidecar 提供 `/health` 和 `/api/chat`。
- `.NET 8` 本地 sidecar 提供 `/admin` 独立浏览器管理控制台。
- `AgentPlanner` 优先识别路基模板创建意图，并输出结构化 `toolCall`。
- 模型 Provider 使用 OpenAI-compatible `/chat/completions`，通过管理控制台的 profile 配置支持 OpenAI、DeepSeek 和 DashScope/阿里百炼/千问等兼容接口。
- API Key 使用 Windows 当前用户 DPAPI 加密保存到项目根目录 `.roadproto-agent/secrets/`，该目录已通过 `.gitignore` 排除，不写入仓库。
- 管理控制台可上传 Markdown skill 和 Markdown 知识库；后端会把启用文档和内置路基模板 skill 组装进 system prompt。
- Markdown skill 主要作为 prompt 上下文和 planner 开发契约；真实 CAD 工具调用必须由 `AgentPlanner` 生成，并经 WPF 确认卡片进入 C++ 白名单工具网关。

## Agent Planner 当前能力

- 识别创建、新建、生成路基模板等意图。
- 识别 `市政道路`、`城市道路`、`市政路`、`城区道路` 等表达，未明确具体等级时使用 `UrbanArterial`。
- 支持默认参数创建路基模板，`componentSource` 使用 `DefaultByRoadGrade`。
- 支持“市政道路默认模板，最右侧增加一个行车道部件”场景，输出 `componentOperations` 增量操作。
- 支持确认短句回看上一轮待执行路基模板工具调用。

## 结构入口

- 代码和文档结构：`docs/architecture/agent_code_structure.md`
- 工具协议：`docs/agent/tool_protocol.md`
- Skill 写作规则：`docs/agent/skill_authoring_rules.md`
- 模块索引：`docs/modules/agent.md`
- 业务流程：`docs/business/agent/设计软件原型Agent.md`
- 工具网关复用：`docs/reuse/agent_tool_gateway.md`

## 不做范围

- 不执行任意 AutoCAD 命令字符串。
- 不把 API Key 写入仓库。
- 不让 Agent 凭空绑定路面结构层模板 handle。
- 不在 WPF 中写 CAD 核心业务逻辑。
