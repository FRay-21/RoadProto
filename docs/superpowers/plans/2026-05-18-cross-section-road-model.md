# Cross-Section Road Model Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the `RD_SECTION_ROAD_MODEL_CREATE` / `RD_SECTION_ROAD_MODEL_EDIT` cross-section road model workflow that assigns existing subgrade templates by priority ranges and creates an editable 3D road model entity.

**Architecture:** Add reusable road model rules in `domain/cross_section`, orchestration in `application/cross_section`, ObjectARX persistence and drawing in `cad_adapter/objectarx/cross_section`, and a WPF bridge/window in `RoadProto.Terrain.UI`. Commands stay in `CROSS_SECTION`; WPF only edits DTOs and never opens CAD entities directly.

**Tech Stack:** C++17, ObjectARX 2021, WPF .NET Framework 4.8, existing request/response file bridge, `RoadProtoCoreTests.exe` for non-AutoCAD tests.

---

## File Structure

- Create `src/domain/cross_section/RoadModel.h`: road model DTOs, template assignment rows, 2D/3D sample structures, generated component line structures.
- Create `src/domain/cross_section/RoadModel.cpp`: validation, template priority resolver, station sampler, 2D alignment interpolation, template expansion, 3D line generation.
- Create `src/application/cross_section/RoadModelBuildService.h`: application input/result types for creating or regenerating a road model.
- Create `src/application/cross_section/RoadModelBuildService.cpp`: normalizes domain inputs and calls `RoadModelBuilder`.
- Create `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.h`: ObjectARX custom entity class for persisted 3D model data.
- Create `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.cpp`: DWG read/write, draw generated 3D lines, extents, transforms.
- Create `src/cad_adapter/objectarx/cross_section/RoadModelDialogBridge.h`: C++ request/response structs and bridge functions.
- Create `src/cad_adapter/objectarx/cross_section/RoadModelDialogBridge.cpp`: key-value bridge file read/write and WPF command queueing.
- Create `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h`: command procedure declarations.
- Create `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`: create/edit/apply commands, entity selection, vertical-curve resolution, template entity reading.
- Create `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelDialogDtos.cs`: WPF DTOs for road model requests/responses.
- Create `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelDialogFile.cs`: WPF key-value request/response parsing.
- Create `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadModelDialogCommands.cs`: WPF command entry that opens the road model window and sends apply command.
- Create `src/ui/wpf/RoadProto.Terrain.UI/RoadModelWindow.xaml`: WPF road model editor.
- Create `src/ui/wpf/RoadProto.Terrain.UI/RoadModelWindow.xaml.cs`: table editing, move up/down, generate/cancel response.
- Modify `src/modules/cross_section/CrossSectionModule.cpp`: register road model commands and keep Ribbon panel.
- Modify `src/app/RoadProtoArx.vcxproj`: include new C++ source files.
- Modify `tests/RoadProtoCoreTests.vcxproj`: include new domain/application and command source files.
- Modify `tests/core_tests.cpp`: add domain, application, module, WPF/Ribbon/Bridge source tests.
- Modify `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`: add road model button, double-click DXF routing, command class assembly registration.
- Modify `src/app/arx_entry/RoadProtoArxEntry.cpp` or the current entity initialization file if needed: initialize/uninitialize `DnRoadModelEntity` next to other custom entities.
- Modify `build/RoadProto.Build.props`, `src/core/version/VersionInfo.cpp`: update to `v0.1.11_20260518_RoadModel`.
- Add docs:
  - `docs/business/cross_section/横断面戴帽_道路模型创建.md`
  - `docs/business/cross_section/道路模型_编辑.md`
  - `docs/business/cross_section/道路模型_WPF桥接回写.md`
  - `docs/reuse/road_model.md`
- Modify docs:
  - `README.md`
  - `docs/modules/cross_section.md`
  - `docs/modules/module_index.md`
  - `docs/reuse/capability_catalog.md`
  - `docs/dev/version_log.md`
  - `tests/README.md`

---

### Task 1: Domain Model And Template Priority Resolver

**Files:**
- Modify: `tests/core_tests.cpp`
- Create: `src/domain/cross_section/RoadModel.h`
- Create: `src/domain/cross_section/RoadModel.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write the failing tests**

Add include near existing cross-section include:

```cpp
#include "domain/cross_section/RoadModel.h"
```

Add tests after `subgradeTemplateCreateServiceBuildsDefaultTemplate()`:

```cpp
void roadModelTemplateResolverUsesHigherPriorityRows()
{
    using namespace roadproto::domain::cross_section;

    RoadModelTemplateAssignment low;
    low.startStation = 0.0;
    low.endStation = 100.0;
    low.templateHandle = L"LOW";
    low.templateName = L"低优先级";

    RoadModelTemplateAssignment high;
    high.startStation = 30.0;
    high.endStation = 60.0;
    high.templateHandle = L"HIGH";
    high.templateName = L"高优先级";

    RoadModelTemplateResolver resolver({high, low});

    const auto at20 = resolver.resolve(20.0);
    CHECK(at20 != nullptr);
    CHECK(at20->templateHandle == L"LOW");

    const auto at40 = resolver.resolve(40.0);
    CHECK(at40 != nullptr);
    CHECK(at40->templateHandle == L"HIGH");

    const auto at120 = resolver.resolve(120.0);
    CHECK(at120 == nullptr);
}

void roadModelTemplateResolverRejectsInvalidRows()
{
    using namespace roadproto::domain::cross_section;

    RoadModelTemplateAssignment invalidRange;
    invalidRange.startStation = 50.0;
    invalidRange.endStation = 10.0;
    invalidRange.templateHandle = L"T1";

    std::wstring errorMessage;
    CHECK(!RoadModelRules::validateAssignments({invalidRange}, errorMessage));
    CHECK(!errorMessage.empty());

    RoadModelTemplateAssignment missingTemplate;
    missingTemplate.startStation = 0.0;
    missingTemplate.endStation = 10.0;
    missingTemplate.templateHandle.clear();

    errorMessage.clear();
    CHECK(!RoadModelRules::validateAssignments({missingTemplate}, errorMessage));
    CHECK(!errorMessage.empty());
}
```

Call both tests in `main()` after `subgradeTemplateCreateServiceBuildsDefaultTemplate();`:

```cpp
    roadModelTemplateResolverUsesHigherPriorityRows();
    roadModelTemplateResolverRejectsInvalidRows();
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: compile fails because `domain/cross_section/RoadModel.h` does not exist.

- [ ] **Step 3: Add the minimal domain API**

Create `src/domain/cross_section/RoadModel.h`:

```cpp
#pragma once

#include "domain/alignment/AlignmentGeometry.h"
#include "domain/cross_section/SubgradeTemplateModel.h"
#include "domain/profile/ProfileVerticalCurveModel.h"

#include <optional>
#include <string>
#include <vector>

namespace roadproto::domain::cross_section {

struct RoadModelPoint3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct RoadModelTemplateAssignment {
    double startStation = 0.0;
    double endStation = 0.0;
    std::wstring templateHandle;
    std::wstring templateName;
};

struct RoadModelConfig {
    std::wstring roadCenterlineHandle;
    std::wstring profileVerticalCurveHandle;
    double sampleInterval = 10.0;
    std::vector<RoadModelTemplateAssignment> assignments;
};

struct RoadModelTemplateSource {
    std::wstring templateHandle;
    SubgradeTemplateData data;
};

struct RoadModelLineKey {
    std::wstring templateHandle;
    SubgradeSide side = SubgradeSide::Right;
    SubgradeComponentType componentType = SubgradeComponentType::TravelLane;
    std::size_t componentIndex = 0;
    std::size_t boundaryIndex = 0;
};

struct RoadModelComponentLine {
    RoadModelLineKey key;
    SubgradeTemplateRgbColor color;
    std::vector<RoadModelPoint3d> points;
};

struct RoadModelData {
    RoadModelConfig config;
    std::vector<RoadModelComponentLine> componentLines;
    int version = 1;
};

struct RoadModelBuildInput {
    RoadModelConfig config;
    std::vector<alignment::AlignmentSamplePoint> alignmentSamples;
    profile::ProfileVerticalCurveData verticalCurve;
    std::vector<RoadModelTemplateSource> templates;
};

struct RoadModelBuildResult {
    bool succeeded = false;
    std::wstring errorMessage;
    RoadModelData data;
    std::vector<double> sampledStations;
};

class RoadModelRules final {
public:
    static bool isSupportedSampleInterval(double sampleInterval);
    static bool validateAssignments(
        const std::vector<RoadModelTemplateAssignment>& assignments,
        std::wstring& errorMessage);
};

class RoadModelTemplateResolver final {
public:
    explicit RoadModelTemplateResolver(std::vector<RoadModelTemplateAssignment> assignments);
    const RoadModelTemplateAssignment* resolve(double station) const;

private:
    std::vector<RoadModelTemplateAssignment> assignments_;
};

class RoadModelStationSampler final {
public:
    static std::vector<double> collectStations(
        double alignmentStart,
        double alignmentEnd,
        const profile::ProfileVerticalCurveData& verticalCurve,
        const std::vector<RoadModelTemplateAssignment>& assignments,
        double sampleInterval);
};

class RoadModelBuilder final {
public:
    static RoadModelBuildResult build(const RoadModelBuildInput& input);
};

} // namespace roadproto::domain::cross_section
```

Create `src/domain/cross_section/RoadModel.cpp`:

