# Pavement Structure Legend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND` command that draws a pavement structure legend from the current road model or its drawn cross sections using ordinary CAD entities.

**Architecture:** Put data formatting and layout planning in `domain/quantity` so it is testable without AutoCAD. Keep ObjectARX work in `cad_adapter/objectarx/drawing_quantity`: selection, source entity reading, model-space scanning, insertion point prompt, and ordinary entity drawing. Register command and Ribbon metadata in the existing drawing quantity module and managed Ribbon extension.

**Tech Stack:** C++17, ObjectARX 2021, RoadProto core tests, WPF AutoCAD Ribbon C# source contract checks.

---

### Task 1: Domain Legend Plan

**Files:**
- Create: `src/domain/quantity/PavementStructureLegend.h`
- Create: `src/domain/quantity/PavementStructureLegend.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write the failing test**

Add a test named `pavementStructureLegendPlannerFormatsTemplateColumnsAndUnmergedLegendItems` to `tests/core_tests.cpp`:

```cpp
void pavementStructureLegendPlannerFormatsTemplateColumnsAndUnmergedLegendItems()
{
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::quantity;

    PavementLayerTemplateData first;
    first.properties.structureCode = L"I-1";
    first.properties.subgradeSoilGroups = {PavementSubgradeSoilGroup::Bedrock};
    first.properties.subgradeMoistureTypes = {PavementSubgradeMoistureType::Dry};
    first.properties.designDeflection = L"E0>60MPa";
    first.properties.cumulativeAxleLoads = L"1200万次";
    first.layers = {
        PavementLayerTemplateLayer{
            PavementLayerType::UpperSurface,
            L"SMA-13S",
            true,
            0.045,
            0.045,
            0.045,
            0.0,
            0.0,
            0.0,
            0.0,
            PavementLayerTemplateDisplayColor{255, 0, 0},
            L"ANSI31",
            0.0,
            1.0},
        PavementLayerTemplateLayer{
            PavementLayerType::Base,
            L"基层",
            false,
            0.30,
            0.28,
            0.32,
            0.0,
            0.0,
            0.0,
            0.0,
            PavementLayerTemplateDisplayColor{0, 255, 0},
            L"BRICK",
            45.0,
            0.5}};

    PavementLayerTemplateData second = first;
    second.properties.structureCode = L"I-2";
    second.layers[0].name = L"AC-20S";
    second.layers[0].hatchPattern = L"ANSI31";

    const auto plan = PavementStructureLegendPlanner::build({
        PavementStructureLegendTemplateSource{L"PV-1", first},
        PavementStructureLegendTemplateSource{L"PV-1", first},
        PavementStructureLegendTemplateSource{L"PV-2", second}});

    CHECK(plan.columns.size() == 2);
    CHECK(plan.columns[0].structureCode == L"I-1");
    CHECK(plan.columns[0].subgradeSoilGroupText == L"基岩");
    CHECK(plan.columns[0].subgradeMoistureText == L"干燥");
    CHECK(plan.columns[0].designDeflection == L"E0>60MPa");
    CHECK(plan.columns[0].cumulativeAxleLoads == L"1200万次");
    CHECK(plan.columns[0].layers.size() == 2);
    CHECK(plan.columns[0].layers[0].thicknessText == L"4.5");
    CHECK(plan.columns[0].layers[1].thicknessText == L"28/32");
    CHECK(std::fabs(plan.columns[0].totalThicknessCm - 34.5) < 1.0e-9);
    CHECK(plan.legendItems.size() == 4);
    CHECK(plan.legendItems[0].layerName == L"SMA-13S");
    CHECK(plan.legendItems[1].layerName == L"基层");
    CHECK(plan.legendItems[2].layerName == L"AC-20S");
    CHECK(plan.layout.structureGraphicWidthCm == 20.0);
}
```

Call it from `main()` near the existing quantity tests.

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `domain/quantity/PavementStructureLegend.h` does not exist.

- [ ] **Step 3: Implement the minimal domain model**

Create `PavementStructureLegend.h/.cpp` with:

```cpp
struct PavementStructureLegendTemplateSource {
    std::wstring handle;
    roadproto::domain::cross_section::PavementLayerTemplateData data;
};

struct PavementStructureLegendLayerPlan {
    std::wstring layerName;
    std::wstring thicknessText;
    double displayThicknessCm = 0.0;
    int colorR = 0;
    int colorG = 0;
    int colorB = 0;
    std::wstring hatchPattern = L"SOLID";
    double hatchAngle = 0.0;
    double hatchScale = 1.0;
};

struct PavementStructureLegendColumnPlan {
    std::wstring templateHandle;
    std::wstring structureCode;
    std::wstring subgradeSoilGroupText;
    std::wstring subgradeMoistureText;
    std::wstring designDeflection;
    std::wstring cumulativeAxleLoads;
    double totalThicknessCm = 0.0;
    std::vector<PavementStructureLegendLayerPlan> layers;
};

struct PavementStructureLegendItemPlan {
    std::wstring layerName;
    int colorR = 0;
    int colorG = 0;
    int colorB = 0;
    std::wstring hatchPattern = L"SOLID";
    double hatchAngle = 0.0;
    double hatchScale = 1.0;
};

struct PavementStructureLegendLayoutPlan {
    double structureGraphicWidthCm = 20.0;
    double columnWidth = 36.0;
    double headerColumnWidth = 16.0;
};

