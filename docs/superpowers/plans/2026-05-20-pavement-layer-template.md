# Pavement Layer Template Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add pavement layer template authoring, bind templates to subgrade components, and generate pavement layer wireframes in cross-section road models.

**Architecture:** Add a reusable C++ domain model for pavement layer templates and section geometry, then follow the existing cross-section pattern: application defaults, ObjectARX custom entity plus key-value WPF bridge, WPF window and XML flow, subgrade component binding, and road-model expansion. CAD access stays in `cad_adapter`; WPF receives DTOs and never touches ObjectARX objects.

**Tech Stack:** C++17, ObjectARX 2021, WPF .NET Framework 4.8, existing C++ core tests, managed bridge tests, RoadProto command/Ribbon infrastructure.

---

## File Structure

- `src/domain/cross_section/PavementLayerTemplateModel.h/.cpp`: pavement layer enums, data, defaults, normalize rules, inner/outer geometry builder, XML-independent domain behavior.
- `src/application/cross_section/PavementLayerTemplateCreateService.h/.cpp`: default template data for create command.
- `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h/.cpp`: DWG persistence, template preview drawing, extents, transform, insertion-point grip.
- `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.h/.cpp`: native request/response key-value files for WPF.
- `src/cad_adapter/objectarx/cross_section/ObjectArxPavementLayerTemplateCommand.h/.cpp`: create/edit/apply commands and CAD insertion point prompt.
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogDtos.cs`: WPF DTOs and labels.
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogFile.cs`: WPF key-value bridge file read/write.
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateXmlFile.cs`: XML import/export with `.rpavement.xml` content.
- `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/PavementLayerTemplateDialogCommands.cs`: managed command that opens the WPF window and sends native apply command.
- `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml/.cs`: reference-image-style editor, preview zoom/pan, dynamic layer parameter groups, XML buttons.
- Existing subgrade files: add pavement template handle/name binding and CAD pick action.
- Existing road model files: add pavement template sources, pavement section nodes, pavement wire lines, persistence, drawing, and viewer preview.
- Type labels are fixed everywhere: `UpperSurface=上面层`, `MiddleSurface=中面层`, `LowerSurface=下面层`, `Base=基层`, `Subbase=底基层`, `Cushion=垫层`. WPF must expose these through a non-editable dropdown, not a free-text type field.

---

### Task 1: Domain Pavement Layer Template Model

**Files:**
- Create: `src/domain/cross_section/PavementLayerTemplateModel.h`
- Create: `src/domain/cross_section/PavementLayerTemplateModel.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write failing core tests**

Add these test functions to `tests/core_tests.cpp` near the other cross-section tests:

```cpp
void pavementLayerTemplateRulesNormalizeThicknessAndCodes()
{
    PavementLayerTemplateData data;
    data.properties.name = L"主线行车道路面结构层";
    data.properties.displayScale = 100.0;
    data.properties.previewWidth = 3.75;
    data.layers = {
        PavementLayerTemplateLayer{
            PavementLayerType::UpperSurface,
            L"4cm 改性沥青混凝土",
            true,
            0.04,
            0.0,
            0.0,
            0.0,
            0.0,
            1.0,
            1.0},
        PavementLayerTemplateLayer{
            PavementLayerType::Base,
            L"水泥稳定碎石",
            false,
            0.0,
            0.18,
            0.20,
            0.15,
            0.25,
            0.0,
            0.0},
    };

    std::wstring errorMessage;
    CHECK(PavementLayerTemplateRules::normalize(data, errorMessage));
    CHECK(std::wstring(pavementLayerTypeCode(PavementLayerType::UpperSurface)) == L"UpperSurface");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::UpperSurface)) == L"上面层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::MiddleSurface)) == L"中面层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::LowerSurface)) == L"下面层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::Base)) == L"基层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::Subbase)) == L"底基层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::Cushion)) == L"垫层");
    CHECK(pavementLayerTypeFromCode(L"Base") == PavementLayerType::Base);
    CHECK(std::fabs(data.layers[0].innerThickness - 0.04) < 1.0e-9);
    CHECK(std::fabs(data.layers[0].outerThickness - 0.04) < 1.0e-9);
    CHECK(!data.layers[1].uniformThickness);
    CHECK(std::fabs(data.layers[1].innerWidening - 0.15) < 1.0e-9);
}

void pavementLayerTemplateGeometryUsesInnerOuterWithoutSlopeWidening()
{
    PavementLayerTemplateData data;
    data.properties.previewWidth = 3.75;
    PavementLayerTemplateLayer layer;
    layer.type = PavementLayerType::Base;
    layer.name = L"基层";
    layer.uniformThickness = false;
    layer.innerThickness = 0.18;
    layer.outerThickness = 0.20;
    layer.innerWidening = 0.10;
    layer.outerWidening = 0.30;
    layer.innerSlope = 1.0;
    layer.outerSlope = 1.0;
    data.layers.push_back(layer);

    const auto section = PavementLayerTemplateRules::buildSection(data, 3.75, SubgradeSide::Right, 100.0, 99.925);
    CHECK(section.succeeded);
    CHECK(section.layers.size() == 1);
    CHECK(std::fabs(section.layers[0].topInner.offset - 0.0) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].topOuter.offset - 3.75) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].bottomInner.offset - 0.10) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].bottomOuter.offset - 4.05) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].bottomInner.elevation - 99.82) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].bottomOuter.elevation - 99.725) < 1.0e-9);
}
```

