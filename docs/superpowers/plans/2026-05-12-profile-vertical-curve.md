# Profile Vertical Curve Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build an independent profile vertical curve design entity that attaches to a profile grade graph, supports design elevation calculation, grips, right-click PVI editing, and WPF double-click editing.

**Architecture:** Keep all vertical curve math in `src/domain/profile` and flow orchestration in `src/application/profile`. ObjectARX code owns entity selection, custom entity drawing, persistence, grips, and commands; WPF owns only parameter display and response-file writing. `DnProfileVerticalCurveEntity` stores a grade graph handle and its own design data, so the grade graph remains a ground-line drawing surface.

**Tech Stack:** C++17, ObjectARX 2021, AutoCAD 2021 x64, .NET Framework 4.8 WPF, existing request/response-file Bridge pattern, existing `RoadProtoCoreTests.vcxproj`.

---

## Scope And Order

Implement in this order:

1. Domain model and default create service.
2. Domain calculator for straight lines and one or more PVIs.
3. Domain edit service for grips and PVI add/delete.
4. PROFILE module command metadata and build project wiring.
5. ObjectARX custom entity with drawing, persistence, grip editing, and class registration.
6. ObjectARX create/edit/add/delete/apply commands.
7. WPF Bridge DTO/file protocol and edit window.
8. Managed Ribbon double-click/right-click/Ribbon integration.
9. Business, reuse, module, version, README, and test documentation.
10. Full build/test/manual verification notes.

The implementation should happen in `F:\0_GPT_道路设计原型功能项目\.worktrees\profile-grade-graph` on branch `codex/profile-grade-graph`. Because this is a worktree, every changed documentation file under `docs/`, `README.md`, and `tests/README.md` must be mirrored to the same relative path in `F:\0_GPT_道路设计原型功能项目` before finish.

## File Structure

Create:

- `src/domain/profile/ProfileVerticalCurveModel.h`: pure domain data structures.
- `src/domain/profile/ProfileVerticalCurveCalculator.h`: pure domain calculation API.
- `src/domain/profile/ProfileVerticalCurveCalculator.cpp`: rebuild, query, sampling, and primitive edit math.
- `src/application/profile/ProfileVerticalCurveCreateService.h`: default create use case input/output.
- `src/application/profile/ProfileVerticalCurveCreateService.cpp`: build a design line from grade graph samples.
- `src/application/profile/ProfileVerticalCurveEditService.h`: dialog/grip/add/delete use case input/output.
- `src/application/profile/ProfileVerticalCurveEditService.cpp`: edit orchestration around calculator.
- `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h`: ObjectARX custom entity declaration.
- `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.cpp`: drawing, DWG persistence, grips, transform, class registration.
- `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.h`: command procedure declarations.
- `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.cpp`: create/edit/apply/add/delete command bodies.
- `src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.h`: C++ request/response DTO and bridge functions.
- `src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.cpp`: temp-file bridge implementation.
- `src/ui/wpf/RoadProto.Terrain.UI/ProfileVerticalCurveWindow.xaml`: WPF edit window.
- `src/ui/wpf/RoadProto.Terrain.UI/ProfileVerticalCurveWindow.xaml.cs`: WPF edit logic.
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileVerticalCurveDialogDtos.cs`: managed DTOs.
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileVerticalCurveDialogFile.cs`: managed request/response parser.
- `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/ProfileVerticalCurveDialogCommands.cs`: `RD_PROFILE_VERTICAL_CURVE_SHOW_WPF_DIALOG`.
- `docs/business/profile/竖曲线_创建.md`: create command business doc.
- `docs/business/profile/竖曲线_编辑.md`: double-click/WPF edit business doc.
- `docs/business/profile/竖曲线_夹点与右键编辑.md`: grip and context-menu business doc.
- `docs/reuse/profile_vertical_curve.md`: reusable vertical curve calculation/entity/Bridge capability.

Modify:

- `tests/core_tests.cpp`: vertical curve tests and command metadata checks.
- `tests/RoadProtoCoreTests.vcxproj`: include new domain/application/command source files.
- `src/app/RoadProtoArx.vcxproj`: include new domain/application/CAD source and headers.
- `src/app/arx_entry/RoadProtoArxEntry.cpp`: initialize/uninitialize `DnProfileVerticalCurveEntity`.
- `src/modules/profile/ProfileModule.cpp`: register five new commands.
- `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h`: expose drawing frame helper access.
- `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.cpp`: implement drawing frame helper.
- `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`: Ribbon button, double-click detection, context menu.
- `docs/modules/profile.md`: command and code landing update.
- `docs/modules/module_index.md`: status update.
- `docs/reuse/capability_catalog.md`: reusable capability update.
- `docs/dev/version_log.md`: version entry after a successful build.
- `tests/README.md`: core and manual verification update.
- `README.md`: V0.1 feature summary update after build.

---

### Task 1: Domain Model And Default Create Service

**Files:**
- Create: `src/domain/profile/ProfileVerticalCurveModel.h`
- Create: `src/application/profile/ProfileVerticalCurveCreateService.h`
- Create: `src/application/profile/ProfileVerticalCurveCreateService.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write failing tests for default creation**

Add these includes near the existing profile includes in `tests/core_tests.cpp`:

```cpp
#include "application/profile/ProfileVerticalCurveCreateService.h"
#include "domain/profile/ProfileVerticalCurveModel.h"
```

Add these test functions before `main()`:

```cpp
void profileVerticalCurveModelDefaultsToDesignLine()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    CHECK(data.version == 1);
    CHECK(data.properties.name == L"\u7ad6\u66f2\u7ebf");
    CHECK(data.properties.designLineColorIndex == 1);
    CHECK(data.properties.sampleInterval == 5.0);
    CHECK(data.controlPoints.empty());
    CHECK(data.pvis.empty());
}

void profileVerticalCurveCreateServiceBuildsDefaultLineFromGraphSamples()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveCreateInput input;
    input.profileGraphHandle = L"ABCD";
    input.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 24.5},
        ProfileGroundSample{150.0, 26.0},
    };

    const ProfileVerticalCurveCreateService service;
    const auto result = service.buildDefaultFromGraph(input);

    CHECK(result.succeeded);
    CHECK(result.errorMessage.empty());
    CHECK(result.data.profileGraphHandle == L"ABCD");
    CHECK(result.data.controlPoints.size() == 2);
    CHECK(result.data.controlPoints[0].role == VerticalCurvePointRole::Start);
    CHECK(std::fabs(result.data.controlPoints[0].station - 100.0) < 1e-9);
    CHECK(std::fabs(result.data.controlPoints[0].elevation - 23.5) < 1e-9);
    CHECK(result.data.controlPoints[1].role == VerticalCurvePointRole::End);
    CHECK(std::fabs(result.data.controlPoints[1].station - 150.0) < 1e-9);
    CHECK(std::fabs(result.data.controlPoints[1].elevation - 26.0) < 1e-9);
    CHECK(result.data.pvis.empty());
}

void profileVerticalCurveCreateServiceRejectsTooFewGroundSamples()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveCreateInput input;
    input.profileGraphHandle = L"ABCD";
    input.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
    };

    const ProfileVerticalCurveCreateService service;
    const auto result = service.buildDefaultFromGraph(input);

    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());
}
```

Call the tests in `main()` after the existing profile grade graph create service tests:

```cpp
    profileVerticalCurveModelDefaultsToDesignLine();
    profileVerticalCurveCreateServiceBuildsDefaultLineFromGraphSamples();
    profileVerticalCurveCreateServiceRejectsTooFewGroundSamples();
```

- [ ] **Step 2: Run the focused test build and confirm RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `application/profile/ProfileVerticalCurveCreateService.h` or `domain/profile/ProfileVerticalCurveModel.h` does not exist.

- [ ] **Step 3: Add the domain model**

Create `src/domain/profile/ProfileVerticalCurveModel.h`:

```cpp
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace roadproto::domain::profile {

enum class VerticalCurvePointRole {
    Start,
    Pvi,
    End,
};

enum class VerticalCurveType {
    TangentOnly,
    Crest,
    Sag,
};

enum class VerticalCurveGripRole {
    StartPoint,
    EndPoint,
    PviPoint,
    RadiusPoint,
};

struct VerticalCurveControlPoint {
    VerticalCurvePointRole role = VerticalCurvePointRole::Pvi;
    double station = 0.0;
    double elevation = 0.0;
};

struct VerticalCurvePvi {
    double station = 0.0;
    double elevation = 0.0;
    double radius = 1000.0;
    bool radiusLocked = false;
};

struct VerticalCurveProperties {
    std::wstring name = L"\u7ad6\u66f2\u7ebf";
    int designLineColorIndex = 1;
    int tangentLineColorIndex = 8;
    int keyPointColorIndex = 2;
    double designLineWidth = 0.35;
    double sampleInterval = 5.0;
    bool showLabels = true;
    bool showTangentLines = true;
};

struct VerticalCurveKeyPoint {
    double station = 0.0;
    double elevation = 0.0;
    bool isHighPoint = false;
};

struct VerticalCurveElement {
    std::size_t pviIndex = 0;
    VerticalCurveType type = VerticalCurveType::TangentOnly;
    double pviStation = 0.0;
    double pviElevation = 0.0;
    double i1 = 0.0;
    double i2 = 0.0;
    double gradeDifference = 0.0;
    double radius = 0.0;
    double length = 0.0;
    double tangentLength = 0.0;
    double bvcStation = 0.0;
    double bvcElevation = 0.0;
    double evcStation = 0.0;
    double evcElevation = 0.0;
    std::optional<VerticalCurveKeyPoint> highLowPoint;
};

struct VerticalCurveSamplePoint {
    double station = 0.0;
    double elevation = 0.0;
};

struct ProfileVerticalCurveData {
    std::wstring profileGraphHandle;
    std::vector<VerticalCurveControlPoint> controlPoints;
    std::vector<VerticalCurvePvi> pvis;
    VerticalCurveProperties properties;
    int version = 1;
};

} // namespace roadproto::domain::profile
```

- [ ] **Step 4: Add the create service**

Create `src/application/profile/ProfileVerticalCurveCreateService.h`:

```cpp
#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"
#include "domain/profile/ProfileVerticalCurveModel.h"

#include <string>
#include <vector>

namespace roadproto::application::profile {

struct ProfileVerticalCurveCreateInput {
    std::wstring profileGraphHandle;
    std::vector<domain::profile::ProfileGroundSample> groundSamples;
};

struct ProfileVerticalCurveCreateResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::profile::ProfileVerticalCurveData data;
};

class ProfileVerticalCurveCreateService {
public:
    ProfileVerticalCurveCreateResult buildDefaultFromGraph(const ProfileVerticalCurveCreateInput& input) const;
};

} // namespace roadproto::application::profile
```

Create `src/application/profile/ProfileVerticalCurveCreateService.cpp`:

```cpp
#include "application/profile/ProfileVerticalCurveCreateService.h"

#include <cmath>

