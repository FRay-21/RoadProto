# 推荐目录结构

```text
src/
  app/
    arx_entry/
    startup/
  core/
    command/
    module/
    logging/
    config/
    error/
    version/
  cad_adapter/
    objectarx/
      agent/              # Agent 原型：受控工具网关的 ObjectARX 执行落点
    transaction/
    selection/
    entity/
    layer/
    annotation/
    block/
    geometry/
  domain/
    common/
    geometry/
    road/
    terrain/
    alignment/
    intersection/
    profile/
    cross_section/
    quantity/
    relation/
  application/
    agent/                # Agent 原型：工具请求解析、schema 校验和参数转换
    terrain/
    alignment/
    intersection/
    profile/
    cross_section/
    drawing_quantity/
  modules/
    agent/                # Agent 原型：AI_AGENT 模块注册和命令元数据
    terrain/
    alignment/
    interchange/
    intersection/
    profile/
    cross_section/
    drawing_quantity/
    utils/
  ui/
    ribbon/
    dialogs/
    wpf/
      RoadProto.Terrain.UI/
        AgentAssistantControl.xaml(.cs)  # Agent 原型：右侧对话面板和工具确认卡片
        AutoCad/                         # Agent 原型：请求文件生成、托管命令和 Ribbon 入口
        Bridge/                          # Agent 原型：WPF DTO；其他 WPF bridge DTO 也在此目录
        Services/                        # Agent 原型：后端 HTTP 客户端
        ViewModels/
    resources/
  agent/                  # Agent 原型：本地 sidecar 和相关测试
    RoadProto.Agent.Host/
      Admin/              # 模型配置、API Key 加密存储、Markdown 文档管理和 /api/admin/*
      Models/             # 后端请求、响应和工具调用 DTO
      Providers/          # OpenAI-compatible 等模型 Provider 适配
      Services/           # 聊天编排、prompt 上下文和配置读取
      Skills/             # 内置 Markdown skill 读取
      Tools/              # 本地规则 planner 和工具参数推断
      wwwroot/admin/      # 独立浏览器管理控制台
    RoadProto.Agent.Tests/
docs/
  architecture/
    agent_code_structure.md  # Agent 原型：代码、文档、运行期目录和扩展落点主契约
  agent/                  # Agent 原型：总览、工具协议和 skill 文档
    overview.md
    tool_protocol.md
    skill_authoring_rules.md
    skills/
  business/
    agent/                # Agent 原型：设计软件原型 Agent 业务文档
  rules/
  modules/
  reuse/
  dev/
assets/
  icons/
artifacts/
  x64/
    Debug/
    Release/
  managed/
    Debug/
    Release/
  agent/
    Debug/
    Release/
samples/
tests/
third_party/
  delaunator-cpp/
.roadproto-agent/          # 本机运行期数据：配置、DPAPI 密钥、上传 skill、知识库和日志；不提交
```

部分模块目录目前只是预留。这样做是为了后续新增命令时不必重新调整仓库结构。

Agent 相关目录的详细职责、禁止事项和新增工具/Provider 流程见 `docs/architecture/agent_code_structure.md`。`src/agent/**/bin/`、`src/agent/**/obj/`、`.roadproto-agent/` 和 `%TEMP%\RoadProtoAgent\` 均为构建或运行期数据，不属于正式源码结构。
