# 路面工程量统计断面计算方法实施计划

> **给 agentic workers：** 必须使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 按任务执行。步骤使用复选框（`- [ ]`）跟踪。

**目标：** 为 `RD_DRAWING_PAVEMENT_QUANTITY_TABLE` 增加“平均断面法”和“依照路面面积方法”两种断面计算方法选择。

**架构：** 体积算法选择沉淀在 `domain/quantity/PavementQuantityTable.*`，CAD 命令只在保存对话框中收集用户选项并转发。Excel 写出层继续只消费最终 `PavementQuantityTable`，不感知算法来源。

**技术栈：** C++17、ObjectARX 2021、Windows `IFileDialogCustomize`、RoadProto 核心测试 `tests/RoadProtoCoreTests.vcxproj`。

---

## 文件结构

- 修改 `tests/core_tests.cpp`：先增加领域算法测试和 CAD 对话框源码契约测试。
- 修改 `src/domain/quantity/PavementQuantityTable.h`：新增断面计算方法枚举和 `build` 重载。
- 修改 `src/domain/quantity/PavementQuantityTable.cpp`：实现 `PlanAreaByThickness` 体积算法，保留旧默认行为。
- 修改 `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.cpp`：保存对话框增加“断面计算方法”单选组并传给领域计算器。
- 修改 `docs/business/drawing_quantity/路面工程量统计表.md`：同步业务规则。
- 修改 `docs/reuse/pavement_quantity_table.md`、`docs/reuse/capability_catalog.md`、`docs/modules/drawing_quantity.md`：同步复用和模块说明。
- 修改 `README.md`、`tests/README.md`、`docs/dev/version_log.md`：同步入口说明、测试说明和版本记录。

---

### Task 1: 先写领域算法失败测试

**Files:**
- Modify: `tests/core_tests.cpp`
- Test: `tests/RoadProtoCoreTests.vcxproj`

- [ ] **Step 1: 增加 `PlanAreaByThickness` 和显式平均断面法测试**

在 `pavementQuantityTableSplitsByStructuresAndUsesAverageEndAreaMethod()` 后添加：

```cpp
void pavementQuantityTableKeepsDefaultAverageEndAreaCalculationMethod()
{
    using namespace roadproto::domain::quantity;

    std::vector<PavementQuantitySectionSample> samples = {
        PavementQuantitySectionSample{0.0, {PavementQuantityLayerSample{L"面层", 10.0, 1.0}}},
        PavementQuantitySectionSample{100.0, {PavementQuantityLayerSample{L"面层", 20.0, 4.0}}},
    };

    std::wstring errorMessage;
    const auto defaultResult = PavementQuantityTableCalculator::build(samples, {}, errorMessage);
    CHECK(errorMessage.empty());

    errorMessage.clear();
    const auto explicitResult = PavementQuantityTableCalculator::build(
        samples,
        {},
        PavementQuantityAggregationMode::ByLayerType,
        PavementQuantityCalculationMethod::AverageEndArea,
        errorMessage);

    CHECK(errorMessage.empty());
    CHECK(defaultResult.rows.size() == 1);
    CHECK(explicitResult.rows.size() == 1);
    if (defaultResult.rows.size() == 1 && explicitResult.rows.size() == 1) {
        CHECK(std::fabs(defaultResult.rows[0].totals[0].projectedArea - explicitResult.rows[0].totals[0].projectedArea) < 1.0e-9);
        CHECK(std::fabs(defaultResult.rows[0].totals[0].volume - explicitResult.rows[0].totals[0].volume) < 1.0e-9);
        CHECK(std::fabs(explicitResult.rows[0].totals[0].projectedArea - 1500.0) < 1.0e-9);
        CHECK(std::fabs(explicitResult.rows[0].totals[0].volume - 250.0) < 1.0e-9);
    }
}

void pavementQuantityTableCanCalculateVolumeByPlanAreaAndThickness()
{
    using namespace roadproto::domain::quantity;

    std::vector<PavementQuantitySectionSample> samples = {
        PavementQuantitySectionSample{0.0, {PavementQuantityLayerSample{L"面层", 10.0, 1.0}}},
        PavementQuantitySectionSample{100.0, {PavementQuantityLayerSample{L"面层", 20.0, 4.0}}},
    };

    std::wstring errorMessage;
    const auto result = PavementQuantityTableCalculator::build(
        samples,
        {},
        PavementQuantityAggregationMode::ByLayerType,
        PavementQuantityCalculationMethod::PlanAreaByThickness,
        errorMessage);

    CHECK(errorMessage.empty());
    CHECK(result.rows.size() == 1);
    if (result.rows.size() == 1 && result.rows[0].totals.size() == 1) {
        CHECK(std::fabs(result.rows[0].totals[0].projectedArea - 1500.0) < 1.0e-9);
        CHECK(std::fabs(result.rows[0].totals[0].volume - 225.0) < 1.0e-9);
    }
}
```

