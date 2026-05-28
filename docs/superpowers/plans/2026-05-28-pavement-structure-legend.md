# 路面结构图例实施计划

> **给后续执行者：** 本计划必须按任务勾选执行。涉及代码行为时先写失败测试，再实现，再运行验证。以后 `docs/superpowers/plans/` 下的计划文档统一使用中文编写；代码标识、命令名、路径和构建命令保持原文。

**目标：** 新增 `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND` 命令，从当前道路模型或已绘制横断面图收集路面结构层模板，并用普通 CAD 实体绘制路面结构图例。

**架构：** `domain/quantity` 负责模板字段格式化、厚度换算和图例布局计划，不依赖 ObjectARX。`cad_adapter/objectarx/drawing_quantity` 负责对象选择、实体读取、模型空间扫描、插入点点取和普通 CAD 实体绘制。`modules/drawing_quantity` 与托管 Ribbon 只负责命令入口。

**技术栈：** C++17、ObjectARX 2021、RoadProto 核心测试、WPF AutoCAD Ribbon 源码契约测试。

---

### 任务 1：领域层图例计划

**文件：**
- 新增：`src/domain/quantity/PavementStructureLegend.h`
- 新增：`src/domain/quantity/PavementStructureLegend.cpp`
- 修改：`tests/core_tests.cpp`
- 修改：`tests/RoadProtoCoreTests.vcxproj`
- 修改：`src/app/RoadProtoArx.vcxproj`

- [x] **步骤 1：写失败测试**

在 `tests/core_tests.cpp` 中新增 `pavementStructureLegendPlannerFormatsTemplateColumnsAndUnmergedLegendItems`，验证：

- 模板列按 handle 去重并保留首次出现顺序。
- `structureCode`、路基土组、路基干湿类型、设计弯沉、累计当量轴次从模板属性读取。
- 厚度由米换算为厘米，等厚显示单值，非等厚显示 `内/外`。
- 总厚度使用非等厚层平均厚度。
- 底部图例项不合并，同填充样式不同层名仍分别输出。

- [x] **步骤 2：运行测试确认 RED**

运行：

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

预期：构建失败，提示 `domain/quantity/PavementStructureLegend.h` 不存在。

- [x] **步骤 3：实现最小领域模型**

新增 `PavementStructureLegendTemplateSource`、`PavementStructureLegendLayerPlan`、`PavementStructureLegendColumnPlan`、`PavementStructureLegendItemPlan`、`PavementStructureLegendLayoutPlan`、`PavementStructureLegendPlan` 和 `PavementStructureLegendPlanner::build`。

实现规则：

- 按 handle 去重模板列。
- 不去重 `legendItems`。
- 米转厘米。
- 整数厘米不带小数，非整数保留一位小数。
- 非等厚层使用平均厚度绘图和累计总厚度，文字显示 `inner/outer`。
- 路基土组和干湿类型用 `、` 连接。

- [x] **步骤 4：运行测试确认 GREEN**

运行 Debug 核心构建和 `artifacts\x64\Debug\RoadProtoCoreTests.exe`。

预期：构建成功，测试输出 `All RoadProto core tests passed.`。

### 任务 2：命令元数据与源码契约

