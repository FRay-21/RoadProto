# 平面布线与道路中线实体 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 `ALIGNMENT` 平面设计模块，支持平面布线、道路中线自定义实体、桩号标注、属性/平曲线参数编辑、Ribbon 入口和第一版夹点刷新。

**Architecture:** 领域层 `src/domain/alignment` 负责回旋线、五单元平曲线、桩号、属性和参数校核，不依赖 ObjectARX。`src/cad_adapter/objectarx/alignment` 负责 AutoCAD 点取、实体绘制、DWG 持久化、对话框、夹点和命令流程。`src/modules/alignment` 只注册模块、命令和 Ribbon 元数据；托管 Ribbon 插件只创建按钮和双击转发。

**Tech Stack:** C++17、ObjectARX 2021、AutoCAD 2021 x64、.NET Framework 4.8 WPF 托管 Ribbon、MSBuild、RoadProto core tests。

---

## File Structure

- Create: `src/domain/alignment/AlignmentGeometry.h`
- Create: `src/domain/alignment/AlignmentGeometry.cpp`
- Create: `src/domain/alignment/StationFormatter.h`
- Create: `src/domain/alignment/StationFormatter.cpp`
- Create: `src/domain/alignment/HorizontalAlignmentBuilder.h`
- Create: `src/domain/alignment/HorizontalAlignmentBuilder.cpp`
- Create: `src/application/alignment/AlignmentCreateService.h`
- Create: `src/application/alignment/AlignmentCreateService.cpp`
- Create: `src/modules/alignment/AlignmentModule.h`
- Create: `src/modules/alignment/AlignmentModule.cpp`
- Create: `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h`
- Create: `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.cpp`
- Create: `src/cad_adapter/objectarx/alignment/AlignmentDialogs.h`
- Create: `src/cad_adapter/objectarx/alignment/AlignmentDialogs.cpp`
- Create: `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.h`
- Create: `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.cpp`
- Create: `docs/modules/alignment.md`
- Create: `docs/business/alignment/平面布线_道路中线.md`
- Create: `docs/reuse/alignment_centerline.md`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `src/app/startup/Startup.cpp`
- Modify: `src/app/arx_entry/RoadProtoArxEntry.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Modify: `docs/rules/command_and_module_rules.md`
- Modify: `docs/modules/module_index.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`
- Modify: `README.md`
- Modify: `build/RoadProto.Build.props`

---

### Task 1: Domain Test Coverage

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Test: `tests/RoadProtoCoreTests.vcxproj`

- [ ] **Step 1: Include the alignment headers in the test file**

Add these includes near the existing domain includes:

```cpp
#include "domain/alignment/AlignmentGeometry.h"
#include "domain/alignment/HorizontalAlignmentBuilder.h"
#include "domain/alignment/StationFormatter.h"
```

- [ ] **Step 2: Add failing tests for station formatting and clothoid math**

Add these test functions before `main()`:

```cpp
void stationFormatterFormatsEngineeringStations()
{
    using roadproto::domain::alignment::StationFormatter;

    CHECK(StationFormatter::format(0.0) == L"K0+000");
    CHECK(StationFormatter::format(100.0) == L"K0+100");
    CHECK(StationFormatter::format(1234.56) == L"K1+234.560");
    CHECK(StationFormatter::format(-1.0) == L"K0+000");
}

void clothoidMathUsesRoadDesignFormulas()
{
    using namespace roadproto::domain::alignment;

    const double radius = 300.0;
    const double ls = 60.0;
    CHECK(std::fabs(clothoidA(radius, ls) * clothoidA(radius, ls) - radius * ls) < 1e-9);
    CHECK(std::fabs(clothoidCurvatureAt(30.0, radius, ls) - (30.0 / (radius * ls))) < 1e-12);
    CHECK(std::fabs(clothoidTangentAngleAt(ls, radius, ls) - (ls / (2.0 * radius))) < 1e-12);
}
```

- [ ] **Step 3: Add failing tests for five-element generation and invalid parameters**

Add this test function:

```cpp
void horizontalAlignmentBuilderCreatesFiveElements()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{100.0, 0.0},
        AlignmentPoint2d{100.0, 100.0},
    };
    input.defaultParameters.radius = 80.0;
    input.defaultParameters.ls1 = 20.0;
    input.defaultParameters.ls2 = 20.0;
    input.properties.stationLabelInterval = 100.0;

    HorizontalAlignmentBuilder builder;
    const auto result = builder.build(input);

    CHECK(result.succeeded);
    CHECK(result.alignment.elements.size() == 5);
    CHECK(result.alignment.elements[0].type == AlignmentElementType::Line);
    CHECK(result.alignment.elements[1].type == AlignmentElementType::SpiralIn);
    CHECK(result.alignment.elements[2].type == AlignmentElementType::CircularArc);
    CHECK(result.alignment.elements[3].type == AlignmentElementType::SpiralOut);
    CHECK(result.alignment.elements[4].type == AlignmentElementType::Line);
    CHECK(result.alignment.featurePoints.size() >= 6);
    CHECK(result.alignment.stationLabels.size() >= 2);
    CHECK(result.alignment.totalLength > 100.0);
}

void horizontalAlignmentBuilderRejectsImpossibleCurve()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{10.0, 0.0},
        AlignmentPoint2d{10.0, 10.0},
    };
    input.defaultParameters.radius = -5.0;

    HorizontalAlignmentBuilder builder;
    const auto result = builder.build(input);

    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());
}
```

- [ ] **Step 4: Add failing module/Ribbon registration test hook**

Add this test after the existing module registration test:

```cpp
void alignmentCommandMetadataUsesExpectedNames()
{
    roadproto::core::CommandRegistry commands;
    commands.registerCommand(roadproto::core::CommandDefinition{
        L"RD_ALIGN_CENTERLINE_CREATE",
        L"平面布线",
        L"ALIGNMENT",
        L"Creates a road centerline alignment entity.",
        &noopCommand,
        true,
        true,
        L"docs/business/alignment/平面布线_道路中线.md",
        true});

    const auto found = commands.find(L"RD_ALIGN_CENTERLINE_CREATE");
    CHECK(found.has_value());
    CHECK(found->moduleCode == L"ALIGNMENT");
    CHECK(found->displayName == L"平面布线");
    CHECK(found->ribbonAttachable);
}
```

- [ ] **Step 5: Call the new tests from `main()`**

Add these calls before the final failure check:

```cpp
    stationFormatterFormatsEngineeringStations();
    clothoidMathUsesRoadDesignFormulas();
    horizontalAlignmentBuilderCreatesFiveElements();
    horizontalAlignmentBuilderRejectsImpossibleCurve();
    alignmentCommandMetadataUsesExpectedNames();
```

- [ ] **Step 6: Add new domain `.cpp` files to core tests project**

In `tests/RoadProtoCoreTests.vcxproj`, add:

```xml
    <ClCompile Include="..\src\domain\alignment\AlignmentGeometry.cpp" />
    <ClCompile Include="..\src\domain\alignment\HorizontalAlignmentBuilder.cpp" />
    <ClCompile Include="..\src\domain\alignment\StationFormatter.cpp" />
