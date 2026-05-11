# RoadProto V0.1 实施计划

> **给后续 agentic worker 的说明：** 执行该计划时应使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans`，并按任务清单逐项推进。

**目标：** 建立 V0.1 ObjectARX/C++ 道路设计原型功能框架。

**架构：** core、domain、application 不依赖 ObjectARX。ObjectARX 访问限制在 `cad_adapter/objectarx` 和 `app/arx_entry`。模块和命令通过统一 registry 注册。

**技术栈：** C++17、Visual Studio/MSBuild、AutoCAD 2021 x64、ObjectARX 2021。

---

### 任务 1：项目骨架

**文件：**
- 创建：`RoadProto.sln`
- 创建：`build/RoadProto.Build.props`
- 创建：`build/ObjectARX2021.props`
- 创建：`src/app/RoadProtoArx.vcxproj`
- 创建：`tests/RoadProtoCoreTests.vcxproj`

- [x] 创建 Visual Studio 解决方案和项目文件。
- [x] 配置 ARX 输出名为 `RoadProto_v0.1.0_20260508_Framework.arx`。
- [x] 配置输出目录为 `artifacts/x64/<Configuration>/`。

### 任务 2：核心注册机制

**文件：**
- 创建：`src/core/command/CommandRegistry.*`
- 创建：`src/core/module/ModuleRegistry.*`

- [x] 实现命令元数据和重复注册检查。
- [x] 实现模块元数据和生命周期回调。
- [x] 增加命令和模块注册行为测试。

### 任务 3：实体关系机制

**文件：**
- 创建：`src/domain/common/EntityId.*`
- 创建：`src/domain/relation/DesignEntity.h`
- 创建：`src/domain/relation/EntityRelationManager.*`

- [x] 实现实体 ID 和统一设计实体描述。
- [x] 实现依赖图操作。
- [x] 实现脏标记和重建请求传播。
- [x] 增加地形到下游实体的传播测试。

### 任务 4：ARX 与适配层边界

**文件：**
- 创建：`src/app/arx_entry/RoadProtoArxEntry.cpp`
- 创建：`src/cad_adapter/objectarx/ObjectArx*.{h,cpp}`

- [x] 实现 ARX 入口生命周期。
- [x] 从 `CommandRegistry` 统一注册 ObjectARX 命令。
- [x] 增加编辑器输出适配器。
- [x] 增加 Ribbon 适配器占位。

### 任务 5：示例模块与文档

**文件：**
- 创建：`src/modules/terrain/*`
- 创建：`src/modules/intersection/*`
- 创建：`src/application/terrain/*`
- 创建：`src/application/intersection/*`
- 创建：`docs/business/*`
- 创建：`docs/reuse/*`
- 创建：`docs/dev/version_log.md`

- [x] 增加地形数模示例命令。
- [x] 增加平交口示例命令。
- [x] 增加架构、业务、复用、版本和 AI 开发规则文档。

### 任务 6：验证

- [x] 构建 `tests/RoadProtoCoreTests.vcxproj`。
- [x] 运行 `artifacts/x64/Debug/RoadProtoCoreTests.exe`。
- [ ] 在安装 ObjectARX 2021 SDK 和 v142 工具集的环境中构建 ARX。
- [ ] 在 AutoCAD 2021 中加载生成的 ARX，并运行 `RD_TERRAIN_MARKDIRTY`。
