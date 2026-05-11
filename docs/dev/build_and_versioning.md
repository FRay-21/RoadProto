# 构建与版本管理

## 本机优先编译工具链

当前本机优先使用 Visual Studio 2026 Insiders 编译 RoadProto：

- IDE：`D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\devenv.exe`
- MSBuild：`D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe`

命令行构建示例：

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

注意：

- 使用 VS2026 只代表本机优先编译入口，不代表项目升级 AutoCAD 或 ObjectARX 目标版本。
- 当前 CAD 目标仍是 AutoCAD 2021 x64 / ObjectARX 2021，工程平台工具集按项目文件配置执行。
- 如果 VS2026 缺少当前工程所需兼容工具集，可临时退回已安装的兼容 MSBuild，但需要在提交说明或版本记录中写明原因。

## 运行期内存排查

AutoCAD 图形界面或 Core Console 测试后，如果出现系统内存异常，需要先记录进程证据：

```powershell
Get-CimInstance Win32_Process -Filter "name='taskhostw.exe'" |
  Select-Object ProcessId,CommandLine,@{Name='WorkingSetMB';Expression={[math]::Round($_.WorkingSetSize/1MB,1)}}
```

同一时间还应检查 `acad.exe`、`accoreconsole.exe`、`Codex.exe`、`devenv.exe` 和 MSBuild 进程。`taskhostw.exe` 是 Windows 宿主进程，只有在内存持续异常、命令行和 RoadProto 操作时间线能对应时，才作为本项目问题继续追踪。测试结束后应关闭残留 AutoCAD 进程，避免重复加载 ARX / 托管 DLL 后累计占用内存。

## ARX 命名规则

```text
RoadProto_版本号_日期_阶段.arx
```

示例：

```text
RoadProto_v0.1.0_20260508_Framework.arx
RoadProto_v0.1.1_20260509_CommandRegistry.arx
RoadProto_v0.2.0_20260515_CadAdapter.arx
```

## 版本阶段

- `v0.1.x`：框架搭建阶段。
- `v0.2.x`：基础 CAD 能力封装阶段。
- `v0.3.x`：道路通用能力沉淀阶段。
- `v1.0.0`：稳定可复用框架。

## 输出目录

- Debug：`artifacts/x64/Debug/`
- Release：`artifacts/x64/Release/`
- 托管 AutoCAD / WPF 插件 Debug：`artifacts/managed/Debug/net48/`
- 托管 AutoCAD / WPF 插件 Release：`artifacts/managed/Release/net48/`

## MSBuild 属性

版本信息集中在 `build/RoadProto.Build.props`：

- `RoadProtoVersion`
- `RoadProtoBuildDate`
- `RoadProtoStage`
- `RoadProtoArxBaseName`

ObjectARX 路径集中在 `build/ObjectARX2021.props`：

- `ObjectARX2021Dir`
- `AutoCAD2021Dir`

## 必须更新的版本记录

每次生成 ARX 后，都必须更新 `docs/dev/version_log.md`，至少记录：

- 版本号。
- 日期。
- ARX 文件名。
- 新增内容。
- 修改内容。
- 已知问题。
- 是否可作为稳定测试版本。