namespace roadproto::application::profile {
namespace {

bool isFiniteSample(const domain::profile::ProfileGroundSample& sample)
{
    return std::isfinite(sample.station) && std::isfinite(sample.elevation);
}

} // namespace

ProfileVerticalCurveCreateResult ProfileVerticalCurveCreateService::buildDefaultFromGraph(
    const ProfileVerticalCurveCreateInput& input) const
{
    ProfileVerticalCurveCreateResult result;
    if (input.profileGraphHandle.empty()) {
        result.errorMessage = L"Profile grade graph handle is required.";
        return result;
    }
    if (input.groundSamples.size() < 2) {
        result.errorMessage = L"At least two profile ground samples are required.";
        return result;
    }

    const auto& first = input.groundSamples.front();
    const auto& last = input.groundSamples.back();
    if (!isFiniteSample(first) || !isFiniteSample(last) || last.station <= first.station) {
        result.errorMessage = L"Profile ground samples must provide finite increasing start and end stations.";
        return result;
    }

    result.data.profileGraphHandle = input.profileGraphHandle;
    result.data.controlPoints = {
        domain::profile::VerticalCurveControlPoint{
            domain::profile::VerticalCurvePointRole::Start,
            first.station,
            first.elevation},
        domain::profile::VerticalCurveControlPoint{
            domain::profile::VerticalCurvePointRole::End,
            last.station,
            last.elevation},
    };
    result.succeeded = true;
    return result;
}

} // namespace roadproto::application::profile
```

- [ ] **Step 5: Wire the new source into test and ARX projects**

In `tests/RoadProtoCoreTests.vcxproj`, add:

```xml
    <ClCompile Include="..\src\application\profile\ProfileVerticalCurveCreateService.cpp" />
```

Place it after the existing `ProfileGradeGraphCreateService.cpp` entry.

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\application\profile\ProfileVerticalCurveCreateService.cpp" />
```

Place it after `ProfileGradeGraphCreateService.cpp`.

- [ ] **Step 6: Run tests and confirm GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: build succeeds and test exe prints a success message with no assertion failure.

- [ ] **Step 7: Commit Task 1**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj src/domain/profile/ProfileVerticalCurveModel.h src/application/profile/ProfileVerticalCurveCreateService.h src/application/profile/ProfileVerticalCurveCreateService.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add profile vertical curve defaults"
```

---

### Task 2: Vertical Curve Calculator

**Files:**
- Create: `src/domain/profile/ProfileVerticalCurveCalculator.h`
- Create: `src/domain/profile/ProfileVerticalCurveCalculator.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write failing calculator tests**

Add include:

```cpp
#include "domain/profile/ProfileVerticalCurveCalculator.h"
```

Add tests:

```cpp
void profileVerticalCurveCalculatorInterpolatesStraightLine()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 100.0, 20.0},
        {VerticalCurvePointRole::End, 200.0, 30.0},
    };

    const auto built = ProfileVerticalCurveCalculator::rebuild(data);
    CHECK(built.succeeded);
    CHECK(built.elements.empty());
    const auto elevation = ProfileVerticalCurveCalculator::elevationAt(built, 150.0);
    CHECK(elevation.succeeded);
    CHECK(std::fabs(elevation.value - 25.0) < 1e-9);
    const auto grade = ProfileVerticalCurveCalculator::gradeAt(built, 150.0);
    CHECK(grade.succeeded);
    CHECK(std::fabs(grade.value - 0.1) < 1e-9);
}

void profileVerticalCurveCalculatorBuildsSingleCrestCurve()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::Pvi, 100.0, 10.0},
        {VerticalCurvePointRole::End, 200.0, 10.0},
    };
    data.pvis = {
        VerticalCurvePvi{100.0, 10.0, 1000.0},
    };

    const auto built = ProfileVerticalCurveCalculator::rebuild(data);
    CHECK(built.succeeded);
    CHECK(built.elements.size() == 1);
    CHECK(built.elements[0].type == VerticalCurveType::Crest);
    CHECK(std::fabs(built.elements[0].i1 - 0.1) < 1e-9);
    CHECK(std::fabs(built.elements[0].i2 - 0.0) < 1e-9);
    CHECK(std::fabs(built.elements[0].gradeDifference + 0.1) < 1e-9);
    CHECK(std::fabs(built.elements[0].length - 100.0) < 1e-9);
    CHECK(std::fabs(built.elements[0].tangentLength - 50.0) < 1e-9);
    CHECK(std::fabs(built.elements[0].bvcStation - 50.0) < 1e-9);
    CHECK(std::fabs(built.elements[0].evcStation - 150.0) < 1e-9);
    CHECK(built.elements[0].highLowPoint.has_value());
    CHECK(built.elements[0].highLowPoint->isHighPoint);
}

void profileVerticalCurveCalculatorSamplesBeyondGradeGraphRange()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, -20.0, 10.0},
        {VerticalCurvePointRole::End, 120.0, 24.0},
    };
    data.properties.sampleInterval = 25.0;

    const auto samples = ProfileVerticalCurveCalculator::sample(data, data.properties.sampleInterval);
    CHECK(samples.succeeded);
    CHECK(samples.points.size() >= 2);
    CHECK(std::fabs(samples.points.front().station + 20.0) < 1e-9);
    CHECK(std::fabs(samples.points.back().station - 120.0) < 1e-9);
}
```

Call them in `main()` after Task 1 vertical curve tests:

```cpp
    profileVerticalCurveCalculatorInterpolatesStraightLine();
    profileVerticalCurveCalculatorBuildsSingleCrestCurve();
    profileVerticalCurveCalculatorSamplesBeyondGradeGraphRange();
```

- [ ] **Step 2: Run tests and confirm RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `ProfileVerticalCurveCalculator.h` does not exist.

- [ ] **Step 3: Add calculator API**

Create `src/domain/profile/ProfileVerticalCurveCalculator.h`:

```cpp
#pragma once

#include "domain/profile/ProfileVerticalCurveModel.h"

#include <string>
#include <vector>

namespace roadproto::domain::profile {

struct ProfileVerticalCurveBuildResult {
    bool succeeded = false;
    std::wstring errorMessage;
    std::vector<std::wstring> warnings;
    std::vector<VerticalCurveControlPoint> orderedControlPoints;
    std::vector<VerticalCurveElement> elements;
};

struct ProfileVerticalCurveQueryResult {
    bool succeeded = false;
    std::wstring errorMessage;
    double value = 0.0;
};

struct ProfileVerticalCurveSampleResult {
    bool succeeded = false;
    std::wstring errorMessage;
    std::vector<VerticalCurveSamplePoint> points;
};

struct ProfileVerticalCurveEditResult {
    bool succeeded = false;
    bool changed = false;
    std::wstring errorMessage;
};

class ProfileVerticalCurveCalculator final {
public:
    static ProfileVerticalCurveBuildResult rebuild(const ProfileVerticalCurveData& data);
    static ProfileVerticalCurveQueryResult elevationAt(const ProfileVerticalCurveBuildResult& built, double station);
    static ProfileVerticalCurveQueryResult gradeAt(const ProfileVerticalCurveBuildResult& built, double station);
    static ProfileVerticalCurveSampleResult sample(const ProfileVerticalCurveData& data, double interval);
    static ProfileVerticalCurveEditResult moveControlPoint(
        ProfileVerticalCurveData& data,
        std::size_t pointIndex,
        double station,
        double elevation);
    static ProfileVerticalCurveEditResult movePvi(
        ProfileVerticalCurveData& data,
        std::size_t pviIndex,
        double station,
        double elevation);
    static ProfileVerticalCurveEditResult updateRadius(
        ProfileVerticalCurveData& data,
        std::size_t pviIndex,
        double radius);
    static ProfileVerticalCurveEditResult insertPvi(
        ProfileVerticalCurveData& data,
        double station,
        double elevation,
        double radius);
    static ProfileVerticalCurveEditResult removePvi(ProfileVerticalCurveData& data, std::size_t pviIndex);
};

} // namespace roadproto::domain::profile
```

- [ ] **Step 4: Add calculator implementation**

Create `src/domain/profile/ProfileVerticalCurveCalculator.cpp` with:

