# Agent 工具协议

## 请求文件

工具请求文件使用 UTF-8 JSON。首版协议已由 WPF Agent 面板、本地 Agent 后端和 C++ 工具网关使用。顶层结构：

```json
{
  "tool": "cross_section.subgrade_template.create",
  "requestId": "uuid",
  "arguments": {},
  "resultPath": "%TEMP%/RoadProtoAgent/RoadProtoAgentToolResult_<pid>_<requestId>.json"
}
```

示例中的 `resultPath` 不是模型可自由填写的字段；实际执行时必须由受信任宿主 WPF 或本地 Agent 后端注入，并且必须位于 RoadProto Agent 专用临时目录。

## 请求字段白名单

顶层只允许以下字段：

- `tool`
- `requestId`
- `arguments`
- `resultPath`

`arguments` 中的字段由对应工具 schema 定义。C++ 工具网关应采用字段白名单策略：未知顶层字段必须拒绝；未知 `arguments` 字段可以按工具 schema 明确选择拒绝或忽略，但策略必须固定并记录到对应 Agent skill 文档中。

## 路径规则

- `resultPath` 只能由受信任宿主 WPF 或本地 Agent 后端生成。
- 模型输出不得直接指定任意 `resultPath`。
- `resultPath` 必须位于 RoadProto Agent 专用临时目录，例如 `%TEMP%/RoadProtoAgent/`。
- 禁止使用相对路径、UNC 路径、网络路径、驱动器根目录、父级路径片段 `..` 或任何路径穿越写法。
- C++ 工具网关写入结果前必须规范化 `resultPath`，确认规范化后的绝对路径仍位于专用临时目录内。
- 结果文件写入只用于写回本次工具执行结果，不是任意路径写入能力。

## 文件限制

- `RD_AI_EXECUTE_TOOL_FILE` 只读取受信任宿主写出的请求文件路径。
- C++ 工具网关应限制请求文件大小，超过上限时拒绝执行。
- 请求文件必须是 UTF-8 JSON；解析失败时不执行工具。
- 若解析失败但仍能从顶层 JSON 中提取可信 `requestId`、`tool` 和合法 `resultPath`，工具网关应写回 `ParseError` 失败结果，便于 WPF 展示。
- 工具执行完成后只写入固定结构的结果 JSON，不附加模型生成的任意内容。

## 结果文件

成功：

```json
{
  "requestId": "uuid",
  "tool": "cross_section.subgrade_template.create",
  "succeeded": true,
  "entityHandle": "AB12",
  "entityType": "DnSubgradeTemplateEntity",
  "message": "已创建路基模板实体。"
}
```

失败：

```json
{
  "requestId": "uuid",
  "tool": "cross_section.subgrade_template.create",
  "succeeded": false,
  "errorCode": "InvalidArguments",
  "message": "路基模板显示比例必须为 1、10、20、50 或 100。"
}
```

常见失败码：

- `ParseError`：请求文件 JSON 解析失败。
- `InvalidTool`：工具名不在白名单内。
- `InvalidArguments`：参数缺失、类型错误或业务校验失败。
- `InvalidInsertionPoint`：插入点不是有限坐标，或插入点模式不合法。
- `Cancelled`：用户取消 CAD 点取或执行流程。
- `CreateEntityFailed`：CAD 实体创建失败。

## 安全规则

- C++ 只接受白名单工具名。
- WPF 必须在执行前展示确认卡片。
- `RD_AI_EXECUTE_TOOL_FILE` 只读取文件，不接收模型生成的任意命令。
- 模型不能绕过 WPF 确认卡片直接触发 C++ 工具网关。
- 工具网关不得执行任意 AutoCAD 命令字符串。
- 失败时必须返回 `errorCode` 和用户可读 `message`。
