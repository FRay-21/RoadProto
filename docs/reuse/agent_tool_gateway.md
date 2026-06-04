# Agent 工具网关复用说明

## 能力

当前状态：计划文档，尚未实现。Agent 工具网关计划提供从 Agent 工具 JSON 到 C++ application/cad_adapter 的受控执行能力。

## 复用边界

- 工具请求解析放在 `src/application/agent`。
- AutoCAD 选择、点取、实体创建放在 `src/cad_adapter/objectarx/agent`。
- 新工具必须注册到白名单。
- 新工具必须有业务文档和 Agent skill 文档。

## 计划新增工具流程

1. 编写 Agent skill。
2. 定义工具 JSON schema。
3. 在 application 层增加参数转换和校验。
4. 在 cad_adapter 层增加执行器。
5. 在 `AI_AGENT` 模块注册命令或工具。
6. 增加核心测试和 AutoCAD 手工验证。
