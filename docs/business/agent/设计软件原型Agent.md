# 设计软件原型 Agent

## 功能范围

本功能提供 RoadProto 内嵌 AI 对话入口。用户可通过右侧 Agent 面板进行问答、软件操作咨询和受控工具调用。首版支持自动化创建路基模板。

## 命令

- `RD_AI_ASSISTANT_OPEN`：打开右侧 Agent 面板。
- `RD_AI_EXECUTE_TOOL_FILE`：执行 Agent 工具 JSON 文件。

## 边界

- Agent 面板不直接读写 CAD 实体。
- 后端服务不依赖 ObjectARX。
- C++ 只执行白名单工具。
- 首版工具只创建 `DnSubgradeTemplateEntity`。

## 首版流程

1. 用户运行 `RD_AI_ASSISTANT_OPEN`。
2. 系统打开右侧 WPF Agent 面板。
3. 用户输入创建路基模板需求。
4. 后端读取 skill 文档并规划工具调用。
5. WPF 展示确认卡片。
6. 用户确认后，C++ 执行 `cross_section.subgrade_template.create`。
7. 系统创建路基模板实体并返回 handle。