```cpp
#include "domain/cross_section/RoadModel.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::cross_section {
namespace {

constexpr double kStationTolerance = 1.0e-7;

bool isFinite(double value)
{
    return std::isfinite(value);
}

bool stationInRange(double station, const RoadModelTemplateAssignment& row)
{
    return station + kStationTolerance >= row.startStation
        && station <= row.endStation + kStationTolerance;
}

} // namespace

bool RoadModelRules::isSupportedSampleInterval(double sampleInterval)
{
    return isFinite(sampleInterval) && sampleInterval > 0.0;
}

bool RoadModelRules::validateAssignments(
    const std::vector<RoadModelTemplateAssignment>& assignments,
    std::wstring& errorMessage)
{
    for (const auto& row : assignments) {
        if (!isFinite(row.startStation) || !isFinite(row.endStation)) {
            errorMessage = L"Road model template assignment stations must be finite.";
            return false;
        }
        if (row.endStation < row.startStation) {
            errorMessage = L"Road model template assignment end station must be greater than or equal to start station.";
            return false;
        }
        if (row.templateHandle.empty()) {
            errorMessage = L"Road model template assignment template handle is required.";
            return false;
        }
    }
    return true;
}

RoadModelTemplateResolver::RoadModelTemplateResolver(std::vector<RoadModelTemplateAssignment> assignments)
    : assignments_(std::move(assignments))
{
}

const RoadModelTemplateAssignment* RoadModelTemplateResolver::resolve(double station) const
{
    if (!isFinite(station)) {
        return nullptr;
    }
    for (const auto& row : assignments_) {
        if (stationInRange(station, row)) {
            return &row;
        }
    }
    return nullptr;
}

std::vector<double> RoadModelStationSampler::collectStations(
    double,
    double,
    const profile::ProfileVerticalCurveData&,
    const std::vector<RoadModelTemplateAssignment>&,
    double)
{
    return {};
}

RoadModelBuildResult RoadModelBuilder::build(const RoadModelBuildInput& input)
{
    RoadModelBuildResult result;
    result.data.config = input.config;
    return result;
}

} // namespace roadproto::domain::cross_section
```

Add source file to both `.vcxproj` files:

```xml
<ClCompile Include="..\src\domain\cross_section\RoadModel.cpp" />
```

For `src/app/RoadProtoArx.vcxproj`, use:

```xml
<ClCompile Include="..\domain\cross_section\RoadModel.cpp" />
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: build succeeds and executable prints `All RoadProto core tests passed.`

- [ ] **Step 5: Commit**

```powershell
git add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj src/domain/cross_section/RoadModel.h src/domain/cross_section/RoadModel.cpp
git commit -m "feat: add road model priority resolver"
```

---

### Task 2: Station Sampler

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `src/domain/cross_section/RoadModel.cpp`

- [ ] **Step 1: Write the failing test**

Add after `roadModelTemplateResolverRejectsInvalidRows()`:

```cpp
void roadModelStationSamplerIncludesIntervalTemplateAndVerticalCurveStations()
{
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData verticalCurve;
    verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 20.0, 100.0},
        {VerticalCurvePointRole::End, 90.0, 101.0},
    };
    verticalCurve.pvis = {
        {50.0, 105.0, 40.0, false},
    };

    RoadModelTemplateAssignment first;
    first.startStation = 0.0;
    first.endStation = 60.0;
    first.templateHandle = L"T1";

    RoadModelTemplateAssignment second;
    second.startStation = 70.0;
    second.endStation = 120.0;
    second.templateHandle = L"T2";

    const auto stations = RoadModelStationSampler::collectStations(
        0.0,
        100.0,
        verticalCurve,
        {first, second},
        25.0);

    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 20.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 45.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 50.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 60.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 70.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 90.0) < 1e-9; }) != stations.end());
    CHECK(std::none_of(stations.begin(), stations.end(), [](double station) { return station < 20.0 || station > 90.0; }));
    CHECK(std::is_sorted(stations.begin(), stations.end()));
}
```

Call it in `main()` after `roadModelTemplateResolverRejectsInvalidRows();`:

```cpp
    roadModelStationSamplerIncludesIntervalTemplateAndVerticalCurveStations();
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: test fails because `stations` is empty.

- [ ] **Step 3: Implement station collection**

Replace `RoadModelStationSampler::collectStations` in `RoadModel.cpp` with:

```cpp
namespace {

void addStation(std::vector<double>& stations, double station, double start, double end)
{
    if (isFinite(station) && station + kStationTolerance >= start && station <= end + kStationTolerance) {
        stations.push_back(std::clamp(station, start, end));
    }
}

void addVerticalCurveStations(
    std::vector<double>& stations,
    const profile::ProfileVerticalCurveData& verticalCurve,
    double start,
    double end)
{
    for (const auto& point : verticalCurve.controlPoints) {
        addStation(stations, point.station, start, end);
    }
    for (const auto& pvi : verticalCurve.pvis) {
        addStation(stations, pvi.station, start, end);
        const double tangent = std::fabs(pvi.radius) * 0.01;
        addStation(stations, pvi.station - tangent, start, end);
        addStation(stations, pvi.station + tangent, start, end);
    }
}

std::vector<double> uniqueSortedStations(std::vector<double> stations)
{
    std::sort(stations.begin(), stations.end());
    std::vector<double> result;
    result.reserve(stations.size());
    for (const auto station : stations) {
        if (result.empty() || std::fabs(station - result.back()) > kStationTolerance) {
            result.push_back(station);
        }
    }
    return result;
}

} // namespace

std::vector<double> RoadModelStationSampler::collectStations(
    double alignmentStart,
    double alignmentEnd,
    const profile::ProfileVerticalCurveData& verticalCurve,
    const std::vector<RoadModelTemplateAssignment>& assignments,
    double sampleInterval)
{
    if (!isFinite(alignmentStart) || !isFinite(alignmentEnd) || alignmentEnd <= alignmentStart) {
        return {};
    }
    if (!RoadModelRules::isSupportedSampleInterval(sampleInterval)) {
        return {};
    }

    double verticalStart = alignmentStart;
    double verticalEnd = alignmentEnd;
    if (!verticalCurve.controlPoints.empty()) {
        verticalStart = verticalCurve.controlPoints.front().station;
        verticalEnd = verticalCurve.controlPoints.front().station;
        for (const auto& point : verticalCurve.controlPoints) {
            verticalStart = std::min(verticalStart, point.station);
            verticalEnd = std::max(verticalEnd, point.station);
        }
    }

    const double effectiveStart = std::max(alignmentStart, verticalStart);
    const double effectiveEnd = std::min(alignmentEnd, verticalEnd);
    if (effectiveEnd < effectiveStart) {
        return {};
    }

    std::vector<double> stations;
    addStation(stations, effectiveStart, effectiveStart, effectiveEnd);
    addStation(stations, effectiveEnd, effectiveStart, effectiveEnd);

    for (double station = effectiveStart; station < effectiveEnd - kStationTolerance; station += sampleInterval) {
        addStation(stations, station, effectiveStart, effectiveEnd);
        if (station + sampleInterval <= station) {
            break;
        }
    }

    for (const auto& row : assignments) {
        const double start = std::max(row.startStation, effectiveStart);
        const double end = std::min(row.endStation, effectiveEnd);
        if (end + kStationTolerance >= start) {
            addStation(stations, start, effectiveStart, effectiveEnd);
            addStation(stations, end, effectiveStart, effectiveEnd);
        }
    }

    addVerticalCurveStations(stations, verticalCurve, effectiveStart, effectiveEnd);
    return uniqueSortedStations(std::move(stations));
}
```

When implementing Task 3, replace the approximate `radius * 0.01` key station logic with actual BVC/EVC from `ProfileVerticalCurveCalculator::rebuild`; keep this task small so the test goes green.

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all core tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/core_tests.cpp src/domain/cross_section/RoadModel.cpp
git commit -m "feat: sample road model stations"
```

---

### Task 3: Road Model Builder

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `src/domain/cross_section/RoadModel.cpp`

- [ ] **Step 1: Write the failing test**

Add after `roadModelStationSamplerIncludesIntervalTemplateAndVerticalCurveStations()`:

```cpp
void roadModelBuilderCreatesThreeDimensionalComponentLines()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateData templateData;
    templateData.properties.name = L"测试模板";
    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;
    lane.fixedSlope = -0.02;
    lane.color = {1, 2, 3};
    templateData.components.push_back(lane);

    RoadModelTemplateAssignment row;
    row.startStation = 0.0;
    row.endStation = 20.0;
    row.templateHandle = L"T1";
    row.templateName = L"测试模板";

    ProfileVerticalCurveData verticalCurve;
    verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 102.0},
    };

    RoadModelBuildInput input;
    input.config.roadCenterlineHandle = L"C1";
    input.config.profileVerticalCurveHandle = L"V1";
    input.config.sampleInterval = 10.0;
    input.config.assignments = {row};
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.verticalCurve = verticalCurve;
    input.templates = {{L"T1", templateData}};

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    CHECK(result.sampledStations.size() == 3);
    CHECK(result.data.componentLines.size() >= 2);
    CHECK(result.data.componentLines.front().points.size() == 3);
    CHECK(std::fabs(result.data.componentLines.front().points.front().z - 100.0) < 1.0e-9);
    CHECK(std::fabs(result.data.componentLines.front().points.back().z - 102.0) < 1.0e-9);
    CHECK(result.data.componentLines.front().key.templateHandle == L"T1");
    CHECK(result.data.componentLines.front().color.r == 1);
}