- [ ] **Step 2: 增加 0 投影宽度不除零测试**

继续添加：

```cpp
void pavementQuantityTablePlanAreaMethodTreatsZeroWidthThicknessAsZero()
{
    using namespace roadproto::domain::quantity;

    std::vector<PavementQuantitySectionSample> samples = {
        PavementQuantitySectionSample{0.0, {PavementQuantityLayerSample{L"面层", 0.0, 5.0}}},
        PavementQuantitySectionSample{50.0, {PavementQuantityLayerSample{L"面层", 0.0, 3.0}}},
    };

    std::wstring errorMessage;
    const auto result = PavementQuantityTableCalculator::build(
        samples,
        {},
        PavementQuantityAggregationMode::ByLayerType,
        PavementQuantityCalculationMethod::PlanAreaByThickness,
        errorMessage);

    CHECK(errorMessage.empty());
    CHECK(result.rows.size() == 1);
    if (result.rows.size() == 1 && result.rows[0].totals.size() == 1) {
        CHECK(std::fabs(result.rows[0].totals[0].projectedArea - 0.0) < 1.0e-9);
        CHECK(std::fabs(result.rows[0].totals[0].volume - 0.0) < 1.0e-9);
    }
}
```

- [ ] **Step 3: 注册新测试**

在 `main()` 中 `pavementQuantityTableSplitsByStructuresAndUsesAverageEndAreaMethod();` 后添加：

```cpp
    pavementQuantityTableKeepsDefaultAverageEndAreaCalculationMethod();
    pavementQuantityTableCanCalculateVolumeByPlanAreaAndThickness();
    pavementQuantityTablePlanAreaMethodTreatsZeroWidthThicknessAsZero();
```

- [ ] **Step 4: 运行测试并确认失败原因正确**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: 编译失败，错误包含 `PavementQuantityCalculationMethod` 未声明或找不到匹配的 `PavementQuantityTableCalculator::build` 重载。

---

### Task 2: 实现领域算法

**Files:**
- Modify: `src/domain/quantity/PavementQuantityTable.h`
- Modify: `src/domain/quantity/PavementQuantityTable.cpp`
- Test: `tests/core_tests.cpp`

- [ ] **Step 1: 在头文件新增枚举和重载**

在 `PavementQuantityAggregationMode` 后添加：

```cpp
enum class PavementQuantityCalculationMethod {
    AverageEndArea,
    PlanAreaByThickness,
};
```

在 `PavementQuantityTableCalculator` 中添加重载：

```cpp
    static PavementQuantityTable build(
        const std::vector<PavementQuantitySectionSample>& samples,
        const std::vector<PavementQuantityStructureRange>& structures,
        PavementQuantityAggregationMode aggregationMode,
        PavementQuantityCalculationMethod calculationMethod,
        std::wstring& errorMessage);
```

- [ ] **Step 2: 在实现文件新增厚度计算辅助函数**

在 `layerValueAt(...)` 后添加：

