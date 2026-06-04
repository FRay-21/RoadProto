# AI_AGENT 模块说明

## 模块定位

`AI_AGENT` 是设计软件原型 Agent 的计划模块，用于承接 RoadProto 右侧 Agent 面板、本地 Agent 后端对接、受控工具调用网关和 Agent skill 文档体系。

当前状态：仅建立文档基线和模块索引。C++ 模块注册、WPF 面板、后端服务和工具执行器仍在开发中。

## 计划命令

| 命令 | 状态 | 说明 | 业务文档 |
| --- | --- | --- | --- |
| `RD_AI_ASSISTANT_OPEN` | 待实现 | 打开或激活 AutoCAD 右侧 Agent 面板 | `docs/business/agent/设计软件原型Agent.md` |
| `RD_AI_EXECUTE_TOOL_FILE` | 待实现 | 执行受控 Agent 工具 JSON 文件 | `docs/business/agent/设计软件原型Agent.md` |

## 计划代码落点

- `src/modules/agent`：计划承接模块注册和命令元数据。
- `src/application/agent`：计划承接工具请求解析、schema 校验和参数转换。
- `src/cad_adapter/objectarx/agent`：计划承接 AutoCAD 点取、实体创建和结果写回。
- `src/ui/wpf/RoadProto.Terrain.UI`：计划承接右侧 Agent 面板和执行确认卡片。
- `src/agent/RoadProto.Agent.Host`：计划承接本地 Agent sidecar。

## 功能文档

- Agent 业务入口：`docs/business/agent/设计软件原型Agent.md`
- Agent 总览：`docs/agent/overview.md`
- 工具协议：`docs/agent/tool_protocol.md`
- Skill 写作规则：`docs/agent/skill_authoring_rules.md`
- 路基模板创建 skill：`docs/agent/skills/cross_section/subgrade_template_create.md`

## 边界

- WPF 不直接读写 CAD 实体。
- 后端服务不依赖 ObjectARX。
- C++ 工具网关只执行白名单工具。
- 自动化工具必须复用现有 domain/application/cad_adapter 能力。