void roadModelBuilderDoesNotConnectAcrossTemplateSwitches()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    auto makeTemplate = [](int red) {
        SubgradeTemplateData data;
        SubgradeTemplateComponent lane;
        lane.side = SubgradeSide::Right;
        lane.type = SubgradeComponentType::TravelLane;
        lane.width = 3.0;
        lane.color = {red, 0, 0};
        data.components.push_back(lane);
        return data;
    };

    RoadModelBuildInput input;
    input.config.roadCenterlineHandle = L"C1";
    input.config.profileVerticalCurveHandle = L"V1";
    input.config.sampleInterval = 10.0;
    input.config.assignments = {
        {0.0, 10.0, L"T1", L"模板1"},
        {10.0, 20.0, L"T2", L"模板2"},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.templates = {
        {L"T1", makeTemplate(10)},
        {L"T2", makeTemplate(20)},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    CHECK(result.data.componentLines.size() >= 4);
    CHECK(std::any_of(result.data.componentLines.begin(), result.data.componentLines.end(), [](const auto& line) {
        return line.key.templateHandle == L"T1" && line.points.size() == 2;
    }));
    CHECK(std::any_of(result.data.componentLines.begin(), result.data.componentLines.end(), [](const auto& line) {
        return line.key.templateHandle == L"T2" && line.points.size() == 2;
    }));
}
```

Call both in `main()` after `roadModelStationSamplerIncludesIntervalTemplateAndVerticalCurveStations();`:

```cpp
    roadModelBuilderCreatesThreeDimensionalComponentLines();
    roadModelBuilderDoesNotConnectAcrossTemplateSwitches();
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: tests fail because `RoadModelBuilder::build` returns `succeeded=false` or no lines.

- [ ] **Step 3: Implement builder**

Update `RoadModel.cpp`:

```cpp
#include "domain/profile/ProfileVerticalCurveCalculator.h"
#include <map>
```

Add helper functions:

```cpp
namespace {

struct AlignmentFrame {
    alignment::AlignmentPoint2d point;
    alignment::AlignmentPoint2d tangent;
    alignment::AlignmentPoint2d normal;
};

bool interpolateAlignmentFrame(
    const std::vector<alignment::AlignmentSamplePoint>& samples,
    double station,
    AlignmentFrame& frame)
{
    if (samples.size() < 2 || !isFinite(station)) {
        return false;
    }

    std::vector<alignment::AlignmentSamplePoint> sorted = samples;
    std::sort(sorted.begin(), sorted.end(), [](const auto& left, const auto& right) {
        return left.station < right.station;
    });

    std::size_t segment = 1;
    while (segment < sorted.size() && station > sorted[segment].station + kStationTolerance) {
        ++segment;
    }
    if (segment >= sorted.size()) {
        segment = sorted.size() - 1;
    }

    const auto& previous = sorted[segment - 1];
    const auto& next = sorted[segment];
    const double span = next.station - previous.station;
    if (span <= kStationTolerance) {
        return false;
    }

    const double t = std::clamp((station - previous.station) / span, 0.0, 1.0);
    frame.point = {
        previous.point.x + (next.point.x - previous.point.x) * t,
        previous.point.y + (next.point.y - previous.point.y) * t,
    };

    const double dx = next.point.x - previous.point.x;
    const double dy = next.point.y - previous.point.y;
    const double length = std::sqrt(dx * dx + dy * dy);
    if (length <= kStationTolerance) {
        return false;
    }
    frame.tangent = {dx / length, dy / length};
    frame.normal = {-frame.tangent.y, frame.tangent.x};
    return true;
}

const RoadModelTemplateSource* findTemplate(
    const std::vector<RoadModelTemplateSource>& templates,
    const std::wstring& handle)
{
    const auto found = std::find_if(templates.begin(), templates.end(), [&handle](const auto& item) {
        return item.templateHandle == handle;
    });
    return found == templates.end() ? nullptr : &*found;
}

struct ActiveBoundaryPoint {
    RoadModelLineKey key;
    SubgradeTemplateRgbColor color;
    RoadModelPoint3d point;
};

bool sameLineKey(const RoadModelLineKey& left, const RoadModelLineKey& right)
{
    return left.templateHandle == right.templateHandle
        && left.side == right.side
        && left.componentType == right.componentType
        && left.componentIndex == right.componentIndex
        && left.boundaryIndex == right.boundaryIndex;
}

void appendBoundaryPoint(
    std::vector<ActiveBoundaryPoint>& points,
    const RoadModelLineKey& key,
    const SubgradeTemplateRgbColor& color,
    const AlignmentFrame& frame,
    double offset,
    double elevation)
{
    points.push_back({
        key,
        color,
        {
            frame.point.x + frame.normal.x * offset,
            frame.point.y + frame.normal.y * offset,
            elevation,
        },
    });
}

std::vector<SubgradeTemplateComponent> componentsForSide(
    const SubgradeTemplateData& data,
    SubgradeSide side)
{
    std::vector<SubgradeTemplateComponent> result;
    for (const auto& component : data.components) {
        if (component.side == side) {
            result.push_back(component);
        }
    }
    return result;
}

void appendTemplateBoundaryPoints(
    std::vector<ActiveBoundaryPoint>& points,
    const RoadModelTemplateAssignment& assignment,
    const SubgradeTemplateData& data,
    const AlignmentFrame& frame,
    double station,
    double centerElevation,
    SubgradeSide side)
{
    const double direction = side == SubgradeSide::Left ? 1.0 : -1.0;
    double offset = 0.0;
    double elevation = centerElevation;
    const auto components = componentsForSide(data, side);
    for (std::size_t i = 0; i < components.size(); ++i) {
        const auto& component = components[i];
        RoadModelLineKey innerKey{assignment.templateHandle, side, component.type, i, 0};
        appendBoundaryPoint(points, innerKey, component.color, frame, offset, elevation);

        const double width = SubgradeTemplateRules::widthAtStation(component, station);
        const double slope = SubgradeTemplateRules::slopeAtStation(component, station);
        offset += direction * width;
        elevation += component.height + std::fabs(width) * slope;

        RoadModelLineKey outerKey{assignment.templateHandle, side, component.type, i, 1};
        appendBoundaryPoint(points, outerKey, component.color, frame, offset, elevation);
    }
}

} // namespace
```

Replace `RoadModelBuilder::build`:

```cpp
RoadModelBuildResult RoadModelBuilder::build(const RoadModelBuildInput& input)
{
    RoadModelBuildResult result;
    result.data.config = input.config;

    if (!RoadModelRules::isSupportedSampleInterval(input.config.sampleInterval)) {
        result.errorMessage = L"Road model sample interval must be greater than 0.";
        return result;
    }
    if (input.alignmentSamples.size() < 2) {
        result.errorMessage = L"Road model requires at least two alignment samples.";
        return result;
    }
    std::wstring errorMessage;
    if (!RoadModelRules::validateAssignments(input.config.assignments, errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    const auto builtVertical = profile::ProfileVerticalCurveCalculator::rebuild(input.verticalCurve);
    if (!builtVertical.succeeded) {
        result.errorMessage = builtVertical.errorMessage;
        return result;
    }

    auto sortedSamples = input.alignmentSamples;
    std::sort(sortedSamples.begin(), sortedSamples.end(), [](const auto& left, const auto& right) {
        return left.station < right.station;
    });
    const double alignmentStart = sortedSamples.front().station;
    const double alignmentEnd = sortedSamples.back().station;
    const auto stations = RoadModelStationSampler::collectStations(
        alignmentStart,
        alignmentEnd,
        input.verticalCurve,
        input.config.assignments,
        input.config.sampleInterval);

    RoadModelTemplateResolver resolver(input.config.assignments);
    std::vector<ActiveBoundaryPoint> previousPoints;
    for (const auto station : stations) {
        const auto* assignment = resolver.resolve(station);
        if (assignment == nullptr) {
            previousPoints.clear();
            continue;
        }
        const auto* source = findTemplate(input.templates, assignment->templateHandle);
        if (source == nullptr) {
            result.errorMessage = L"Road model template handle was not found in template sources.";
            return result;
        }

        AlignmentFrame frame;
        if (!interpolateAlignmentFrame(sortedSamples, station, frame)) {
            result.errorMessage = L"Road model could not interpolate alignment frame.";
            return result;
        }
        const auto elevation = profile::ProfileVerticalCurveCalculator::elevationAt(builtVertical, station);
        if (!elevation.succeeded) {
            previousPoints.clear();
            continue;
        }

        std::vector<ActiveBoundaryPoint> currentPoints;
        appendTemplateBoundaryPoints(
            currentPoints,
            *assignment,
            source->data,
            frame,
            station,
            elevation.value,
            SubgradeSide::Left);
        appendTemplateBoundaryPoints(
            currentPoints,
            *assignment,
            source->data,
            frame,
            station,
            elevation.value,
            SubgradeSide::Right);

        if (!previousPoints.empty()) {
            for (const auto& current : currentPoints) {
                const auto previous = std::find_if(previousPoints.begin(), previousPoints.end(), [&current](const auto& item) {
                    return sameLineKey(item.key, current.key);
                });
                if (previous == previousPoints.end()) {
                    continue;
                }

                auto line = std::find_if(result.data.componentLines.begin(), result.data.componentLines.end(), [&current](const auto& item) {
                    return sameLineKey(item.key, current.key)
                        && !item.points.empty()
                        && std::fabs(item.points.back().x - previous->point.x) < kStationTolerance
                        && std::fabs(item.points.back().y - previous->point.y) < kStationTolerance
                        && std::fabs(item.points.back().z - previous->point.z) < kStationTolerance;
                });
                if (line == result.data.componentLines.end()) {
                    RoadModelComponentLine newLine;
                    newLine.key = current.key;
                    newLine.color = current.color;
                    newLine.points.push_back(previous->point);
                    newLine.points.push_back(current.point);
                    result.data.componentLines.push_back(std::move(newLine));
                } else {
                    line->points.push_back(current.point);
                }
            }
        }

        result.sampledStations.push_back(station);
        previousPoints = std::move(currentPoints);
    }

    if (result.data.componentLines.empty()) {
        result.errorMessage = L"Road model did not generate any component lines.";
        return result;
    }

    result.succeeded = true;
    return result;
}
```

Then improve `RoadModelStationSampler::collectStations` to use actual BVC/EVC:

```cpp
const auto built = profile::ProfileVerticalCurveCalculator::rebuild(verticalCurve);
if (built.succeeded) {
    for (const auto& element : built.elements) {
        addStation(stations, element.bvcStation, effectiveStart, effectiveEnd);
        addStation(stations, element.evcStation, effectiveStart, effectiveEnd);
    }
}
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all core tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/core_tests.cpp src/domain/cross_section/RoadModel.cpp
git commit -m "feat: build road model component lines"
```

---

### Task 4: Application Build Service

**Files:**
- Modify: `tests/core_tests.cpp`
- Create: `src/application/cross_section/RoadModelBuildService.h`
- Create: `src/application/cross_section/RoadModelBuildService.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write the failing test**

Add include:

```cpp
#include "application/cross_section/RoadModelBuildService.h"
```

Add after road model builder tests:

```cpp
void roadModelBuildServiceRejectsMissingHandlesAndDelegatesBuild()
{
    using namespace roadproto::application::cross_section;
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments = {{0.0, 10.0, L"T1", L"模板"}};

    RoadModelBuildService service;
    auto result = service.build(input);
    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());

    input.config.roadCenterlineHandle = L"C1";
    input.config.profileVerticalCurveHandle = L"V1";
    input.alignmentSamples = {{{0.0, 0.0}, 0.0}, {{10.0, 0.0}, 10.0}};
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 10.0, 100.0},
    };
    SubgradeTemplateData data;
    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;
    data.components.push_back(lane);
    input.templates = {{L"T1", data}};

    result = service.build(input);
    CHECK(result.succeeded);
    CHECK(result.data.config.roadCenterlineHandle == L"C1");
    CHECK(result.data.config.profileVerticalCurveHandle == L"V1");
}
```

Call it in `main()` after `roadModelBuilderDoesNotConnectAcrossTemplateSwitches();`.

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: compile fails because `RoadModelBuildService.h` does not exist.

- [ ] **Step 3: Implement service**

Create `src/application/cross_section/RoadModelBuildService.h`:

```cpp
#pragma once

