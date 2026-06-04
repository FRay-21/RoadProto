# AI_AGENT 模块说明

## 模块定位

`AI_AGENT` 是设计软件原型 Agent 模块，用于承接 RoadProto 右侧 Agent 面板、本地 Agent 后端对接、受控工具调用网关和 Agent skill 文档体系。

当前状态：原型链路已实现。模块已接入 C++ `AI_AGENT` 命令注册、WPF 右侧 Agent 面板、本地 `.NET 8` sidecar、独立浏览器管理控制台、OpenAI-compatible 模型 Provider 配置和首个自动化路基模板工具。AutoCAD 2021 图形界面的完整端到端点验仍待手工执行。

## 命令

| 命令 | 状态 | 说明 | 业务文档 |
| --- | --- | --- | --- |
| `RD_AI_ASSISTANT_OPEN` | 已实现 | 打开或激活 AutoCAD 右侧 WPF Agent 面板 | `docs/business/agent/设计软件原型Agent.md` |
| `RD_AI_EXECUTE_TOOL_FILE` | 已实现 | 执行受控 Agent 工具 JSON 文件 | `docs/business/agent/设计软件原型Agent.md` |

## 代码落点

- `src/modules/agent`：模块注册、命令元数据和 Ribbon 模型挂接。
- `src/application/agent`：工具请求解析、顶层字段白名单、请求大小限制、schema 校验和路基模板参数转换。
- `src/cad_adapter/objectarx/agent`：AutoCAD 插入点点取、实体创建、结果 JSON 写回和结果路径白名单限制。
- `src/ui/wpf/RoadProto.Terrain.UI`：右侧 Agent 面板、后端 HTTP 客户端、工具确认卡片、受控请求文件生成和 Ribbon 可见入口。
- `src/agent/RoadProto.Agent.Host`：本地 Agent sidecar，提供 `/health`、`/api/chat`、`/admin`、本地规则 planner、prompt 上下文组装和 OpenAI-compatible Provider。
- `src/agent/RoadProto.Agent.Host/Admin`：管理控制台配置、密钥、Markdown 文档存储和 `/api/admin/*` API。
- `src/agent/RoadProto.Agent.Host/wwwroot/admin`：独立浏览器管理页面，支持模型 Profile、API Key、连接测试、skill 和知识库 Markdown 管理。
- `src/agent/RoadProto.Agent.Tests`：后端 planner、API 和 Provider 配置测试。

## 功能文档

- Agent 业务入口：`docs/business/agent/设计软件原型Agent.md`
- Agent 总览：`docs/agent/overview.md`
- 工具协议：`docs/agent/tool_protocol.md`
- Skill 写作规则：`docs/agent/skill_authoring_rules.md`
- 路基模板创建 skill：`docs/agent/skills/cross_section/subgrade_template_create.md`
- 工具网关复用说明：`docs/reuse/agent_tool_gateway.md`

## 边界

- WPF 不直接读写 CAD 实体。
- 后端服务不依赖 ObjectARX。
- C++ 工具网关只执行白名单工具。
- 自动化工具必须复用现有 domain/application/cad_adapter 能力。
- `resultPath` 只能写入 `%TEMP%\RoadProtoAgent\` 下的受控结果文件。
- 管理控制台只管理模型配置、API Key 和 Markdown 上下文，不直接执行 CAD 工具。

## 验证状态

- 已通过 `RoadProto.sln` Release 构建。
- 已通过 `RoadProtoCoreTests.exe` 核心测试。
- 已通过 `dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj`。
- 已通过 Agent Host Release 构建和 WPF Release 构建。
- 已通过 `/admin` 本地管理控制台浏览器点验。
- AutoCAD 2021 图形界面的 Agent 面板、确认卡片、实体创建和结果文件完整链路仍需手工点验。