```

- [ ] **Step 7: Run tests and verify RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `domain/alignment/*.h` do not exist yet.

---

### Task 2: Domain Alignment Model

**Files:**
- Create: `src/domain/alignment/AlignmentGeometry.h`
- Create: `src/domain/alignment/AlignmentGeometry.cpp`
- Create: `src/domain/alignment/StationFormatter.h`
- Create: `src/domain/alignment/StationFormatter.cpp`
- Test: `tests/core_tests.cpp`

- [ ] **Step 1: Create alignment geometry declarations**

Create `src/domain/alignment/AlignmentGeometry.h`:

```cpp
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace roadproto::domain::alignment {

struct AlignmentPoint2d {
    double x = 0.0;
    double y = 0.0;
};

enum class RoadGrade {
    Expressway,
    FirstClass,
    SecondClass,
    ThirdClass,
    FourthClass,
    UrbanExpressway,
    UrbanArterial,
    UrbanSubArterial,
    UrbanBranch,
    Other,
    Unclassified,
};

struct RoadCenterlineProperties {
    std::wstring roadName = L"K1";
    RoadGrade roadGrade = RoadGrade::Other;
    bool linkedTerrainEnabled = false;
    std::wstring linkedTerrainHandle;
    double stationLabelInterval = 100.0;
};

struct HorizontalCurveParameters {
    double tangentIn = 0.0;
    double tangentOut = 0.0;
    double ls1 = 20.0;
    double radius = 80.0;
    double ls2 = 20.0;
};

enum class AlignmentElementType {
    Line,
    SpiralIn,
    CircularArc,
    SpiralOut,
};

enum class AlignmentFeaturePointType {
    Start,
    End,
    PI,
    TS,
    SC,
    CS,
    ST,
    TC,
    CT,
    TangentIntersection,
    ArcMid,
};

struct AlignmentSamplePoint {
    AlignmentPoint2d point;
    double station = 0.0;
};

struct HorizontalAlignmentElement {
    AlignmentElementType type = AlignmentElementType::Line;
    AlignmentPoint2d start;
    AlignmentPoint2d end;
    double startStation = 0.0;
    double length = 0.0;
    double radius = 0.0;
    double spiralLength = 0.0;
    std::vector<AlignmentSamplePoint> samples;
};

struct HorizontalAlignmentFeaturePoint {
    AlignmentFeaturePointType type = AlignmentFeaturePointType::Start;
    AlignmentPoint2d point;
    double station = 0.0;
};

struct StationLabel {
    AlignmentPoint2d point;
    double station = 0.0;
    std::wstring text;
};

struct HorizontalAlignment {
    RoadCenterlineProperties properties;
    std::vector<AlignmentPoint2d> controlPoints;
    std::vector<HorizontalCurveParameters> curveParameters;
    std::vector<HorizontalAlignmentElement> elements;
    std::vector<HorizontalAlignmentFeaturePoint> featurePoints;
    std::vector<StationLabel> stationLabels;
    double totalLength = 0.0;
};

double distance(const AlignmentPoint2d& a, const AlignmentPoint2d& b);
double clothoidA(double radius, double spiralLength);
double clothoidCurvatureAt(double stationOnSpiral, double radius, double spiralLength);
double clothoidTangentAngleAt(double stationOnSpiral, double radius, double spiralLength);
std::wstring roadGradeToDisplayName(RoadGrade grade);
RoadGrade roadGradeFromIndex(std::size_t index);
std::vector<std::wstring> roadGradeDisplayNames();

} // namespace roadproto::domain::alignment
```

- [ ] **Step 2: Implement geometry helpers**

Create `src/domain/alignment/AlignmentGeometry.cpp`:

```cpp
#include "domain/alignment/AlignmentGeometry.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::alignment {

double distance(const AlignmentPoint2d& a, const AlignmentPoint2d& b)
{
    const auto dx = b.x - a.x;
    const auto dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

double clothoidA(double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength < 0.0) {
        return 0.0;
    }
    return std::sqrt(radius * spiralLength);
}

double clothoidCurvatureAt(double stationOnSpiral, double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength <= 0.0) {
        return 0.0;
    }
    const auto s = std::clamp(stationOnSpiral, 0.0, spiralLength);
    return s / (radius * spiralLength);
}

double clothoidTangentAngleAt(double stationOnSpiral, double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength <= 0.0) {
        return 0.0;
    }
    const auto s = std::clamp(stationOnSpiral, 0.0, spiralLength);
    return (s * s) / (2.0 * radius * spiralLength);
}

std::vector<std::wstring> roadGradeDisplayNames()
{
    return {
        L"高速公路",
        L"一级道路",
        L"二级道路",
        L"三级道路",
        L"四级道路",
        L"城市快速路",
        L"城市主干道",
        L"城市次干道",
        L"城市支路",
        L"其他道路",
        L"等外公路",
    };
}

std::wstring roadGradeToDisplayName(RoadGrade grade)
{
    const auto names = roadGradeDisplayNames();
    const auto index = static_cast<std::size_t>(grade);
    return index < names.size() ? names[index] : names[static_cast<std::size_t>(RoadGrade::Other)];
}

RoadGrade roadGradeFromIndex(std::size_t index)
{
    const auto names = roadGradeDisplayNames();
    if (index >= names.size()) {
        return RoadGrade::Other;
    }
    return static_cast<RoadGrade>(index);
}

} // namespace roadproto::domain::alignment
```

- [ ] **Step 3: Create station formatter declarations**

Create `src/domain/alignment/StationFormatter.h`:

```cpp
#pragma once

#include <string>

namespace roadproto::domain::alignment {

class StationFormatter {
public:
    static std::wstring format(double station);
};

} // namespace roadproto::domain::alignment
```

- [ ] **Step 4: Implement station formatting**

Create `src/domain/alignment/StationFormatter.cpp`:

```cpp
#include "domain/alignment/StationFormatter.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace roadproto::domain::alignment {

std::wstring StationFormatter::format(double station)
{
    const auto safeStation = std::max(0.0, station);
    const auto kilometer = static_cast<int>(std::floor(safeStation / 1000.0));
    const auto meter = safeStation - static_cast<double>(kilometer) * 1000.0;

    std::wstringstream stream;
    stream << L"K" << kilometer << L"+";
    if (meter < 100.0) {
        stream << L"0";
    }
    if (meter < 10.0) {
        stream << L"0";
    }

    const auto roundedMeter = std::round(meter);
    if (std::fabs(meter - roundedMeter) < 1e-6) {
        stream << static_cast<int>(roundedMeter);
    } else {
        stream << std::fixed << std::setprecision(3) << meter;
    }
    return stream.str();
}

} // namespace roadproto::domain::alignment
```

- [ ] **Step 5: Run tests and confirm partial GREEN/remaining RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build now fails because `HorizontalAlignmentBuilder.h` does not exist.

---

### Task 3: Horizontal Alignment Builder

**Files:**
- Create: `src/domain/alignment/HorizontalAlignmentBuilder.h`
- Create: `src/domain/alignment/HorizontalAlignmentBuilder.cpp`
- Test: `tests/core_tests.cpp`

- [ ] **Step 1: Create builder declarations**

Create `src/domain/alignment/HorizontalAlignmentBuilder.h`:

```cpp
#pragma once

#include "domain/alignment/AlignmentGeometry.h"

#include <string>

namespace roadproto::domain::alignment {

struct HorizontalAlignmentInput {
    RoadCenterlineProperties properties;
    std::vector<AlignmentPoint2d> controlPoints;
    HorizontalCurveParameters defaultParameters;
    std::vector<HorizontalCurveParameters> curveParameters;
};

struct HorizontalAlignmentBuildResult {
    bool succeeded = false;
    std::wstring errorMessage;
    HorizontalAlignment alignment;
};

class HorizontalAlignmentBuilder {
public:
    HorizontalAlignmentBuildResult build(const HorizontalAlignmentInput& input) const;

private:
    static bool validate(const HorizontalAlignmentInput& input, std::wstring& errorMessage);
};

} // namespace roadproto::domain::alignment
```

- [ ] **Step 2: Implement helper math and sampling**

Create `src/domain/alignment/HorizontalAlignmentBuilder.cpp` with this structure:

```cpp
#include "domain/alignment/HorizontalAlignmentBuilder.h"

#include "domain/alignment/StationFormatter.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::alignment {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1e-8;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
Vec2 operator*(Vec2 a, double scale) { return {a.x * scale, a.y * scale}; }

Vec2 toVec(const AlignmentPoint2d& point) { return {point.x, point.y}; }
AlignmentPoint2d toPoint(Vec2 value) { return {value.x, value.y}; }

double length(Vec2 value)
{
    return std::sqrt(value.x * value.x + value.y * value.y);
}

Vec2 normalized(Vec2 value)
{
    const auto len = length(value);
    if (len <= kEpsilon) {
        return {};
    }
    return {value.x / len, value.y / len};
}

double dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

double cross(Vec2 a, Vec2 b)
{
    return a.x * b.y - a.y * b.x;
}

Vec2 rotate(Vec2 value, double angle)
{
    const auto c = std::cos(angle);
    const auto s = std::sin(angle);
    return {value.x * c - value.y * s, value.x * s + value.y * c};
}

std::vector<AlignmentSamplePoint> sampleLine(const AlignmentPoint2d& start, const AlignmentPoint2d& end, double startStation)
{
    const auto len = distance(start, end);
    const auto divisions = std::max(1, static_cast<int>(std::ceil(len / 10.0)));
    std::vector<AlignmentSamplePoint> samples;
    samples.reserve(static_cast<std::size_t>(divisions) + 1);
    for (int i = 0; i <= divisions; ++i) {
        const auto t = static_cast<double>(i) / static_cast<double>(divisions);
        samples.push_back({
            AlignmentPoint2d{start.x + (end.x - start.x) * t, start.y + (end.y - start.y) * t},
            startStation + len * t});
    }
    return samples;
}

void appendStationLabels(HorizontalAlignment& alignment, double interval)
{
    if (interval <= 0.0 || alignment.elements.empty()) {
        return;
    }
    for (double station = 0.0; station <= alignment.totalLength + 1e-6; station += interval) {
        AlignmentSamplePoint nearest = alignment.elements.front().samples.front();
        double best = std::fabs(nearest.station - station);
        for (const auto& element : alignment.elements) {
            for (const auto& sample : element.samples) {
                const auto delta = std::fabs(sample.station - station);
                if (delta < best) {
                    best = delta;
                    nearest = sample;
                }
            }
        }
        alignment.stationLabels.push_back(StationLabel{nearest.point, station, StationFormatter::format(station)});
    }
}

} // namespace
```

- [ ] **Step 3: Implement validation**

Add this validation function in the same `.cpp`:

```cpp
bool HorizontalAlignmentBuilder::validate(const HorizontalAlignmentInput& input, std::wstring& errorMessage)
{
    if (input.controlPoints.size() < 3) {
        errorMessage = L"平面布线至少需要起点、交点和第三点。";
        return false;
    }
    if (input.defaultParameters.radius <= 0.0) {
        errorMessage = L"圆曲线半径必须大于 0。";
        return false;
    }
    if (input.defaultParameters.ls1 < 0.0 || input.defaultParameters.ls2 < 0.0) {
        errorMessage = L"缓和曲线长度不能为负。";
        return false;
    }
    for (std::size_t i = 1; i < input.controlPoints.size(); ++i) {
        if (distance(input.controlPoints[i - 1], input.controlPoints[i]) <= kEpsilon) {
            errorMessage = L"控制点不能重合。";
            return false;
        }
    }
    return true;
}
```

- [ ] **Step 4: Implement first-version five-element build**

Add this `build` implementation. It uses practical sampled geometry and keeps all ObjectARX-free:

```cpp
HorizontalAlignmentBuildResult HorizontalAlignmentBuilder::build(const HorizontalAlignmentInput& input) const
{
    HorizontalAlignmentBuildResult result;
    std::wstring error;
    if (!validate(input, error)) {
        result.errorMessage = error;
        return result;
    }

    HorizontalAlignment alignment;
    alignment.properties = input.properties;
    alignment.controlPoints = input.controlPoints;
    alignment.curveParameters = input.curveParameters;

    const auto& p0 = input.controlPoints[0];
    const auto& pi = input.controlPoints[1];
    const auto& p2 = input.controlPoints[2];
    const auto inDir = normalized(toVec(pi) - toVec(p0));
    const auto outDir = normalized(toVec(p2) - toVec(pi));
    const auto incomingToPi = normalized(toVec(p0) - toVec(pi));
    const auto turnAngle = std::acos(std::clamp(dot(incomingToPi, outDir), -1.0, 1.0));
    if (turnAngle <= kEpsilon || std::fabs(kPi - turnAngle) <= kEpsilon) {
        result.errorMessage = L"三点接近共线，无法生成五单元平曲线。";
        return result;
    }

    const auto params = input.curveParameters.empty() ? input.defaultParameters : input.curveParameters.front();
    if (params.radius <= 0.0 || params.ls1 < 0.0 || params.ls2 < 0.0) {
        result.errorMessage = L"平曲线参数无效。";
        return result;
    }

    const auto beta1 = clothoidTangentAngleAt(params.ls1, params.radius, params.ls1);
    const auto beta2 = clothoidTangentAngleAt(params.ls2, params.radius, params.ls2);
    const auto circularAngle = std::max(0.0, turnAngle - beta1 - beta2);
    const auto arcLength = params.radius * circularAngle;
    const auto tangentDistance = params.radius * std::tan(turnAngle / 2.0) + (params.ls1 + params.ls2) / 2.0;
    const auto availableIn = distance(p0, pi);
    const auto availableOut = distance(pi, p2);
    if (tangentDistance >= availableIn || tangentDistance >= availableOut) {
        result.errorMessage = L"切线长度不足，无法布设当前半径和缓和曲线长度。";
        return result;
    }

    const auto ts = toPoint(toVec(pi) - inDir * tangentDistance);
    const auto st = toPoint(toVec(pi) + outDir * tangentDistance);
    const auto sc = toPoint(toVec(ts) + inDir * params.ls1);
    const auto cs = toPoint(toVec(st) - outDir * params.ls2);

    double station = 0.0;
    auto addElement = [&](AlignmentElementType type, AlignmentPoint2d start, AlignmentPoint2d end, double len, double radius, double spiralLength) {
        HorizontalAlignmentElement element;
        element.type = type;
        element.start = start;
        element.end = end;
        element.startStation = station;
        element.length = len;
        element.radius = radius;
        element.spiralLength = spiralLength;
        element.samples = sampleLine(start, end, station);
        alignment.elements.push_back(std::move(element));
        station += len;
    };

    addElement(AlignmentElementType::Line, p0, ts, distance(p0, ts), 0.0, 0.0);
    addElement(AlignmentElementType::SpiralIn, ts, sc, params.ls1, params.radius, params.ls1);
    addElement(AlignmentElementType::CircularArc, sc, cs, arcLength, params.radius, 0.0);
    addElement(AlignmentElementType::SpiralOut, cs, st, params.ls2, params.radius, params.ls2);
    addElement(AlignmentElementType::Line, st, p2, distance(st, p2), 0.0, 0.0);
    alignment.totalLength = station;
    alignment.curveParameters = {params};

    alignment.featurePoints = {
        {AlignmentFeaturePointType::Start, p0, 0.0},
        {AlignmentFeaturePointType::PI, pi, distance(p0, ts)},
        {AlignmentFeaturePointType::TS, ts, alignment.elements[0].startStation + alignment.elements[0].length},
        {AlignmentFeaturePointType::SC, sc, alignment.elements[1].startStation + alignment.elements[1].length},
        {AlignmentFeaturePointType::CS, cs, alignment.elements[2].startStation + alignment.elements[2].length},
        {AlignmentFeaturePointType::ST, st, alignment.elements[3].startStation + alignment.elements[3].length},
        {AlignmentFeaturePointType::ArcMid, toPoint((toVec(sc) + toVec(cs)) * 0.5), alignment.elements[2].startStation + alignment.elements[2].length / 2.0},
        {AlignmentFeaturePointType::End, p2, alignment.totalLength},
    };

    appendStationLabels(alignment, alignment.properties.stationLabelInterval);
    result.succeeded = true;
    result.alignment = std::move(alignment);
    return result;
}
```

- [ ] **Step 5: Run tests and verify GREEN**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: executable prints `All RoadProto core tests passed.`

- [ ] **Step 6: Commit if git is available**

If this directory is later converted to a git worktree, commit:

```powershell
git add tests src/domain/alignment
git commit -m "feat: add alignment domain builder"
```

---

### Task 4: Application Service and Alignment Module

**Files:**
- Create: `src/application/alignment/AlignmentCreateService.h`
- Create: `src/application/alignment/AlignmentCreateService.cpp`
- Create: `src/modules/alignment/AlignmentModule.h`
- Create: `src/modules/alignment/AlignmentModule.cpp`
- Modify: `src/app/startup/Startup.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Test: `tests/RoadProtoCoreTests.vcxproj`

- [ ] **Step 1: Create application service**

Create `src/application/alignment/AlignmentCreateService.h`:

```cpp
#pragma once

#include "domain/alignment/HorizontalAlignmentBuilder.h"

namespace roadproto::application {

class AlignmentCreateService {
public:
    domain::alignment::HorizontalAlignmentBuildResult build(
        const domain::alignment::HorizontalAlignmentInput& input) const;
};

} // namespace roadproto::application
```

Create `src/application/alignment/AlignmentCreateService.cpp`:

```cpp
#include "application/alignment/AlignmentCreateService.h"

namespace roadproto::application {

domain::alignment::HorizontalAlignmentBuildResult AlignmentCreateService::build(
    const domain::alignment::HorizontalAlignmentInput& input) const
{
    domain::alignment::HorizontalAlignmentBuilder builder;
    return builder.build(input);
}

} // namespace roadproto::application
```

- [ ] **Step 2: Create module header**

Create `src/modules/alignment/AlignmentModule.h`:

```cpp
#pragma once

#include "core/module/ModuleRegistry.h"

namespace roadproto::modules::alignment {

core::ModuleDefinition createAlignmentModule();

} // namespace roadproto::modules::alignment
```

- [ ] **Step 3: Create module implementation**

Create `src/modules/alignment/AlignmentModule.cpp`:

```cpp
#include "modules/alignment/AlignmentModule.h"

#include "cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::alignment {
namespace {

void registerAlignmentCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_ALIGN_CENTERLINE_CREATE",
        L"平面布线",
        L"ALIGNMENT",
        L"Creates a road centerline alignment entity from picked control points.",
        cad_adapter::objectarx::alignmentCenterlineCreateCommandProcedure(),
        true,
        true,
        L"docs/business/alignment/平面布线_道路中线.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_ALIGN_CURVE_PARAM_EDIT",
        L"编辑平曲线参数",
        L"ALIGNMENT",
        L"Edits horizontal curve tangent, spiral and radius parameters for a road centerline.",
        cad_adapter::objectarx::alignmentCurveParamEditCommandProcedure(),
        true,
        true,
        L"docs/business/alignment/平面布线_道路中线.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_ALIGN_CENTERLINE_EDIT_HANDLE",
        L"按 handle 编辑道路中线",
        L"ALIGNMENT",
        L"Internal double-click bridge command that edits road centerline properties by handle.",
        cad_adapter::objectarx::alignmentCenterlineEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/alignment/平面布线_道路中线.md",
        false});
}

void registerAlignmentRibbon(ui::RibbonModel& ribbonModel)
{
    ribbonModel.ensurePanel(L"ALIGNMENT", L"平面设计");
}

} // namespace

core::ModuleDefinition createAlignmentModule()
{
    return core::ModuleDefinition{
        L"Alignment",
        L"ALIGNMENT",
        L"Horizontal alignment and road centerline prototype module.",
        []() { return true; },
        []() { return true; },
        &registerAlignmentCommands,
        &registerAlignmentRibbon,
        L"docs/modules/alignment.md"};
}

} // namespace roadproto::modules::alignment
```

- [ ] **Step 4: Register module in startup**

Modify `src/app/startup/Startup.cpp`:

```cpp
#include "modules/alignment/AlignmentModule.h"
```

Then add alignment before intersection:

```cpp
    moduleRegistry.registerModule(modules::alignment::createAlignmentModule());
```

- [ ] **Step 5: Add files to ARX project**

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\domain\alignment\AlignmentGeometry.cpp" />
    <ClCompile Include="..\domain\alignment\HorizontalAlignmentBuilder.cpp" />
    <ClCompile Include="..\domain\alignment\StationFormatter.cpp" />
    <ClCompile Include="..\application\alignment\AlignmentCreateService.cpp" />
    <ClCompile Include="..\modules\alignment\AlignmentModule.cpp" />
```

- [ ] **Step 6: Run build and verify expected linker RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails because `ObjectArxAlignmentCommand.h` and command procedure definitions do not exist yet.

---

### Task 5: ObjectARX Command Stubs

**Files:**
- Create: `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.h`
- Create: `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Test: ARX Debug build

- [ ] **Step 1: Create command header**

Create `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.h`:

```cpp
#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx {

core::CommandProcedure alignmentCenterlineCreateCommandProcedure();
core::CommandProcedure alignmentCurveParamEditCommandProcedure();
core::CommandProcedure alignmentCenterlineEditHandleCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx
```

- [ ] **Step 2: Create command stub implementation**

Create `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.cpp`:

```cpp
#include "cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.h"

#include "app/startup/ApplicationContext.h"

namespace roadproto::cad_adapter::objectarx {
namespace {

void runAlignmentCenterlineCreateCommand()
{
    app::ApplicationContext::instance().editor().writeMessage(
        L"RD_ALIGN_CENTERLINE_CREATE: 平面布线命令已注册，实体创建将在后续任务接入。");
}

void runAlignmentCurveParamEditCommand()
{
    app::ApplicationContext::instance().editor().writeMessage(
        L"RD_ALIGN_CURVE_PARAM_EDIT: 平曲线参数编辑命令已注册，实体选择将在后续任务接入。");
}

void runAlignmentCenterlineEditHandleCommand()
{
    app::ApplicationContext::instance().editor().writeMessage(
        L"RD_ALIGN_CENTERLINE_EDIT_HANDLE: 双击编辑入口已注册，handle 编辑将在后续任务接入。");
}

} // namespace

core::CommandProcedure alignmentCenterlineCreateCommandProcedure()
{
    return &runAlignmentCenterlineCreateCommand;
}

core::CommandProcedure alignmentCurveParamEditCommandProcedure()
{
    return &runAlignmentCurveParamEditCommand;
}

core::CommandProcedure alignmentCenterlineEditHandleCommandProcedure()
{
    return &runAlignmentCenterlineEditHandleCommand;
}

} // namespace roadproto::cad_adapter::objectarx
```

- [ ] **Step 3: Add command source to ARX project**

Add to `src/app/RoadProtoArx.vcxproj`:

```xml
    <ClCompile Include="..\cad_adapter\objectarx\alignment\ObjectArxAlignmentCommand.cpp" />
```

- [ ] **Step 4: Run ARX build and verify GREEN**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: Debug ARX builds successfully. If ObjectARX paths are unavailable, record the exact MSBuild error and continue with domain/core tests.

---

### Task 6: Road Centerline Custom Entity

**Files:**
- Create: `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h`
- Create: `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.cpp`
- Modify: `src/app/arx_entry/RoadProtoArxEntry.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Test: ARX Debug build

- [ ] **Step 1: Create entity header**

Create `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h`:

```cpp
#pragma once

#include "domain/alignment/HorizontalAlignmentBuilder.h"

#include "dbents.h"
#include "dbmain.h"

class DnRoadCenterlineEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnRoadCenterlineEntity);

    DnRoadCenterlineEntity();

    void setAlignment(const roadproto::domain::alignment::HorizontalAlignment& alignment);
    const roadproto::domain::alignment::HorizontalAlignment& alignment() const;
    void setProperties(const roadproto::domain::alignment::RoadCenterlineProperties& properties);
    roadproto::domain::alignment::RoadCenterlineProperties properties() const;

    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* filer) override;
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* filer) const override;

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* worldDraw) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& transform) override;
    Acad::ErrorStatus subGetGripPoints(
        AcGePoint3dArray& gripPoints,
        AcDbIntArray& osnapModes,
        AcDbIntArray& geomIds) const override;
    Acad::ErrorStatus subMoveGripPointsAt(const AcDbIntArray& indices, const AcGeVector3d& offset) override;

private:
    roadproto::domain::alignment::HorizontalAlignment alignment_;
};

namespace roadproto::cad_adapter::objectarx {

void initializeRoadCenterlineEntityClass();
void uninitializeRoadCenterlineEntityClass();

} // namespace roadproto::cad_adapter::objectarx
```

- [ ] **Step 2: Create entity implementation shell**

Create `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.cpp` with `ACRX_DXF_DEFINE_MEMBERS` and simple read/write helpers:

```cpp
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"

#include "acgi.h"
#include "dbproxy.h"
#include "geassign.h"
#include "rxregsvc.h"

#include <algorithm>

using roadproto::domain::alignment::AlignmentPoint2d;
using roadproto::domain::alignment::HorizontalAlignment;
using roadproto::domain::alignment::HorizontalAlignmentBuilder;
using roadproto::domain::alignment::HorizontalAlignmentInput;
using roadproto::domain::alignment::RoadCenterlineProperties;

ACRX_DXF_DEFINE_MEMBERS(
    DnRoadCenterlineEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNROADCENTERLINEENTITY,
    "RoadProto Road Centerline");

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;

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

} // namespace
```

- [ ] **Step 3: Implement setters and DWG persistence**

Add these methods:

```cpp
DnRoadCenterlineEntity::DnRoadCenterlineEntity() = default;

void DnRoadCenterlineEntity::setAlignment(const HorizontalAlignment& alignment)
{
    assertWriteEnabled();
    alignment_ = alignment;
}

const HorizontalAlignment& DnRoadCenterlineEntity::alignment() const
{
    assertReadEnabled();
    return alignment_;
}

void DnRoadCenterlineEntity::setProperties(const RoadCenterlineProperties& properties)
{
    assertWriteEnabled();
    alignment_.properties = properties;
}

RoadCenterlineProperties DnRoadCenterlineEntity::properties() const
{
    assertReadEnabled();
    return alignment_.properties;
}

Acad::ErrorStatus DnRoadCenterlineEntity::dwgInFields(AcDbDwgFiler* filer)
{
    assertWriteEnabled();
    auto status = AcDbEntity::dwgInFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    Adesk::Int16 version = 0;
    filer->readInt16(&version);
    alignment_.properties.roadName = readWideString(filer);
    Adesk::Int32 grade = 0;
    filer->readInt32(&grade);
    alignment_.properties.roadGrade = static_cast<roadproto::domain::alignment::RoadGrade>(grade);
    Adesk::Int8 linked = 0;
    filer->readInt8(&linked);
    alignment_.properties.linkedTerrainEnabled = linked != 0;
    alignment_.properties.linkedTerrainHandle = readWideString(filer);
    filer->readDouble(&alignment_.properties.stationLabelInterval);

    Adesk::Int32 pointCount = 0;
    filer->readInt32(&pointCount);
    alignment_.controlPoints.clear();
    for (Adesk::Int32 i = 0; i < pointCount; ++i) {
        AlignmentPoint2d point;
        filer->readDouble(&point.x);
        filer->readDouble(&point.y);
        alignment_.controlPoints.push_back(point);
    }

    HorizontalAlignmentInput input;
    input.properties = alignment_.properties;
    input.controlPoints = alignment_.controlPoints;
    HorizontalAlignmentBuilder builder;
    const auto rebuilt = builder.build(input);
    if (rebuilt.succeeded) {
        alignment_ = rebuilt.alignment;
    }
    return filer->filerStatus();
}

Acad::ErrorStatus DnRoadCenterlineEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, alignment_.properties.roadName);
    filer->writeInt32(static_cast<Adesk::Int32>(alignment_.properties.roadGrade));
    filer->writeInt8(alignment_.properties.linkedTerrainEnabled ? 1 : 0);
    writeWideString(filer, alignment_.properties.linkedTerrainHandle);
    filer->writeDouble(alignment_.properties.stationLabelInterval);
    filer->writeInt32(static_cast<Adesk::Int32>(alignment_.controlPoints.size()));
    for (const auto& point : alignment_.controlPoints) {
        filer->writeDouble(point.x);
        filer->writeDouble(point.y);
    }
    return filer->filerStatus();
}
```

- [ ] **Step 4: Implement simple drawing, extents, transform, grips**

Add:

```cpp
Adesk::Boolean DnRoadCenterlineEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr) {
        return Adesk::kTrue;
    }

    worldDraw->subEntityTraits().setColor(5);
    for (const auto& element : alignment_.elements) {
        if (element.samples.size() < 2) {
            continue;
        }
        for (std::size_t i = 1; i < element.samples.size(); ++i) {
            AcGePoint3d line[2] = {
                AcGePoint3d(element.samples[i - 1].point.x, element.samples[i - 1].point.y, 0.0),
                AcGePoint3d(element.samples[i].point.x, element.samples[i].point.y, 0.0)};
            worldDraw->geometry().polyline(2, line);
        }
    }

    worldDraw->subEntityTraits().setColor(2);
    for (const auto& feature : alignment_.featurePoints) {
        AcGePoint3d marker[2] = {
            AcGePoint3d(feature.point.x - 0.5, feature.point.y, 0.0),
            AcGePoint3d(feature.point.x + 0.5, feature.point.y, 0.0)};
        worldDraw->geometry().polyline(2, marker);
    }
    return Adesk::kTrue;
}

Acad::ErrorStatus DnRoadCenterlineEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    bool hasPoint = false;
    for (const auto& element : alignment_.elements) {
        for (const auto& sample : element.samples) {
            extents.addPoint(AcGePoint3d(sample.point.x, sample.point.y, 0.0));
            hasPoint = true;
        }
    }
    return hasPoint ? Acad::eOk : Acad::eInvalidExtents;
}

Acad::ErrorStatus DnRoadCenterlineEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    assertWriteEnabled();
    for (auto& point : alignment_.controlPoints) {
        AcGePoint3d transformed(point.x, point.y, 0.0);
        transformed.transformBy(transform);
        point.x = transformed.x;
        point.y = transformed.y;
    }
    HorizontalAlignmentInput input;
    input.properties = alignment_.properties;
    input.controlPoints = alignment_.controlPoints;
    HorizontalAlignmentBuilder builder;
    const auto rebuilt = builder.build(input);
    if (rebuilt.succeeded) {
        alignment_ = rebuilt.alignment;
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadCenterlineEntity::subGetGripPoints(
    AcGePoint3dArray& gripPoints,
    AcDbIntArray&,
    AcDbIntArray&) const
{
    assertReadEnabled();
    for (const auto& point : alignment_.controlPoints) {
        gripPoints.append(AcGePoint3d(point.x, point.y, 0.0));
    }
    for (const auto& feature : alignment_.featurePoints) {
        gripPoints.append(AcGePoint3d(feature.point.x, feature.point.y, 0.0));
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadCenterlineEntity::subMoveGripPointsAt(const AcDbIntArray& indices, const AcGeVector3d& offset)
{
    assertWriteEnabled();
    for (int i = 0; i < indices.length(); ++i) {
        const auto index = indices.at(i);
        if (index >= 0 && static_cast<std::size_t>(index) < alignment_.controlPoints.size()) {
            alignment_.controlPoints[static_cast<std::size_t>(index)].x += offset.x;
            alignment_.controlPoints[static_cast<std::size_t>(index)].y += offset.y;
        }
    }
    HorizontalAlignmentInput input;
    input.properties = alignment_.properties;
    input.controlPoints = alignment_.controlPoints;
    HorizontalAlignmentBuilder builder;
    const auto rebuilt = builder.build(input);
    if (rebuilt.succeeded) {
        alignment_ = rebuilt.alignment;
    }
    return Acad::eOk;
}
```

- [ ] **Step 5: Register entity class**

Add class initialization to `DnRoadCenterlineEntity.cpp`:

```cpp
namespace roadproto::cad_adapter::objectarx {

void initializeRoadCenterlineEntityClass()
{
    DnRoadCenterlineEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeRoadCenterlineEntityClass()
{
    deleteAcRxClass(DnRoadCenterlineEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
```

- [ ] **Step 6: Wire class registration in ARX entry**

Modify `src/app/arx_entry/RoadProtoArxEntry.cpp`:

```cpp
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"
```

In `kInitAppMsg`, after terrain entity init:

```cpp
        cad_adapter::objectarx::initializeRoadCenterlineEntityClass();
```

In `kUnloadAppMsg`, before terrain uninit:

```cpp
        cad_adapter::objectarx::uninitializeRoadCenterlineEntityClass();
```

- [ ] **Step 7: Add entity source to project and build**

Add to `src/app/RoadProtoArx.vcxproj`:

```xml
    <ClCompile Include="..\cad_adapter\objectarx\alignment\DnRoadCenterlineEntity.cpp" />
```

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: Debug ARX builds successfully or reports only environment-specific ObjectARX path/toolset errors.

---

### Task 7: ObjectARX Create and Edit Commands

**Files:**
- Modify: `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.cpp`
- Create: `src/cad_adapter/objectarx/alignment/AlignmentDialogs.h`
- Create: `src/cad_adapter/objectarx/alignment/AlignmentDialogs.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Test: ARX Debug build

- [ ] **Step 1: Create dialog data contracts**

Create `src/cad_adapter/objectarx/alignment/AlignmentDialogs.h`:

```cpp
#pragma once

#include "domain/alignment/HorizontalAlignmentBuilder.h"

namespace roadproto::cad_adapter::objectarx {

struct RoadCenterlinePropertiesDialogInput {
    domain::alignment::RoadCenterlineProperties properties;
};

struct RoadCenterlinePropertiesDialogResult {
    bool accepted = false;
    domain::alignment::RoadCenterlineProperties properties;
};

struct HorizontalCurveParamDialogInput {
    domain::alignment::HorizontalCurveParameters parameters;
};

struct HorizontalCurveParamDialogResult {
    bool accepted = false;
    domain::alignment::HorizontalCurveParameters parameters;
};

bool showRoadCenterlinePropertiesDialog(
    const RoadCenterlinePropertiesDialogInput& input,
    RoadCenterlinePropertiesDialogResult& result);

bool showHorizontalCurveParamDialog(
    const HorizontalCurveParamDialogInput& input,
    HorizontalCurveParamDialogResult& result);

} // namespace roadproto::cad_adapter::objectarx
```

- [ ] **Step 2: Add first-version non-UI dialog implementation**

Create `src/cad_adapter/objectarx/alignment/AlignmentDialogs.cpp`:

```cpp
#include "cad_adapter/objectarx/alignment/AlignmentDialogs.h"

namespace roadproto::cad_adapter::objectarx {

bool showRoadCenterlinePropertiesDialog(
    const RoadCenterlinePropertiesDialogInput& input,
    RoadCenterlinePropertiesDialogResult& result)
{
    result.accepted = true;
    result.properties = input.properties;
    if (result.properties.roadName.empty()) {
        result.properties.roadName = L"K1";
    }
    if (result.properties.stationLabelInterval <= 0.0) {
        result.properties.stationLabelInterval = 100.0;
    }
    return true;
}

bool showHorizontalCurveParamDialog(
    const HorizontalCurveParamDialogInput& input,
    HorizontalCurveParamDialogResult& result)
{
    result.accepted = true;
    result.parameters = input.parameters;
    return true;
}

} // namespace roadproto::cad_adapter::objectarx
```

This keeps the first ARX build stable. A later task can replace this with Win32 controls matching `TerrainTinCreateDialog`.

- [ ] **Step 3: Implement model-space append and point-picking command path**

Replace the stub create command in `ObjectArxAlignmentCommand.cpp` with code using `acedGetPoint`, `AlignmentCreateService`, and `DnRoadCenterlineEntity`. Include:

```cpp
#include "application/alignment/AlignmentCreateService.h"
#include "cad_adapter/objectarx/alignment/AlignmentDialogs.h"
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"

#include "aced.h"
#include "dbsymtb.h"
```

Add helpers:

```cpp
bool appendEntityToModelSpace(AcDbEntity* entity, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr) {
        return false;
    }

    AcDbBlockTable* blockTable = nullptr;
    if (database->getBlockTable(blockTable, AcDb::kForRead) != Acad::eOk || blockTable == nullptr) {
        return false;
    }

    AcDbBlockTableRecord* modelSpace = nullptr;
    const auto status = blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForWrite);
    blockTable->close();
    if (status != Acad::eOk || modelSpace == nullptr) {
        return false;
    }

    const auto appendStatus = modelSpace->appendAcDbEntity(entityId, entity);
    modelSpace->close();
    return appendStatus == Acad::eOk;
}

bool getPoint(const wchar_t* prompt, roadproto::domain::alignment::AlignmentPoint2d& point)
{
    ads_point raw;
    if (acedGetPoint(nullptr, prompt, raw) != RTNORM) {
        return false;
    }
    point.x = raw[0];
    point.y = raw[1];
    return true;
}
```

- [ ] **Step 4: Implement `runAlignmentCenterlineCreateCommand`**

Use this first-version flow:

```cpp
void runAlignmentCenterlineCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_ALIGN_CENTERLINE_CREATE: 依次选择起点、交点和第三点。");

    using namespace domain::alignment;
    AlignmentPoint2d start;
    AlignmentPoint2d pi;
    AlignmentPoint2d end;
    if (!getPoint(L"\n请选择路线起点: ", start)
        || !getPoint(L"\n请选择路线交点: ", pi)
        || !getPoint(L"\n请选择第三点: ", end)) {
        editor.writeWarning(L"平面布线已取消。");
        return;
    }

    HorizontalAlignmentInput input;
    input.controlPoints = {start, pi, end};
    input.properties.roadName = L"K1";
    input.properties.stationLabelInterval = 100.0;

    application::AlignmentCreateService service;
    auto result = service.build(input);
    if (!result.succeeded) {
        editor.writeError(L"平面布线失败: " + result.errorMessage);
        return;
    }

    RoadCenterlinePropertiesDialogInput dialogInput;
    dialogInput.properties = result.alignment.properties;
    RoadCenterlinePropertiesDialogResult dialogResult;
    if (!showRoadCenterlinePropertiesDialog(dialogInput, dialogResult)) {
        editor.writeWarning(L"道路中线创建已取消。");
        return;
    }

    input.properties = dialogResult.properties;
    result = service.build(input);
    if (!result.succeeded) {
        editor.writeError(L"道路中线属性更新后重建失败: " + result.errorMessage);
        return;
    }

    auto* entity = new DnRoadCenterlineEntity();
    entity->setAlignment(result.alignment);
    AcDbObjectId entityId;
    if (!appendEntityToModelSpace(entity, entityId)) {
        delete entity;
        editor.writeError(L"插入 DnRoadCenterlineEntity 失败。");
        return;
    }
    entity->close();
    editor.writeMessage(L"已创建道路中线自定义实体。");
}
```

- [ ] **Step 5: Add source to project and build**

Add to `src/app/RoadProtoArx.vcxproj`:

```xml
    <ClCompile Include="..\cad_adapter\objectarx\alignment\AlignmentDialogs.cpp" />
```

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: Debug ARX builds.

---

### Task 8: Entity Selection, Handle Edit, and Curve Parameter Edit

**Files:**
- Modify: `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.cpp`
- Modify: `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h`
- Modify: `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.cpp`
- Test: ARX Debug build

- [ ] **Step 1: Add entity mutator for curve parameters**

In `DnRoadCenterlineEntity.h`, add:

```cpp
    bool rebuildWithCurveParameters(const roadproto::domain::alignment::HorizontalCurveParameters& parameters);
```

In `.cpp`, implement:

```cpp
bool DnRoadCenterlineEntity::rebuildWithCurveParameters(
    const roadproto::domain::alignment::HorizontalCurveParameters& parameters)
{
    assertWriteEnabled();
    roadproto::domain::alignment::HorizontalAlignmentInput input;
    input.properties = alignment_.properties;
    input.controlPoints = alignment_.controlPoints;
    input.defaultParameters = parameters;
    input.curveParameters = {parameters};
    roadproto::domain::alignment::HorizontalAlignmentBuilder builder;
    const auto rebuilt = builder.build(input);
    if (!rebuilt.succeeded) {
        return false;
    }
    alignment_ = rebuilt.alignment;
    return true;
}
```

- [ ] **Step 2: Add selection helper**

In `ObjectArxAlignmentCommand.cpp`, add:

```cpp
bool findRoadCenterlineEntityInSelectionSet(const ads_name selectionSet, AcDbObjectId& entityId)
{
    Adesk::Int32 length = 0;
    if (acedSSLength(selectionSet, &length) != RTNORM || length <= 0) {
        return false;
    }
    for (Adesk::Int32 i = 0; i < length; ++i) {
        ads_name entityName;
        if (acedSSName(selectionSet, i, entityName) != RTNORM) {
            continue;
        }
        AcDbObjectId candidateId;
        if (acdbGetObjectId(candidateId, entityName) != Acad::eOk) {
            continue;
        }
        DnRoadCenterlineEntity* entity = nullptr;
        if (acdbOpenObject(entity, candidateId, AcDb::kForRead) == Acad::eOk && entity != nullptr) {
            entity->close();
            entityId = candidateId;
            return true;
        }
    }
    return false;
}

bool selectRoadCenterlineEntity(AcDbObjectId& entityId)
{
    ads_name selectionSet;
    if (acedSSGet(nullptr, nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }
    SelectionSetGuard guard(selectionSet);
    return findRoadCenterlineEntityInSelectionSet(selectionSet, entityId);
}
```

Add include:

```cpp
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
```

- [ ] **Step 3: Implement curve parameter edit command**

Replace stub:

```cpp
void runAlignmentCurveParamEditCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    AcDbObjectId entityId;
    editor.writeMessage(L"RD_ALIGN_CURVE_PARAM_EDIT: 请选择道路中线自定义实体。");
    if (!selectRoadCenterlineEntity(entityId)) {
        editor.writeWarning(L"未选择道路中线自定义实体。");
        return;
    }

    DnRoadCenterlineEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开道路中线自定义实体。");
        return;
    }

    HorizontalCurveParamDialogInput input;
    const auto& alignment = entity->alignment();
    if (!alignment.curveParameters.empty()) {
        input.parameters = alignment.curveParameters.front();
    }

    HorizontalCurveParamDialogResult result;
    if (!showHorizontalCurveParamDialog(input, result)) {
        entity->close();
        editor.writeWarning(L"平曲线参数编辑已取消。");
        return;
    }

    if (!entity->rebuildWithCurveParameters(result.parameters)) {
        entity->close();
        editor.writeError(L"平曲线参数无效，无法重建道路中线。");
        return;
    }

    entity->recordGraphicsModified(true);
    entity->close();
    editor.writeMessage(L"道路中线平曲线参数已更新。");
}
```

- [ ] **Step 4: Implement handle edit command**

Add helper:

```cpp
bool resolveObjectIdFromHandle(const std::wstring& handleText, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr || handleText.empty()) {
        return false;
    }
    AcDbHandle handle(handleText.c_str());
    return database->getAcDbObjectId(entityId, false, handle) == Acad::eOk && !entityId.isNull();
}
```

Replace stub:

```cpp
void runAlignmentCenterlineEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR handleBuffer[64] = {};
    if (acedGetString(Adesk::kFalse, L"\n道路中线 handle: ", handleBuffer) != RTNORM) {
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handleBuffer, entityId)) {
        editor.writeWarning(L"未找到 handle 对应的道路中线实体。");
        return;
    }

    DnRoadCenterlineEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开道路中线实体。");
        return;
    }

    RoadCenterlinePropertiesDialogInput input;
    input.properties = entity->properties();
    RoadCenterlinePropertiesDialogResult result;
    if (!showRoadCenterlinePropertiesDialog(input, result)) {
        entity->close();
        return;
    }

    entity->setProperties(result.properties);
    entity->recordGraphicsModified(true);
    entity->close();
}
```

- [ ] **Step 5: Build and verify GREEN**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: Debug ARX builds.

---

### Task 9: Visible Ribbon and Double-Click Bridge

**Files:**
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Test: `src/ui/wpf/RoadProto.Terrain.UI/RoadProto.Terrain.UI.csproj`

- [ ] **Step 1: Add alignment constants**

In `RoadProtoRibbonExtension.cs`, add:

```csharp
    private const string AlignmentPanelId = "ROADPROTO_ALIGNMENT_PANEL";
    private const string AlignmentCreateButtonId = "ROADPROTO_RD_ALIGN_CENTERLINE_CREATE";
    private const string AlignmentCurveEditButtonId = "ROADPROTO_RD_ALIGN_CURVE_PARAM_EDIT";
    private const string RoadCenterlineDxfName = "DNROADCENTERLINEENTITY";
```

- [ ] **Step 2: Create alignment panel and buttons**

After the terrain panel setup, add:

```csharp
        var alignmentPanel = tab.Panels.FirstOrDefault(item => item.Source.Id == AlignmentPanelId);
        if (alignmentPanel == null)
        {
            var source = new RibbonPanelSource
            {
                Id = AlignmentPanelId,
                Title = "平面设计",
            };
            alignmentPanel = new RibbonPanel { Source = source };
            tab.Panels.Add(alignmentPanel);
        }

        if (!alignmentPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == AlignmentCreateButtonId))
        {
            alignmentPanel.Source.Items.Add(CreateAlignmentCommandButton(
                AlignmentCreateButtonId,
                "平面布线",
                "点取控制点并创建道路中线自定义实体",
                "RD_ALIGN_CENTERLINE_CREATE "));
        }

        if (!alignmentPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == AlignmentCurveEditButtonId))
        {
            alignmentPanel.Source.Items.Add(CreateAlignmentCommandButton(
                AlignmentCurveEditButtonId,
                "编辑平曲线参数",
                "选择道路中线并编辑 T1、T2、LS1、R、LS2",
                "RD_ALIGN_CURVE_PARAM_EDIT "));
        }
```

- [ ] **Step 3: Add road centerline double-click detection**

In `OnBeginDoubleClick`, before returning from failed terrain detection, add road centerline handling:

```csharp
            if (TryFindEntityByDxfName(document, e.Location, RoadCenterlineDxfName, out var roadCenterlineId))
            {
                QueueRoadCenterlineEditByHandle(document, roadCenterlineId);
                return;
            }
```

Add a generic helper by adapting current terrain lookup:

```csharp
    private static bool TryFindEntityByDxfName(Document document, Point3d location, string dxfName, out ObjectId entityId)
```

Inside it, use `new TypedValue((int)DxfCode.Start, dxfName)` and the same implied/crossing-window pattern.

- [ ] **Step 4: Add alignment icon helper and command button**

Add:

```csharp
    private static RibbonButton CreateAlignmentCommandButton(string id, string text, string toolTip, string command)
    {
        var icon = CreateAlignmentIcon();
        return new RibbonButton
        {
            Id = id,
            Text = text,
            ShowText = true,
            ShowImage = true,
            Image = icon,
            LargeImage = icon,
            Size = RibbonItemSize.Standard,
            Orientation = Orientation.Horizontal,
            ToolTip = toolTip,
            CommandParameter = command,
            CommandHandler = new SendCommandHandler(),
        };
    }

    private static ImageSource CreateAlignmentIcon()
    {
        var drawingGroup = new DrawingGroup();
        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            new Pen(new SolidColorBrush(Color.FromRgb(95, 103, 116)), 1.2),
            Geometry.Parse("M 3,18 L 9,18 M 15,6 L 21,6")));
        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            new Pen(new SolidColorBrush(Color.FromRgb(28, 115, 220)), 2.0),
            Geometry.Parse("M 3,18 C 8,18 8,6 13,6 S 17,6 21,6")));
        drawingGroup.Children.Add(new GeometryDrawing(
            new SolidColorBrush(Color.FromRgb(250, 216, 61)),
            null,
            Geometry.Parse("M 7,16 L 10,18 L 7,20 Z M 14,4 L 17,6 L 14,8 Z")));
        drawingGroup.Freeze();
        return new DrawingImage(drawingGroup);
    }
```

- [ ] **Step 5: Queue road centerline handle edit**

Add:

```csharp
    private static void QueueRoadCenterlineEditByHandle(Document document, ObjectId entityId)
    {
        var handle = entityId.Handle.ToString();
        if (!string.IsNullOrWhiteSpace(handle))
        {
            document.SendStringToExecute($"RD_ALIGN_CENTERLINE_EDIT_HANDLE {handle} ", true, false, true);
        }
    }
```

- [ ] **Step 6: Build managed Ribbon**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: build succeeds with 0 errors.

---

### Task 10: Ribbon Rules and Project Documentation

**Files:**
- Modify: `docs/rules/command_and_module_rules.md`
- Create: `docs/modules/alignment.md`
- Modify: `docs/modules/module_index.md`
- Create: `docs/business/alignment/平面布线_道路中线.md`
- Create: `docs/reuse/alignment_centerline.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `tests/README.md`
- Modify: `README.md`
- Modify: `docs/dev/version_log.md`
- Modify: `build/RoadProto.Build.props`

- [ ] **Step 1: Add Ribbon rules to command/module rules**

Append to `docs/rules/command_and_module_rules.md`:

```markdown
## Ribbon 更新规则

新增可挂接 Ribbon 的命令时，必须同步更新两层 Ribbon：

1. C++ 框架层：模块的 `registerRibbon` 必须调用 `RibbonModel::ensurePanel`，命令元数据必须设置 `ribbonAttachable = true`。
2. AutoCAD 可见层：当前由托管插件 `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs` 创建可见 Ribbon，新增按钮必须在该文件中添加面板、按钮、命令发送和图标。
3. 按钮尺寸沿用现有 `RibbonItemSize.Standard`、`Orientation.Horizontal`，同一面板内保持一致。
4. 图标可以先用托管插件内置矢量图标；若使用 PNG，路径应记录在 `assets/icons/README.md`，命名优先使用命令名。
5. 模块文档必须说明 Ribbon 位置、按钮显示名、命令名和图标策略。
```

- [ ] **Step 2: Create alignment module doc**

Create `docs/modules/alignment.md`:

```markdown
# 平面设计模块

## 模块编码

`ALIGNMENT`

## 模块目的

承载道路平面线形、道路中线实体和后续纵断面、横断面、交叉口联动所需的路线基础对象。

## V0.1 命令

| 命令 | 说明 | 业务文档 |
| --- | --- | --- |
| `RD_ALIGN_CENTERLINE_CREATE` | 点取控制点并生成道路中线自定义实体。 | `docs/business/alignment/平面布线_道路中线.md` |
| `RD_ALIGN_CURVE_PARAM_EDIT` | 编辑道路中线的 `T1`、`T2`、`LS1`、`R`、`LS2`。 | `docs/business/alignment/平面布线_道路中线.md` |
| `RD_ALIGN_CENTERLINE_EDIT_HANDLE` | 内部双击 Bridge 命令，按 handle 打开道路中线属性编辑。 | `docs/business/alignment/平面布线_道路中线.md` |

## Ribbon

- AutoCAD Ribbon 位置：`RoadProto` 选项卡 / `平面设计` 面板。
- 按钮：`平面布线`、`编辑平曲线参数`。
- 图标：托管 Ribbon 插件内置小型道路中线图标，尺寸与数模按钮一致。

## 边界

领域层负责回旋线、平曲线、桩号和属性数据；ObjectARX 适配层负责点取、实体绘制、DWG 持久化、夹点和命令；托管 Ribbon 插件只创建按钮和双击转发。
```

- [ ] **Step 3: Create business document**

Create `docs/business/alignment/平面布线_道路中线.md`:

```markdown
# 平面布线_道路中线

## 基本信息

- 功能名称：平面布线
- 所属模块：平面设计
- 命令名称：`RD_ALIGN_CENTERLINE_CREATE`、`RD_ALIGN_CURVE_PARAM_EDIT`
- 业务文档维护人：RoadProto
- 原型版本：v0.1.7
- 是否可复用：是

## 功能背景

道路中线是后续纵断面、横断面、交叉口、地形取高和出图算量的基础对象。本原型验证由少量控制点生成带缓和曲线的道路中线实体。

## 业务目标

用户点取起点、交点和第三点后，自动生成直线、缓和曲线、圆曲线、缓和曲线、直线五单元，并在图中显示道路中线、桩号和曲线参数标注。

## 输入条件

- CAD 选择对象：创建时点取控制点；编辑时选择道路中线实体；数模关联时选择 `DnTerrainTinEntity`。
- 用户输入参数：道路名称、道路等级、是否关联数模、桩号标注间距、`T1`、`T2`、`LS1`、`R`、`LS2`。
- 已有设计实体：可选地形数模实体。
- 外部数据：无。

## 输出结果

- CAD 图形实体：`DnRoadCenterlineEntity`。
- 领域实体：道路中线属性、平曲线要素、特征点和桩号。
- 表格或报告：无。
- 更新通知或重建请求：保存地形关联 handle，并预留通过 `EntityRelationManager` 表达依赖。

## 操作流程

1. 用户运行 `RD_ALIGN_CENTERLINE_CREATE` 或点击 Ribbon `平面布线`。
2. 用户依次点取起点、交点和第三点。
3. 系统生成道路中线五单元并插入自定义实体。
4. 用户双击实体可修改属性。
5. 用户运行 `RD_ALIGN_CURVE_PARAM_EDIT` 可修改平曲线参数。

## 关键业务规则

- 缓和曲线采用回旋线，满足 `rho * l = A^2` 与 `A^2 = R * Ls`。
- 起点桩号默认为 `K0+000`。
- 桩号标注间距默认 `100m`。
- 道路名称默认按 `K1`、`K2`、`K3` 递增。
- 无效半径、负缓和曲线长或切线空间不足时不得写入实体。

## 可复用性说明

- 可复用内容：回旋线计算、五单元平曲线构建、桩号格式化、道路中线自定义实体。
- 临时原型内容：第一版属性窗口和参数窗口可先用简化 C++ 对话框。
- 正式复用前需要改造的内容：规范参数推荐、纵断面取高、完整夹点动态预览和独立标注实体。

## 与其他模块的依赖关系

- 上游模块：地形数模，可选关联。
- 下游模块：纵断面、横断面、交叉口、出图算量。
- 实体联动行为：第一版保存数模 handle，后续接入持久化关系管理。

## 后续对接正式 EICAD 功能的注意事项

- UI 可替换，但平曲线和桩号规则应继续沉淀在 C++ 领域层。
- 不得让 WPF 或 Ribbon 事件直接操作 ObjectARX 实体业务规则。
```

- [ ] **Step 4: Create reuse document**

Create `docs/reuse/alignment_centerline.md`:

```markdown
# 道路中线与平曲线计算

## 能力分类

通用道路设计能力

## 能力说明

提供道路中线控制点、五单元平曲线、回旋线缓和曲线、桩号格式化和道路中线自定义实体基础能力。

## 当前实现

- 源码路径：`src/domain/alignment`、`src/cad_adapter/objectarx/alignment`
- 对外类型/函数：`HorizontalAlignmentBuilder`、`StationFormatter`、`DnRoadCenterlineEntity`
- 当前使用该能力的命令：`RD_ALIGN_CENTERLINE_CREATE`、`RD_ALIGN_CURVE_PARAM_EDIT`

## 可复用内容

- 回旋线公式与采样。
- 平曲线五单元构建。
- 桩号格式化。
- 道路中线实体绘制与 DWG 持久化。

## 不可复用或临时内容

- 第一版简化属性窗口。
- 第一版夹点拖动过程没有连续动态预览。

## 依赖关系

- domain 依赖：无 ObjectARX 依赖。
- cad_adapter 依赖：ObjectARX 实体和命令。
- 模块依赖：`ALIGNMENT`。

## 扩展说明

后续可增加规范参数推荐、纵断面高程取样、路线文件流转、独立标注实体和完整 grip 动态预览。

## 验证方式

- 测试路径：`tests/core_tests.cpp`
- AutoCAD 手工验证：加载 ARX 后创建、编辑、保存并 `REGEN` 道路中线实体。
```

- [ ] **Step 5: Update indexes and version**

Update:

```markdown
docs/modules/module_index.md: 将 `平面设计` 状态改为 `道路中线原型已实现`，文档改为 `docs/modules/alignment.md`。
docs/reuse/capability_catalog.md: 增加 `平曲线五单元构建`、`桩号格式化`、`道路中线自定义实体`。
tests/README.md: 增加 alignment domain tests。
README.md: 当前版本改为 `v0.1.7`，阶段改为 `AlignmentCenterline`，新增命令说明。
build/RoadProto.Build.props: `RoadProtoVersion` 改为 `v0.1.7`，`RoadProtoBuildDate` 改为 `20260509`，`RoadProtoStage` 改为 `AlignmentCenterline`。
docs/dev/version_log.md: 新增 `v0.1.7` 记录，列出新增平面布线、道路中线实体、Ribbon 和已知限制。
```

---

### Task 11: Final Verification

**Files:**
- Test: `tests/RoadProtoCoreTests.vcxproj`
- Test: `src/app/RoadProtoArx.vcxproj`
- Test: `src/ui/wpf/RoadProto.Terrain.UI/RoadProto.Terrain.UI.csproj`

- [ ] **Step 1: Run core tests**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected:

```text
All RoadProto core tests passed.
```

- [ ] **Step 2: Build ARX Debug**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: `artifacts/x64/Debug/RoadProto_v0.1.7_20260509_AlignmentCenterline.arx` is produced.

- [ ] **Step 3: Build ARX Release**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' src\app\RoadProtoArx.vcxproj /p:Configuration=Release /p:Platform=x64
```

Expected: `artifacts/x64/Release/RoadProto_v0.1.7_20260509_AlignmentCenterline.arx` is produced.

- [ ] **Step 4: Build managed Ribbon**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

Expected: `artifacts/managed/Release/net48/RoadProto.Terrain.UI.dll` is produced.

- [ ] **Step 5: Manual AutoCAD verification**

In AutoCAD 2021:

```text
ARXLOAD artifacts\x64\Release\RoadProto_v0.1.7_20260509_AlignmentCenterline.arx
NETLOAD artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll
```

Verify:

```text
RoadProto / 平面设计 / 平面布线 button is visible.
RoadProto / 平面设计 / 编辑平曲线参数 button is visible.
RD_ALIGN_CENTERLINE_CREATE creates a DNROADCENTERLINEENTITY after three picked points.
RD_ALIGN_CURVE_PARAM_EDIT selects the road centerline and refreshes it.
Double-clicking the entity triggers RD_ALIGN_CENTERLINE_EDIT_HANDLE.
SAVE, reopen, REGEN do not crash.
Grip moving a control point rebuilds the entity.
```

- [ ] **Step 6: Record verification results**

Update `docs/dev/version_log.md` with the exact build and manual verification outcome. If AutoCAD GUI verification is not possible in the current run, state that Core tests and builds passed and GUI verification remains pending.

---

## Self-Review

- Spec coverage: The plan covers domain formulas, five-unit generation, station labels, road properties, terrain-link fields, two visible commands, one handle command, custom entity, Ribbon, double-click bridge, docs, versioning and verification.
- Placeholder scan: The plan contains no unresolved placeholders or missing file paths.
- Type consistency: The command names are `RD_ALIGN_CENTERLINE_CREATE`, `RD_ALIGN_CURVE_PARAM_EDIT`, and `RD_ALIGN_CENTERLINE_EDIT_HANDLE`; the entity class is `DnRoadCenterlineEntity`; the DXF name is `DNROADCENTERLINEENTITY`.