```cpp
#include "domain/profile/ProfileVerticalCurveCalculator.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace roadproto::domain::profile {
namespace {

constexpr double kStationEpsilon = 1e-8;
constexpr double kGradeEpsilon = 1e-12;
constexpr double kMinRadius = 1.0;

bool isFinitePoint(const VerticalCurveControlPoint& point)
{
    return std::isfinite(point.station) && std::isfinite(point.elevation);
}

bool isFinitePvi(const VerticalCurvePvi& pvi)
{
    return std::isfinite(pvi.station) && std::isfinite(pvi.elevation)
        && std::isfinite(pvi.radius) && pvi.radius >= kMinRadius;
}

double linearElevation(const VerticalCurveControlPoint& a, const VerticalCurveControlPoint& b, double station)
{
    const auto span = b.station - a.station;
    if (std::fabs(span) <= kStationEpsilon) {
        return a.elevation;
    }
    const auto t = (station - a.station) / span;
    return a.elevation + (b.elevation - a.elevation) * t;
}

double linearGrade(const VerticalCurveControlPoint& a, const VerticalCurveControlPoint& b)
{
    const auto span = b.station - a.station;
    if (std::fabs(span) <= kStationEpsilon) {
        return 0.0;
    }
    return (b.elevation - a.elevation) / span;
}

std::vector<VerticalCurveControlPoint> orderedPoints(const ProfileVerticalCurveData& data)
{
    auto points = data.controlPoints;
    if (points.size() == 2 && !data.pvis.empty()) {
        points.reserve(data.pvis.size() + 2);
        for (const auto& pvi : data.pvis) {
            points.push_back(VerticalCurveControlPoint{VerticalCurvePointRole::Pvi, pvi.station, pvi.elevation});
        }
    }
    std::sort(points.begin(), points.end(), [](const auto& left, const auto& right) {
        if (std::fabs(left.station - right.station) > kStationEpsilon) {
            return left.station < right.station;
        }
        return static_cast<int>(left.role) < static_cast<int>(right.role);
    });
    if (!points.empty()) {
        points.front().role = VerticalCurvePointRole::Start;
        points.back().role = VerticalCurvePointRole::End;
        for (std::size_t i = 1; i + 1 < points.size(); ++i) {
            points[i].role = VerticalCurvePointRole::Pvi;
        }
    }
    return points;
}

const VerticalCurvePvi* findPviForPoint(const ProfileVerticalCurveData& data, const VerticalCurveControlPoint& point)
{
    for (const auto& pvi : data.pvis) {
        if (std::fabs(pvi.station - point.station) <= kStationEpsilon
            && std::fabs(pvi.elevation - point.elevation) <= 1e-8) {
            return &pvi;
        }
    }
    return nullptr;
}

ProfileVerticalCurveQueryResult makeFailure(const std::wstring& message)
{
    ProfileVerticalCurveQueryResult result;
    result.errorMessage = message;
    return result;
}

} // namespace

ProfileVerticalCurveBuildResult ProfileVerticalCurveCalculator::rebuild(const ProfileVerticalCurveData& data)
{
    ProfileVerticalCurveBuildResult result;
    result.orderedControlPoints = orderedPoints(data);
    if (result.orderedControlPoints.size() < 2) {
        result.errorMessage = L"At least start and end control points are required.";
        return result;
    }
    for (const auto& point : result.orderedControlPoints) {
        if (!isFinitePoint(point)) {
            result.errorMessage = L"Vertical curve control points must use finite station and elevation values.";
            return result;
        }
    }
    for (std::size_t i = 1; i < result.orderedControlPoints.size(); ++i) {
        if (result.orderedControlPoints[i].station <= result.orderedControlPoints[i - 1].station + kStationEpsilon) {
            result.errorMessage = L"Vertical curve control point stations must be strictly increasing.";
            return result;
        }
    }
    for (const auto& pvi : data.pvis) {
        if (!isFinitePvi(pvi)) {
            result.errorMessage = L"Vertical curve PVI radius, station, and elevation must be finite and radius must be positive.";
            return result;
        }
    }

    for (std::size_t i = 1; i + 1 < result.orderedControlPoints.size(); ++i) {
        const auto& previous = result.orderedControlPoints[i - 1];
        const auto& pviPoint = result.orderedControlPoints[i];
        const auto& next = result.orderedControlPoints[i + 1];
        const auto* pvi = findPviForPoint(data, pviPoint);
        if (pvi == nullptr) {
            continue;
        }

        const auto i1 = linearGrade(previous, pviPoint);
        const auto i2 = linearGrade(pviPoint, next);
        const auto a = i2 - i1;
        if (std::fabs(a) <= kGradeEpsilon) {
            continue;
        }

        VerticalCurveElement element;
        element.pviIndex = static_cast<std::size_t>(pvi - data.pvis.data());
        element.type = a < 0.0 ? VerticalCurveType::Crest : VerticalCurveType::Sag;
        element.pviStation = pviPoint.station;
        element.pviElevation = pviPoint.elevation;
        element.i1 = i1;
        element.i2 = i2;
        element.gradeDifference = a;
        element.radius = pvi->radius;
        element.length = std::fabs(a) * pvi->radius;
        element.tangentLength = element.length * 0.5;
        element.bvcStation = element.pviStation - element.tangentLength;
        element.evcStation = element.pviStation + element.tangentLength;
        element.bvcElevation = element.pviElevation - i1 * element.tangentLength;
        element.evcElevation = element.pviElevation + i2 * element.tangentLength;
        if (element.bvcStation < previous.station - kStationEpsilon || element.evcStation > next.station + kStationEpsilon) {
            result.warnings.push_back(L"Vertical curve overlaps adjacent tangent range.");
        }

        const auto zeroGradeX = -i1 * element.length / a;
        if (zeroGradeX >= -kStationEpsilon && zeroGradeX <= element.length + kStationEpsilon) {
            const auto x = std::clamp(zeroGradeX, 0.0, element.length);
            const auto station = element.bvcStation + x;
            const auto elevation = element.bvcElevation + i1 * x + (a * x * x) / (2.0 * element.length);
            element.highLowPoint = VerticalCurveKeyPoint{station, elevation, element.type == VerticalCurveType::Crest};
        }
        result.elements.push_back(element);
    }

    result.succeeded = true;
    return result;
}

ProfileVerticalCurveQueryResult ProfileVerticalCurveCalculator::elevationAt(
    const ProfileVerticalCurveBuildResult& built,
    double station)
{
    if (!built.succeeded) {
        return makeFailure(built.errorMessage);
    }
    if (!std::isfinite(station)) {
        return makeFailure(L"Station must be finite.");
    }
    for (const auto& element : built.elements) {
        if (station >= element.bvcStation - kStationEpsilon && station <= element.evcStation + kStationEpsilon) {
            const auto x = std::clamp(station - element.bvcStation, 0.0, element.length);
            ProfileVerticalCurveQueryResult result;
            result.succeeded = true;
            result.value = element.bvcElevation + element.i1 * x
                + (element.gradeDifference * x * x) / (2.0 * element.length);
            return result;
        }
    }
    for (std::size_t i = 1; i < built.orderedControlPoints.size(); ++i) {
        const auto& a = built.orderedControlPoints[i - 1];
        const auto& b = built.orderedControlPoints[i];
        if (station <= b.station + kStationEpsilon) {
            ProfileVerticalCurveQueryResult result;
            result.succeeded = true;
            result.value = linearElevation(a, b, station);
            return result;
        }
    }
    ProfileVerticalCurveQueryResult result;
    result.succeeded = true;
    result.value = linearElevation(built.orderedControlPoints[built.orderedControlPoints.size() - 2], built.orderedControlPoints.back(), station);
    return result;
}

ProfileVerticalCurveQueryResult ProfileVerticalCurveCalculator::gradeAt(
    const ProfileVerticalCurveBuildResult& built,
    double station)
{
    if (!built.succeeded) {
        return makeFailure(built.errorMessage);
    }
    if (!std::isfinite(station)) {
        return makeFailure(L"Station must be finite.");
    }
    for (const auto& element : built.elements) {
        if (station >= element.bvcStation - kStationEpsilon && station <= element.evcStation + kStationEpsilon) {
            const auto x = std::clamp(station - element.bvcStation, 0.0, element.length);
            ProfileVerticalCurveQueryResult result;
            result.succeeded = true;
            result.value = element.i1 + element.gradeDifference * x / element.length;
            return result;
        }
    }
    for (std::size_t i = 1; i < built.orderedControlPoints.size(); ++i) {
        const auto& a = built.orderedControlPoints[i - 1];
        const auto& b = built.orderedControlPoints[i];
        if (station <= b.station + kStationEpsilon) {
            ProfileVerticalCurveQueryResult result;
            result.succeeded = true;
            result.value = linearGrade(a, b);
            return result;
        }
    }
    ProfileVerticalCurveQueryResult result;
    result.succeeded = true;
    result.value = linearGrade(built.orderedControlPoints[built.orderedControlPoints.size() - 2], built.orderedControlPoints.back());
    return result;
}

ProfileVerticalCurveSampleResult ProfileVerticalCurveCalculator::sample(
    const ProfileVerticalCurveData& data,
    double interval)
{
    ProfileVerticalCurveSampleResult result;
    if (!std::isfinite(interval) || interval <= 0.0) {
        result.errorMessage = L"Vertical curve sample interval must be greater than zero.";
        return result;
    }
    const auto built = rebuild(data);
    if (!built.succeeded) {
        result.errorMessage = built.errorMessage;
        return result;
    }

    const auto start = built.orderedControlPoints.front().station;
    const auto end = built.orderedControlPoints.back().station;
    for (double station = start; station < end; station += interval) {
        const auto elevation = elevationAt(built, station);
        if (elevation.succeeded) {
            result.points.push_back(VerticalCurveSamplePoint{station, elevation.value});
        }
    }
    const auto endElevation = elevationAt(built, end);
    if (endElevation.succeeded) {
        result.points.push_back(VerticalCurveSamplePoint{end, endElevation.value});
    }
    result.succeeded = result.points.size() >= 2;
    if (!result.succeeded) {
        result.errorMessage = L"Vertical curve sampling produced fewer than two points.";
    }
    return result;
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::moveControlPoint(
    ProfileVerticalCurveData& data,
    std::size_t pointIndex,
    double station,
    double elevation)
{
    ProfileVerticalCurveEditResult result;
    if (pointIndex >= data.controlPoints.size() || !std::isfinite(station) || !std::isfinite(elevation)) {
        result.errorMessage = L"Invalid vertical curve control point edit.";
        return result;
    }
    data.controlPoints[pointIndex].station = station;
    data.controlPoints[pointIndex].elevation = elevation;
    const auto rebuilt = rebuild(data);
    if (!rebuilt.succeeded) {
        result.errorMessage = rebuilt.errorMessage;
        return result;
    }
    result.succeeded = true;
    result.changed = true;
    return result;
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::movePvi(
    ProfileVerticalCurveData& data,
    std::size_t pviIndex,
    double station,
    double elevation)
{
    ProfileVerticalCurveEditResult result;
    if (pviIndex >= data.pvis.size() || !std::isfinite(station) || !std::isfinite(elevation)) {
        result.errorMessage = L"Invalid vertical curve PVI edit.";
        return result;
    }
    data.pvis[pviIndex].station = station;
    data.pvis[pviIndex].elevation = elevation;
    if (data.controlPoints.size() >= pviIndex + 3) {
        data.controlPoints[pviIndex + 1] = VerticalCurveControlPoint{VerticalCurvePointRole::Pvi, station, elevation};
    }
    const auto rebuilt = rebuild(data);
    if (!rebuilt.succeeded) {
        result.errorMessage = rebuilt.errorMessage;
        return result;
    }
    result.succeeded = true;
    result.changed = true;
    return result;
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::updateRadius(
    ProfileVerticalCurveData& data,
    std::size_t pviIndex,
    double radius)
{
    ProfileVerticalCurveEditResult result;
    if (pviIndex >= data.pvis.size() || !std::isfinite(radius) || radius < kMinRadius) {
        result.errorMessage = L"Vertical curve radius must be finite and greater than or equal to 1.0.";
        return result;
    }
    data.pvis[pviIndex].radius = radius;
    const auto rebuilt = rebuild(data);
    if (!rebuilt.succeeded) {
        result.errorMessage = rebuilt.errorMessage;
        return result;
    }
    result.succeeded = true;
    result.changed = true;
    return result;
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::insertPvi(
    ProfileVerticalCurveData& data,
    double station,
    double elevation,
    double radius)
{
    ProfileVerticalCurveEditResult result;
    if (!std::isfinite(station) || !std::isfinite(elevation) || !std::isfinite(radius) || radius < kMinRadius) {
        result.errorMessage = L"Invalid vertical curve PVI insertion.";
        return result;
    }
    data.pvis.push_back(VerticalCurvePvi{station, elevation, radius});
    data.controlPoints.push_back(VerticalCurveControlPoint{VerticalCurvePointRole::Pvi, station, elevation});
    std::sort(data.pvis.begin(), data.pvis.end(), [](const auto& left, const auto& right) {
        return left.station < right.station;
    });
    data.controlPoints = orderedPoints(data);
    const auto rebuilt = rebuild(data);
    if (!rebuilt.succeeded) {
        result.errorMessage = rebuilt.errorMessage;
        return result;
    }
    result.succeeded = true;
    result.changed = true;
    return result;
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::removePvi(
    ProfileVerticalCurveData& data,
    std::size_t pviIndex)
{
    ProfileVerticalCurveEditResult result;
    if (pviIndex >= data.pvis.size()) {
        result.errorMessage = L"Invalid vertical curve PVI deletion.";
        return result;
    }
    data.pvis.erase(data.pvis.begin() + static_cast<std::ptrdiff_t>(pviIndex));
    data.controlPoints = orderedPoints(data);
    const auto rebuilt = rebuild(data);
    if (!rebuilt.succeeded) {
        result.errorMessage = rebuilt.errorMessage;
        return result;
    }
    result.succeeded = true;
    result.changed = true;
    return result;
}

} // namespace roadproto::domain::profile
```

- [ ] **Step 5: Add calculator source to project files**

In `tests/RoadProtoCoreTests.vcxproj`, add:

```xml
    <ClCompile Include="..\src\domain\profile\ProfileVerticalCurveCalculator.cpp" />
```

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\domain\profile\ProfileVerticalCurveCalculator.cpp" />
```

Place both after `ProfileGradeGraphLayout.cpp`.

- [ ] **Step 6: Run tests and confirm GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all tests pass.

- [ ] **Step 7: Commit Task 2**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj src/domain/profile/ProfileVerticalCurveCalculator.h src/domain/profile/ProfileVerticalCurveCalculator.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: calculate profile vertical curves"
```

---

### Task 3: Vertical Curve Edit Service

**Files:**
- Create: `src/application/profile/ProfileVerticalCurveEditService.h`
- Create: `src/application/profile/ProfileVerticalCurveEditService.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write failing edit service tests**

Add include:

```cpp
#include "application/profile/ProfileVerticalCurveEditService.h"
```

Add tests:

```cpp
void profileVerticalCurveEditServiceAddsAndDeletesPvi()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::End, 200.0, 10.0},
    };

    const ProfileVerticalCurveEditService service;
    const auto added = service.addPvi(data, 100.0, 12.0, 800.0);
    CHECK(added.succeeded);
    CHECK(added.changed);
    CHECK(data.pvis.size() == 1);
    CHECK(data.controlPoints.size() == 3);

    const auto deleted = service.deletePvi(data, 0);
    CHECK(deleted.succeeded);
    CHECK(deleted.changed);
    CHECK(data.pvis.empty());
    CHECK(data.controlPoints.size() == 2);
}