struct PavementStructureLegendPlan {
    PavementStructureLegendLayoutPlan layout;
    std::vector<PavementStructureLegendColumnPlan> columns;
    std::vector<PavementStructureLegendItemPlan> legendItems;
};

class PavementStructureLegendPlanner {
public:
    static PavementStructureLegendPlan build(
        const std::vector<PavementStructureLegendTemplateSource>& sources);
};
```

Implementation rules:
- Deduplicate template columns by handle while preserving first occurrence.
- Do not deduplicate `legendItems`.
- Convert meters to centimeters.
- Format integer centimeters without decimals and fractional centimeters with one decimal.
- Use average thickness for non-uniform display height and total thickness; show text as `inner/outer`.
- Join soil groups and moisture types with `、`.

- [ ] **Step 4: Run test to verify it passes**

Run the same MSBuild command, then:

```powershell
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: tests pass.

### Task 2: Command Metadata and Source Contracts

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `src/modules/drawing_quantity/DrawingQuantityModule.cpp`
- Modify: `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.h`
- Modify: `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `tests/RoadProtoCoreTests.vcxproj`

- [ ] **Step 1: Write failing source-contract tests**

Add checks that expect:

```cpp
CHECK(moduleSource.find("RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND") != std::string::npos);
CHECK(moduleSource.find("路面结构图例") != std::string::npos);
CHECK(commandSource.find("DnRoadModelEntity") != std::string::npos);
CHECK(commandSource.find("DnRoadModelSectionDrawingEntity") != std::string::npos);
CHECK(commandSource.find("DnPavementLayerTemplateEntity") != std::string::npos);
CHECK(commandSource.find("collectSectionDrawingsForRoadModel") != std::string::npos);
CHECK(commandSource.find("appendOrdinaryLegendEntities") != std::string::npos);
CHECK(ribbonSource.find("RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND ") != std::string::npos);
```

- [ ] **Step 2: Run test to verify it fails**

Run Debug core test build and executable. Expected: source-contract test fails because the command does not exist.

- [ ] **Step 3: Add registration skeleton**

Create command header exposing:

```cpp
core::CommandProcedure pavementStructureLegendCommandProcedure();
```

Create command source with a test-build stub and ObjectARX function names required by the contract. Register in `DrawingQuantityModule.cpp` with business doc `docs/business/drawing_quantity/路面结构图例.md`. Add managed Ribbon constants and button sending `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND `.

- [ ] **Step 4: Run test to verify it passes**

Run Debug core test build and executable.

### Task 3: ObjectARX Ordinary Drawing Implementation

**Files:**
- Modify: `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: Extend failing source-contract test**

Require ordinary CAD entity APIs:

```cpp
CHECK(commandSource.find("new AcDbLine") != std::string::npos);
CHECK(commandSource.find("new AcDbPolyline") != std::string::npos);
CHECK(commandSource.find("new AcDbText") != std::string::npos);
CHECK(commandSource.find("new AcDbHatch") != std::string::npos);
CHECK(commandSource.find("acedGetPoint") != std::string::npos);
CHECK(commandSource.find("appendAcDbEntity") != std::string::npos);
CHECK(commandSource.find("PavementStructureLegendPlanner::build") != std::string::npos);
```

- [ ] **Step 2: Run test to verify it fails**

Run Debug core test build and executable. Expected: source-contract test fails on missing drawing APIs.

- [ ] **Step 3: Implement ObjectARX command**

Implement:
- Select `DnRoadModelEntity` or `DnRoadModelSectionDrawingEntity`.
- If section drawing selected, scan model space for all drawings with the same `roadModelHandle`.
- Collect template handles from section drawing faces first, then road model `pavementLayerLines`.
- Read `DnPavementLayerTemplateEntity::templateData()`.
- Prompt insertion point with `acedGetPoint`.
- Create ordinary line/text/polyline/hatch entities.
- Draw field rows, structure columns, per-layer rectangles, per-layer thickness text, total thickness row, and unmerged bottom legend items.

- [ ] **Step 4: Run test to verify it passes**

Run Debug core test build and executable.

### Task 4: Project Files and Documentation

**Files:**
- Create: `docs/business/drawing_quantity/路面结构图例.md`
- Modify: `docs/modules/drawing_quantity.md`
- Modify: `docs/modules/module_index.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `README.md`
- Modify: `tests/README.md`
- Modify: `docs/dev/version_log.md`

- [ ] **Step 1: Write failing doc/source tests**

Add source-contract checks for `路面结构图例`, `PavementStructureLegendPlanner`, and `RD_DRAWING_PAVEMENT_STRUCTURE_LEGEND` in README, module docs, reuse catalog, tests README, and version log.

- [ ] **Step 2: Run test to verify it fails**

Run Debug core test build and executable. Expected: doc checks fail.

- [ ] **Step 3: Update docs**

Document command behavior, inputs, ordinary CAD outputs, field rows, bottom legend no-merge rule, tests, and AutoCAD manual verification.

- [ ] **Step 4: Run test to verify it passes**

Run Debug core test build and executable.

### Task 5: Full Verification

**Files:**
- No new files unless verification reveals a build issue.

- [ ] **Step 1: Run Debug core build and tests**

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

- [ ] **Step 2: Run Release core build and tests**

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Release /p:Platform=x64
artifacts\x64\Release\RoadProtoCoreTests.exe
```

- [ ] **Step 3: Build managed Ribbon plugin**

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

- [ ] **Step 4: Build full solution Release**

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

- [ ] **Step 5: Run diff whitespace check**

```powershell
git diff --check
```
