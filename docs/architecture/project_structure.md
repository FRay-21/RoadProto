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
        AutoCad/
        Bridge/
        ViewModels/
    resources/
  agent/                  # Agent 原型：本地 sidecar 和相关测试
    RoadProto.Agent.Host/
    RoadProto.Agent.Tests/
docs/
  agent/                  # Agent 原型：总览、工具协议和 skill 文档
  business/
    agent/                # Agent 原型：设计软件原型 Agent 业务文档
  rules/
  modules/
  reuse/
  architecture/
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
samples/
tests/
third_party/
  delaunator-cpp/
```

部分模块目录目前只是预留。这样做是为了后续新增命令时不必重新调整仓库结构。
