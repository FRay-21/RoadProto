# RoadProto Agent 管理控制台设计

日期：2026-06-04

## 背景

当前 RoadProto Agent 后端 `RoadProto.Agent.Host` 已提供 `/health` 和 `/api/chat`，模型配置主要依赖 `appsettings.example.json` 和环境变量。这个方式适合开发验证，但对日常使用过于繁琐：用户需要手动编辑配置、设置环境变量、理解 Provider 参数，并且无法在启动后通过界面导入 skill 或知识库。

本设计目标是在本地 Agent sidecar 中增加独立浏览器管理控制台。用户启动后端后访问 `http://127.0.0.1:17831/admin`，即可配置模型、输入 API Key、导入 Agent skill 文档和知识库 Markdown 文件。AutoCAD 内的 WPF Agent 面板继续专注聊天、确认工具调用和结果展示，不承载配置管理职责。

## 目标

- 后端启动后即可通过 `/admin` 打开本地管理页面。
- 支持配置 OpenAI-compatible 模型 Profile，包括 OpenAI、DeepSeek、DashScope/阿里百炼/千问和自定义兼容接口。
- 支持在网页输入 API Key，并使用 Windows 当前用户加密后保存到本机。
- 支持选择默认模型 Profile，并立即影响 `/api/chat`。
- 支持上传、查看、启用、禁用和删除 Markdown skill 文档。
- 支持上传、查看、启用、禁用和删除 Markdown 知识库文件。
- 支持连接测试，帮助用户确认模型、skill、知识库和后端状态。
- 保持 C++ ObjectARX、WPF 和 Agent 后端边界清晰：配置管理只在 `.NET 8` sidecar 中实现。

## 不做范围

- 首版不做云端账户、多用户登录或远程访问。
- 首版不做向量库、embedding 检索或文档分块召回。
- 首版不允许网页直接执行 CAD 工具或任意 AutoCAD 命令。
- 首版不把 API Key 写入仓库、DWG、README 或普通明文 JSON。
- 首版不在 WPF Agent 面板内实现完整配置页面。

## 推荐方案

采用独立浏览器管理控制台：

```text
RoadProto.Agent.Host
  /admin                      静态管理页面入口
  /api/admin/status           后端与配置状态
  /api/admin/model-profiles   模型 Profile 管理
  /api/admin/skills           Skill 文档管理
  /api/admin/knowledge        知识库 Markdown 管理
  /api/chat                   继续提供 Agent 对话
```

管理页面使用 ASP.NET Core 静态文件托管，首版可采用无构建步骤的 HTML/CSS/JavaScript。这样不引入前端构建链，不增加 Node 依赖，也更适合当前 V0.1 框架阶段。

## 页面结构

首版采用左侧导航 + 右侧工作区：

- `模型配置`
  - 显示已有 Profile 列表。
  - 支持新增、编辑、删除 Profile。
  - 支持供应商预设：OpenAI、DeepSeek、DashScope/阿里百炼/千问。
  - 支持自定义 OpenAI-compatible Base URL。
  - 支持填写模型名、温度、超时时间、是否默认。
  - 支持输入 API Key、清除 API Key 和测试连接。
  - API Key 保存后只显示“已配置”和脱敏尾号，不回显完整内容。
- `Skill 文档`
  - 显示内置 skill 和用户上传 skill。
  - 支持上传 `.md` 文件。
  - 支持启用、禁用、查看摘要和删除用户上传 skill。
  - 内置 skill 不允许删除，只允许查看和启用状态控制。
- `知识库`
  - 支持上传 `.md` 文件。
  - 支持启用、禁用、查看摘要和删除。
  - 首版按启用文件顺序拼接到模型 system prompt。
- `连接测试`
  - 显示后端运行状态。
  - 显示默认模型 Profile 状态。
  - 显示 API Key 是否已配置。
  - 显示启用 skill 数量和启用知识库数量。
  - 提供一次模型连接测试按钮。

## 本地存储设计

所有运行期配置保存在用户本地目录，不写入仓库。推荐目录：

```text
%LOCALAPPDATA%\RoadProto\Agent\
  config.json
  secrets\
    <secretId>.bin
  skills\
    <documentId>.md
  knowledge\
    <documentId>.md
```

`config.json` 保存可公开的配置元数据，例如：

```json
{
  "defaultModelProfile": "dashscope-qwen",
  "modelProfiles": [
    {
      "name": "dashscope-qwen",
      "provider": "OpenAICompatible",
      "baseUrl": "https://dashscope.aliyuncs.com/compatible-mode/v1",
      "model": "qwen-plus",
      "temperature": 0.2,
      "timeoutSeconds": 60,
      "secretId": "profile-dashscope-qwen",
      "apiKeyMask": "sk-***abcd"
    }
  ],
  "skills": [],
  "knowledge": []
}
```

API Key 单独保存到 `secrets/<secretId>.bin`，内容使用 Windows DPAPI 的 CurrentUser 范围加密。后端读取模型 Profile 时通过 `secretId` 解密 API Key；前端永远不读取完整 API Key。