void profileVerticalCurveEditServiceAppliesDialogEdit()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::Pvi, 100.0, 10.0},
        {VerticalCurvePointRole::End, 200.0, 10.0},
    };
    data.pvis = {VerticalCurvePvi{100.0, 10.0, 1000.0}};

    ProfileVerticalCurveDialogEdit edit;
    edit.name = L"VC-1";
    edit.startStation = 0.0;
    edit.startElevation = 1.0;
    edit.endStation = 210.0;
    edit.endElevation = 11.0;
    edit.pvis = {VerticalCurvePvi{105.0, 12.0, 900.0}};

    const ProfileVerticalCurveEditService service;
    const auto result = service.applyDialogEdit(data, edit);
    CHECK(result.succeeded);
    CHECK(result.changed);
    CHECK(data.properties.name == L"VC-1");
    CHECK(std::fabs(data.controlPoints.front().elevation - 1.0) < 1e-9);
    CHECK(std::fabs(data.controlPoints.back().station - 210.0) < 1e-9);
    CHECK(std::fabs(data.pvis[0].radius - 900.0) < 1e-9);
}
```

Call them in `main()`:

```cpp
    profileVerticalCurveEditServiceAddsAndDeletesPvi();
    profileVerticalCurveEditServiceAppliesDialogEdit();
```

- [ ] **Step 2: Run tests and confirm RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `ProfileVerticalCurveEditService.h` does not exist.

- [ ] **Step 3: Add edit service API**

Create `src/application/profile/ProfileVerticalCurveEditService.h`:

```cpp
#pragma once

#include "domain/profile/ProfileVerticalCurveCalculator.h"
#include "domain/profile/ProfileVerticalCurveModel.h"

#include <string>
#include <vector>

namespace roadproto::application::profile {

struct ProfileVerticalCurveDialogEdit {
    std::wstring name;
    double startStation = 0.0;
    double startElevation = 0.0;
    double endStation = 0.0;
    double endElevation = 0.0;
    std::vector<domain::profile::VerticalCurvePvi> pvis;
};

struct ProfileVerticalCurveGripEdit {
    domain::profile::VerticalCurveGripRole role = domain::profile::VerticalCurveGripRole::StartPoint;
    std::size_t index = 0;
    double station = 0.0;
    double elevation = 0.0;
    double radius = 0.0;
};

using ProfileVerticalCurveEditResult = domain::profile::ProfileVerticalCurveEditResult;

class ProfileVerticalCurveEditService {
public:
    ProfileVerticalCurveEditResult applyDialogEdit(
        domain::profile::ProfileVerticalCurveData& data,
        const ProfileVerticalCurveDialogEdit& edit) const;

    ProfileVerticalCurveEditResult applyGripMove(
        domain::profile::ProfileVerticalCurveData& data,
        const ProfileVerticalCurveGripEdit& edit) const;

    ProfileVerticalCurveEditResult addPvi(
        domain::profile::ProfileVerticalCurveData& data,
        double station,
        double elevation,
        double radius) const;

    ProfileVerticalCurveEditResult deletePvi(
        domain::profile::ProfileVerticalCurveData& data,
        std::size_t pviIndex) const;
};

} // namespace roadproto::application::profile
```

- [ ] **Step 4: Add edit service implementation**

Create `src/application/profile/ProfileVerticalCurveEditService.cpp`:

```cpp
#include "application/profile/ProfileVerticalCurveEditService.h"

#include <cmath>

namespace roadproto::application::profile {
namespace {

using roadproto::domain::profile::ProfileVerticalCurveCalculator;
using roadproto::domain::profile::ProfileVerticalCurveData;
using roadproto::domain::profile::ProfileVerticalCurveEditResult;
using roadproto::domain::profile::VerticalCurveControlPoint;
using roadproto::domain::profile::VerticalCurveGripRole;
using roadproto::domain::profile::VerticalCurvePointRole;

ProfileVerticalCurveEditResult validate(ProfileVerticalCurveData& data)
{
    ProfileVerticalCurveEditResult result;
    const auto rebuilt = ProfileVerticalCurveCalculator::rebuild(data);
    if (!rebuilt.succeeded) {
        result.errorMessage = rebuilt.errorMessage;
        return result;
    }
    result.succeeded = true;
    result.changed = true;
    return result;
}

} // namespace

ProfileVerticalCurveEditResult ProfileVerticalCurveEditService::applyDialogEdit(
    ProfileVerticalCurveData& data,
    const ProfileVerticalCurveDialogEdit& edit) const
{
    if (!std::isfinite(edit.startStation) || !std::isfinite(edit.startElevation)
        || !std::isfinite(edit.endStation) || !std::isfinite(edit.endElevation)
        || edit.endStation <= edit.startStation) {
        ProfileVerticalCurveEditResult result;
        result.errorMessage = L"Vertical curve dialog edit has invalid start or end point.";
        return result;
    }

    data.properties.name = edit.name.empty() ? L"\u7ad6\u66f2\u7ebf" : edit.name;
    data.pvis = edit.pvis;
    data.controlPoints.clear();
    data.controlPoints.push_back(VerticalCurveControlPoint{VerticalCurvePointRole::Start, edit.startStation, edit.startElevation});
    for (const auto& pvi : data.pvis) {
        data.controlPoints.push_back(VerticalCurveControlPoint{VerticalCurvePointRole::Pvi, pvi.station, pvi.elevation});
    }
    data.controlPoints.push_back(VerticalCurveControlPoint{VerticalCurvePointRole::End, edit.endStation, edit.endElevation});
    return validate(data);
}

ProfileVerticalCurveEditResult ProfileVerticalCurveEditService::applyGripMove(
    ProfileVerticalCurveData& data,
    const ProfileVerticalCurveGripEdit& edit) const
{
    switch (edit.role) {
    case VerticalCurveGripRole::StartPoint:
        return ProfileVerticalCurveCalculator::moveControlPoint(data, 0, edit.station, edit.elevation);
    case VerticalCurveGripRole::EndPoint:
        return data.controlPoints.empty()
            ? ProfileVerticalCurveEditResult{false, false, L"Vertical curve has no endpoint."}
            : ProfileVerticalCurveCalculator::moveControlPoint(data, data.controlPoints.size() - 1, edit.station, edit.elevation);
    case VerticalCurveGripRole::PviPoint:
        return ProfileVerticalCurveCalculator::movePvi(data, edit.index, edit.station, edit.elevation);
    case VerticalCurveGripRole::RadiusPoint:
        return ProfileVerticalCurveCalculator::updateRadius(data, edit.index, edit.radius);
    default:
        return ProfileVerticalCurveEditResult{false, false, L"Unsupported vertical curve grip edit."};
    }
}

ProfileVerticalCurveEditResult ProfileVerticalCurveEditService::addPvi(
    ProfileVerticalCurveData& data,
    double station,
    double elevation,
    double radius) const
{
    return ProfileVerticalCurveCalculator::insertPvi(data, station, elevation, radius);
}

ProfileVerticalCurveEditResult ProfileVerticalCurveEditService::deletePvi(
    ProfileVerticalCurveData& data,
    std::size_t pviIndex) const
{
    return ProfileVerticalCurveCalculator::removePvi(data, pviIndex);
}

} // namespace roadproto::application::profile
```

- [ ] **Step 5: Add edit service source to projects**

In `tests/RoadProtoCoreTests.vcxproj`, add:

```xml
    <ClCompile Include="..\src\application\profile\ProfileVerticalCurveEditService.cpp" />
```

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\application\profile\ProfileVerticalCurveEditService.cpp" />
```

- [ ] **Step 6: Run tests and confirm GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all tests pass.

- [ ] **Step 7: Commit Task 3**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj src/application/profile/ProfileVerticalCurveEditService.h src/application/profile/ProfileVerticalCurveEditService.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: edit profile vertical curve data"
```

---

### Task 4: PROFILE Command Metadata And Project Wiring

**Files:**
- Create: `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.h`
- Create: `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.cpp`
- Modify: `src/modules/profile/ProfileModule.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write failing metadata tests**

In `profileModuleRegistersCommandsAndRibbonPanel()` in `tests/core_tests.cpp`, after the existing profile command checks, add:

```cpp
    const auto verticalCurveCreate = commandRegistry.findCommand(L"RD_PROFILE_VERTICAL_CURVE_CREATE");
    CHECK(verticalCurveCreate.has_value());
    CHECK(verticalCurveCreate->displayName == L"\u521b\u5efa\u7ad6\u66f2\u7ebf");
    CHECK(verticalCurveCreate->moduleCode == L"PROFILE");
    CHECK(verticalCurveCreate->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u521b\u5efa.md");
    CHECK(verticalCurveCreate->ribbonAttachable);

    const auto verticalCurveEdit = commandRegistry.findCommand(L"RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE");
    CHECK(verticalCurveEdit.has_value());
    CHECK(verticalCurveEdit->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u7f16\u8f91.md");
    CHECK(!verticalCurveEdit->ribbonAttachable);

    const auto verticalCurveApply = commandRegistry.findCommand(L"RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE");
    CHECK(verticalCurveApply.has_value());
    CHECK(verticalCurveApply->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u7f16\u8f91.md");

    const auto verticalCurveAddPvi = commandRegistry.findCommand(L"RD_PROFILE_VERTICAL_CURVE_ADD_PVI");
    CHECK(verticalCurveAddPvi.has_value());
    CHECK(verticalCurveAddPvi->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u5939\u70b9\u4e0e\u53f3\u952e\u7f16\u8f91.md");

    const auto verticalCurveDeletePvi = commandRegistry.findCommand(L"RD_PROFILE_VERTICAL_CURVE_DELETE_PVI");
    CHECK(verticalCurveDeletePvi.has_value());
    CHECK(verticalCurveDeletePvi->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u5939\u70b9\u4e0e\u53f3\u952e\u7f16\u8f91.md");
```

- [ ] **Step 2: Run tests and confirm RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: test exe fails because the new commands are not registered.

- [ ] **Step 3: Add command procedure declarations and test-build stubs**

Create `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.h`:

```cpp
#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::profile {

core::CommandProcedure profileVerticalCurveCreateCommandProcedure();
core::CommandProcedure profileVerticalCurveEditHandleCommandProcedure();
core::CommandProcedure profileVerticalCurveApplyDialogFileCommandProcedure();
core::CommandProcedure profileVerticalCurveAddPviCommandProcedure();
core::CommandProcedure profileVerticalCurveDeletePviCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::profile
```

Create `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.cpp`:

```cpp
#include "cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.h"

namespace roadproto::cad_adapter::objectarx::profile {
namespace {

void runProfileVerticalCurveCreateCommand()
{
}

void runProfileVerticalCurveEditHandleCommand()
{
}

void runProfileVerticalCurveApplyDialogFileCommand()
{
}

void runProfileVerticalCurveAddPviCommand()
{
}

void runProfileVerticalCurveDeletePviCommand()
{
}

} // namespace

core::CommandProcedure profileVerticalCurveCreateCommandProcedure()
{
    return &runProfileVerticalCurveCreateCommand;
}

core::CommandProcedure profileVerticalCurveEditHandleCommandProcedure()
{
    return &runProfileVerticalCurveEditHandleCommand;
}

core::CommandProcedure profileVerticalCurveApplyDialogFileCommandProcedure()
{
    return &runProfileVerticalCurveApplyDialogFileCommand;
}

core::CommandProcedure profileVerticalCurveAddPviCommandProcedure()
{
    return &runProfileVerticalCurveAddPviCommand;
}

core::CommandProcedure profileVerticalCurveDeletePviCommandProcedure()
{
    return &runProfileVerticalCurveDeletePviCommand;
}

} // namespace roadproto::cad_adapter::objectarx::profile
```

