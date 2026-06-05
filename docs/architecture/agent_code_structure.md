# 设计软件原型 Agent 代码与文档结构

## 目标

本文档约定 RoadProto Agent 原型新增目录、代码落点、文档落点和扩展边界。后续继续增加大模型 Provider、Agent 工具、skill、知识库接入或 WPF 面板交互时，优先按本文档放置文件，避免 Agent 功能把既有 C++ ObjectARX 框架、WPF UI 和业务文档体系搅在一起。

Agent 采用“四段式”结构：

```text
WPF Agent 面板 -> .NET 8 本地 sidecar -> WPF 受控请求文件 -> C++ 白名单工具网关 -> CAD 实体/命令能力
```

其中模型只参与意图分析和普通回复，不直接执行 CAD 命令；CAD 写入必须经过 WPF 确认卡片和 C++ 白名单工具网关。

## 总体分层

| 层 | 目录 | 职责 | 依赖 ObjectARX |
| --- | --- | --- | --- |
| 模块注册层 | `src/modules/agent/` | 注册 `AI_AGENT` 模块、命令元数据和 Ribbon 分组 | 否 |
| WPF 交互层 | `src/ui/wpf/RoadProto.Terrain.UI/` | 右侧 Agent 面板、HTTP 调用、确认卡片、请求文件生成、AutoCAD 命令发送 | 仅 AutoCAD .NET 入口可依赖 |
| 本地后端层 | `src/agent/RoadProto.Agent.Host/` | `/health`、`/api/chat`、`/admin`、模型 Provider、配置、skill/知识库读取、工具意图生成 | 否 |
| 后端测试层 | `src/agent/RoadProto.Agent.Tests/` | 后端 API、配置存储、prompt 上下文、planner 和 Provider 测试 | 否 |
| 工具请求应用层 | `src/application/agent/` | Agent 工具 JSON 解析、顶层字段白名单、schema 校验、参数映射 | 否 |
| CAD 工具网关层 | `src/cad_adapter/objectarx/agent/` | `RD_AI_EXECUTE_TOOL_FILE`、可信路径校验、CAD 点取、实体创建、结果写回 | 是 |
| Agent 文档层 | `docs/agent/` | Agent 总览、工具协议、skill 写作规则和内置 skill | 否 |
| Agent 业务文档 | `docs/business/agent/` | Agent 功能边界、用户流程、命令流程和验收说明 | 否 |
| Agent 复用说明 | `docs/reuse/agent_tool_gateway.md` | 白名单工具网关复用边界和新增工具流程 | 否 |

## 源码目录职责

### `src/modules/agent/`

用于声明 `AI_AGENT` 模块和命令元数据。只能做模块注册、命令注册和 Ribbon 元数据挂接，不写 HTTP、JSON 解析、模型调用或 CAD 实体创建逻辑。

当前命令：

- `RD_AI_ASSISTANT_OPEN`：打开或激活右侧 WPF Agent 面板。
- `RD_AI_EXECUTE_TOOL_FILE`：执行受控 Agent 工具请求文件。

新增 Agent 命令时，必须继续通过 `CommandRegistry` 注册，并在命令元数据中指向独立业务文档。

### `src/ui/wpf/RoadProto.Terrain.UI/`

该目录是 AutoCAD 内可见 WPF 插件。Agent 相关文件分散在既有 WPF 工程内，但职责必须清晰：