Add both function calls in `main()` after the current subgrade template tests.

- [ ] **Step 2: Run core tests and confirm RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `PavementLayerTemplateModel.h`, `PavementLayerTemplateData`, `PavementLayerTemplateRules`, and layer type conversion functions do not exist.

- [ ] **Step 3: Implement domain header**

Create `src/domain/cross_section/PavementLayerTemplateModel.h` with:

```cpp
#pragma once

#include "domain/cross_section/SubgradeTemplateModel.h"

#include <string>
#include <vector>

namespace roadproto::domain::cross_section {

enum class PavementLayerType {
    UpperSurface,
    MiddleSurface,
    LowerSurface,
    Base,
    Subbase,
    Cushion
};

struct PavementLayerTemplateLayer {
    PavementLayerType type = PavementLayerType::UpperSurface;
    std::wstring name = L"上面层";
    bool uniformThickness = true;
    double thickness = 0.04;
    double innerThickness = 0.04;
    double outerThickness = 0.04;
    double innerWidening = 0.0;
    double outerWidening = 0.0;
    double innerSlope = 0.0;
    double outerSlope = 0.0;
};

struct PavementLayerTemplateProperties {
    std::wstring name = L"默认路面结构层模板";
    double displayScale = 100.0;
    double previewWidth = 3.75;
};

struct PavementLayerTemplateData {
    PavementLayerTemplateProperties properties;
    std::vector<PavementLayerTemplateLayer> layers;
};

struct PavementLayerSectionPoint {
    double offset = 0.0;
    double elevation = 0.0;
};

struct PavementLayerSectionLayer {
    PavementLayerType type = PavementLayerType::UpperSurface;
    std::wstring name;
    std::size_t layerIndex = 0;
    PavementLayerSectionPoint topInner;
    PavementLayerSectionPoint topOuter;
    PavementLayerSectionPoint bottomInner;
    PavementLayerSectionPoint bottomOuter;
};

struct PavementLayerTemplateSection {
    bool succeeded = false;
    std::wstring errorMessage;
    SubgradeSide side = SubgradeSide::Right;
    std::vector<PavementLayerSectionLayer> layers;
};

class PavementLayerTemplateDefaults {
public:
    static PavementLayerTemplateData create();
};

class PavementLayerTemplateRules {
public:
    static bool isSupportedDisplayScale(double displayScale);
    static bool isSupportedPreviewWidth(double previewWidth);
    static bool normalize(PavementLayerTemplateData& data, std::wstring& errorMessage);
    static PavementLayerTemplateSection buildSection(
        const PavementLayerTemplateData& data,
        double baseWidth,
        SubgradeSide side,
        double innerTopElevation,
        double outerTopElevation);
};

const wchar_t* pavementLayerTypeCode(PavementLayerType type);
const wchar_t* pavementLayerTypeDisplayName(PavementLayerType type);
PavementLayerType pavementLayerTypeFromCode(
    const std::wstring& code,
    PavementLayerType fallback = PavementLayerType::UpperSurface);

} // namespace roadproto::domain::cross_section
```

- [ ] **Step 4: Implement domain rules**

Create `src/domain/cross_section/PavementLayerTemplateModel.cpp` with normalization that rejects non-finite values, positive thickness requirements, positive display scale, positive preview width, and empty layer names replaced by display names. `buildSection` must use inner/outer widening only, without adding `thickness * slope` to offsets.

The key geometry body should be:

```cpp
PavementLayerTemplateSection PavementLayerTemplateRules::buildSection(
    const PavementLayerTemplateData& input,
    double baseWidth,
    SubgradeSide side,
    double innerTopElevation,
    double outerTopElevation)
{
    PavementLayerTemplateSection section;
    section.side = side;
    if (!std::isfinite(baseWidth) || baseWidth <= 0.0) {
        section.errorMessage = L"Pavement layer base width must be positive.";
        return section;
    }

    PavementLayerTemplateData data = input;
    std::wstring error;
    if (!normalize(data, error)) {
        section.errorMessage = error;
        return section;
    }

    double innerOffset = 0.0;
    double outerOffset = baseWidth;
    double currentInnerElevation = innerTopElevation;
    double currentOuterElevation = outerTopElevation;
    for (std::size_t i = 0; i < data.layers.size(); ++i) {
        const auto& layer = data.layers[i];
        PavementLayerSectionLayer sectionLayer;
        sectionLayer.type = layer.type;
        sectionLayer.name = layer.name;
        sectionLayer.layerIndex = i;
        sectionLayer.topInner = {innerOffset, currentInnerElevation};
        sectionLayer.topOuter = {outerOffset, currentOuterElevation};

        const double innerBottomOffset = innerOffset + layer.innerWidening;
        const double outerBottomOffset = outerOffset + layer.outerWidening;
        sectionLayer.bottomInner = {innerBottomOffset, currentInnerElevation - layer.innerThickness};
        sectionLayer.bottomOuter = {outerBottomOffset, currentOuterElevation - layer.outerThickness};
        section.layers.push_back(sectionLayer);

        innerOffset = innerBottomOffset;
        outerOffset = outerBottomOffset;
        currentInnerElevation = sectionLayer.bottomInner.elevation;
        currentOuterElevation = sectionLayer.bottomOuter.elevation;
    }

    section.succeeded = !section.layers.empty();
    if (!section.succeeded) {
        section.errorMessage = L"Pavement layer template has no layers.";
    }
    return section;
}
```

- [ ] **Step 5: Add project entries**

Add `..\src\domain\cross_section\PavementLayerTemplateModel.cpp` to `tests/RoadProtoCoreTests.vcxproj`.

Add `..\domain\cross_section\PavementLayerTemplateModel.cpp` to `src/app/RoadProtoArx.vcxproj`.

- [ ] **Step 6: Run tests and commit**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
git add src/domain/cross_section/PavementLayerTemplateModel.* tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj
git commit -m "feat: add pavement layer template domain model"
```

Expected: core build succeeds and the new pavement layer domain tests pass.

---

### Task 2: Road Model Pavement Layer Data And Builder

**Files:**
- Modify: `src/domain/cross_section/RoadModel.h`
- Modify: `src/domain/cross_section/RoadModel.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing road model tests**

Add tests that build a road model with one right-side travel lane bound to a pavement template source:

```cpp
void roadModelBuilderCreatesPavementLayerWireLinesForBoundSubgradeComponent()
{
    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.75;
    lane.fixedSlope = -0.02;
    lane.pavementLayerLinked = true;
    lane.pavementLayerHandle = L"PV-1";
    lane.pavementLayerName = L"行车道路面结构层";

    SubgradeTemplateData subgrade;
    subgrade.components.push_back(lane);

    PavementLayerTemplateData pavement;
    pavement.properties.name = L"行车道路面结构层";
    PavementLayerTemplateLayer upper;
    upper.type = PavementLayerType::UpperSurface;
    upper.name = L"上面层";
    upper.uniformThickness = true;
    upper.thickness = 0.04;
    pavement.layers.push_back(upper);

    RoadModelBuildInput input = makeMinimalRoadModelBuildInput();
    input.config.assignments = {RoadModelTemplateAssignment{0.0, 20.0, L"SG-1", L"路基模板"}};
    input.templates = {RoadModelTemplateSource{L"SG-1", subgrade}};
    input.pavementLayerTemplates = {RoadModelPavementLayerTemplateSource{L"PV-1", pavement}};

    const auto result = RoadModelBuilder::build(input);
    CHECK(result.succeeded);
    CHECK(!result.data.pavementLayerLines.empty());
    CHECK(std::any_of(result.data.wireLines.begin(), result.data.wireLines.end(), [](const auto& line) {
        return line.kind == RoadModelWireLineKind::PavementLayer;
    }));
    CHECK(std::any_of(result.data.sections.begin(), result.data.sections.end(), [](const auto& section) {
        return !section.rightPavementLayerNodes.empty();
    }));
}
```

Add a second test that uses one left-side component with `innerWidening = 0.10` and `outerWidening = 0.30`. Assert that the generated left-side pavement nodes put the inner boundary closer to centerline than the outer boundary after conversion, proving the domain-side inner/outer definition is converted by `SubgradeSide` instead of treated as literal left/right.

Use the existing helper style around `roadModelBuilderCreatesThreeDimensionalComponentLines`; if no helper exists, make a local minimal input with two alignment samples at stations `0` and `20`, a vertical curve from `0` to `20`, sample interval `10`, and no terrain triangles.

- [ ] **Step 2: Run tests and confirm RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `pavementLayerName`, `RoadModelPavementLayerTemplateSource`, `pavementLayerTemplates`, `pavementLayerLines`, `rightPavementLayerNodes`, and `RoadModelWireLineKind::PavementLayer` are missing.

- [ ] **Step 3: Extend subgrade binding data**

In `src/domain/cross_section/SubgradeTemplateModel.h`, add:

```cpp
std::wstring pavementLayerName;
```

Keep `pavementLayerThickness` temporarily for backward compatibility with old DWG and bridge fields, but stop using it for new road model generation.

- [ ] **Step 4: Extend road model types**