- [ ] **Step 4: Register commands in PROFILE module**

In `src/modules/profile/ProfileModule.cpp`, add:

```cpp
#include "cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.h"
```

Inside `registerProfileCommands`, after the grade graph commands, add:

```cpp
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_VERTICAL_CURVE_CREATE",
        L"\u521b\u5efa\u7ad6\u66f2\u7ebf",
        L"PROFILE",
        L"Creates an independent vertical curve design entity attached to a profile grade graph.",
        cad_adapter::objectarx::profile::profileVerticalCurveCreateCommandProcedure(),
        true,
        true,
        L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u521b\u5efa.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE",
        L"\u6309 handle \u7f16\u8f91\u7ad6\u66f2\u7ebf",
        L"PROFILE",
        L"Internal double-click bridge command that edits a profile vertical curve by handle.",
        cad_adapter::objectarx::profile::profileVerticalCurveEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u7f16\u8f91.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE",
        L"\u5e94\u7528\u7ad6\u66f2\u7ebf\u5bf9\u8bdd\u6846\u7ed3\u679c",
        L"PROFILE",
        L"Internal WPF bridge command that applies profile vertical curve dialog response files.",
        cad_adapter::objectarx::profile::profileVerticalCurveApplyDialogFileCommandProcedure(),
        true,
        false,
        L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u7f16\u8f91.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_VERTICAL_CURVE_ADD_PVI",
        L"\u65b0\u589e\u7ad6\u66f2\u7ebf\u53d8\u5761\u70b9",
        L"PROFILE",
        L"Adds a PVI to a selected profile vertical curve entity.",
        cad_adapter::objectarx::profile::profileVerticalCurveAddPviCommandProcedure(),
        true,
        true,
        L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u5939\u70b9\u4e0e\u53f3\u952e\u7f16\u8f91.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_VERTICAL_CURVE_DELETE_PVI",
        L"\u5220\u9664\u7ad6\u66f2\u7ebf\u53d8\u5761\u70b9",
        L"PROFILE",
        L"Deletes a PVI from a selected profile vertical curve entity.",
        cad_adapter::objectarx::profile::profileVerticalCurveDeletePviCommandProcedure(),
        true,
        true,
        L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u5939\u70b9\u4e0e\u53f3\u952e\u7f16\u8f91.md",
        false});
```

- [ ] **Step 5: Add command source to projects**

In `tests/RoadProtoCoreTests.vcxproj`, add:

```xml
    <ClCompile Include="..\src\cad_adapter\objectarx\profile\ObjectArxProfileVerticalCurveCommand.cpp" />
```

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\cad_adapter\objectarx\profile\ObjectArxProfileVerticalCurveCommand.cpp" />
```

- [ ] **Step 6: Run tests and confirm GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all tests pass.

- [ ] **Step 7: Commit Task 4**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj src/modules/profile/ProfileModule.cpp src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.h src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: register profile vertical curve commands"
```

---

### Task 5: Grade Graph Drawing Frame And Vertical Curve Entity Skeleton

**Files:**
- Modify: `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h`
- Modify: `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.cpp`
- Create: `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h`
- Create: `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.cpp`
- Modify: `src/app/arx_entry/RoadProtoArxEntry.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Add grade graph drawing frame**

In `DnProfileGradeGraphEntity.h`, add before the entity class:

```cpp
struct ProfileGradeGraphDrawingFrame {
    bool valid = false;
    double minStation = 0.0;
    double baseElevation = 0.0;
    double verticalScale = 10.0;
    AcGePoint3d insertionPoint;
    AcGeVector3d xAxis;
    AcGeVector3d yAxis;
};
```

Add this public method to `DnProfileGradeGraphEntity`:

```cpp
    ProfileGradeGraphDrawingFrame drawingFrame() const;
```

In `DnProfileGradeGraphEntity.cpp`, implement:

```cpp
ProfileGradeGraphDrawingFrame DnProfileGradeGraphEntity::drawingFrame() const
{
    assertReadEnabled();
    ProfileGradeGraphDrawingFrame frame;
    const auto layout = ProfileGradeGraphLayout::calculate(graphData_);
    if (!layout.succeeded || !areAxesUsable(xAxis_, yAxis_)) {
        return frame;
    }
    frame.valid = true;
    frame.minStation = layout.minStation;
    frame.baseElevation = layout.baseElevation;
    frame.verticalScale = graphData_.properties.verticalScale;
    frame.insertionPoint = insertionPoint_;
    frame.xAxis = xAxis_;
    frame.yAxis = yAxis_;
    return frame;
}
```

- [ ] **Step 2: Add vertical curve entity declaration**

Create `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h`:

```cpp
#pragma once

#include "domain/profile/ProfileVerticalCurveModel.h"

#include "dbents.h"
#include "dbmain.h"

class DnProfileVerticalCurveEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnProfileVerticalCurveEntity);

    DnProfileVerticalCurveEntity();

    void setCurveData(const roadproto::domain::profile::ProfileVerticalCurveData& data);
    const roadproto::domain::profile::ProfileVerticalCurveData& curveData() const;

    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* filer) override;
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* filer) const override;

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* worldDraw) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& transform) override;

private:
    roadproto::domain::profile::ProfileVerticalCurveData curveData_;
};

namespace roadproto::cad_adapter::objectarx {

void initializeProfileVerticalCurveEntityClass();
void uninitializeProfileVerticalCurveEntityClass();

} // namespace roadproto::cad_adapter::objectarx
```

- [ ] **Step 3: Add minimal vertical curve entity implementation**

Create `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.cpp`:

```cpp
#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"

#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "domain/profile/ProfileVerticalCurveCalculator.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbapserv.h"
#include "dbproxy.h"
#include "rxregsvc.h"

using roadproto::domain::profile::ProfileVerticalCurveData;
using roadproto::domain::profile::ProfileVerticalCurveCalculator;
using roadproto::domain::profile::VerticalCurveControlPoint;
using roadproto::domain::profile::VerticalCurvePvi;
using roadproto::domain::profile::VerticalCurvePointRole;

ACRX_DXF_DEFINE_MEMBERS(
    DnProfileVerticalCurveEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNPROFILEVERTICALCURVEENTITY,
    "RoadProto Profile Vertical Curve");

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;
constexpr Adesk::Int32 kMaxPersistedPoints = 100000;

std::wstring readWideString(AcDbDwgFiler* filer)
{
    ACHAR* value = nullptr;
    filer->readString(&value);
    std::wstring result = value == nullptr ? L"" : value;
    acutDelString(value);
    return result;
}

void writeWideString(AcDbDwgFiler* filer, const std::wstring& value)
{
    filer->writeString(value.c_str());
}

void markGraphicsModifiedIfResident(AcDbEntity& entity)
{
    if (!entity.objectId().isNull()) {
        entity.recordGraphicsModified(true);
    }
}

} // namespace

DnProfileVerticalCurveEntity::DnProfileVerticalCurveEntity() = default;

void DnProfileVerticalCurveEntity::setCurveData(const ProfileVerticalCurveData& data)
{
    assertWriteEnabled();
    curveData_ = data;
    markGraphicsModifiedIfResident(*this);
}

const ProfileVerticalCurveData& DnProfileVerticalCurveEntity::curveData() const
{
    return curveData_;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::dwgInFields(AcDbDwgFiler* filer)
{
    assertWriteEnabled();
    auto status = AcDbEntity::dwgInFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    Adesk::Int16 version = 0;
    filer->readInt16(&version);
    if (version > kEntityVersion) {
        return Acad::eMakeMeProxy;
    }

    curveData_.version = version;
    curveData_.profileGraphHandle = readWideString(filer);
    curveData_.properties.name = readWideString(filer);
    filer->readInt32(&curveData_.properties.designLineColorIndex);
    filer->readInt32(&curveData_.properties.tangentLineColorIndex);
    filer->readInt32(&curveData_.properties.keyPointColorIndex);
    filer->readDouble(&curveData_.properties.designLineWidth);
    filer->readDouble(&curveData_.properties.sampleInterval);

    Adesk::Int32 pointCount = 0;
    filer->readInt32(&pointCount);
    if (pointCount < 0 || pointCount > kMaxPersistedPoints) {
        return Acad::eInvalidInput;
    }
    curveData_.controlPoints.clear();
    for (Adesk::Int32 i = 0; i < pointCount; ++i) {
        Adesk::Int32 role = 0;
        VerticalCurveControlPoint point;
        filer->readInt32(&role);
        filer->readDouble(&point.station);
        filer->readDouble(&point.elevation);
        point.role = static_cast<VerticalCurvePointRole>(role);
        curveData_.controlPoints.push_back(point);
    }

    Adesk::Int32 pviCount = 0;
    filer->readInt32(&pviCount);
    if (pviCount < 0 || pviCount > kMaxPersistedPoints) {
        return Acad::eInvalidInput;
    }
    curveData_.pvis.clear();
    for (Adesk::Int32 i = 0; i < pviCount; ++i) {
        VerticalCurvePvi pvi;
        filer->readDouble(&pvi.station);
        filer->readDouble(&pvi.elevation);
        filer->readDouble(&pvi.radius);
        Adesk::Int8 locked = 0;
        filer->readInt8(&locked);
        pvi.radiusLocked = locked != 0;
        curveData_.pvis.push_back(pvi);
    }
    return filer->filerStatus();
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, curveData_.profileGraphHandle);
    writeWideString(filer, curveData_.properties.name);
    filer->writeInt32(curveData_.properties.designLineColorIndex);
    filer->writeInt32(curveData_.properties.tangentLineColorIndex);
    filer->writeInt32(curveData_.properties.keyPointColorIndex);
    filer->writeDouble(curveData_.properties.designLineWidth);
    filer->writeDouble(curveData_.properties.sampleInterval);

    filer->writeInt32(static_cast<Adesk::Int32>(curveData_.controlPoints.size()));
    for (const auto& point : curveData_.controlPoints) {
        filer->writeInt32(static_cast<Adesk::Int32>(point.role));
        filer->writeDouble(point.station);
        filer->writeDouble(point.elevation);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(curveData_.pvis.size()));
    for (const auto& pvi : curveData_.pvis) {
        filer->writeDouble(pvi.station);
        filer->writeDouble(pvi.elevation);
        filer->writeDouble(pvi.radius);
        filer->writeInt8(pvi.radiusLocked ? 1 : 0);
    }
    return filer->filerStatus();
}