```cpp
double layerThicknessAt(
    const std::vector<PavementQuantitySectionSample>& samples,
    const std::wstring& layerName,
    double station)
{
    const auto width = layerValueAt(samples, layerName, station, true);
    if (width <= kStationTolerance) {
        return 0.0;
    }
    const auto area = layerValueAt(samples, layerName, station, false);
    return area / width;
}
```

- [ ] **Step 3: 让旧重载委托到平均断面法**

把现有实现改为：

```cpp
PavementQuantityTable PavementQuantityTableCalculator::build(
    const std::vector<PavementQuantitySectionSample>& samples,
    const std::vector<PavementQuantityStructureRange>& structures,
    std::wstring& errorMessage)
{
    return build(
        samples,
        structures,
        PavementQuantityAggregationMode::ByLayerType,
        PavementQuantityCalculationMethod::AverageEndArea,
        errorMessage);
}

PavementQuantityTable PavementQuantityTableCalculator::build(
    const std::vector<PavementQuantitySectionSample>& samples,
    const std::vector<PavementQuantityStructureRange>& structures,
    PavementQuantityAggregationMode aggregationMode,
    std::wstring& errorMessage)
{
    return build(
        samples,
        structures,
        aggregationMode,
        PavementQuantityCalculationMethod::AverageEndArea,
        errorMessage);
}
```

- [ ] **Step 4: 新增带计算方法参数的主实现**

把原带 `aggregationMode` 的主实现签名改为：

```cpp
PavementQuantityTable PavementQuantityTableCalculator::build(
    const std::vector<PavementQuantitySectionSample>& samples,
    const std::vector<PavementQuantityStructureRange>& structures,
    PavementQuantityAggregationMode aggregationMode,
    PavementQuantityCalculationMethod calculationMethod,
    std::wstring& errorMessage)
```

在累计循环中替换体积计算为：

```cpp
            const auto segmentProjectedArea = (startWidth + endWidth) * 0.5 * length;
            row.totals[layerIndex].projectedArea += segmentProjectedArea;
            if (calculationMethod == PavementQuantityCalculationMethod::PlanAreaByThickness) {
                const auto startThickness = layerThicknessAt(normalizedSamples, layerName, start);
                const auto endThickness = layerThicknessAt(normalizedSamples, layerName, end);
                row.totals[layerIndex].volume += segmentProjectedArea * (startThickness + endThickness) * 0.5;
            } else {
                row.totals[layerIndex].volume += (startArea + endArea) * 0.5 * length;
            }
```

- [ ] **Step 5: 运行核心测试确认通过**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: `All RoadProto core tests passed.`

- [ ] **Step 6: 提交领域算法**

Run:

```powershell
git add tests/core_tests.cpp src/domain/quantity/PavementQuantityTable.h src/domain/quantity/PavementQuantityTable.cpp
git commit -m "feat: add pavement quantity calculation methods"
```

---

### Task 3: 给保存对话框增加断面计算方法选项

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.cpp`
- Test: `tests/RoadProtoCoreTests.vcxproj`

- [ ] **Step 1: 先扩展源码契约测试**

在 `pavementQuantityCommandSourceContainsAggregationModeSaveDialog()` 中追加：

```cpp
    CHECK(source.find("断面计算方法") != std::string::npos);
    CHECK(source.find("平均断面法") != std::string::npos);
    CHECK(source.find("依照路面面积方法") != std::string::npos);
    CHECK(source.find("PavementQuantityCalculationMethod::AverageEndArea") != std::string::npos);
    CHECK(source.find("PavementQuantityCalculationMethod::PlanAreaByThickness") != std::string::npos);
