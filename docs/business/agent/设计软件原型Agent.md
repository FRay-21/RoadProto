# 设计软件原型 Agent

## 功能范围

本功能是 RoadProto 内嵌 AI 对话入口原型。用户可通过 AutoCAD 右侧 WPF Agent 面板进行闲聊、软件操作咨询、道路设计专业知识问答，以及对受控工具调用进行确认。

首版已接通本地 `.NET 8` Agent sidecar、Agent skill 文档读取、OpenAI-compatible 模型 Provider 配置和 C++ 白名单工具网关。首个自动化原子函数为 `cross_section.subgrade_template.create`，用于创建独立的 `DnSubgradeTemplateEntity` 路基模板实体。

当前状态：代码链路已实现并通过自动化构建与测试；AutoCAD 2021 图形界面的完整端到端点验仍待手工执行，因此暂不标记为稳定版本。

本业务文档只描述用户流程和功能边界。Agent 代码目录、文档目录、运行期目录、新增工具流程和新增 Provider 流程以 `docs/architecture/agent_code_structure.md` 为准。

## 命令

- `RD_AI_ASSISTANT_OPEN`：打开或激活 AutoCAD 右侧 WPF Agent 面板。
- `RD_AI_EXECUTE_TOOL_FILE`：执行 `%TEMP%\RoadProtoAgent\` 下的受控 Agent 工具 JSON 文件。

## 边界

- Agent 面板只负责输入、展示、确认和结果反馈，不直接读写 CAD 实体。
- 本地 Agent 后端不依赖 ObjectARX，不直接操作 DWG。
- C++ 工具网关只执行白名单工具，不执行任意 AutoCAD 命令字符串。
- 首版工具只创建 `DnSubgradeTemplateEntity`；未提供任意实体修改、删除或命令执行能力。
- API Key 由独立浏览器管理控制台输入，并使用 Windows 当前用户 DPAPI 加密保存到项目根目录 `.roadproto-agent/secrets/`；该目录已通过 `.gitignore` 排除，不写入仓库或 DWG。
- 管理控制台不直接执行 CAD 工具；所有 CAD 写入仍必须通过 WPF 确认卡片和 C++ 白名单工具网关。

## 管理控制台

`RoadProto.Agent.Host` 提供独立浏览器管理控制台 `/admin`。启动后端后可访问：

```text
http://127.0.0.1:17831/admin
```

该页面用于配置 OpenAI-compatible 模型 Profile、输入并加密保存 API Key、执行模型连接测试、导入 Markdown skill 和导入 Markdown 知识库。模型 Profile 支持 OpenAI、DeepSeek、DashScope/阿里百炼/千问等兼容接口，也支持自定义 OpenAI-compatible Base URL。

管理控制台保存的配置位于项目根目录 `.roadproto-agent/config.json`；API Key 以 Windows 当前用户 DPAPI 加密文件形式保存到 `.roadproto-agent/secrets/`。上传的 skill 和知识库 Markdown 分别保存到 `.roadproto-agent/skills/` 和 `.roadproto-agent/knowledge/`。这些本机配置文件不进入 Git 仓库，不写入 DWG。若首次运行时发现旧的 `%LOCALAPPDATA%\RoadProto\Agent\` 中存在配置，而项目根目录 `.roadproto-agent/` 还没有本地数据，后端会自动复制旧配置到项目目录，避免已有模型配置丢失。

后端在处理普通问答时，会把默认系统提示、内置路基模板创建 skill、启用的用户 skill 和启用的知识库 Markdown 组合为 system prompt。工具意图仍由本地 planner 优先识别；模型返回只作为普通回复，不自动执行工具。

## 业务流程

1. 用户启动本地 Agent sidecar，默认监听 `http://127.0.0.1:17831`。
2. 用户可先打开 `/admin` 管理控制台，配置默认模型 Profile、API Key、skill 和知识库 Markdown。
3. 用户在 AutoCAD 中运行 `RD_AI_ASSISTANT_OPEN`，或点击 Ribbon 的 `Agent / AI 助手`。
4. WPF 打开右侧 Agent 面板，并通过 `/health` 检查本地后端状态。
5. 用户输入自然语言需求。
6. 后端优先使用本地规则 planner 和 Agent skill 文档识别创建路基模板意图；普通问答可交给管理控制台配置的默认 OpenAI-compatible 模型 Provider。
7. 若识别出 `cross_section.subgrade_template.create`，WPF 展示工具确认卡片，确认前不执行 CAD 写入；当用户在下一轮只输入“确认”“继续”“执行”等短句时，后端会优先回看最近的用户原始需求，并可从上一条待执行摘要中兜底恢复工具调用，避免模型只生成口头承诺而不触发工具卡片。
8. 用户确认后，WPF 在 `%TEMP%\RoadProtoAgent\` 生成受控请求文件和结果文件路径。
9. WPF 发送 `RD_AI_EXECUTE_TOOL_FILE`，由 C++ 工具网关读取请求、校验工具名、参数、插入点和结果路径。
10. 工具网关复用现有路基模板领域模型、创建服务和 ObjectARX 实体创建能力，生成 `DnSubgradeTemplateEntity`。
11. 工具网关写回固定结构结果 JSON，包含成功状态、实体 handle、实体类型、错误码或提示信息。

## 首版路基模板参数

`cross_section.subgrade_template.create` 支持模板名称、道路等级、设计速度、路基宽度、车道数、车道宽度、硬路肩、土路肩、中分带、填挖方边坡、边沟、路面结构说明、显示比例、插入点、默认部件生成方式和显式部件列表。用户没有提供的参数由工具 mapper 使用默认值补齐。

显式部件列表可表达部件类型、左右侧、宽度、高度差、颜色、坡度模式、固定坡度、变宽表、坡度变化表和路面结构层模板引用。当前 WPF 首版只展示确认卡片和结果路径，不做结果文件轮询。

## 验证状态

- 自动化验证已通过：Agent 后端测试、Agent Host Release 构建和管理控制台浏览器点验；`RoadProto.sln` Release 构建、核心测试和 WPF Release 构建仍按当前分支最终验证结果记录。
- Core Console 脚本烟测尝试受限于本机脚本加载路径/命令注册验证，未形成可采信的命令级结果文件。
- 图形界面验证仍需在 AutoCAD 2021 中手工执行：加载 ARX 和托管 DLL，启动 Agent sidecar，打开 `/admin` 配置模型与 Markdown 文档，打开 `RD_AI_ASSISTANT_OPEN`，输入路基模板创建需求，确认工具卡片，并检查 DWG 中生成的 `DnSubgradeTemplateEntity`。