In `RoadModel.h`, include `PavementLayerTemplateModel.h` and add:

```cpp
struct RoadModelPavementLayerTemplateSource {
    std::wstring templateHandle;
    PavementLayerTemplateData data;
};

struct RoadModelPavementLayerLineKey {
    std::wstring subgradeTemplateHandle;
    std::wstring pavementLayerTemplateHandle;
    SubgradeSide side = SubgradeSide::Right;
    std::size_t componentIndex = 0;
    std::size_t layerIndex = 0;
    std::size_t boundaryIndex = 0;
};

struct RoadModelPavementLayerLine {
    RoadModelPavementLayerLineKey key;
    RoadModelWireColor color;
    std::vector<RoadModelPoint3d> points;
};
```

Add `PavementLayer` to `RoadModelSectionNodeKind`, `RoadModelWireLineKind`, and `RoadModelSectionPreviewSegmentKind`.

Add to `RoadModelSection`:

```cpp
std::vector<RoadModelSectionNode> leftPavementLayerNodes;
std::vector<RoadModelSectionNode> rightPavementLayerNodes;
```

Add to `RoadModelData`:

```cpp
std::vector<RoadModelPavementLayerLine> pavementLayerLines;
```

Add to `RoadModelBuildInput`:

```cpp
std::vector<RoadModelPavementLayerTemplateSource> pavementLayerTemplates;
```

- [ ] **Step 5: Implement builder expansion**

In `RoadModel.cpp`, add helpers beside the existing active boundary helpers:

- `findPavementLayerTemplate`
- `findOrNormalizePavementLayerTemplate`
- `appendPavementLayerBoundaryPointsForComponent`
- `appendPavementLayerBoundaryPoint`

The road model builder should:

1. While iterating subgrade components in `appendTemplateBoundaryPoints`, expose each component top inner/outer point and component index.
2. For every component with `pavementLayerLinked == true` and non-empty `pavementLayerHandle`, find the matching pavement template source.
3. Call `PavementLayerTemplateRules::buildSection` with the component width, side, top inner elevation, and top outer elevation.
4. Convert inner/outer offsets to world points with existing `pointAtOffset`.
5. Store pavement section nodes separately from subgrade/slope nodes.
6. Accumulate pavement longitudinal boundary lines in `result.data.pavementLayerLines`.
7. Include pavement nodes when creating `RoadModelWireLineKind::PavementLayer` ribs and longitudinal lines.

Keep the existing subgrade and slope behavior unchanged when no pavement templates are bound.

- [ ] **Step 6: Run tests and commit**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
git add src/domain/cross_section/SubgradeTemplateModel.* src/domain/cross_section/RoadModel.* tests/core_tests.cpp
git commit -m "feat: generate pavement layer road model lines"
```

Expected: existing road model tests still pass, and the new pavement layer road model test passes.

---

### Task 3: Native Pavement Layer Entity, Bridge, And Commands

**Files:**
- Create: `src/application/cross_section/PavementLayerTemplateCreateService.h`
- Create: `src/application/cross_section/PavementLayerTemplateCreateService.cpp`
- Create: `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h`
- Create: `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.cpp`
- Create: `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.h`
- Create: `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.cpp`
- Create: `src/cad_adapter/objectarx/cross_section/ObjectArxPavementLayerTemplateCommand.h`
- Create: `src/cad_adapter/objectarx/cross_section/ObjectArxPavementLayerTemplateCommand.cpp`
- Modify: `src/app/arx_entry/RoadProtoArxEntry.cpp`
- Modify: `src/modules/cross_section/CrossSectionModule.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Test: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing source-contract tests**

Add tests in `tests/core_tests.cpp` that read source text and check:

- `DNPAVEMENTLAYERTEMPLATEENTITY` appears in entity source.
- `dwgOutFields` and `dwgInFields` write/read template name, display scale, preview width, and layer fields.
- `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE`, `EDIT_HANDLE`, and `APPLY_DIALOG_FILE` are registered in `CrossSectionModule.cpp`.
- `initializePavementLayerTemplateEntityClass()` and `uninitializePavementLayerTemplateEntityClass()` are called in `RoadProtoArxEntry.cpp`.
- `ObjectArxPavementLayerTemplateCommand.cpp` prompts for insertion point text `请选择路面结构层模板插入位置`.

- [ ] **Step 2: Run tests and confirm RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: source-contract tests fail because the native files and registrations do not exist.

- [ ] **Step 3: Implement create service**

Create service files mirroring `SubgradeTemplateCreateService`:

```cpp
struct PavementLayerTemplateCreateInput {
    std::wstring name;
    double displayScale = 100.0;
};

struct PavementLayerTemplateCreateResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::cross_section::PavementLayerTemplateData templateData;
};

class PavementLayerTemplateCreateService {
public:
    PavementLayerTemplateCreateResult create(const PavementLayerTemplateCreateInput& input) const;
};
```