#include "domain/cross_section/RoadModel.h"

namespace roadproto::application::cross_section {

using RoadModelBuildInput = domain::cross_section::RoadModelBuildInput;
using RoadModelBuildResult = domain::cross_section::RoadModelBuildResult;

class RoadModelBuildService final {
public:
    RoadModelBuildResult build(const RoadModelBuildInput& input) const;
};

} // namespace roadproto::application::cross_section
```

Create `src/application/cross_section/RoadModelBuildService.cpp`:

```cpp
#include "application/cross_section/RoadModelBuildService.h"

namespace roadproto::application::cross_section {

RoadModelBuildResult RoadModelBuildService::build(const RoadModelBuildInput& input) const
{
    if (input.config.roadCenterlineHandle.empty()) {
        return RoadModelBuildResult{false, L"Road centerline handle is required."};
    }
    if (input.config.profileVerticalCurveHandle.empty()) {
        return RoadModelBuildResult{false, L"Profile vertical curve handle is required."};
    }
    return domain::cross_section::RoadModelBuilder::build(input);
}

} // namespace roadproto::application::cross_section
```

Add source file to `.vcxproj` files:

```xml
<ClCompile Include="..\src\application\cross_section\RoadModelBuildService.cpp" />
```

For `src/app/RoadProtoArx.vcxproj`, use:

```xml
<ClCompile Include="..\application\cross_section\RoadModelBuildService.cpp" />
```

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all core tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj src/application/cross_section/RoadModelBuildService.h src/application/cross_section/RoadModelBuildService.cpp
git commit -m "feat: add road model build service"
```

---

### Task 5: Command Metadata And Module Registration

**Files:**
- Modify: `tests/core_tests.cpp`
- Create: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h`
- Create: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- Modify: `src/modules/cross_section/CrossSectionModule.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write the failing test**

Update `crossSectionModuleRegistersSubgradeTemplateCommandsAndRibbonPanel()` to also assert road model commands:

```cpp
    const auto roadModelCreate = commands.find(L"RD_SECTION_ROAD_MODEL_CREATE");
    CHECK(roadModelCreate.has_value());
    if (roadModelCreate.has_value()) {
        CHECK(roadModelCreate->displayName == L"横断面戴帽");
        CHECK(roadModelCreate->moduleCode == L"CROSS_SECTION");
        CHECK(roadModelCreate->businessDocPath == L"docs/business/cross_section/横断面戴帽_道路模型创建.md");
        CHECK(roadModelCreate->ribbonAttachable);
        CHECK(roadModelCreate->isPrototype);
        CHECK(roadModelCreate->reusable);
    }

    const auto roadModelEdit = commands.find(L"RD_SECTION_ROAD_MODEL_EDIT");
    CHECK(roadModelEdit.has_value());
    if (roadModelEdit.has_value()) {
        CHECK(roadModelEdit->displayName == L"编辑道路模型");
        CHECK(roadModelEdit->ribbonAttachable);
    }

    const auto roadModelEditHandle = commands.find(L"RD_SECTION_ROAD_MODEL_EDIT_HANDLE");
    CHECK(roadModelEditHandle.has_value());
    if (roadModelEditHandle.has_value()) {
        CHECK(!roadModelEditHandle->ribbonAttachable);
        CHECK(roadModelEditHandle->businessDocPath == L"docs/business/cross_section/道路模型_编辑.md");
    }

    const auto roadModelApply = commands.find(L"RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE");
    CHECK(roadModelApply.has_value());
    if (roadModelApply.has_value()) {
        CHECK(!roadModelApply->ribbonAttachable);
        CHECK(roadModelApply->businessDocPath == L"docs/business/cross_section/道路模型_WPF桥接回写.md");
    }
```

Update `startupRegistrationIncludesCrossSectionModule()`:

```cpp
    CHECK(commands.contains(L"RD_SECTION_ROAD_MODEL_CREATE"));
    CHECK(commands.contains(L"RD_SECTION_ROAD_MODEL_EDIT"));
```

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: tests fail because commands are not registered.

- [ ] **Step 3: Add command stubs and registration**

Create `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h`:

```cpp
#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::cross_section {

core::CommandProcedure roadModelCreateCommandProcedure();
core::CommandProcedure roadModelEditCommandProcedure();
core::CommandProcedure roadModelEditHandleCommandProcedure();
core::CommandProcedure roadModelApplyDialogFileCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::cross_section
```

Create `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`:

```cpp
#include "cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h"

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

void runRoadModelCreateCommand()
{
}

void runRoadModelEditCommand()
{
}

void runRoadModelEditHandleCommand()
{
}

void runRoadModelApplyDialogFileCommand()
{
}

} // namespace

core::CommandProcedure roadModelCreateCommandProcedure()
{
    return &runRoadModelCreateCommand;
}

core::CommandProcedure roadModelEditCommandProcedure()
{
    return &runRoadModelEditCommand;
}

core::CommandProcedure roadModelEditHandleCommandProcedure()
{
    return &runRoadModelEditHandleCommand;
}

core::CommandProcedure roadModelApplyDialogFileCommandProcedure()
{
    return &runRoadModelApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
```

Modify `src/modules/cross_section/CrossSectionModule.cpp`:

```cpp
#include "cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h"
```

Add command registrations after subgrade commands:

```cpp
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_ROAD_MODEL_CREATE",
        L"横断面戴帽",
        L"CROSS_SECTION",
        L"Assigns subgrade templates by station priority and creates a 3D road model.",
        cad_adapter::objectarx::cross_section::roadModelCreateCommandProcedure(),
        true,
        true,
        L"docs/business/cross_section/横断面戴帽_道路模型创建.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_ROAD_MODEL_EDIT",
        L"编辑道路模型",
        L"CROSS_SECTION",
        L"Selects and edits an existing cross-section road model.",
        cad_adapter::objectarx::cross_section::roadModelEditCommandProcedure(),
        true,
        true,
        L"docs/business/cross_section/道路模型_编辑.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_ROAD_MODEL_EDIT_HANDLE",
        L"按 handle 编辑道路模型",
        L"CROSS_SECTION",
        L"Internal double-click bridge command that edits a road model by handle.",
        cad_adapter::objectarx::cross_section::roadModelEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/道路模型_编辑.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE",
        L"应用道路模型对话框结果",
        L"CROSS_SECTION",
        L"Internal WPF bridge command that applies road model dialog response files.",
        cad_adapter::objectarx::cross_section::roadModelApplyDialogFileCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/道路模型_WPF桥接回写.md",
        false});
```

Add `ObjectArxRoadModelCommand.cpp` to both `.vcxproj` files.

- [ ] **Step 4: Run test to verify it passes**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all core tests pass.

- [ ] **Step 5: Commit**

```powershell
git add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj src/modules/cross_section/CrossSectionModule.cpp src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp
git commit -m "feat: register road model commands"
```

---

### Task 6: WPF DTOs, Window, Ribbon, And Double-Click Source Contracts

**Files:**
- Modify: `tests/core_tests.cpp`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelDialogDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelDialogFile.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadModelDialogCommands.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelWindow.xaml.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`

- [ ] **Step 1: Write the failing tests**

Add after `managedRibbonExtensionRegistersSubgradeTemplateEntryPoints()`:

```cpp
void managedRibbonExtensionRegistersRoadModelEntryPoints()
{
    const auto sourcePath = findRepositoryRootForTests()
        / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "AutoCad" / "RoadProtoRibbonExtension.cs";
    const auto source = readTextFileForTests(sourcePath);

    CHECK(source.find("RD_SECTION_ROAD_MODEL_CREATE") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_EDIT") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_EDIT_HANDLE") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_EDIT_HANDLE {handle}\\n") != std::string::npos);
    CHECK(source.find("DNROADMODELENTITY") != std::string::npos);
    CHECK(source.find("RoadModelDialogCommands") != std::string::npos);
}