**文件：**
- 修改：`tests/core_tests.cpp`
- 修改：`src/modules/drawing_quantity/DrawingQuantityModule.cpp`
- 新增：`src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.h`
- 新增：`src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.cpp`
- 修改：`src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- 修改：`src/app/RoadProtoArx.vcxproj`
- 修改：`tests/RoadProtoCoreTests.vcxproj`

- [x] **步骤 1：写失败源码契约测试**

在核心测试中检查：

```cpp
CHECK(commands.contains(L"RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND"));
CHECK(source.find("DnRoadModelEntity") != std::string::npos);
CHECK(source.find("DnRoadModelSectionDrawingEntity") != std::string::npos);
CHECK(source.find("DnPavementLayerTemplateEntity") != std::string::npos);
CHECK(source.find("collectSectionDrawingsForRoadModel") != std::string::npos);
CHECK(source.find("appendOrdinaryLegendEntities") != std::string::npos);
CHECK(ribbonSource.find("RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND ") != std::string::npos);
```

- [x] **步骤 2：运行测试确认 RED**

运行 Debug 核心构建和测试。预期：源码契约失败，因为命令和 Ribbon 入口尚未实现。

- [x] **步骤 3：增加注册骨架**

新增命令头文件，暴露：

```cpp
core::CommandProcedure pavementStructureLegendCommandProcedure();
```

新增命令源文件，先包含测试构建 stub 和后续 ObjectARX 所需函数名。注册 `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND`，显示名 `路面结构图例`，业务文档路径 `docs/business/drawing_quantity/路面结构图例.md`。托管 Ribbon 增加按钮并发送 `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND `。

- [x] **步骤 4：运行测试确认 GREEN**

运行 Debug 核心构建和测试。

### 任务 3：ObjectARX 普通实体绘制

**文件：**
- 修改：`src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.cpp`
- 修改：`tests/core_tests.cpp`

- [x] **步骤 1：扩展失败源码契约测试**

要求命令源码包含普通 CAD 实体 API：

```cpp
CHECK(commandSource.find("new AcDbLine") != std::string::npos);
CHECK(commandSource.find("new AcDbPolyline") != std::string::npos);
CHECK(commandSource.find("new AcDbText") != std::string::npos);
CHECK(commandSource.find("new AcDbHatch") != std::string::npos);
CHECK(commandSource.find("acedGetPoint") != std::string::npos);
CHECK(commandSource.find("appendAcDbEntity") != std::string::npos);
CHECK(commandSource.find("PavementStructureLegendPlanner::build") != std::string::npos);
```

- [x] **步骤 2：运行测试确认 RED**

运行 Debug 核心构建和测试。预期：缺少普通实体绘制 API。

- [x] **步骤 3：实现 ObjectARX 命令**

实现：

- 支持选择 `DnRoadModelEntity` 或 `DnRoadModelSectionDrawingEntity`。
- 若选择横断面图，读取 `roadModelHandle` 并扫描模型空间同一道路模型的所有横断面图。
- 优先从横断面图 `faces[].sourceTemplateHandle` 收集模板 handle，再从道路模型 `pavementLayerLines` 兼容回退。
- 读取 `DnPavementLayerTemplateEntity::templateData()`。
- 使用 `acedGetPoint` 点取插入位置。
- 创建普通 `AcDbLine`、`AcDbPolyline`、`AcDbText`、`AcDbHatch` 并追加到模型空间。
- 绘制字段行、模板列、层厚矩形、每层厚度文字、总厚度行和底部不合并图例项。

- [x] **步骤 4：运行测试确认 GREEN**

运行 Debug 核心构建和测试。

### 任务 4：文档与版本记录

**文件：**
- 新增：`docs/business/drawing_quantity/路面结构图例.md`
- 修改：`docs/modules/drawing_quantity.md`
- 修改：`docs/modules/module_index.md`
- 修改：`docs/reuse/capability_catalog.md`
- 修改：`README.md`
- 修改：`tests/README.md`
- 修改：`docs/dev/version_log.md`

- [x] **步骤 1：补充文档存在性测试**

核心测试补充检查路面结构图例业务文档存在；README、模块文档、复用目录、测试说明和版本记录随实现同步更新：

- `路面结构图例`
- `PavementStructureLegendPlanner`
- `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND`

- [x] **步骤 2：运行测试确认 RED**

运行 Debug 核心构建和测试。预期：新增文档前业务文档存在性检查会失败。

- [x] **步骤 3：更新文档**

记录命令行为、输入对象、普通 CAD 输出、字段行、底部图例不合并规则、测试范围和 AutoCAD 手工验证步骤。

- [x] **步骤 4：运行测试确认 GREEN**

运行 Debug 核心构建和测试。

### 任务 5：完整验证

- [x] **步骤 1：Debug 核心构建和测试**

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

- [x] **步骤 2：Release 核心构建和测试**

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Release /p:Platform=x64
artifacts\x64\Release\RoadProtoCoreTests.exe
```

- [x] **步骤 3：构建托管 Ribbon 插件**

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

- [x] **步骤 4：构建完整 Release 解决方案**

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

- [ ] **步骤 5：检查空白字符**

```powershell
git diff --check
```