```

- [ ] **Step 2: 运行测试并确认失败原因正确**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: 测试失败，失败项来自新增的源码契约字符串未找到。

- [ ] **Step 3: 增加 CAD 命令引用和常量**

在 using 区添加：

```cpp
using roadproto::domain::quantity::PavementQuantityCalculationMethod;
```

在控制 ID 常量区添加：

```cpp
constexpr DWORD kCalculationMethodControlId = 201;
constexpr DWORD kCalculationMethodAverageEndAreaItemId = 1;
constexpr DWORD kCalculationMethodPlanAreaItemId = 2;
```

- [ ] **Step 4: 扩展输出路径提示函数签名和默认值**

把 `promptXlsFilePath` 签名改为：

```cpp
bool promptXlsFilePath(
    const ACHAR* title,
    const ACHAR* defaultFileName,
    std::wstring& path,
    PavementQuantityAggregationMode& aggregationMode,
    PavementQuantityCalculationMethod& calculationMethod,
    roadproto::cad_adapter::IEditor& editor)
```

函数开头添加默认值：

```cpp
    calculationMethod = PavementQuantityCalculationMethod::AverageEndArea;
```

- [ ] **Step 5: 在保存对话框中新增单选组**

在统计方式 `EndVisualGroup()` 后添加：

```cpp
            customize->StartVisualGroup(kCalculationMethodControlId + 10, L"断面计算方法");
            customize->AddRadioButtonList(kCalculationMethodControlId);
            customize->AddControlItem(
                kCalculationMethodControlId,
                kCalculationMethodAverageEndAreaItemId,
                L"平均断面法");
            customize->AddControlItem(
                kCalculationMethodControlId,
                kCalculationMethodPlanAreaItemId,
                L"依照路面面积方法");
            customize->SetSelectedControlItem(
                kCalculationMethodControlId,
                kCalculationMethodAverageEndAreaItemId);
            customize->EndVisualGroup();
```

- [ ] **Step 6: 读取断面计算方法选择**

在读取 `aggregationMode` 的代码后添加：

```cpp
                DWORD selectedCalculationMethod = kCalculationMethodAverageEndAreaItemId;
                if (SUCCEEDED(customize->GetSelectedControlItem(kCalculationMethodControlId, &selectedCalculationMethod))
                    && selectedCalculationMethod == kCalculationMethodPlanAreaItemId) {
                    calculationMethod = PavementQuantityCalculationMethod::PlanAreaByThickness;
                }
```

- [ ] **Step 7: 更新回退提示和命令调用**

把回退提示改为：

```cpp
    editor.writeWarning(L"系统保存对话框不可用，使用默认按部件和结构层统计模式、平均断面法。");
```

在命令主流程中增加变量：

```cpp
    PavementQuantityCalculationMethod calculationMethod = PavementQuantityCalculationMethod::AverageEndArea;
```

调用 `promptXlsFilePath` 时传入 `calculationMethod`，调用领域计算器时改为：

```cpp
    const auto table = PavementQuantityTableCalculator::build(
        samples,
        structures,
        aggregationMode,
        calculationMethod,
        errorMessage);
```

- [ ] **Step 8: 运行核心测试确认通过**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: `All RoadProto core tests passed.`

- [ ] **Step 9: 提交 CAD 对话框接入**

Run:

```powershell
git add tests/core_tests.cpp src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.cpp
git commit -m "feat: add pavement quantity method selector"
```

---

### Task 4: 同步业务文档、复用说明和验证记录

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `docs/business/drawing_quantity/路面工程量统计表.md`
- Modify: `docs/reuse/pavement_quantity_table.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/modules/drawing_quantity.md`
- Modify: `README.md`
- Modify: `tests/README.md`
- Modify: `docs/dev/version_log.md`

- [ ] **Step 1: 先补文档契约测试**

在 `pavementLayerTemplateDocumentationAndVersionContracts()` 的路面工程量统计表相关断言附近添加：

```cpp
    CHECK(drawingQuantityModule.find("断面计算方法") != std::string::npos);
    CHECK(drawingQuantityModule.find("依照路面面积方法") != std::string::npos);
    CHECK(quantityBusinessDoc.find("断面计算方法") != std::string::npos);
    CHECK(quantityBusinessDoc.find("依照路面面积方法") != std::string::npos);