| 目录或文件 | 职责 |
| --- | --- |
| `AgentAssistantControl.xaml`、`AgentAssistantControl.xaml.cs` | 右侧 Agent 面板 UI、聊天历史、工具确认卡片、用户确认/取消交互 |
| `Services/AgentBackendClient.cs` | 调用本地 sidecar 的 `/health` 和 `/api/chat`，不做模型业务判断 |
| `Bridge/AgentDtos.cs` | WPF 侧 Agent 请求、响应和工具调用 DTO |
| `AutoCad/AgentToolExecutionFile.cs` | 生成 `%TEMP%\RoadProtoAgent\` 下的受控请求文件和结果文件路径 |
| `AutoCad/RoadProtoRibbonExtension.cs` | Agent Ribbon 入口和 `RD_AI_ASSISTANT_OPEN` 托管命令入口 |

WPF 只能展示参数、确认工具调用、写受控请求文件、发送 `RD_AI_EXECUTE_TOOL_FILE`。WPF 不得直接创建、修改、删除 `AcDbEntity`，也不得绕过 C++ 工具网关执行 CAD 写入。

### `src/agent/RoadProto.Agent.Host/`

该目录是 `.NET 8` 本地 sidecar，是 Agent 的后端服务。它不依赖 ObjectARX，也不读写 DWG。

| 子目录或文件 | 职责 |
| --- | --- |
| `Program.cs` | 服务启动、依赖注入、静态文件、`/health`、`/api/chat`、`/admin` 路由 |
| `appsettings.example.json` | 首次启动 seed 配置示例，不保存真实 API Key |
| `Admin/` | 运行期模型配置、API Key 加密存储、Markdown skill/知识库管理和 `/api/admin/*` |
| `Models/` | 后端请求、响应、工具调用和工具参数 DTO |
| `Providers/` | 大模型 Provider 适配，当前为 OpenAI-compatible `/chat/completions` |
| `Services/` | 聊天编排、prompt 上下文组装、配置读取和工具调用候选生成 |
| `Skills/` | 内置 Markdown skill 读取 |
| `Tools/` | 本地规则 planner 和工具参数默认值推断 |
| `wwwroot/admin/` | 独立浏览器管理控制台前端页面、样式和脚本 |

新增模型 Provider 时，优先放在 `Providers/`；新增运行期管理能力时，放在 `Admin/` 和 `wwwroot/admin/`；新增本地意图规则或工具参数推断时，放在 `Tools/` 或 `Services/`。不得把模型调用、API Key 读取或管理控制台逻辑写进 WPF 或 C++。

### `src/agent/RoadProto.Agent.Tests/`

该目录只放 `.NET 8` 后端自动化测试，覆盖：

- `/api/chat` 和 `/api/admin/*` API。
- 模型 Profile 配置和 DPAPI API Key 存储。
- Markdown skill 和知识库上传。
- prompt 上下文组装。
- 本地 planner 和确认短句回看历史逻辑。
- OpenAI-compatible Provider 请求格式。

`bin/`、`obj/` 是构建缓存，不属于正式源码结构，不得在文档中作为开发落点，也不得提交到 Git。

### `src/application/agent/`

该目录是 C++ Agent 工具请求应用层，负责不依赖 AutoCAD 的工具协议处理：

- 解析工具请求 JSON。
- 校验顶层字段白名单。
- 校验请求字段类型和工具 schema。
- 把 Agent 参数映射为现有 application/domain 可用的数据。
- 对显示比例、部件宽度、坡度数据等业务参数做可测试校验。

新增 Agent 工具时，优先在这里增加 DTO、解析和参数映射测试。这里不得包含 ObjectARX 头文件，也不得直接创建 CAD 实体。

### `src/cad_adapter/objectarx/agent/`

该目录是唯一可以直接操作 AutoCAD 的 Agent 工具网关落点，负责：

- 注册和执行 `RD_AI_EXECUTE_TOOL_FILE`。
- 校验请求文件路径和结果文件路径是否位于可信目录。
- 在需要时提示用户从 CAD 中点取插入点。
- 调用现有 application/domain/cad_adapter 能力创建或更新 CAD 对象。
- 写回固定结构的结果 JSON。

新增工具必须进入白名单，不允许把模型生成的任意 AutoCAD 命令字符串传给 `acedCommand` 或 `SendStringToExecute`。

## 文档目录职责

| 目录或文件 | 职责 |
| --- | --- |
| `docs/architecture/agent_code_structure.md` | Agent 代码与文档结构主契约 |
| `docs/agent/overview.md` | Agent 功能总览和当前状态 |
| `docs/agent/tool_protocol.md` | 工具请求/结果 JSON 协议、安全规则和失败码 |
| `docs/agent/skill_authoring_rules.md` | Agent skill 编写规范 |
| `docs/agent/skills/<module>/<tool_name>.md` | 单个工具的触发语义、参数规则、默认值和示例 JSON |
| `docs/business/agent/设计软件原型Agent.md` | 用户流程、命令流程、业务边界和验收说明 |
| `docs/modules/agent.md` | `AI_AGENT` 模块索引、命令清单和代码落点摘要 |
| `docs/reuse/agent_tool_gateway.md` | 工具网关复用说明和新增工具流程 |
| `docs/dev/version_log.md` | Agent 相关版本记录和验证状态 |

文档职责不能混用：业务流程写在 `docs/business/agent/`，工具协议写在 `docs/agent/tool_protocol.md`，新工具 skill 写在 `docs/agent/skills/`，模块索引写在 `docs/modules/agent.md`，复用能力写在 `docs/reuse/`。

## 运行期和构建目录

以下目录不是源码目录：

| 目录 | 用途 | 是否提交 |
| --- | --- | --- |
| `.roadproto-agent/` | 本机运行期配置、加密 API Key、上传 skill、上传知识库和日志 | 否 |
| `%TEMP%\RoadProtoAgent\` | WPF 与 C++ 工具网关之间的请求/结果临时文件 | 否 |
| `artifacts/agent/<Configuration>/net8.0/` | Agent Host 构建产物 | 否，按构建产物同步规则复制 |
| `src/agent/**/bin/`、`src/agent/**/obj/` | .NET 构建缓存 | 否 |

真实 API Key 只能保存到 `.roadproto-agent/secrets/` 的 DPAPI 加密文件，不得写入 `appsettings.example.json`、Markdown 文档、测试文件或 Git 提交。

## 新增 Agent 工具流程

新增第二个、第三个 Agent 工具时，按以下顺序处理：

1. 在 `docs/business/<module>/` 编写对应独立业务文档。
2. 在 `docs/agent/skills/<module>/<tool_name>.md` 编写工具 skill，明确触发语义、参数、默认值、追问规则、确认文案和示例 JSON。
3. 必要时更新 `docs/agent/tool_protocol.md`，补充工具参数 schema 或失败码。
4. 在 `src/agent/RoadProto.Agent.Host/Tools/` 或 `Services/` 增加本地 planner/编排逻辑，并增加后端测试。
5. 在 `src/application/agent/` 增加 C++ 工具请求 DTO、解析、字段校验和参数映射，并增加核心测试。
6. 在 `src/cad_adapter/objectarx/agent/` 增加白名单执行分发和 CAD 执行逻辑，只调用明确的 application/cad_adapter 能力。
7. 在 `src/ui/wpf/RoadProto.Terrain.UI/` 增加确认卡片摘要或结果展示，不直接操作 CAD 实体。
8. 更新 `docs/modules/agent.md`、`docs/reuse/agent_tool_gateway.md`、`README.md`、`tests/README.md` 和 `docs/dev/version_log.md`。

## 新增模型 Provider 流程

新增千问、阿里百炼、DeepSeek、ChatGPT 或其他 OpenAI-compatible 以外的 Provider 时：

1. 在 `src/agent/RoadProto.Agent.Host/Providers/` 增加 Provider 客户端或适配器。
2. 在 `src/agent/RoadProto.Agent.Host/Admin/` 增加运行期配置字段校验和保存逻辑。
3. 在 `src/agent/RoadProto.Agent.Host/wwwroot/admin/` 更新管理控制台输入项。
4. 在 `src/agent/RoadProto.Agent.Tests/` 增加请求格式和配置读写测试。
5. 更新 `docs/agent/overview.md`、`docs/business/agent/设计软件原型Agent.md` 和 `README.md`。

不得把 API Key 写入源代码；不得让 Provider 直接返回 CAD 命令字符串作为执行入口。

## 新增 skill 或知识库规则

- 内置工具 skill 写入 `docs/agent/skills/`，随仓库版本管理。
- 用户通过管理控制台上传的 skill 和知识库写入 `.roadproto-agent/skills/` 与 `.roadproto-agent/knowledge/`，属于本机运行期数据，不进入 Git。
- 内置 skill 应描述工具能力和约束；知识库 Markdown 应描述软件操作、专业知识或项目规则，不承载任意执行指令。
- 后端组装 prompt 时可以读取这些 Markdown，但工具执行仍必须由本地 planner 生成结构化工具调用并由 WPF 确认。

## 禁止事项

- 禁止在 `src/agent/RoadProto.Agent.Host/` 引入 ObjectARX 依赖。
- 禁止在 WPF 里直接创建、修改或删除 CAD 实体。
- 禁止在 C++ 工具网关执行模型生成的任意命令字符串。
- 禁止把真实 API Key、`.roadproto-agent/`、`%TEMP%\RoadProtoAgent\`、`bin/`、`obj/` 或本机日志提交到仓库。
- 禁止把多个 Agent 工具的业务规则长期混写在同一份业务文档中。
- 禁止把 Agent 专项规则只写在 README，而不同步到本文档、模块文档和对应业务文档。

## 验证要求

Agent 相关修改至少按风险选择以下验证：

- 后端逻辑：`dotnet test src\agent\RoadProto.Agent.Tests\RoadProto.Agent.Tests.csproj`。
- C++ 工具请求/参数映射：构建并运行 `RoadProtoCoreTests.exe`。
- WPF 改动：`dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release`。
- sidecar 构建：`dotnet build src\agent\RoadProto.Agent.Host\RoadProto.Agent.Host.csproj -c Release`。
- 全量交付：`RoadProto.sln` Release 构建。
- CAD 写入链路：AutoCAD 2021 中加载主项目 ARX 和托管 DLL，启动 sidecar，打开 `RD_AI_ASSISTANT_OPEN`，触发工具确认卡片并检查结果 JSON 和 DWG 实体。
