# RoadProto Agent 总览

## 目标

RoadProto Agent 是 CAD 内嵌设计软件原型助手。当前原型已经接通右侧 WPF 对话面板、本地 Agent 后端、模型 Provider 配置、Agent skill 文档读取、受控工具调用和首个自动化工具：创建路基模板。

当前状态：代码链路已实现并通过自动化构建与测试；AutoCAD 2021 图形界面的完整端到端点验仍待手工执行，因此暂不标记为稳定版本。

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
- 本地规则 planner 优先识别路基模板创建意图。
- 模型 Provider 使用 OpenAI-compatible `/chat/completions`，通过管理控制台的 profile 配置支持 OpenAI、DeepSeek 和 DashScope/阿里百炼/千问等兼容接口。
- API Key 使用 Windows 当前用户 DPAPI 加密保存到项目根目录 `.roadproto-agent/secrets/`，该目录已通过 `.gitignore` 排除，不写入仓库。
- 管理控制台可上传 Markdown skill 和 Markdown 知识库；后端会把启用文档和内置路基模板 skill 组装进 system prompt。

## 不做范围

- 不执行任意 AutoCAD 命令字符串。
- 不把 API Key 写入仓库。
- 不让 Agent 凭空绑定路面结构层模板 handle。
- 不在 WPF 中写 CAD 核心业务逻辑。