```

读取复用文档后添加：

```cpp
    CHECK(quantityReuseDoc.find("依照路面面积方法") != std::string::npos);
```

如果当前函数中没有合适位置读取 `quantityReuseDoc`，使用已有变量，不重复读取文件。

- [ ] **Step 2: 运行测试并确认失败原因正确**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: 测试失败，失败项来自文档中尚未出现的“断面计算方法”或“依照路面面积方法”。

- [ ] **Step 3: 更新业务文档**

在 `docs/business/drawing_quantity/路面工程量统计表.md` 中调整：

- “用户输入参数”改为“输出 `.xls` 文件路径、统计方式和断面计算方法”。
- 操作流程第 6 步改为“选择输出路径、统计方式和断面计算方法”。
- 关键业务规则增加：

```markdown
- 断面计算方法支持 `平均断面法` 与 `依照路面面积方法`。
- `平均断面法` 为当前精确算法：体积按相邻断面同层横断面面积平均值乘以桩号间距。
- `依照路面面积方法` 中，面积列仍为平面投影面积；体积按该层平面投影面积乘以该层厚度，厚度由断面横断面面积除以投影宽度反推，相邻桩号取平均厚度。
```

- [ ] **Step 4: 更新复用和模块说明**

在 `docs/reuse/pavement_quantity_table.md`、`docs/reuse/capability_catalog.md`、`docs/modules/drawing_quantity.md` 中说明：

```markdown
体积算法可选择平均断面法或依照路面面积方法；后者保持面积列为平面投影面积，体积按平面面积乘平均厚度计算。
```

- [ ] **Step 5: 更新 README、测试说明和版本记录**

在 `README.md` 的路面工程量统计表段落中加入断面计算方法说明。  
在 `tests/README.md` 的出图出表测试范围中加入 `PavementQuantityCalculationMethod` 和“依照路面面积方法”。  
在 `docs/dev/version_log.md` 的“未发布”段落新增：

```markdown
  - 路面工程量统计表导出保存界面新增“断面计算方法”，支持 `平均断面法` 和 `依照路面面积方法`；后者面积列仍为平面投影面积，体积按平面面积乘平均厚度计算。
```

- [ ] **Step 6: 运行核心测试确认通过**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: `All RoadProto core tests passed.`

- [ ] **Step 7: 提交文档同步**

Run:

```powershell
git add tests/core_tests.cpp docs/business/drawing_quantity/路面工程量统计表.md docs/reuse/pavement_quantity_table.md docs/reuse/capability_catalog.md docs/modules/drawing_quantity.md README.md tests/README.md docs/dev/version_log.md
git commit -m "docs: update pavement quantity method docs"
```

---

### Task 5: 最终验证

**Files:**
- Verify: `RoadProto.sln`
- Verify: `src/ui/wpf/RoadProto.Terrain.UI/RoadProto.Terrain.UI.csproj`

- [ ] **Step 1: 运行 Debug 核心测试**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: `All RoadProto core tests passed.`

- [ ] **Step 2: 运行 Release 核心测试**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Release /p:Platform=x64
artifacts\x64\Release\RoadProtoCoreTests.exe
```

Expected: `All RoadProto core tests passed.`

- [ ] **Step 3: 构建托管 WPF 插件**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

Expected: 两次输出包含 `Build succeeded.` 或中文等价成功信息。

- [ ] **Step 4: 构建解决方案**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' RoadProto.sln /p:Configuration=Debug /p:Platform=x64
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

Expected: Debug 和 Release 均构建成功。

- [ ] **Step 5: 检查空白和行尾问题**

Run:

```powershell
git diff --check
git status --short
```

Expected: `git diff --check` 无输出；`git status --short` 只显示本次计划内文件或为空。

- [ ] **Step 6: 提交验证记录**

如果验证过程修改了 `docs/dev/version_log.md` 的验证状态，执行：

```powershell
git add docs/dev/version_log.md
git commit -m "docs: record pavement quantity method verification"
```