void roadModelWpfBridgeSourceContainsRequiredFields()
{
    const auto root = findRepositoryRootForTests();
    const auto dtoPath = root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "Bridge" / "RoadModelDialogDtos.cs";
    const auto filePath = root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "Bridge" / "RoadModelDialogFile.cs";
    const auto windowPath = root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "RoadModelWindow.xaml";
    const auto windowCodePath = root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "RoadModelWindow.xaml.cs";

    const auto dto = readTextFileForTests(dtoPath);
    const auto file = readTextFileForTests(filePath);
    const auto xaml = readTextFileForTests(windowPath);
    const auto code = readTextFileForTests(windowCodePath);

    CHECK(dto.find("RoadCenterlineHandle") != std::string::npos);
    CHECK(dto.find("ProfileVerticalCurveHandle") != std::string::npos);
    CHECK(dto.find("SampleInterval") != std::string::npos);
    CHECK(dto.find("RoadModelTemplateAssignmentDto") != std::string::npos);
    CHECK(file.find("assignmentCount") != std::string::npos);
    CHECK(file.find("RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE") == std::string::npos);
    CHECK(xaml.find("横断面戴帽") != std::string::npos);
    CHECK(xaml.find("路基模板") != std::string::npos);
    CHECK(code.find("MoveAssignment") != std::string::npos);
    CHECK(code.find("BuildResponse") != std::string::npos);
}
```

Call both in `main()` after `managedRibbonExtensionRegistersSubgradeTemplateEntryPoints();`.

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: source contract tests fail because files and strings do not exist.

- [ ] **Step 3: Add WPF DTOs and file bridge**

Create `RoadModelDialogDtos.cs`:

```csharp
using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public sealed class RoadModelTemplateAssignmentDto
{
    public double StartStation { get; set; }
    public double EndStation { get; set; }
    public string TemplateHandle { get; set; } = string.Empty;
    public string TemplateName { get; set; } = string.Empty;

    public RoadModelTemplateAssignmentDto Clone()
        => new()
        {
            StartStation = StartStation,
            EndStation = EndStation,
            TemplateHandle = TemplateHandle,
            TemplateName = TemplateName,
        };
}

public sealed class RoadModelDialogRequest
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public string ProfileVerticalCurveHandle { get; set; } = string.Empty;
    public double SampleInterval { get; set; } = 10.0;
    public List<RoadModelTemplateAssignmentDto> Assignments { get; set; } = new();
}

public sealed class RoadModelDialogResponse
{
    public bool Accepted { get; set; }
    public string Handle { get; set; } = string.Empty;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public string ProfileVerticalCurveHandle { get; set; } = string.Empty;
    public double SampleInterval { get; set; } = 10.0;
    public List<RoadModelTemplateAssignmentDto> Assignments { get; set; } = new();
}
```

Create `RoadModelDialogFile.cs` by copying the key-value style from `SubgradeTemplateDialogFile.cs` and using these keys:

```text
accepted
handle
responsePath
roadCenterlineHandle
profileVerticalCurveHandle
sampleInterval
assignmentCount
assignment.<i>.startStation
assignment.<i>.endStation
assignment.<i>.templateHandle
assignment.<i>.templateName
```

Use `UTF8Encoding(false)`, invariant culture `"R"` double formatting, and percent escaping identical to existing bridge files.

- [ ] **Step 4: Add WPF window**

Create `RoadModelWindow.xaml`:

```xml
<Window x:Class="RoadProto.Terrain.UI.RoadModelWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="横断面戴帽"
        Width="860"
        Height="520"
        WindowStartupLocation="CenterOwner">
    <DockPanel Margin="12">
        <StackPanel DockPanel.Dock="Bottom" Orientation="Horizontal" HorizontalAlignment="Right" Margin="0,12,0,0">
            <Button Width="88" Margin="0,0,8,0" Content="生成模型" Click="OnGenerateModel"/>
            <Button Width="88" Content="取消" Click="OnCancel"/>
        </StackPanel>
        <TabControl>
            <TabItem Header="路基模板">
                <DockPanel Margin="8">
                    <StackPanel DockPanel.Dock="Top" Orientation="Horizontal" Margin="0,0,0,8">
                        <TextBlock VerticalAlignment="Center" Text="模型采样间距"/>
                        <TextBox Width="90" Margin="8,0,16,0" Text="{Binding SampleIntervalText, UpdateSourceTrigger=PropertyChanged}"/>
                        <Button Width="70" Margin="0,0,6,0" Content="新增行" Click="OnAddAssignment"/>
                        <Button Width="70" Margin="0,0,6,0" Content="删除行" Click="OnDeleteAssignment"/>
                        <Button Width="60" Margin="0,0,6,0" Content="上移" Click="OnMoveUp"/>
                        <Button Width="60" Margin="0,0,6,0" Content="下移" Click="OnMoveDown"/>
                        <Button Width="88" Content="选择模板" Click="OnPickTemplate"/>
                    </StackPanel>
                    <DataGrid ItemsSource="{Binding Assignments}"
                              SelectedItem="{Binding SelectedAssignment}"
                              AutoGenerateColumns="False"
                              CanUserAddRows="False"
                              HeadersVisibility="Column"
                              GridLinesVisibility="All">
                        <DataGrid.Columns>
                            <DataGridTextColumn Header="起点桩号" Binding="{Binding StartStation}"/>
                            <DataGridTextColumn Header="终点桩号" Binding="{Binding EndStation}"/>
                            <DataGridTextColumn Header="路基模板 handle" Binding="{Binding TemplateHandle}" Width="180"/>
                            <DataGridTextColumn Header="路基模板名称" Binding="{Binding TemplateName}" Width="*"/>
                        </DataGrid.Columns>
                    </DataGrid>
                </DockPanel>
            </TabItem>
            <TabItem Header="其他" IsEnabled="False"/>
        </TabControl>
    </DockPanel>
</Window>
```

Create `RoadModelWindow.xaml.cs`:

```csharp
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class RoadModelWindow : Window, INotifyPropertyChanged
{
    private readonly RoadModelDialogRequest _request;
    private RoadModelTemplateAssignmentDto? _selectedAssignment;
    private string _sampleIntervalText;

    public RoadModelWindow(RoadModelDialogRequest request)
    {
        _request = request;
        _sampleIntervalText = request.SampleInterval.ToString("R", CultureInfo.InvariantCulture);
        Assignments = new ObservableCollection<RoadModelTemplateAssignmentDto>(
            request.Assignments.Select(item => item.Clone()));
        InitializeComponent();
        DataContext = this;
    }

    public ObservableCollection<RoadModelTemplateAssignmentDto> Assignments { get; }
    public RoadModelDialogResponse? Response { get; private set; }

    public string SampleIntervalText
    {
        get => _sampleIntervalText;
        set
        {
            _sampleIntervalText = value;
            OnPropertyChanged();
        }
    }

    public RoadModelTemplateAssignmentDto? SelectedAssignment
    {
        get => _selectedAssignment;
        set
        {
            _selectedAssignment = value;
            OnPropertyChanged();
        }
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    private void OnAddAssignment(object sender, RoutedEventArgs e)
    {
        Assignments.Add(new RoadModelTemplateAssignmentDto());
        SelectedAssignment = Assignments.LastOrDefault();
    }

    private void OnDeleteAssignment(object sender, RoutedEventArgs e)
    {
        if (SelectedAssignment == null)
        {
            return;
        }
        var index = Assignments.IndexOf(SelectedAssignment);
        Assignments.Remove(SelectedAssignment);
        SelectedAssignment = Assignments.Count == 0 ? null : Assignments[System.Math.Min(index, Assignments.Count - 1)];
    }

    private void OnMoveUp(object sender, RoutedEventArgs e) => MoveAssignment(-1);
    private void OnMoveDown(object sender, RoutedEventArgs e) => MoveAssignment(1);

    private void MoveAssignment(int delta)
    {
        if (SelectedAssignment == null)
        {
            return;
        }
        var index = Assignments.IndexOf(SelectedAssignment);
        var target = index + delta;
        if (index < 0 || target < 0 || target >= Assignments.Count)
        {
            return;
        }
        Assignments.Move(index, target);
    }

    private void OnPickTemplate(object sender, RoutedEventArgs e)
    {
        MessageBox.Show("第一版模板选择由 C++ 命令在关闭窗口后处理。请先手工填写模板 handle。", "RoadProto");
    }

    private void OnGenerateModel(object sender, RoutedEventArgs e)
    {
        if (!double.TryParse(SampleIntervalText, NumberStyles.Float, CultureInfo.InvariantCulture, out var sampleInterval)
            || sampleInterval <= 0.0)
        {
            MessageBox.Show("模型采样间距必须大于 0。", "RoadProto");
            return;
        }

        Response = BuildResponse(true, sampleInterval);
        DialogResult = true;
    }

    private void OnCancel(object sender, RoutedEventArgs e)
    {
        Response = BuildResponse(false, _request.SampleInterval);
        DialogResult = false;
    }

    private RoadModelDialogResponse BuildResponse(bool accepted, double sampleInterval)
        => new()
        {
            Accepted = accepted,
            Handle = _request.Handle,
            RoadCenterlineHandle = _request.RoadCenterlineHandle,
            ProfileVerticalCurveHandle = _request.ProfileVerticalCurveHandle,
            SampleInterval = sampleInterval,
            Assignments = Assignments.Select(item => item.Clone()).ToList(),
        };

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
}
```

- [ ] **Step 5: Add WPF command class and ribbon wiring**

Create `AutoCad/RoadModelDialogCommands.cs`:

```csharp
using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class RoadModelDialogCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoRoadModelDialog_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_SECTION_ROAD_MODEL_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowRoadModelDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto road model dialog pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            var request = RoadModelDialogFile.ReadRequest(requestPath);
            var window = new RoadModelWindow(request);
            var dialogResult = window.ShowDialog();
            var response = window.Response ?? new RoadModelDialogResponse
            {
                Accepted = dialogResult == true,
                Handle = request.Handle,
                RoadCenterlineHandle = request.RoadCenterlineHandle,
                ProfileVerticalCurveHandle = request.ProfileVerticalCurveHandle,
                SampleInterval = request.SampleInterval,
                Assignments = request.Assignments,
            };

            RoadModelDialogFile.WriteResponse(request.ResponsePath, response);
            var responseCommandPath = request.ResponsePath.Replace('\\', '/');
            document.SendStringToExecute($"RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n", true, false, true);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto road model WPF dialog failed: {error.Message}");
        }
    }
}
```

Modify `RoadProtoRibbonExtension.cs`:

```csharp
[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.RoadModelDialogCommands))]
```

Add constants:

```csharp
private const string RoadModelCreateButtonId = "ROADPROTO_RD_SECTION_ROAD_MODEL_CREATE";
private const string RoadModelEditButtonId = "ROADPROTO_RD_SECTION_ROAD_MODEL_EDIT";
private const string RoadModelDxfName = "DNROADMODELENTITY";
```

Add two cross-section buttons:

```csharp
crossSectionPanel.Source.Items.Add(CreateCrossSectionCommandButton(
    RoadModelCreateButtonId,
    "横断面戴帽",
    "选择道路中线并按路基模板优先级生成三维道路模型",
    "RD_SECTION_ROAD_MODEL_CREATE "));