Implementation should call `PavementLayerTemplateDefaults::create()`, apply input name/display scale, normalize, and return errors without CAD calls.

- [ ] **Step 4: Implement custom entity**

Create `DnPavementLayerTemplateEntity` using the `DnSlopeTemplateEntity`/`DnSubgradeTemplateEntity` pattern:

- Store `PavementLayerTemplateData templateData_`.
- Store `AcGePoint3d insertionPoint_`, `xAxis_`, and `yAxis_`.
- `dwgOutFields` writes version, insertion point, axes, template properties, layer count, and all layer values.
- `dwgInFields` reads old/current version and normalizes data.
- `subWorldDraw` calls `PavementLayerTemplateRules::buildSection` with `previewWidth`, then draws layer polygons/lines and Chinese labels.
- `subGetGripPoints` returns insertion point.
- `subMoveGripPointsAt` translates insertion point.
- Initialize/uninitialize functions call `rxInit`, `acrxBuildClassHierarchy`, and `deleteAcRxClass`.

- [ ] **Step 5: Implement native bridge**

Create `PavementLayerTemplateDialogBridge` with request/response structs:

```cpp
struct PavementLayerTemplateDialogRequest {
    std::wstring handle;
    std::wstring responsePath;
    AcGePoint3d insertionPoint;
    PavementLayerTemplateData data;
};

struct PavementLayerTemplateDialogResponse {
    bool accepted = false;
    std::wstring handle;
    AcGePoint3d insertionPoint;
    PavementLayerTemplateData data;
};
```

Use the same key-value escaping style as `SubgradeTemplateDialogBridge.cpp`. Field names:

- `templateName`
- `displayScale`
- `previewWidth`
- `layerCount`
- `layer.N.type`
- `layer.N.name`
- `layer.N.uniformThickness`
- `layer.N.thickness`
- `layer.N.innerThickness`
- `layer.N.outerThickness`
- `layer.N.innerWidening`
- `layer.N.outerWidening`
- `layer.N.innerSlope`
- `layer.N.outerSlope`

- [ ] **Step 6: Implement native commands and registration**

Implement command behavior:

- Create command prompts insertion point, builds default template, queues WPF dialog.
- Edit handle command opens existing `DnPavementLayerTemplateEntity` and queues WPF dialog.
- Apply response command creates a new entity when response handle is empty; otherwise updates existing entity.

Register in `CrossSectionModule.cpp` with business docs:

- `docs/business/cross_section/路面结构层模板_创建.md`
- `docs/business/cross_section/路面结构层模板_编辑.md`
- `docs/business/cross_section/路面结构层模板_WPF桥接回写.md`

- [ ] **Step 7: Add project entries, run tests, and commit**

