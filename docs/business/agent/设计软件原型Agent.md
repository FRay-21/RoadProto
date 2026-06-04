# 设计软件原型 Agent

## 功能范围

本功能是 RoadProto 内嵌 AI 对话入口的原型计划文档。后续计划允许用户通过右侧 Agent 面板进行问答、软件操作咨询和受控工具调用，并以自动化创建路基模板作为首个原子函数。

当前状态：仅完成文档基线和设计入口说明，右侧 Agent 面板、本地后端、C++ 工具网关和自动创建路基模板能力仍在开发中。

## 命令

- `RD_AI_ASSISTANT_OPEN`：计划用于打开右侧 Agent 面板。
- `RD_AI_EXECUTE_TOOL_FILE`：计划用于执行 Agent 工具 JSON 文件。

## 边界

- Agent 面板不直接读写 CAD 实体。
- 后端服务不依赖 ObjectARX。
- C++ 只执行白名单工具。
- 首版计划工具只创建 `DnSubgradeTemplateEntity`。

## 计划流程

1. 用户运行 `RD_AI_ASSISTANT_OPEN`。
2. 系统打开右侧 WPF Agent 面板。
3. 用户输入创建路基模板需求。
4. 后端读取 skill 文档并规划工具调用。
5. WPF 展示确认卡片。
6. 用户确认后，C++ 执行 `cross_section.subgrade_template.create`。
7. 计划由 C++ 工具网关创建路基模板实体并返回 handle。

上述流程为待实现流程。当前文档不表示命令、面板或工具网关已可在 AutoCAD 中运行。