crossSectionPanel.Source.Items.Add(CreateCrossSectionCommandButton(
    RoadModelEditButtonId,
    "编辑道路模型",
    "选择并编辑横断面戴帽道路模型",
    "RD_SECTION_ROAD_MODEL_EDIT "));
```

Add double-click route in `OnBeginDoubleClick`:

```csharp
if (TryFindEntityByDxfName(document, e.Location, RoadModelDxfName, out var roadModelId))
{
    if (!SuppressDuplicateDoubleClick(roadModelId))
    {
        QueueRoadModelEditByHandle(document, roadModelId);
    }
    return;
}
```

Add method:

```csharp
private static void QueueRoadModelEditByHandle(Document document, ObjectId entityId)
{
    var handle = entityId.Handle.ToString();
    if (string.IsNullOrWhiteSpace(handle))
    {
        return;
    }

    document.SendStringToExecute($"RD_SECTION_ROAD_MODEL_EDIT_HANDLE {handle}\n", true, false, true);
}
```

- [ ] **Step 6: Run tests and WPF build**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: core tests pass and WPF Debug build succeeds.

- [ ] **Step 7: Commit**

```powershell
git add tests/core_tests.cpp src/ui/wpf/RoadProto.Terrain.UI
git commit -m "feat: add road model WPF shell"
```

---

### Task 7: C++ Road Model Bridge

**Files:**
- Modify: `tests/core_tests.cpp`
- Create: `src/cad_adapter/objectarx/cross_section/RoadModelDialogBridge.h`
- Create: `src/cad_adapter/objectarx/cross_section/RoadModelDialogBridge.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write the failing source contract test**

Add after `roadModelWpfBridgeSourceContainsRequiredFields()`:

```cpp
void roadModelCppBridgeSourceContainsRequiredFields()
{
    const auto root = findRepositoryRootForTests();
    const auto bridgePath = root / "src" / "cad_adapter" / "objectarx" / "cross_section" / "RoadModelDialogBridge.cpp";
    const auto source = readTextFileForTests(bridgePath);

    CHECK(source.find("roadCenterlineHandle") != std::string::npos);
    CHECK(source.find("profileVerticalCurveHandle") != std::string::npos);
    CHECK(source.find("sampleInterval") != std::string::npos);
    CHECK(source.find("assignmentCount") != std::string::npos);
    CHECK(source.find("RoadProtoRoadModelDialog_") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_SHOW_WPF_DIALOG") != std::string::npos);
}
```

Call it in `main()` after `roadModelWpfBridgeSourceContainsRequiredFields();`.

- [ ] **Step 2: Run test to verify it fails**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: test fails because bridge file does not exist.

- [ ] **Step 3: Implement bridge header**

Create `RoadModelDialogBridge.h`:

```cpp
#pragma once

#include "domain/cross_section/RoadModel.h"

#include <string>

namespace roadproto::cad_adapter::objectarx::cross_section {

struct RoadModelDialogRequest {
    std::wstring handle;
    std::wstring responsePath;
    std::wstring roadCenterlineHandle;
    std::wstring profileVerticalCurveHandle;
    double sampleInterval = 10.0;
    std::vector<domain::cross_section::RoadModelTemplateAssignment> assignments;
};

struct RoadModelDialogResponse {
    bool accepted = false;
    std::wstring handle;
    std::wstring roadCenterlineHandle;
    std::wstring profileVerticalCurveHandle;
    double sampleInterval = 10.0;
    std::vector<domain::cross_section::RoadModelTemplateAssignment> assignments;
};

bool queueRoadModelWpfDialog(const RoadModelDialogRequest& request, std::wstring& errorMessage);
bool readRoadModelDialogResponse(
    const std::wstring& path,
    RoadModelDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
```

- [ ] **Step 4: Implement bridge cpp**

Create `RoadModelDialogBridge.cpp` by following `SubgradeTemplateDialogBridge.cpp` patterns. Use these functions and keys:

```cpp
writeKeyValue(stream, L"handle", request.handle);
writeKeyValue(stream, L"responsePath", responsePath);
writeKeyValue(stream, L"roadCenterlineHandle", request.roadCenterlineHandle);
writeKeyValue(stream, L"profileVerticalCurveHandle", request.profileVerticalCurveHandle);
writeKeyValue(stream, L"sampleInterval", request.sampleInterval);
writeKeyValue(stream, L"assignmentCount", static_cast<int>(request.assignments.size()));
```

For rows:

```cpp
const auto prefix = L"assignment." + std::to_wstring(i) + L".";
writeKeyValue(stream, prefix + L"startStation", row.startStation);
writeKeyValue(stream, prefix + L"endStation", row.endStation);
writeKeyValue(stream, prefix + L"templateHandle", row.templateHandle);
writeKeyValue(stream, prefix + L"templateName", row.templateName);
```

Pending file name:

```cpp
RoadProtoRoadModelDialog_<pid>.pending
```

Queued command:

```cpp
acedCommandS(RTSTR, L"RD_SECTION_ROAD_MODEL_SHOW_WPF_DIALOG", RTNONE);
```

Response reader must parse the same fields and set `accepted`.

Add `RoadModelDialogBridge.cpp` to `src/app/RoadProtoArx.vcxproj`. It is not needed in `tests/RoadProtoCoreTests.vcxproj` because the test reads source text.

- [ ] **Step 5: Run tests**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all core tests pass.

- [ ] **Step 6: Commit**

```powershell
git add tests/core_tests.cpp src/app/RoadProtoArx.vcxproj src/cad_adapter/objectarx/cross_section/RoadModelDialogBridge.h src/cad_adapter/objectarx/cross_section/RoadModelDialogBridge.cpp
git commit -m "feat: add road model dialog bridge"
```

---

### Task 8: DnRoadModelEntity

**Files:**
- Create: `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.h`
- Create: `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `src/app/arx_entry/RoadProtoArxEntry.cpp`

- [ ] **Step 1: Add entity header**

Create `DnRoadModelEntity.h`:

```cpp
#pragma once

#include "domain/cross_section/RoadModel.h"

#include "dbents.h"
#include "dbmain.h"

class DnRoadModelEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnRoadModelEntity);

    DnRoadModelEntity();

    void setRoadModelData(const roadproto::domain::cross_section::RoadModelData& data);
    const roadproto::domain::cross_section::RoadModelData& roadModelData() const;

    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* filer) override;
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* filer) const override;

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* worldDraw) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& transform) override;

private:
    roadproto::domain::cross_section::RoadModelData data_;
};

namespace roadproto::cad_adapter::objectarx {

void initializeRoadModelEntityClass();
void uninitializeRoadModelEntityClass();

} // namespace roadproto::cad_adapter::objectarx
```

- [ ] **Step 2: Add entity implementation**

Create `DnRoadModelEntity.cpp`:

```cpp
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"

#include "acgi.h"
#include "dbproxy.h"

using roadproto::domain::cross_section::RoadModelComponentLine;
using roadproto::domain::cross_section::RoadModelData;
using roadproto::domain::cross_section::RoadModelPoint3d;

ACRX_DXF_DEFINE_MEMBERS(
    DnRoadModelEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kNoOperation,
    DNROADMODELENTITY,
    RoadProto);

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;

std::wstring readWideString(AcDbDwgFiler* filer)
{
    const ACHAR* value = nullptr;
    filer->readString(&value);
    return value == nullptr ? L"" : std::wstring(value);
}

void writeWideString(AcDbDwgFiler* filer, const std::wstring& value)
{
    filer->writeString(value.c_str());
}

void readPoint(AcDbDwgFiler* filer, RoadModelPoint3d& point)
{
    filer->readDouble(&point.x);
    filer->readDouble(&point.y);
    filer->readDouble(&point.z);
}

void writePoint(AcDbDwgFiler* filer, const RoadModelPoint3d& point)
{
    filer->writeDouble(point.x);
    filer->writeDouble(point.y);
    filer->writeDouble(point.z);
}

} // namespace

DnRoadModelEntity::DnRoadModelEntity() = default;

void DnRoadModelEntity::setRoadModelData(const RoadModelData& data)
{
    assertWriteEnabled();
    data_ = data;
}

const RoadModelData& DnRoadModelEntity::roadModelData() const
{
    return data_;
}