## API 设计

### 状态

`GET /api/admin/status`

返回后端状态、默认模型、Profile 数量、启用 skill 数量、启用知识库数量和存储目录。

### 模型 Profile

`GET /api/admin/model-profiles`

返回 Profile 列表，API Key 只返回 `hasApiKey` 和 `apiKeyMask`。

`POST /api/admin/model-profiles`

新增或更新 Profile。请求可包含 API Key；若 API Key 为空且已有密钥，则保留原密钥。

`DELETE /api/admin/model-profiles/{name}`

删除 Profile，同时删除对应 encrypted secret。若删除默认 Profile，需要重新选择第一个可用 Profile 或清空默认值。

`POST /api/admin/model-profiles/{name}/default`

设置默认模型 Profile。

`POST /api/admin/model-profiles/{name}/test`

使用该 Profile 发送最小连接测试请求。测试失败不改变配置。

### Skill 文档

`GET /api/admin/skills`

返回内置 skill 和用户上传 skill 列表。

`POST /api/admin/skills/upload`

上传 `.md` 文件。后端校验扩展名、文件大小和文件名，保存为安全的 `documentId.md`。

`PATCH /api/admin/skills/{id}`

更新启用状态或显示名称。

`DELETE /api/admin/skills/{id}`

删除用户上传 skill。内置 skill 不允许删除。

### 知识库

`GET /api/admin/knowledge`

返回知识库 Markdown 文件列表。

`POST /api/admin/knowledge/upload`

上传 `.md` 文件。

`PATCH /api/admin/knowledge/{id}`

更新启用状态或显示名称。

`DELETE /api/admin/knowledge/{id}`

删除知识库文件。

## 文件安全规则

- 只接受 `.md` 文件。
- 单文件首版最大 2 MB。
- 文件名只用于显示，不用于磁盘路径。
- 磁盘路径使用后端生成的 `documentId`，禁止路径穿越。
- 读取 Markdown 时使用 UTF-8。
- 删除文件时只允许删除本地 Agent 存储目录内的文件。
- 上传内容不直接执行，不作为 HTML 注入；前端展示 Markdown 摘要时按纯文本处理。

## 运行期集成

新增 `AgentConfigurationStore` 负责读取和写入本地配置。`AgentChatService` 不再只依赖启动时 `IOptions<RoadProtoAgentOptions>`，而是在每次处理 `/api/chat` 时读取当前默认模型 Profile。

新增 `AgentPromptContextService` 负责组装 system prompt：

1. 固定 RoadProto Agent system prompt。
2. 启用的内置 skill。
3. 启用的用户 skill。
4. 启用的知识库 Markdown。

首版需要设置最大 prompt 字符数，例如 skill 与知识库合计最多 30000 字符。超过上限时按启用顺序截断，并在连接测试页提示“已截断”。后续再扩展向量库和检索。

## UI 交互原则

- 不做营销式首页，`/admin` 第一屏就是可操作的配置界面。
- UI 保持工具型、密度适中、状态明确。
- 保存、测试、上传和删除都要有明确状态反馈。
- 删除 Profile、删除文档和清除 API Key 需要二次确认。
- API Key 输入框默认为空；保存后显示脱敏状态，不自动填回完整 Key。
- 如果后端没有默认模型，页面顶部提示用户先配置默认 Profile。

## 错误处理

- Base URL 非 URL 时拒绝保存。
- Profile 名称为空、重复或包含非法字符时拒绝保存。
- 模型名为空时拒绝保存。
- API Key 可为空保存，但连接测试会提示未配置。
- 上传非 `.md`、超出大小或空文件时拒绝。
- 配置文件损坏时，后端保留损坏文件备份并创建空配置，同时在状态页提示。
- DPAPI 解密失败时，标记该 Profile 的 API Key 不可用，并允许用户重新输入。

## 测试范围

- `AgentConfigurationStore`：默认目录、配置读写、损坏配置恢复、默认 Profile 选择。
- `AgentSecretStore`：API Key 加密保存、解密读取、清除、脱敏显示。
- 管理 API：Profile 增删改查、默认模型设置、连接测试失败提示。
- 文档管理：上传 `.md`、拒绝非 Markdown、拒绝路径穿越、启用禁用、删除。
- Prompt 组装：内置 skill、用户 skill、知识库启用状态和字符上限。
- `/api/chat`：使用管理控制台设置的默认 Profile，而不是只使用启动配置。
- 静态页面：`/admin` 返回 HTML，关键控件和 API 路径存在。

## 后续扩展

- WPF Agent 面板增加“打开管理控制台”按钮。
- 支持导入整个文件夹或批量 Markdown。
- 支持知识库向量化、embedding Provider 和按问题召回。
- 支持工具权限开关，例如某些 Agent 工具需要二次确认或禁用。
- 支持导出/导入管理控制台配置，但导出时默认不包含 API Key。