Adesk::Boolean DnProfileVerticalCurveEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr) {
        return Adesk::kTrue;
    }
    const auto samples = ProfileVerticalCurveCalculator::sample(curveData_, curveData_.properties.sampleInterval);
    if (!samples.succeeded) {
        return Adesk::kTrue;
    }
    worldDraw->subEntityTraits().setColor(static_cast<Adesk::UInt16>(curveData_.properties.designLineColorIndex));
    for (std::size_t i = 1; i < samples.points.size(); ++i) {
        AcGePoint3d line[2] = {
            AcGePoint3d(samples.points[i - 1].station, samples.points[i - 1].elevation, 0.0),
            AcGePoint3d(samples.points[i].station, samples.points[i].elevation, 0.0)};
        worldDraw->geometry().polyline(2, line);
    }
    return Adesk::kTrue;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    const auto samples = ProfileVerticalCurveCalculator::sample(curveData_, curveData_.properties.sampleInterval);
    if (!samples.succeeded) {
        return Acad::eInvalidExtents;
    }
    for (const auto& sample : samples.points) {
        extents.addPoint(AcGePoint3d(sample.station, sample.elevation, 0.0));
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::subTransformBy(const AcGeMatrix3d&)
{
    assertWriteEnabled();
    return Acad::eOk;
}

namespace roadproto::cad_adapter::objectarx {

void initializeProfileVerticalCurveEntityClass()
{
    DnProfileVerticalCurveEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeProfileVerticalCurveEntityClass()
{
    deleteAcRxClass(DnProfileVerticalCurveEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
```

This initial drawing uses raw station/elevation coordinates. Task 6 will map through the attached grade graph frame.

- [ ] **Step 4: Register entity class in ARX entry**

In `src/app/arx_entry/RoadProtoArxEntry.cpp`, add include:

```cpp
#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"
```

After `initializeProfileGradeGraphEntityClass();`, add:

```cpp
        cad_adapter::objectarx::initializeProfileVerticalCurveEntityClass();
```

Before `uninitializeProfileGradeGraphEntityClass();`, add:

```cpp
        cad_adapter::objectarx::uninitializeProfileVerticalCurveEntityClass();
```

- [ ] **Step 5: Add entity source to ARX project**

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\cad_adapter\objectarx\profile\DnProfileVerticalCurveEntity.cpp" />
```

- [ ] **Step 6: Build ARX**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build succeeds. If the compiler rejects duplicate helper names because anonymous namespace helpers match another translation unit, keep helpers in the anonymous namespace and verify file-local linkage; no rename is needed unless the error points to the same file.

- [ ] **Step 7: Commit Task 5**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/app/RoadProtoArx.vcxproj src/app/arx_entry/RoadProtoArxEntry.cpp src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.cpp src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add profile vertical curve entity"
```

---

### Task 6: ObjectARX Commands And Grade Graph Mapping

**Files:**
- Modify: `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h`
- Modify: `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.cpp`
- Modify: `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Replace command stubs with ObjectARX implementation**

In `ObjectArxProfileVerticalCurveCommand.cpp`, keep the empty functions under `#ifdef ROADPROTO_TEST_BUILD` and add the real implementation under `#ifndef ROADPROTO_TEST_BUILD`. Use the same helper style as `ObjectArxProfileGradeGraphCommand.cpp`:

```cpp
#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/profile/ProfileVerticalCurveCreateService.h"
#include "application/profile/ProfileVerticalCurveEditService.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <sstream>
#endif
```

Implement these helpers in the `#ifndef ROADPROTO_TEST_BUILD` block:

```cpp
bool appendEntityToModelSpace(AcDbEntity* entity, AcDbObjectId& entityId);
bool resolveObjectIdFromHandle(const std::wstring& handleText, AcDbObjectId& entityId);
std::wstring entityHandleText(AcDbEntity* entity);
bool selectProfileGradeGraphEntity(AcDbObjectId& entityId);
bool selectProfileVerticalCurveEntity(AcDbObjectId& entityId);
bool readProfileGraph(AcDbObjectId entityId, ProfileVerticalCurveCreateInput& input);
bool promptGraphPointAsStationElevation(DnProfileVerticalCurveEntity& entity, double& station, double& elevation);
```

Use direct equivalents from `ObjectArxProfileGradeGraphCommand.cpp` for selection, model-space append, handle text, and handle resolution. For `readProfileGraph`, open `DnProfileGradeGraphEntity`, set `input.profileGraphHandle = entityHandleText(graphEntity)`, and set `input.groundSamples = graphEntity->graphData().groundSamples`.

- [ ] **Step 2: Implement create command**

Inside `runProfileVerticalCurveCreateCommand()`:

```cpp
auto& editor = app::ApplicationContext::instance().editor();
editor.writeMessage(L"RD_PROFILE_VERTICAL_CURVE_CREATE: 请选择纵断面拉坡图实体。");

AcDbObjectId graphId;
if (!selectProfileGradeGraphEntity(graphId)) {
    editor.writeWarning(L"未选择纵断面拉坡图实体。");
    return;
}

ProfileVerticalCurveCreateInput input;
if (!readProfileGraph(graphId, input)) {
    editor.writeError(L"无法读取纵断面拉坡图实体。");
    return;
}

const ProfileVerticalCurveCreateService service;
const auto result = service.buildDefaultFromGraph(input);
if (!result.succeeded) {
    editor.writeError(result.errorMessage.empty() ? L"竖曲线数据生成失败。" : result.errorMessage);
    return;
}

auto* entity = new DnProfileVerticalCurveEntity();
entity->setCurveData(result.data);

AcDbObjectId entityId;
if (!appendEntityToModelSpace(entity, entityId)) {
    delete entity;
    editor.writeError(L"插入 DnProfileVerticalCurveEntity 失败。");
    return;
}

const auto handle = entityHandleText(entity);
entity->close();
acedUpdateDisplay();
editor.writeMessage(L"已创建竖曲线实体，handle: " + handle + L"。");
```

- [ ] **Step 3: Implement add/delete PVI commands**

For `runProfileVerticalCurveAddPviCommand()`, select the curve, prompt a point, map that point to station/elevation through the entity helper from Step 5 below, call `ProfileVerticalCurveEditService::addPvi(data, station, elevation, 1000.0)`, set the data, and refresh display.

For `runProfileVerticalCurveDeletePviCommand()`, select the curve, choose nearest PVI by station/elevation distance in drawing coordinates, call `deletePvi`, set data, and refresh display. If no PVI exists, write warning `当前竖曲线没有可删除的变坡点。`.

- [ ] **Step 4: Implement grade graph mapping in entity**

Add private helpers to `DnProfileVerticalCurveEntity.cpp`:

```cpp
bool resolveObjectIdFromHandle(const std::wstring& handleText, AcDbObjectId& entityId);
bool loadGraphFrame(const std::wstring& handleText, ProfileGradeGraphDrawingFrame& frame);
AcGePoint3d mapDesignPoint(const ProfileGradeGraphDrawingFrame& frame, double station, double elevation);
bool unmapDesignPoint(const ProfileGradeGraphDrawingFrame& frame, const AcGePoint3d& point, double& station, double& elevation);
```

Replace Task 5 raw station/elevation drawing with mapped drawing:

```cpp
ProfileGradeGraphDrawingFrame frame;
if (!loadGraphFrame(curveData_.profileGraphHandle, frame) || !frame.valid) {
    return Adesk::kTrue;
}
const auto samples = ProfileVerticalCurveCalculator::sample(curveData_, curveData_.properties.sampleInterval);
if (!samples.succeeded) {
    return Adesk::kTrue;
}
for (std::size_t i = 1; i < samples.points.size(); ++i) {
    AcGePoint3d line[2] = {
        mapDesignPoint(frame, samples.points[i - 1].station, samples.points[i - 1].elevation),
        mapDesignPoint(frame, samples.points[i].station, samples.points[i].elevation)};
    worldDraw->geometry().polyline(2, line);
}
```

- [ ] **Step 5: Add grip APIs**

In `DnProfileVerticalCurveEntity.h`, add overrides:

```cpp
    Acad::ErrorStatus subGetGripPoints(AcGePoint3dArray& gripPoints, AcDbIntArray& osnapModes, AcDbIntArray& geomIds) const override;
    Acad::ErrorStatus subMoveGripPointsAt(const AcDbIntArray& indices, const AcGeVector3d& offset) override;
```

In `DnProfileVerticalCurveEntity.cpp`, implement grips for start, end, each PVI, and each radius point. Use the same index convention in all functions:

```text
0 = start
1 = end
2..(2+pviCount-1) = PVI points
2+pviCount..(2+pviCount*2-1) = radius points
```

For start/end/PVI grips, add or subtract the offset from the grip point, unmap to station/elevation, and call `ProfileVerticalCurveEditService::applyGripMove`. For radius grips, convert the offset length to `radius = max(1.0, oldRadius + offset.length() * 10.0)`.

- [ ] **Step 6: Build ARX**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build succeeds.

- [ ] **Step 7: Commit Task 6**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.cpp src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.cpp src/app/RoadProtoArx.vcxproj
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: create and edit profile vertical curve entities"
```

---

### Task 7: WPF Bridge And Edit Window

**Files:**
- Create: `src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.h`
- Create: `src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.cpp`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileVerticalCurveDialogDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileVerticalCurveDialogFile.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/ProfileVerticalCurveWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/ProfileVerticalCurveWindow.xaml.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/ProfileVerticalCurveDialogCommands.cs`
- Modify: `src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Add C++ Bridge files using the grade graph Bridge pattern**

Create `ProfileVerticalCurveDialogBridge.h` with:

```cpp
#pragma once

#include "domain/profile/ProfileVerticalCurveModel.h"

#include <string>
#include <vector>

namespace roadproto::cad_adapter::objectarx::profile {

struct ProfileVerticalCurveDialogRequest {
    std::wstring handle;
    std::wstring responsePath;
    std::wstring profileGraphHandle;
    std::wstring name;
    double startStation = 0.0;
    double startElevation = 0.0;
    double endStation = 0.0;
    double endElevation = 0.0;
    int currentPviIndex = 0;
    std::vector<roadproto::domain::profile::VerticalCurvePvi> pvis;
};

struct ProfileVerticalCurveDialogResponse {
    bool accepted = false;
    std::wstring handle;
    std::wstring name;
    double startStation = 0.0;
    double startElevation = 0.0;
    double endStation = 0.0;
    double endElevation = 0.0;
    std::vector<roadproto::domain::profile::VerticalCurvePvi> pvis;
};

bool queueProfileVerticalCurveWpfDialog(const ProfileVerticalCurveDialogRequest& request, std::wstring& errorMessage);
bool readProfileVerticalCurveDialogResponse(
    const std::wstring& responsePath,
    ProfileVerticalCurveDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::profile
```

Implement `.cpp` with local UTF-8 conversion, percent escaping, temp request/response path creation, pending request writing, indexed PVI key serialization, indexed PVI key parsing, and this AutoCAD command dispatch:

```cpp
const std::wstring command = L"RD_PROFILE_VERTICAL_CURVE_SHOW_WPF_DIALOG\n";
```

Write each PVI as indexed keys:

```text
pviCount=2
pvi.0.station=100
pvi.0.elevation=12
pvi.0.radius=1000
pvi.1.station=200
pvi.1.elevation=10
pvi.1.radius=800
```

- [ ] **Step 2: Add managed DTOs and file parser**

Create `ProfileVerticalCurveDialogDtos.cs`:

```csharp
namespace RoadProto.Terrain.UI.Bridge;

public sealed class ProfileVerticalCurvePviDto
{
    public double Station { get; set; }
    public double Elevation { get; set; }
    public double Radius { get; set; } = 1000.0;
}

public sealed class ProfileVerticalCurveDialogRequest
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public string ProfileGraphHandle { get; set; } = string.Empty;
    public string Name { get; set; } = "竖曲线";
    public double StartStation { get; set; }
    public double StartElevation { get; set; }
    public double EndStation { get; set; }
    public double EndElevation { get; set; }
    public int CurrentPviIndex { get; set; }
    public List<ProfileVerticalCurvePviDto> Pvis { get; set; } = [];
}

public sealed class ProfileVerticalCurveDialogResponse
{
    public bool Accepted { get; set; }
    public string Handle { get; set; } = string.Empty;
    public string Name { get; set; } = "竖曲线";
    public double StartStation { get; set; }
    public double StartElevation { get; set; }
    public double EndStation { get; set; }
    public double EndElevation { get; set; }
    public List<ProfileVerticalCurvePviDto> Pvis { get; set; } = [];
}
```

Create `ProfileVerticalCurveDialogFile.cs` with the same `ReadValues`, `GetDouble`, `Escape`, and `Unescape` approach used in `ProfileGradeGraphDialogFile.cs`, plus indexed PVI read/write loops.

- [ ] **Step 3: Add WPF window**

Create `ProfileVerticalCurveWindow.xaml` with a compact dialog:

```xml
<Window x:Class="RoadProto.Terrain.UI.ProfileVerticalCurveWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="竖曲线"
        Width="560"
        Height="560"
        MinWidth="520"
        MinHeight="520"
        WindowStartupLocation="CenterScreen"
        Background="#F3F5F7">
    <Grid Margin="22">
        <Grid.RowDefinitions>
            <RowDefinition Height="Auto" />
            <RowDefinition Height="*" />
            <RowDefinition Height="Auto" />
        </Grid.RowDefinitions>
        <StackPanel Grid.Row="0" Margin="0,0,0,16">
            <TextBlock Text="竖曲线" FontSize="24" FontWeight="SemiBold" />
            <TextBlock x:Name="GraphHandleTextBlock" Margin="0,5,0,0" Foreground="#5D6875" />
        </StackPanel>
        <Border Grid.Row="1" Background="White" BorderBrush="#D8DEE6" BorderThickness="1" CornerRadius="8" Padding="18">
            <ScrollViewer VerticalScrollBarVisibility="Auto">
                <Grid>
                    <Grid.RowDefinitions>
                        <RowDefinition Height="Auto" />
                        <RowDefinition Height="Auto" />
                        <RowDefinition Height="Auto" />
                        <RowDefinition Height="Auto" />
                        <RowDefinition Height="Auto" />
                        <RowDefinition Height="Auto" />
                        <RowDefinition Height="Auto" />
                    </Grid.RowDefinitions>
                    <Grid.ColumnDefinitions>
                        <ColumnDefinition Width="145" />
                        <ColumnDefinition Width="*" />
                    </Grid.ColumnDefinitions>
                    <TextBlock Grid.Row="0" Grid.Column="0" Text="名称" Margin="0,0,12,12" />
                    <TextBox x:Name="NameTextBox" Grid.Row="0" Grid.Column="1" Margin="0,0,0,12" />
                    <TextBlock Grid.Row="1" Grid.Column="0" Text="起点桩号" Margin="0,0,12,12" />
                    <TextBox x:Name="StartStationTextBox" Grid.Row="1" Grid.Column="1" Margin="0,0,0,12" />
                    <TextBlock Grid.Row="2" Grid.Column="0" Text="起点高程" Margin="0,0,12,12" />
                    <TextBox x:Name="StartElevationTextBox" Grid.Row="2" Grid.Column="1" Margin="0,0,0,12" />
                    <TextBlock Grid.Row="3" Grid.Column="0" Text="终点桩号" Margin="0,0,12,12" />
                    <TextBox x:Name="EndStationTextBox" Grid.Row="3" Grid.Column="1" Margin="0,0,0,12" />
                    <TextBlock Grid.Row="4" Grid.Column="0" Text="终点高程" Margin="0,0,12,12" />
                    <TextBox x:Name="EndElevationTextBox" Grid.Row="4" Grid.Column="1" Margin="0,0,0,12" />
                    <TextBlock Grid.Row="5" Grid.Column="0" Text="当前变坡点" Margin="0,0,12,12" />
                    <StackPanel Grid.Row="5" Grid.Column="1" Orientation="Horizontal" Margin="0,0,0,12">
                        <Button Content="上一个" Click="PreviousPvi_Click" Margin="0,0,8,0" />
                        <TextBlock x:Name="CurrentPviTextBlock" VerticalAlignment="Center" Margin="0,0,8,0" />
                        <Button Content="下一个" Click="NextPvi_Click" />
                    </StackPanel>
                    <StackPanel Grid.Row="6" Grid.ColumnSpan="2">
                        <TextBlock Text="PVI 桩号" />
                        <TextBox x:Name="PviStationTextBox" Margin="0,4,0,8" />
                        <TextBlock Text="PVI 高程" />
                        <TextBox x:Name="PviElevationTextBox" Margin="0,4,0,8" />
                        <TextBlock Text="半径 R" />
                        <TextBox x:Name="PviRadiusTextBox" Margin="0,4,0,0" />
                    </StackPanel>
                </Grid>
            </ScrollViewer>
        </Border>
        <Border Grid.Row="2" Background="White" BorderBrush="#D8DEE6" BorderThickness="1" CornerRadius="8" Padding="18" Margin="0,16,0,0">
            <Grid>
                <Grid.ColumnDefinitions>
                    <ColumnDefinition Width="*" />
                    <ColumnDefinition Width="Auto" />
                </Grid.ColumnDefinitions>
                <TextBlock x:Name="ErrorTextBlock" Foreground="#B42318" TextWrapping="Wrap" VerticalAlignment="Center" />
                <StackPanel Grid.Column="1" Orientation="Horizontal">
                    <Button Content="确定" Click="OkButton_Click" Margin="0,0,10,0" />
                    <Button Content="取消" Click="CancelButton_Click" />
                </StackPanel>
            </Grid>
        </Border>
    </Grid>
</Window>
```

Create `.xaml.cs` that loads the request, keeps `_currentPviIndex`, writes current text boxes back before switching, validates finite numeric fields, and returns `ProfileVerticalCurveDialogResponse`.

- [ ] **Step 4: Add managed command**

Create `AutoCad/ProfileVerticalCurveDialogCommands.cs`:

```csharp
using System;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class ProfileVerticalCurveDialogCommands
{
    [CommandMethod("RD_PROFILE_VERTICAL_CURVE_SHOW_WPF_DIALOG")]
    public void ShowProfileVerticalCurveDialog()
    {
        var pendingPath = Path.Combine(Path.GetTempPath(), $"RoadProtoProfileVerticalCurveDialog_{Environment.ProcessId}.pending");
        var requestPath = File.ReadAllText(pendingPath, Encoding.UTF8);
        var request = ProfileVerticalCurveDialogFile.ReadRequest(requestPath);
        var window = new ProfileVerticalCurveWindow(request);
        var accepted = window.ShowDialog() == true;
        var response = window.Response ?? new ProfileVerticalCurveDialogResponse
        {
            Accepted = accepted,
            Handle = request.Handle,
            Name = request.Name,
            StartStation = request.StartStation,
            StartElevation = request.StartElevation,
            EndStation = request.EndStation,
            EndElevation = request.EndElevation,
            Pvis = request.Pvis,
        };
        ProfileVerticalCurveDialogFile.WriteResponse(request.ResponsePath, response);
        Autodesk.AutoCAD.ApplicationServices.Core.Application.DocumentManager.MdiActiveDocument?
            .SendStringToExecute($"RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE \"{request.ResponsePath}\" ", true, false, true);
    }
}
```

- [ ] **Step 5: Hook C++ edit/apply commands**

In `ObjectArxProfileVerticalCurveCommand.cpp`, implement `runProfileVerticalCurveEditHandleCommand()` to read handle, open `DnProfileVerticalCurveEntity`, build `ProfileVerticalCurveDialogRequest`, and call `queueProfileVerticalCurveWpfDialog`.

Implement `runProfileVerticalCurveApplyDialogFileCommand()` to read response path, parse response, build `ProfileVerticalCurveDialogEdit`, call `ProfileVerticalCurveEditService::applyDialogEdit`, write entity data, and call `acedUpdateDisplay()`.

Add `ProfileVerticalCurveDialogBridge.cpp` to `src/app/RoadProtoArx.vcxproj`.

- [ ] **Step 6: Build ARX and WPF**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: both builds succeed.

- [ ] **Step 7: Commit Task 7**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/app/RoadProtoArx.vcxproj src/cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.cpp src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.h src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.cpp src/ui/wpf/RoadProto.Terrain.UI/ProfileVerticalCurveWindow.xaml src/ui/wpf/RoadProto.Terrain.UI/ProfileVerticalCurveWindow.xaml.cs src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileVerticalCurveDialogDtos.cs src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileVerticalCurveDialogFile.cs src/ui/wpf/RoadProto.Terrain.UI/AutoCad/ProfileVerticalCurveDialogCommands.cs
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: edit profile vertical curves with wpf"
```

---

### Task 8: Ribbon, Double-Click, And Right-Click Integration

**Files:**
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`

- [ ] **Step 1: Add constants**

Add near the profile constants:

```csharp
private const string ProfileVerticalCurveButtonId = "ROADPROTO_RD_PROFILE_VERTICAL_CURVE_CREATE";
private const string ProfileVerticalCurveDxfName = "DNPROFILEVERTICALCURVEENTITY";
```

- [ ] **Step 2: Add Ribbon button**

After the profile grade graph button registration, add:

```csharp
if (!profilePanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == ProfileVerticalCurveButtonId))
{
    profilePanel.Source.Items.Add(CreateProfileCommandButton(
        ProfileVerticalCurveButtonId,
        "创建竖曲线",
        "选择纵断面拉坡图并创建竖曲线设计线",
        "RD_PROFILE_VERTICAL_CURVE_CREATE "));
}
```

- [ ] **Step 3: Add double-click dispatch**

In `OnBeginDoubleClick`, after profile grade graph detection, add:

```csharp
if (TryFindEntityByDxfName(document, e.Location, ProfileVerticalCurveDxfName, out var profileVerticalCurveId))
{
    if (!SuppressDuplicateDoubleClick(profileVerticalCurveId))
    {
        QueueProfileVerticalCurveEditByHandle(document, profileVerticalCurveId);
    }
}
```

Add:

```csharp
private static void QueueProfileVerticalCurveEditByHandle(Document document, ObjectId entityId)
{
    var handle = entityId.Handle.ToString();
    if (string.IsNullOrWhiteSpace(handle))
    {
        return;
    }

    document.SendStringToExecute($"RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE {handle} ", true, false, true);
}
```

- [ ] **Step 4: Add context menu commands**

Use AutoCAD .NET document command dispatch for first version. Add two public commands to `RoadProtoRibbonExtension`:

```csharp
[CommandMethod("RD_PROFILE_VERTICAL_CURVE_CONTEXT_ADD_PVI")]
public void ContextAddProfileVerticalCurvePvi()
{
    CoreApplication.DocumentManager.MdiActiveDocument?
        .SendStringToExecute("RD_PROFILE_VERTICAL_CURVE_ADD_PVI ", true, false, true);
}

[CommandMethod("RD_PROFILE_VERTICAL_CURVE_CONTEXT_DELETE_PVI")]
public void ContextDeleteProfileVerticalCurvePvi()
{
    CoreApplication.DocumentManager.MdiActiveDocument?
        .SendStringToExecute("RD_PROFILE_VERTICAL_CURVE_DELETE_PVI ", true, false, true);
}
```

If adding a true contextual popup requires extra AutoCAD APIs not available in this target, keep these commands callable and document the first version as command/Ribbon driven plus future context menu polish.

- [ ] **Step 5: Build WPF**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: build succeeds.

- [ ] **Step 6: Commit Task 8**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: expose profile vertical curve ui entrypoints"
```

---

### Task 9: Documentation And Reuse Notes

**Files:**
- Create: `docs/business/profile/竖曲线_创建.md`
- Create: `docs/business/profile/竖曲线_编辑.md`
- Create: `docs/business/profile/竖曲线_夹点与右键编辑.md`
- Create: `docs/reuse/profile_vertical_curve.md`
- Modify: `docs/modules/profile.md`
- Modify: `docs/modules/module_index.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`
- Modify: `README.md`

- [ ] **Step 1: Add business doc for creation**

Create `docs/business/profile/竖曲线_创建.md`:

```markdown
# 竖曲线创建

## 基本信息

- 功能名称：竖曲线创建
- 所属模块：纵断面设计
- 命令名称：`RD_PROFILE_VERTICAL_CURVE_CREATE`
- 对应代码入口：`ObjectArxProfileVerticalCurveCommand.*`、`ProfileVerticalCurveCreateService.*`、`DnProfileVerticalCurveEntity.*`
- 业务文档维护人：RoadProto
- 原型版本：V0.1.x
- 是否可复用：是

## 功能背景

竖曲线用于在道路纵断面上表达设计高程线。当前纵断面拉坡图已经提供地面线和图面坐标映射，本功能在拉坡图之上创建独立竖曲线实体。

## 业务目标

- 选择纵断面拉坡图后创建独立竖曲线实体。
- 默认连接地面线起终点形成设计直线。
- 为后续 PVI、半径、设计高程查询和下游联动提供对象基础。

## 输入条件

- CAD 选择对象：`DnProfileGradeGraphEntity`。
- 用户输入参数：无。
- 已有设计实体：纵断面拉坡图实体。
- 外部数据：无。

## 输出结果

- CAD 图形实体：`DnProfileVerticalCurveEntity`。
- 领域实体：`ProfileVerticalCurveData`。
- 表格或报告：无。
- 更新通知或重建请求：竖曲线依赖拉坡图的关系预留。

## 操作流程

1. 运行 `RD_PROFILE_VERTICAL_CURVE_CREATE`。
2. 选择纵断面拉坡图实体。
3. 系统读取地面线首末样本。
4. 系统创建竖曲线实体并关联拉坡图 handle。
5. 图中显示连接起终点设计高程的直线。

## 关键业务规则

- 默认竖曲线无 PVI。
- 起点取拉坡图第一个地面线样本。
- 终点取拉坡图最后一个地面线样本。
- 竖曲线是独立自定义实体，不写入拉坡图实体内部。

## 可复用性说明

- 可复用内容：默认竖曲线数据创建服务和实体持久化模式。
- 临时原型内容：第一版图形样式和标注布局。
- 正式复用前需要改造的内容：规范参数推荐和方案管理。

## 与其他模块的依赖关系

- 上游模块：纵断面拉坡图。
- 下游模块：横断面、三维模型、土方计算、排水设计。
- 实体联动行为：竖曲线依赖拉坡图，后续拉坡图变化时应标记竖曲线刷新。

## 后续对接正式 EICAD 功能的注意事项

- 后续应支持从外部纵断面设计文件导入 PVI。
- 后续应支持多设计线方案。
```

- [ ] **Step 2: Add edit and grip docs**

Create `docs/business/profile/竖曲线_编辑.md` and `docs/business/profile/竖曲线_夹点与右键编辑.md` with the same template structure. Include the exact command names from the command table and state that WPF only edits DTO fields, while C++ applies data to ObjectARX entities.

- [ ] **Step 3: Add reuse doc**

Create `docs/reuse/profile_vertical_curve.md`:

```markdown
# 竖曲线复用说明

## 能力分类

通用道路设计能力

## 能力说明

竖曲线能力用于在纵断面中维护设计高程线，支持起终点、PVI、半径、BVC/EVC、高低点、任意桩号高程和瞬时坡度查询。

## 当前实现

- 源码路径：`src/domain/profile/ProfileVerticalCurveModel.h`、`src/domain/profile/ProfileVerticalCurveCalculator.*`、`src/application/profile/ProfileVerticalCurveCreateService.*`、`src/application/profile/ProfileVerticalCurveEditService.*`
- 对外类型/函数：`ProfileVerticalCurveData`、`ProfileVerticalCurveCalculator::rebuild`、`elevationAt`、`gradeAt`、`sample`
- 当前使用该能力的命令：`RD_PROFILE_VERTICAL_CURVE_CREATE`、`RD_PROFILE_VERTICAL_CURVE_ADD_PVI`、`RD_PROFILE_VERTICAL_CURVE_DELETE_PVI`

## 可复用内容

- 对称二次抛物线竖曲线计算。
- 任意桩号设计高程和坡度查询。
- PVI 新增、删除、拖动和半径编辑。
- 独立自定义实体关联拉坡图的持久化模式。

## 不可复用或临时内容

- 第一版 WPF 窗口布局。
- 第一版右键入口。
- 第一版标注样式。

## 依赖关系

- domain 依赖：纵断面地面线样本和通用 STL。
- cad_adapter 依赖：ObjectARX 自定义实体和图面绘制。
- 模块依赖：PROFILE 命令注册。

## 扩展说明

- 可扩展非对称竖曲线。
- 可扩展竖曲线要素表。
- 可供横断面、三维模型、土方和排水设计查询设计高程。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- AutoCAD 手工验证：创建竖曲线、夹点拖动、右键新增/删除 PVI、双击编辑、DWG 保存重开。
```

- [ ] **Step 4: Update module and capability docs**

Update `docs/modules/profile.md` command table to add the five vertical curve commands. Update code landing table with the new domain/application/cad_adapter/WPF files.

Update `docs/modules/module_index.md` profile row status to mention竖曲线设计对象.

Update `docs/reuse/capability_catalog.md` with rows:

```markdown
| 纵断面竖曲线计算 | V0.1.x 原型，支持起终点、PVI、半径、BVC/EVC、高低点、任意桩号高程和坡度查询 | `src/domain/profile/ProfileVerticalCurveCalculator.*` |
| 纵断面竖曲线自定义实体 | V0.1.x 原型，独立关联拉坡图并支持夹点和 DWG 持久化 | `src/cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.*` |
| 纵断面竖曲线 WPF 编辑 Bridge | V0.1.x 原型，通过请求/响应文件编辑起终点、PVI 和半径 | `src/cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.*` |
```

- [ ] **Step 5: Update version/test/README docs after successful build**

After Task 10 verification succeeds, update `docs/dev/version_log.md`, `tests/README.md`, and `README.md` with exact version/build status from that run. Use current RoadProto naming rules in `build/RoadProto.Build.props`.

- [ ] **Step 6: Mirror docs to main project directory**

For every doc changed in this task, run:

```powershell
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\docs\business\profile\竖曲线_创建.md" -Destination "docs\business\profile\竖曲线_创建.md" -Force
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\docs\business\profile\竖曲线_编辑.md" -Destination "docs\business\profile\竖曲线_编辑.md" -Force
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\docs\business\profile\竖曲线_夹点与右键编辑.md" -Destination "docs\business\profile\竖曲线_夹点与右键编辑.md" -Force
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\docs\reuse\profile_vertical_curve.md" -Destination "docs\reuse\profile_vertical_curve.md" -Force
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\docs\modules\profile.md" -Destination "docs\modules\profile.md" -Force
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\docs\modules\module_index.md" -Destination "docs\modules\module_index.md" -Force
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\docs\reuse\capability_catalog.md" -Destination "docs\reuse\capability_catalog.md" -Force
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\docs\dev\version_log.md" -Destination "docs\dev\version_log.md" -Force
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\tests\README.md" -Destination "tests\README.md" -Force
Copy-Item -LiteralPath ".worktrees\profile-grade-graph\README.md" -Destination "README.md" -Force
```

- [ ] **Step 7: Commit Task 9**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add docs/business/profile/竖曲线_创建.md docs/business/profile/竖曲线_编辑.md docs/business/profile/竖曲线_夹点与右键编辑.md docs/reuse/profile_vertical_curve.md docs/modules/profile.md docs/modules/module_index.md docs/reuse/capability_catalog.md docs/dev/version_log.md tests/README.md README.md
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "docs: document profile vertical curve workflow"
```

---

### Task 10: Full Verification And Push

**Files:**
- No new files unless verification exposes a defect that must be fixed.

- [ ] **Step 1: Run core tests**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: build exit code `0`; test exe exit code `0`.

- [ ] **Step 2: Run ARX Debug build**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build exit code `0`.

- [ ] **Step 3: Run managed WPF Debug build**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: build exit code `0`.

- [ ] **Step 4: Run ARX Release build**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Release /p:Platform=x64
```

Expected: build exit code `0`.

- [ ] **Step 5: Run managed WPF Release build**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

Expected: build exit code `0`.

- [ ] **Step 6: Manual AutoCAD smoke test**

Run in AutoCAD 2021:

```text
ARXLOAD artifacts\x64\Debug\RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx
NETLOAD artifacts\managed\Debug\net48\RoadProto.Terrain.UI.dll
RD_PROFILE_GRADE_GRAPH_CREATE
RD_PROFILE_VERTICAL_CURVE_CREATE
RD_PROFILE_VERTICAL_CURVE_ADD_PVI
RD_PROFILE_VERTICAL_CURVE_DELETE_PVI
```

Expected manual observations:

- A grade graph can be created from existing test data.
- `RD_PROFILE_VERTICAL_CURVE_CREATE` creates a design line attached to that graph.
- Adding a PVI changes the design line into a vertical curve.
- Deleting the PVI returns to a straight design line.
- Double-click opens the vertical curve WPF edit window.
- DWG save, reopen, and `REGEN` preserve the vertical curve entity.

- [ ] **Step 7: Check process cleanup after AutoCAD test**

Run:

```powershell
Get-CimInstance Win32_Process -Filter "name='acad.exe' or name='accoreconsole.exe' or name='taskhostw.exe'" |
  Select-Object ProcessId,Name,CommandLine,@{Name='WorkingSetMB';Expression={[math]::Round($_.WorkingSetSize/1MB,1)}}
```

Expected: no unexpected hidden `acad.exe` or `accoreconsole.exe` remains after closing AutoCAD. Record any remaining process in the final notes.

- [ ] **Step 8: Verify doc mirror**

Run from main project directory:

```powershell
Compare-Object -ReferenceObject (Get-Content -Encoding UTF8 -LiteralPath ".worktrees\profile-grade-graph\docs\superpowers\plans\2026-05-12-profile-vertical-curve.md") -DifferenceObject (Get-Content -Encoding UTF8 -LiteralPath "docs\superpowers\plans\2026-05-12-profile-vertical-curve.md")
```

Expected: no output.

- [ ] **Step 9: Push branch**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" push origin codex/profile-grade-graph
```

Expected: push succeeds.

---

## Self-Review Checklist

- Spec coverage: tasks cover data structure, calculator, drawing, commands, grip editing, right-click command path, double-click WPF editing, docs, tests, build, and manual verification.
- TDD coverage: domain/application behavior is test-first. ObjectARX and WPF integration are build-first plus manual AutoCAD verification because they depend on AutoCAD runtime.
- Project wiring: both C++ project files are explicitly updated; WPF SDK project auto-includes new files.
- Worktree document mirroring: Task 9 and Task 10 include mirror verification for documentation.
- Known first-version limitation: true contextual right-click menu may remain command-driven if AutoCAD .NET context menu APIs are not reliable in the target environment; commands still satisfy add/delete PVI workflow and are documented.
