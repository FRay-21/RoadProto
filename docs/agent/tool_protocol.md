# Agent 工具协议

## 请求文件

工具请求文件使用 UTF-8 JSON。顶层结构：

```json
{
  "tool": "cross_section.subgrade_template.create",
  "requestId": "uuid",
  "arguments": {},
  "resultPath": "%TEMP%/RoadProtoAgentToolResult_<pid>_<requestId>.json"
}
```

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
  "errorCode": "InvalidDisplayScale",
  "message": "路基模板显示比例必须为 1、10、20、50 或 100。"
}
```

## 安全规则

- C++ 只接受白名单工具名。
- WPF 必须在执行前展示确认卡片。
- `RD_AI_EXECUTE_TOOL_FILE` 只读取文件，不接收模型生成的任意命令。
- 失败时必须返回 `errorCode` 和用户可读 `message`。
