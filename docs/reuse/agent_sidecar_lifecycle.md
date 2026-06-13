# Agent sidecar 生命周期管理

## 能力分类

模块专用能力。

## 能力说明

该能力用于在 AutoCAD WPF Agent 面板打开时自动启动本地 `.NET 8` sidecar，并在面板关闭或托管插件卸载时关闭本次由面板启动的进程。它解决用户每次手动运行 `RoadProto.Agent.Host` 的负担，同时避免误杀用户手动启动或其他进程提供的现有后端。

## 当前实现

- 源码路径：`src/ui/wpf/RoadProto.Terrain.UI/Services/AgentBackendProcess.cs`
- 对外类型/函数：`AgentBackendProcess.StartOrAttachAsync()`、`AgentBackendProcess.StopOwnedProcess()`、`AgentBackendProcess.HasOwnedProcess`
- 当前使用该能力的命令：`RD_AI_ASSISTANT_OPEN`

## 可复用内容

- 先探测 `/health`，已有可用服务时只复用连接，不取得关闭所有权。
- 自动从当前托管 DLL 所在路径向上查找项目根目录，并优先启动 `artifacts/agent/<Configuration>/net8.0/RoadProto.Agent.Host.exe`。
- 只记录并关闭本次由 WPF 面板启动的进程，避免影响外部手动启动的服务。
- 在托管插件卸载时兜底关闭自有进程，减少 AutoCAD 退出后的后台残留。

## 不可复用或临时内容

- 当前固定服务地址为 `http://127.0.0.1:17831`，服务名固定为 `RoadProto.Agent.Host`。
- 当前只服务于 Agent 面板，不作为项目级通用进程监督器。
- 当前关闭方式以进程退出为目标，不负责保存运行期管理控制台状态；配置已由后端自身写入 `.roadproto-agent/`。

## 依赖关系

- domain 依赖：无。
- cad_adapter 依赖：无。
- 模块依赖：依赖托管 WPF 插件和 Agent Host 构建产物。

## 扩展说明

- 若后续新增多个本地 sidecar，应为每个 sidecar 独立维护健康检查地址、构建产物路径和进程所有权，不要共用一个全局强杀逻辑。
- 若后续允许用户在面板内配置端口，应把端口配置同步到 `AgentBackendClient` 和 `AgentBackendProcess`，并在文档中记录默认值和迁移规则。

## 验证方式

- 测试路径：`tests/core_tests.cpp` 的 Agent 源码契约检查覆盖自动启动、关闭自有进程和输入焦点保护入口。
- AutoCAD 手工验证：加载 ARX 和托管 DLL 后运行 `RD_AI_ASSISTANT_OPEN`，确认后端自动启动；关闭 Agent 面板后确认本次自动启动的 `RoadProto.Agent.Host` 退出；手动预先启动后端时，关闭面板不应结束该外部进程。