Add new `.cpp` files to `src/app/RoadProtoArx.vcxproj`. Add command `.cpp` to `tests/RoadProtoCoreTests.vcxproj` under test build.

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
git add src/application/cross_section/PavementLayerTemplateCreateService.* src/cad_adapter/objectarx/cross_section/*PavementLayerTemplate* src/app/arx_entry/RoadProtoArxEntry.cpp src/modules/cross_section/CrossSectionModule.cpp src/app/RoadProtoArx.vcxproj tests/RoadProtoCoreTests.vcxproj tests/core_tests.cpp
git commit -m "feat: add pavement layer template native entity"
```

Expected: source-contract tests pass and the new command metadata is available.

---

### Task 4: WPF Pavement Layer Window And XML Flow

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogFile.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateXmlFile.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/PavementLayerTemplateDialogCommands.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Test: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] **Step 1: Write failing managed tests**

Add managed tests for:

- Key-value request reads all layer fields with invariant culture.
- Key-value response writes all layer fields and escapes Unicode/newlines.
- XML round-trips `.rpavement.xml` content including thickness consistency and inner/outer fields.
- XAML contains `PreviewCanvas`, `LayerCountBox`, `SaveXml`, `ImportXml`, `MouseWheel`, and middle-button pan handlers.
- Ribbon extension contains `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE`.

- [ ] **Step 2: Run managed tests and confirm RED**

Run:

```powershell
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
```

Expected: tests fail because WPF DTO/file/window/XML support does not exist.

- [ ] **Step 3: Implement DTOs and file bridge**

DTOs should mirror the native field names and use `PavementLayerType` enum:

```csharp
public enum PavementLayerType
{
    UpperSurface,
    MiddleSurface,
    LowerSurface,
    Base,
    Subbase,
    Cushion,
}

public sealed class PavementLayerTemplateLayerDto
{
    public PavementLayerType Type { get; set; } = PavementLayerType.UpperSurface;
    public string Name { get; set; } = "上面层";
    public bool UniformThickness { get; set; } = true;
    public double Thickness { get; set; } = 0.04;
    public double InnerThickness { get; set; } = 0.04;
    public double OuterThickness { get; set; } = 0.04;
    public double InnerWidening { get; set; }
    public double OuterWidening { get; set; }
    public double InnerSlope { get; set; }
    public double OuterSlope { get; set; }
}

public sealed class PavementLayerTypeOptionDto
{
    public PavementLayerType Value { get; }
    public string Label { get; }

    public PavementLayerTypeOptionDto(PavementLayerType value, string label)
    {
        Value = value;
        Label = label;
    }
}
```

Use `CultureInfo.InvariantCulture` for all numeric file values.

Expose the dropdown source as a fixed list:

```csharp
public static readonly IReadOnlyList<PavementLayerTypeOptionDto> LayerTypeOptions =
    new[]
    {
        new PavementLayerTypeOptionDto(PavementLayerType.UpperSurface, "上面层"),
        new PavementLayerTypeOptionDto(PavementLayerType.MiddleSurface, "中面层"),
        new PavementLayerTypeOptionDto(PavementLayerType.LowerSurface, "下面层"),
        new PavementLayerTypeOptionDto(PavementLayerType.Base, "基层"),
        new PavementLayerTypeOptionDto(PavementLayerType.Subbase, "底基层"),
        new PavementLayerTypeOptionDto(PavementLayerType.Cushion, "垫层"),
    };
```

- [ ] **Step 4: Implement XML read/write**

Use `System.Xml.Linq` in `PavementLayerTemplateXmlFile.cs`. Root element:

```xml
<RoadProtoPavementLayerTemplate version="1">
  <Properties name="主线行车道路面结构层" displayScale="100" previewWidth="3.75" />
  <Layers>
    <Layer type="UpperSurface" name="4cm 改性沥青混凝土" uniformThickness="true" thickness="0.04" innerThickness="0.04" outerThickness="0.04" innerWidening="0" outerWidening="0" innerSlope="1" outerSlope="1" />
  </Layers>
</RoadProtoPavementLayerTemplate>
```

Import updates the in-memory window model only. Export writes the current in-memory window model only.

- [ ] **Step 5: Implement WPF window**

Window requirements:

- Top row: template name, import button, save button, display scale.
- Left: dark preview canvas with layer geometry, thickness/widening/slope labels, mouse-wheel zoom, middle-button drag pan.
- Right: `ScrollViewer` containing common fields and dynamic per-layer parameter groups.
- Layer type uses a non-editable `ComboBox` bound to `LayerTypeOptions` with `DisplayMemberPath="Label"` and `SelectedValuePath="Value"`.
- Field labels use `内侧` and `外侧` only; do not introduce `左侧` or `右侧` wording in the pavement layer template UI.
- Layer count input adds/removes layers by cloning defaults.
- Thickness consistency checkbox toggles between one thickness field and inner/outer thickness fields.
- Bottom buttons: OK, Cancel, Apply.
- Apply writes response and sends native apply command while keeping the window open.

Follow existing `SubgradeTemplateWindow.xaml.cs` style for parsing, Canvas drawing, and response construction.

- [ ] **Step 6: Implement managed AutoCAD command and Ribbon**

Create `PavementLayerTemplateDialogCommands.cs` with command `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_SHOW_WPF_DIALOG`. It reads pending request path from `%TEMP%\RoadProtoPavementLayerTemplateDialog_<pid>.pending`, opens `PavementLayerTemplateWindow`, writes response, and sends:

```text
RD_SECTION_PAVEMENT_LAYER_TEMPLATE_APPLY_DIALOG_FILE "<responsePath>"
```

Add Ribbon button in `RoadProtoRibbonExtension.cs` under `横断面设计` panel:

- Button text: `创建路面结构层模板`
- Command: `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE`

Add double-click forwarding for DXF `DNPAVEMENTLAYERTEMPLATEENTITY` to `RD_SECTION_PAVEMENT_LAYER_TEMPLATE_EDIT_HANDLE <handle>`.

- [ ] **Step 7: Run tests, build WPF, and commit**

Run:

```powershell
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
git add src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplate* src/ui/wpf/RoadProto.Terrain.UI/AutoCad/PavementLayerTemplateDialogCommands.cs src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml* src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs tests/RoadProtoManagedBridgeTests/Program.cs
git commit -m "feat: add pavement layer template WPF editor"
```

Expected: managed bridge tests pass and WPF Debug build succeeds.

---

### Task 5: Subgrade Template Binding To Pavement Layer Templates

**Files:**
- Modify: `src/domain/cross_section/SubgradeTemplateModel.h/.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/SubgradeTemplateDialogBridge.h/.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxSubgradeTemplateCommand.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SubgradeTemplateDialogDtos.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SubgradeTemplateDialogFile.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/SubgradeTemplateWindow.xaml/.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SubgradeTemplateDialogCommands.cs`
- Test: `tests/core_tests.cpp`
- Test: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] **Step 1: Write failing tests**

Core tests:

- Subgrade normalize preserves `pavementLayerHandle` and `pavementLayerName` when linked.
- Native bridge source contains `pickPavementLayerTemplate` action and selected component index.
- `ObjectArxSubgradeTemplateCommand.cpp` can select `DnPavementLayerTemplateEntity`.

Managed tests:

- `SubgradeTemplateDialogFile` reads/writes `component.N.pavementLayerName`.
- `SubgradeTemplateWindow.xaml` contains `选择结构层模板`.
- `SubgradeTemplateDialogCommands.cs` preserves current components during a pick action.

- [ ] **Step 2: Run tests and confirm RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
```

Expected: failures on missing pavement layer name/action/index support.

- [ ] **Step 3: Extend DTO and native bridge fields**

Add `pavementLayerName` beside existing subgrade pavement fields in native and managed bridge files:

```text
component.N.pavementLayerLinked
component.N.pavementLayerHandle
component.N.pavementLayerName
component.N.pavementLayerThickness
```

Keep `pavementLayerThickness` for backward compatibility, but hide it from the WPF UI and do not use it in road model generation.

- [ ] **Step 4: Add pick action**

Extend native `SubgradeTemplateDialogResponse` with:

- action enum: `None`, `PickPavementLayerTemplate`
- `pickComponentIndex`

When WPF requests pick:

1. Native apply command reads current response.
2. Native command prompts user to select `DnPavementLayerTemplateEntity`.
3. Native reads entity handle and template name.
4. Native updates that component's handle/name/linked flag.
5. Native queues the subgrade WPF dialog again with the updated data.

- [ ] **Step 5: Update WPF subgrade UI**

In selected component parameter area:

- Add checkbox or status row `启用路面结构层模板`.
- Add read-only text boxes for template name and handle.
- Add button `选择结构层模板`.
- Add button `清除结构层模板`.

All component types are allowed to bind; do not filter by `SubgradeComponentType`.

- [ ] **Step 6: Run tests and commit**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
git add src/domain/cross_section/SubgradeTemplateModel.* src/cad_adapter/objectarx/cross_section/SubgradeTemplateDialogBridge.* src/cad_adapter/objectarx/cross_section/ObjectArxSubgradeTemplateCommand.cpp src/cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.cpp src/ui/wpf/RoadProto.Terrain.UI/Bridge/SubgradeTemplateDialog* src/ui/wpf/RoadProto.Terrain.UI/SubgradeTemplateWindow.xaml* src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SubgradeTemplateDialogCommands.cs tests/core_tests.cpp tests/RoadProtoManagedBridgeTests/Program.cs
git commit -m "feat: bind subgrade components to pavement templates"
```

Expected: subgrade templates round-trip pavement template handle/name, and WPF can request CAD picking.

---

### Task 6: Road Model Native Integration, Persistence, Drawing, And Viewer

**Files:**
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.h/.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewerDtos.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewerFile.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelSectionViewerWindow.xaml/.cs`
- Test: `tests/core_tests.cpp`
- Test: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] **Step 1: Write failing tests**

Core source-contract tests:

- `ObjectArxRoadModelCommand.cpp` opens `DnPavementLayerTemplateEntity` from subgrade component handles.
- `RoadModelBuildInput` receives `pavementLayerTemplates`.
- `DnRoadModelEntity.cpp` persists `pavementLayerLines`.
- `drawRoadModelWireLines` handles `RoadModelWireLineKind::PavementLayer`.
- `RoadModelSectionViewerBridge.cpp` writes preview segment kind `PavementLayer`.

Managed tests:

- `RoadModelSectionViewerFile.ReadRequest` parses `kind=PavementLayer`.
- viewer legend contains `结构层`.

- [ ] **Step 2: Run tests and confirm RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
```

Expected: tests fail because pavement template reading/persistence/viewer support is missing.

- [ ] **Step 3: Collect pavement template sources**

In `ObjectArxRoadModelCommand.cpp`, after reading subgrade template sources:

1. Walk all `SubgradeTemplateData.components`.
2. Collect unique non-empty `pavementLayerHandle` values where `pavementLayerLinked` is true.
3. Resolve each handle to `DnPavementLayerTemplateEntity`.
4. Push `{handle, entity->templateData()}` to `input.pavementLayerTemplates`.

If a bound handle cannot be found, write an editor error that names the subgrade component and handle, then stop generation.

- [ ] **Step 4: Persist pavement lines and nodes**

In `DnRoadModelEntity.cpp`:

- Bump road model data persistence version.
- Add `writeRoadModelPavementLayerLine` and `readRoadModelPavementLayerLine`.
- Write/read `RoadModelSection.leftPavementLayerNodes` and `rightPavementLayerNodes`.
- Draw pavement wire lines using a distinct color policy: use layer-type color when available, otherwise neutral gray/white.
- Keep old DWG read compatibility by treating missing pavement collections as empty.

- [ ] **Step 5: Update section viewer**

In native bridge, emit `PavementLayer` segments from `RoadModelSectionPreviewBuilder`.

In WPF:

- Add enum value `PavementLayer`.
- Draw pavement layer segments with a visible color.
- Add legend text `结构层`.

- [ ] **Step 6: Run tests and commit**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
git add src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.* src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.cpp src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewer* src/ui/wpf/RoadProto.Terrain.UI/RoadModelSectionViewerWindow.xaml* tests/core_tests.cpp tests/RoadProtoManagedBridgeTests/Program.cs
git commit -m "feat: persist and view pavement layer road model lines"
```

Expected: generated road model data persists pavement layer lines and section viewer bridge can display them.

---

### Task 7: Documentation, Module Index, Version, And README

**Files:**
- Create: `docs/business/cross_section/路面结构层模板_创建.md`
- Create: `docs/business/cross_section/路面结构层模板_编辑.md`
- Create: `docs/business/cross_section/路面结构层模板_WPF桥接回写.md`
- Create: `docs/reuse/pavement_layer_template.md`
- Modify: `docs/business/cross_section/路基模板_创建.md`
- Modify: `docs/business/cross_section/路基模板_编辑.md`
- Modify: `docs/business/cross_section/横断面戴帽_道路模型创建.md`
- Modify: `docs/business/cross_section/查看横断面.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/reuse/road_model.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`
- Modify: `README.md`
- Modify: `build/RoadProto.Build.props`

- [ ] **Step 1: Add business docs**

Create three pavement layer business docs using `docs/business/业务文档模板.md` structure:

- Create: command flow, data model, inner/outer definitions, XML behavior.
- Edit: double-click/handle flow, persistence behavior.
- Bridge: request/response fields and WPF apply behavior.

- [ ] **Step 2: Update existing cross-section docs**

Update:

- Subgrade create/edit docs: all components can bind pavement layer templates; no left/right wording for layer parameters.
- Road model create doc: road model generation expands bound pavement templates into three-dimensional wireframes.
- View section doc: preview includes pavement layer segments.
- Cross-section module doc: command table, Ribbon table, code landing table, update section.

- [ ] **Step 3: Add reuse docs and version**

Create `docs/reuse/pavement_layer_template.md` with reusable boundaries:

- domain model
- XML file
- ObjectARX entity
- WPF bridge
- road model expansion

Update `docs/reuse/capability_catalog.md` and `docs/reuse/road_model.md`.

Update `build/RoadProto.Build.props`, `README.md`, and `docs/dev/version_log.md` to the next `v0.1.x` version and a stage name such as `PavementLayerTemplate`.

- [ ] **Step 4: Run docs/source tests and commit**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
git add docs/business/cross_section docs/reuse docs/modules/cross_section.md docs/dev/version_log.md tests/README.md README.md build/RoadProto.Build.props
git commit -m "docs: document pavement layer templates"
```

Expected: command metadata and doc path source-contract tests pass.

---

### Task 8: Final Verification And Build Artifact Sync

**Files:**
- All modified source, tests, docs, and project files.

- [ ] **Step 1: Run full core tests**

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all core tests pass.

- [ ] **Step 2: Run managed bridge tests**

```powershell
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
```

Expected: all managed bridge tests pass.

- [ ] **Step 3: Build WPF plugin**

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

Expected: `artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll` exists.

- [ ] **Step 4: Build ARX**

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

Expected: versioned ARX exists under `artifacts\x64\Release\`.

- [ ] **Step 5: Manual AutoCAD verification**

In AutoCAD 2021:

```text
ARXLOAD artifacts\x64\Release\<versioned RoadProto arx>
NETLOAD artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll
RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE
RD_SECTION_SUBGRADE_TEMPLATE_CREATE
RD_SECTION_ROAD_MODEL_CREATE
RD_SECTION_ROAD_MODEL_VIEW_SECTION
```

Expected:

- Create command prompts for insertion point.
- Pavement layer window opens and previews layers.
- XML save/import works.
- Entity appears in DWG and double-click edit reopens with saved fields.
- Subgrade component can pick the pavement layer template.
- Road model generation draws pavement layer wireframes.
- View section shows pavement layer lines.

- [ ] **Step 6: Sync worktree outputs if applicable and final commit**

If the implementation ran in `.worktrees/<branch>`, sync source/docs back to `F:\0_GPT_道路设计原型功能项目` according to `AGENTS.md`. If Release artifacts were built in a worktree, copy:

- `.worktrees/<branch>/artifacts/x64/Release/*` to `F:\0_GPT_道路设计原型功能项目\artifacts\x64\Release\`
- `.worktrees/<branch>/artifacts/managed/Release/net48/*` to `F:\0_GPT_道路设计原型功能项目\artifacts\managed\Release\net48\`

Commit any final fixes:

```powershell
git status --short
git add <changed files>
git commit -m "feat: add pavement layer template workflow"
```

Expected: final repository state contains source, docs, tests, and version metadata for the complete feature.