Acad::ErrorStatus DnRoadModelEntity::dwgInFields(AcDbDwgFiler* filer)
{
    auto status = AcDbEntity::dwgInFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    Adesk::Int16 version = 0;
    filer->readInt16(&version);
    data_ = RoadModelData{};
    data_.version = version;
    data_.config.roadCenterlineHandle = readWideString(filer);
    data_.config.profileVerticalCurveHandle = readWideString(filer);
    filer->readDouble(&data_.config.sampleInterval);

    Adesk::Int32 assignmentCount = 0;
    filer->readInt32(&assignmentCount);
    for (Adesk::Int32 i = 0; i < assignmentCount; ++i) {
        roadproto::domain::cross_section::RoadModelTemplateAssignment row;
        filer->readDouble(&row.startStation);
        filer->readDouble(&row.endStation);
        row.templateHandle = readWideString(filer);
        row.templateName = readWideString(filer);
        data_.config.assignments.push_back(std::move(row));
    }

    Adesk::Int32 lineCount = 0;
    filer->readInt32(&lineCount);
    for (Adesk::Int32 i = 0; i < lineCount; ++i) {
        RoadModelComponentLine line;
        line.key.templateHandle = readWideString(filer);
        Adesk::Int32 side = 0;
        Adesk::Int32 type = 0;
        Adesk::Int32 componentIndex = 0;
        Adesk::Int32 boundaryIndex = 0;
        filer->readInt32(&side);
        filer->readInt32(&type);
        filer->readInt32(&componentIndex);
        filer->readInt32(&boundaryIndex);
        line.key.side = static_cast<roadproto::domain::cross_section::SubgradeSide>(side);
        line.key.componentType = static_cast<roadproto::domain::cross_section::SubgradeComponentType>(type);
        line.key.componentIndex = static_cast<std::size_t>(componentIndex);
        line.key.boundaryIndex = static_cast<std::size_t>(boundaryIndex);
        filer->readInt32(&line.color.r);
        filer->readInt32(&line.color.g);
        filer->readInt32(&line.color.b);

        Adesk::Int32 pointCount = 0;
        filer->readInt32(&pointCount);
        for (Adesk::Int32 pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
            RoadModelPoint3d point;
            readPoint(filer, point);
            line.points.push_back(point);
        }
        data_.componentLines.push_back(std::move(line));
    }

    return filer->filerStatus();
}

Acad::ErrorStatus DnRoadModelEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, data_.config.roadCenterlineHandle);
    writeWideString(filer, data_.config.profileVerticalCurveHandle);
    filer->writeDouble(data_.config.sampleInterval);

    filer->writeInt32(static_cast<Adesk::Int32>(data_.config.assignments.size()));
    for (const auto& row : data_.config.assignments) {
        filer->writeDouble(row.startStation);
        filer->writeDouble(row.endStation);
        writeWideString(filer, row.templateHandle);
        writeWideString(filer, row.templateName);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(data_.componentLines.size()));
    for (const auto& line : data_.componentLines) {
        writeWideString(filer, line.key.templateHandle);
        filer->writeInt32(static_cast<Adesk::Int32>(line.key.side));
        filer->writeInt32(static_cast<Adesk::Int32>(line.key.componentType));
        filer->writeInt32(static_cast<Adesk::Int32>(line.key.componentIndex));
        filer->writeInt32(static_cast<Adesk::Int32>(line.key.boundaryIndex));
        filer->writeInt32(line.color.r);
        filer->writeInt32(line.color.g);
        filer->writeInt32(line.color.b);
        filer->writeInt32(static_cast<Adesk::Int32>(line.points.size()));
        for (const auto& point : line.points) {
            writePoint(filer, point);
        }
    }

    return filer->filerStatus();
}

Adesk::Boolean DnRoadModelEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr) {
        return Adesk::kFalse;
    }

    for (const auto& line : data_.componentLines) {
        if (line.points.size() < 2) {
            continue;
        }
        AcCmColor color;
        color.setRGB(
            static_cast<Adesk::UInt8>(std::clamp(line.color.r, 0, 255)),
            static_cast<Adesk::UInt8>(std::clamp(line.color.g, 0, 255)),
            static_cast<Adesk::UInt8>(std::clamp(line.color.b, 0, 255)));
        worldDraw->subEntityTraits().setTrueColor(color);

        for (std::size_t i = 1; i < line.points.size(); ++i) {
            AcGePoint3d segment[2] = {
                {line.points[i - 1].x, line.points[i - 1].y, line.points[i - 1].z},
                {line.points[i].x, line.points[i].y, line.points[i].z},
            };
            worldDraw->geometry().polyline(2, segment);
        }
    }

    return Adesk::kTrue;
}

Acad::ErrorStatus DnRoadModelEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    bool hasPoint = false;
    for (const auto& line : data_.componentLines) {
        for (const auto& point : line.points) {
            const AcGePoint3d cadPoint(point.x, point.y, point.z);
            if (!hasPoint) {
                extents.set(cadPoint, cadPoint);
                hasPoint = true;
            } else {
                extents.addPoint(cadPoint);
            }
        }
    }
    return hasPoint ? Acad::eOk : Acad::eInvalidExtents;
}

Acad::ErrorStatus DnRoadModelEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    assertWriteEnabled();
    for (auto& line : data_.componentLines) {
        for (auto& point : line.points) {
            AcGePoint3d cadPoint(point.x, point.y, point.z);
            cadPoint.transformBy(transform);
            point.x = cadPoint.x;
            point.y = cadPoint.y;
            point.z = cadPoint.z;
        }
    }
    return Acad::eOk;
}

