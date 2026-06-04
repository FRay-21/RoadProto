# Agent 工具网关复用说明

## 能力

Agent 工具网关提供从受控 Agent 工具 JSON 到 C++ application/cad_adapter 的白名单执行能力。当前已用于 `cross_section.subgrade_template.create`，可自动化创建 `DnSubgradeTemplateEntity` 路基模板实体。

该能力由 WPF 或受信任宿主生成请求文件和结果文件路径，由 `RD_AI_EXECUTE_TOOL_FILE` 触发 C++ 工具网关读取请求、校验参数、执行工具并写回固定结构结果 JSON。

## 复用边界

- 工具请求解析、顶层字段白名单、请求大小限制和参数转换放在 `src/application/agent`。
- AutoCAD 点取、实体创建、结果文件写回和 `resultPath` 白名单限制放在 `src/cad_adapter/objectarx/agent`。
- 新工具必须注册到 C++ 白名单，禁止执行任意 AutoCAD 命令字符串。
- 新工具必须有业务文档、Agent skill 文档和协议说明。
- WPF 只展示确认卡片和结果，不直接读写 CAD 实体。
- Agent 后端只生成意图、回复和工具调用候选，不依赖 ObjectARX。
- `/admin` 管理控制台只管理模型 Profile、API Key、skill 和知识库 Markdown，不直接执行工具；CAD 写入仍必须经过 WPF 确认卡片和 C++ 白名单工具网关。

## 首版工具

- 工具 ID：`cross_section.subgrade_template.create`
- 实体类型：`DnSubgradeTemplateEntity`
- 请求目录：`%TEMP%\RoadProtoAgent\`
- 结果结构：`requestId`、`tool`、`succeeded`、`entityHandle`、`entityType`、`message`；失败时返回 `errorCode` 和 `message`
- 关键校验：未知顶层字段拒绝、请求文件大小限制、工具名白名单、插入点有限数值校验、显示比例校验、部件宽度和坡度数据校验、结果路径专用目录限制

## 新增工具流程

1. 编写独立业务文档，明确用户场景、命令边界和手工验证流程。
2. 编写 Agent skill，说明触发条件、参数默认值、追问规则和 JSON schema。
3. 在 `docs/agent/tool_protocol.md` 或对应工具文档中补充协议字段。
4. 在 `src/application/agent` 增加参数 DTO、解析、白名单字段校验和到业务模型的转换。
5. 在 `src/cad_adapter/objectarx/agent` 增加执行器，只调用明确的 CAD Adapter 或应用服务能力。
6. 在 `AI_AGENT` 模块或工具分发处注册工具名。
7. 在 WPF Agent 面板中增加确认卡片展示字段和用户可理解的摘要。
8. 增加不依赖 AutoCAD 的核心测试和后端测试。
9. 在 AutoCAD 2021 中加载 ARX、托管 DLL 和本地 sidecar 做图形界面手工验证。
10. 更新模块文档、复用目录、README、测试说明和版本记录。
