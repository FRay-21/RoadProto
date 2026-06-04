# RoadProto Agent 总览

## 目标

RoadProto Agent 是 CAD 内嵌设计软件原型助手。首版提供右侧 WPF 对话面板、本地 Agent 后端、模型 Provider 配置、Agent skill 文档读取、受控工具调用和首个自动化工具：创建路基模板。

## 边界

- WPF 只负责输入、展示、确认和结果反馈。
- Agent 后端不依赖 ObjectARX，不读写 CAD 对象。
- C++ 工具网关只执行白名单工具。
- domain/application 继续承载业务规则。
- cad_adapter/objectarx 继续承载 AutoCAD API 调用。

## 首版命令

- `RD_AI_ASSISTANT_OPEN`：打开右侧 Agent 面板。
- `RD_AI_EXECUTE_TOOL_FILE`：执行 Agent 工具 JSON 文件。

## 首版工具

- `cross_section.subgrade_template.create`：自动化创建 `DnSubgradeTemplateEntity` 路基模板实体。

## 不做范围

- 不执行任意 AutoCAD 命令字符串。
- 不把 API Key 写入仓库。
- 不让 Agent 凭空绑定路面结构层模板 handle。
- 不在 WPF 中写 CAD 核心业务逻辑。