namespace roadproto::cad_adapter::objectarx {

void initializeRoadModelEntityClass()
{
    DnRoadModelEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeRoadModelEntityClass()
{
    deleteAcRxClass(DnRoadModelEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
```

Add missing includes after first build errors appear, such as `<algorithm>`, `<cassert>`, and utility headers.

- [ ] **Step 3: Wire entity initialization**

Find existing entity initialization/uninitialization calls in `src/app/arx_entry/RoadProtoArxEntry.cpp`. Add:

```cpp
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
```

Next to other custom entity init calls:

```cpp
roadproto::cad_adapter::objectarx::initializeRoadModelEntityClass();
```

Next to uninit calls:

```cpp
roadproto::cad_adapter::objectarx::uninitializeRoadModelEntityClass();
```

Add `DnRoadModelEntity.cpp` to `src/app/RoadProtoArx.vcxproj`.

- [ ] **Step 4: Build ARX Debug**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: Debug ARX builds. Fix compiler errors in the entity only.

- [ ] **Step 5: Commit**

```powershell
git add src/app/RoadProtoArx.vcxproj src/app/arx_entry/RoadProtoArxEntry.cpp src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.h src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.cpp
git commit -m "feat: add road model custom entity"
```

---

### Task 9: ObjectARX Road Model Commands

**Files:**
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`

- [ ] **Step 1: Implement common helpers**

In `ObjectArxRoadModelCommand.cpp`, replace stubs with ObjectARX implementation under `#ifndef ROADPROTO_TEST_BUILD`. Include:

```cpp
#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/cross_section/RoadModelBuildService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
#include "cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/RoadModelDialogBridge.h"
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <algorithm>
#include <cwctype>
#include <sstream>
#include <string>
#include <vector>
#endif
```

Add helper functions copied and adapted from profile/subgrade commands:

```cpp
bool appendEntityToModelSpace(AcDbEntity* entity, AcDbObjectId& entityId);
bool resolveObjectIdFromHandle(const std::wstring& handleText, AcDbObjectId& entityId);
std::wstring entityHandleText(AcDbEntity* entity);
template <typename TEntity> bool selectTypedEntity(AcDbObjectId& entityId);
bool readRoadCenterline(AcDbObjectId id, std::wstring& handle, std::vector<alignment::AlignmentSamplePoint>& samples);
bool readVerticalCurve(AcDbObjectId id, std::wstring& handle, profile::ProfileVerticalCurveData& data);
bool readSubgradeTemplate(const std::wstring& handle, RoadModelTemplateSource& source);
bool findUniqueVerticalCurveForCenterline(const std::wstring& centerlineHandle, AcDbObjectId& verticalCurveId);
bool queueDialogForRoadModelCreate(...);
bool queueDialogForRoadModelEdit(AcDbObjectId roadModelId);
```

Use `acedSSGet` to select typed entities, mirroring existing `selectTypedEntity` in `ObjectArxProfileVerticalCurveCommand.cpp`.

- [ ] **Step 2: Implement vertical-curve discovery**

In `findUniqueVerticalCurveForCenterline`:

1. Iterate model space.
2. Collect `DnProfileGradeGraphEntity` whose `graphData().roadCenterlineHandle == centerlineHandle`.
3. Collect `DnProfileVerticalCurveEntity` whose `curveData().profileGraphHandle` matches one of those graph handles.
4. If exactly one curve found, return it.
5. If zero or multiple found, return false so caller prompts user.

- [ ] **Step 3: Implement create command**

`runRoadModelCreateCommand()`:

```cpp
auto& editor = app::ApplicationContext::instance().editor();
editor.writeMessage(L"RD_SECTION_ROAD_MODEL_CREATE: 请选择道路中线。");

AcDbObjectId centerlineId;
if (!selectTypedEntity<DnRoadCenterlineEntity>(centerlineId)) {
    editor.writeWarning(L"未选择道路中线实体。");
    return;
}

std::wstring centerlineHandle;
std::vector<alignment::AlignmentSamplePoint> alignmentSamples;
if (!readRoadCenterline(centerlineId, centerlineHandle, alignmentSamples)) {
    editor.writeError(L"无法读取道路中线数据。");
    return;
}

AcDbObjectId verticalCurveId;
if (!findUniqueVerticalCurveForCenterline(centerlineHandle, verticalCurveId)) {
    editor.writeMessage(L"未找到唯一关联竖曲线，请选择竖曲线实体。");
    if (!selectTypedEntity<DnProfileVerticalCurveEntity>(verticalCurveId)) {
        editor.writeWarning(L"横断面戴帽已取消。");
        return;
    }
}

std::wstring verticalCurveHandle;
profile::ProfileVerticalCurveData verticalCurve;
if (!readVerticalCurve(verticalCurveId, verticalCurveHandle, verticalCurve)) {
    editor.writeError(L"无法读取竖曲线数据。");
    return;
}

RoadModelDialogRequest request;
request.roadCenterlineHandle = centerlineHandle;
request.profileVerticalCurveHandle = verticalCurveHandle;
request.sampleInterval = 10.0;
queueRoadModelWpfDialog(request, errorMessage);
```

The create command does not build the model until WPF response arrives.

- [ ] **Step 4: Implement edit commands**

`runRoadModelEditCommand()` selects `DnRoadModelEntity`, then calls `queueDialogForRoadModelEdit`.

`runRoadModelEditHandleCommand()` reads handle with `acedGetString`, resolves to `DnRoadModelEntity`, then calls `queueDialogForRoadModelEdit`.

`queueDialogForRoadModelEdit` reads `entity->roadModelData().config` into `RoadModelDialogRequest`.

- [ ] **Step 5: Implement apply command**

`runRoadModelApplyDialogFileCommand()`:

1. Read response path.
2. Read `RoadModelDialogResponse`.
3. If not accepted, return.
4. Resolve centerline and vertical curve handles.
5. Read alignment samples and vertical curve data.
6. For each assignment handle, open `DnSubgradeTemplateEntity` and collect template data.
7. Build `RoadModelBuildInput`.
8. Call `RoadModelBuildService`.
9. If response handle empty, create new `DnRoadModelEntity` and append to model space.
10. If response handle set, open existing entity for write and replace data.
11. `recordGraphicsModified(true)`, close entity, `acedUpdateDisplay()`.

- [ ] **Step 6: Build ARX Debug**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: Debug ARX builds. Fix command compile errors without changing domain behavior.

- [ ] **Step 7: Commit**

```powershell
git add src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp
git commit -m "feat: implement road model commands"
```

---

### Task 10: Business Docs, Reuse Docs, Version, README

**Files:**
- Add: `docs/business/cross_section/横断面戴帽_道路模型创建.md`
- Add: `docs/business/cross_section/道路模型_编辑.md`
- Add: `docs/business/cross_section/道路模型_WPF桥接回写.md`
- Add: `docs/reuse/road_model.md`
- Modify: `README.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/modules/module_index.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`
- Modify: `build/RoadProto.Build.props`
- Modify: `src/core/version/VersionInfo.cpp`

- [ ] **Step 1: Update version files**

In `build/RoadProto.Build.props`:

```xml
<RoadProtoVersion>v0.1.11</RoadProtoVersion>
<RoadProtoBuildDate>20260518</RoadProtoBuildDate>
<RoadProtoStage>RoadModel</RoadProtoStage>
```

In `VersionInfo.cpp`, update the matching runtime version fields to `v0.1.11`, `20260518`, and `RoadModel`.

- [ ] **Step 2: Add business docs**

Create `横断面戴帽_道路模型创建.md` with:

```markdown
# 横断面戴帽道路模型创建

## 基本信息

- 功能名称：横断面戴帽道路模型创建
- 所属模块：横断面设计
- 命令名称：`RD_SECTION_ROAD_MODEL_CREATE`
- 对应代码入口：`src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- 原型版本：`v0.1.11`
- 是否可复用：部分可复用

## 功能背景

本功能用于把道路中线、纵断面竖曲线和已有路基模板按桩号范围组合，生成三维道路模型线框。

## 输入条件

- CAD 选择对象：`DnRoadCenterlineEntity`
- 已有设计实体：关联或用户选择的 `DnProfileVerticalCurveEntity`
- 用户输入参数：模型采样间距、路基模板优先级表
- 模板来源：已有 `DnSubgradeTemplateEntity`

## 输出结果

- CAD 图形实体：`DnRoadModelEntity`
- 领域实体：道路模型配置和三维部件线
- 更新通知或重建请求：第一版不自动接入关系重建

## 操作流程

1. 运行 `RD_SECTION_ROAD_MODEL_CREATE`。
2. 选择道路中线。
3. 系统自动查找同一道路中线关联的竖曲线；找不到唯一对象时提示用户选择竖曲线。
4. 打开“横断面戴帽”WPF 窗口。
5. 在 `路基模板` tab 中填写桩号范围和已有路基模板 handle。
6. 调整表格行顺序表达优先级。
7. 点击“生成模型”。
8. 系统创建 `DnRoadModelEntity` 并绘制三维部件线。

## 关键业务规则

- 表格行越靠上优先级越高。
- 行范围可短于或长于道路中线，生成时只取有效交集。
- 未命中任何模板的中线桩号段不生成。
- 每个路基部件至少生成对应三维线。
```

Create edit and bridge docs with the same business template sections and command names `RD_SECTION_ROAD_MODEL_EDIT`, `RD_SECTION_ROAD_MODEL_EDIT_HANDLE`, `RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE`.

- [ ] **Step 3: Add reuse doc**

Create `docs/reuse/road_model.md`:

```markdown
# 道路模型复用说明

## 能力分类

通用道路设计能力

## 能力说明

道路模型能力把道路中线、竖曲线设计高程和横断面路基模板合成为三维部件线，可复用于后续三维模型、出图、算量和道路对象联动。

## 当前实现

- 源码路径：`src/domain/cross_section/RoadModel.*`
- 对外类型/函数：`RoadModelBuilder`、`RoadModelStationSampler`、`RoadModelTemplateResolver`
- 当前使用该能力的命令：`RD_SECTION_ROAD_MODEL_CREATE`、`RD_SECTION_ROAD_MODEL_EDIT`

## 可复用内容

- 模板优先级解析。
- 道路模型采样桩号收集。
- 横断面模板三维展开。
- 三维部件线数据结构。

## 不可复用或临时内容

- WPF 请求/响应文件桥接属于原型阶段 UI 解耦方式。
- 第一版仅生成线框，不生成面、实体体或算量。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- AutoCAD 手工验证：创建、编辑、双击编辑、保存重开和 `REGEN`
```

- [ ] **Step 4: Update module docs and README**

Update:

- `README.md`: current version, command list, cross-section module description, ARX load example.
- `docs/modules/cross_section.md`: command table, Ribbon entries, code landing table, boundaries.
- `docs/modules/module_index.md`: cross-section status.
- `docs/reuse/capability_catalog.md`: add road model domain/application/entity/bridge rows.
- `tests/README.md`: add V0.1.11 automated and manual validation.
- `docs/dev/version_log.md`: add v0.1.11 entry with truthful build validation status; if verification is still pending at Task 10, state that final validation is scheduled in Task 11.

- [ ] **Step 5: Run docs-related tests**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all core tests pass.

- [ ] **Step 6: Commit**

```powershell
git add README.md docs tests/README.md build/RoadProto.Build.props src/core/version/VersionInfo.cpp
git commit -m "docs: document road model workflow"
```

---

### Task 11: Full Build Verification And Main Directory Sync

**Files:**
- Modify only if verification exposes compile issues.

- [ ] **Step 1: Run core test build and executable**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected:

```text
All RoadProto core tests passed.
```

- [ ] **Step 2: Run WPF Debug and Release builds**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

Expected: both builds succeed with 0 errors.

- [ ] **Step 3: Run ARX Debug and Release builds**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' RoadProto.sln /p:Configuration=Debug /p:Platform=x64
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

Expected:

- `artifacts/x64/Debug/RoadProto_v0.1.11_20260518_RoadModel.arx`
- `artifacts/x64/Release/RoadProto_v0.1.11_20260518_RoadModel.arx`
- `artifacts/managed/Debug/net48/RoadProto.Terrain.UI.dll`
- `artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll`

- [ ] **Step 4: Update version log build validation**

If all builds pass, update `docs/dev/version_log.md` v0.1.11 validation lines with exact results. If any build cannot run because local dependencies are missing, record the exact command and error in the final response and leave the version log entry truthful.

- [ ] **Step 5: Check git status**

Run:

```powershell
git status --short
```

Expected: only intentional files are modified. No `.vs/`, `bin/`, `obj/`, or temporary files are staged.

- [ ] **Step 6: Commit verification/doc adjustment**

If Step 4 changed the version log:

```powershell
git add docs/dev/version_log.md
git commit -m "docs: record road model build validation"
```

- [ ] **Step 7: Main-directory sync**

This work is currently planned in the main project directory `F:\0_GPT_道路设计原型功能项目`. If implementation is executed in a `.worktrees/<branch>` directory, sync the source/document scope back to the main directory before final response:

```powershell
robocopy <worktree>\src F:\0_GPT_道路设计原型功能项目\src /MIR /XD bin obj
robocopy <worktree>\tests F:\0_GPT_道路设计原型功能项目\tests /MIR /XD bin obj
robocopy <worktree>\docs F:\0_GPT_道路设计原型功能项目\docs /MIR
robocopy <worktree>\build F:\0_GPT_道路设计原型功能项目\build /MIR
robocopy <worktree>\assets F:\0_GPT_道路设计原型功能项目\assets /MIR
robocopy <worktree>\third_party F:\0_GPT_道路设计原型功能项目\third_party /MIR
copy <worktree>\README.md F:\0_GPT_道路设计原型功能项目\README.md
copy <worktree>\RoadProto.sln F:\0_GPT_道路设计原型功能项目\RoadProto.sln
```

If build artifacts are produced in a worktree and the user needs main-directory load paths, also sync:

```powershell
robocopy <worktree>\artifacts\x64\Debug F:\0_GPT_道路设计原型功能项目\artifacts\x64\Debug RoadProto_v0.1.11_20260518_RoadModel.arx *.pdb
robocopy <worktree>\artifacts\x64\Release F:\0_GPT_道路设计原型功能项目\artifacts\x64\Release RoadProto_v0.1.11_20260518_RoadModel.arx *.pdb
robocopy <worktree>\artifacts\managed\Debug\net48 F:\0_GPT_道路设计原型功能项目\artifacts\managed\Debug\net48 RoadProto.Terrain.UI.dll *.pdb
robocopy <worktree>\artifacts\managed\Release\net48 F:\0_GPT_道路设计原型功能项目\artifacts\managed\Release\net48 RoadProto.Terrain.UI.dll *.pdb
```

- [ ] **Step 8: Final status**

Final response must include:

- Commands run and pass/fail status.
- ARX path: `F:\0_GPT_道路设计原型功能项目\artifacts\x64\Release\RoadProto_v0.1.11_20260518_RoadModel.arx`
- Managed DLL path: `F:\0_GPT_道路设计原型功能项目\artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll`
- Any AutoCAD manual checks not performed.
