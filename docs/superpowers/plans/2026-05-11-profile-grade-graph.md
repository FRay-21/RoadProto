# Profile Grade Graph Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first longitudinal profile grade graph feature: select a road centerline, derive ground samples from linked TIN or DMX, insert a custom profile graph entity, and edit/update its display properties through WPF.

**Architecture:** Add a full `PROFILE` module. Keep DMX parsing, graph data, layout, defaults, and testable rules in `domain/profile`; keep workflow orchestration in `application/profile`; keep AutoCAD selection, file dialogs, custom entity drawing, DWG persistence, and handle commands in `cad_adapter/objectarx/profile`; keep WPF strictly as DTO, display, and command forwarding.

**Tech Stack:** C++17, ObjectARX 2021, AutoCAD 2021, WPF .NET Framework 4.8, existing file-based WPF Bridge pattern, `RoadProtoCoreTests`, `RoadProtoManagedBridgeTests`.

---

## File Structure

Create:

- `src/domain/profile/ProfileGradeGraphModel.h`: source type enum, sample/property/data/layout structs.
- `src/domain/profile/ProfileDmxFile.h`
- `src/domain/profile/ProfileDmxFile.cpp`: parse `.dmx` files and text streams.
- `src/domain/profile/ProfileGradeGraphLayout.h`
- `src/domain/profile/ProfileGradeGraphLayout.cpp`: base elevation, extents, x/y mapping.
- `src/application/profile/ProfileGradeGraphCreateService.h`
- `src/application/profile/ProfileGradeGraphCreateService.cpp`: build graph data from prepared samples and defaults.
- `src/modules/profile/ProfileModule.h`
- `src/modules/profile/ProfileModule.cpp`: register `PROFILE` commands and Ribbon panel.
- `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h`
- `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.cpp`: custom entity, drawing, persistence, display update.
- `src/cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.h`
- `src/cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.cpp`: request/response file bridge.
- `src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h`
- `src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.cpp`: create/edit/apply commands.
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileGradeGraphDialogDtos.cs`
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileGradeGraphDialogFile.cs`
- `src/ui/wpf/RoadProto.Terrain.UI/ProfileGradeGraphWindow.xaml`
- `src/ui/wpf/RoadProto.Terrain.UI/ProfileGradeGraphWindow.xaml.cs`
- `docs/business/profile/纵断面拉坡图_创建.md`
- `docs/business/profile/纵断面拉坡图_属性编辑.md`
- `docs/modules/profile.md`

Modify:

- `tests/core_tests.cpp`
- `tests/RoadProtoCoreTests.vcxproj`
- `tests/RoadProtoManagedBridgeTests/Program.cs`
- `tests/RoadProtoManagedBridgeTests/RoadProtoManagedBridgeTests.csproj`
- `src/app/RoadProtoArx.vcxproj`
- `src/app/startup/Startup.cpp`
- `src/app/arx_entry/RoadProtoArxEntry.cpp`
- `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AlignmentDialogCommands.cs`
- `docs/modules/module_index.md`
- `docs/reuse/capability_catalog.md`
- `docs/dev/version_log.md`
- `tests/README.md`

Use this Git command path in every commit step:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe"
```

---

### Task 1: Domain Model, DMX Parser, and Layout Rules

**Files:**
- Create: `src/domain/profile/ProfileGradeGraphModel.h`
- Create: `src/domain/profile/ProfileDmxFile.h`
- Create: `src/domain/profile/ProfileDmxFile.cpp`
- Create: `src/domain/profile/ProfileGradeGraphLayout.h`
- Create: `src/domain/profile/ProfileGradeGraphLayout.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`

- [ ] **Step 1: Write failing tests for DMX parsing and layout**

Add these includes near the other domain includes in `tests/core_tests.cpp`:

```cpp
#include "domain/profile/ProfileDmxFile.h"
#include "domain/profile/ProfileGradeGraphLayout.h"
```

Add these test functions before `alignmentCommandMetadataUsesExpectedNames()`:

```cpp
void profileDmxFileParsesStationsAndKeepsDuplicates()
{
    const std::wstring content =
        L"// comment\r\n"
        L"0.00000000 21.25100000\r\n"
        L"2.70000000 19.95400000\r\n"
        L"2.70000000 19.93400000\r\n"
        L"37123.456_2 36.12000000\r\n"
        L"bad line\r\n";

    const auto parsed = roadproto::domain::profile::ProfileDmxFile::parseText(content);

    CHECK(parsed.succeeded);
    CHECK(parsed.samples.size() == 4);
    CHECK(parsed.invalidLineCount == 1);
    CHECK(std::fabs(parsed.samples[0].station - 0.0) < 1e-9);
    CHECK(std::fabs(parsed.samples[0].elevation - 21.251) < 1e-9);
    CHECK(std::fabs(parsed.samples[1].station - 2.7) < 1e-9);
    CHECK(std::fabs(parsed.samples[2].station - 2.7) < 1e-9);
    CHECK(parsed.samples[3].rawStationText == L"37123.456_2");
    CHECK(parsed.samples[3].breakChainIndex == 2);
}

void profileDmxFileRejectsTooFewValidSamples()
{
    const auto parsed = roadproto::domain::profile::ProfileDmxFile::parseText(L"0.0 10.0\r\nbad\r\n");

    CHECK(!parsed.succeeded);
    CHECK(parsed.samples.size() == 1);
    CHECK(parsed.invalidLineCount == 1);
    CHECK(!parsed.errorMessage.empty());
}

void profileGradeGraphLayoutMapsStationAndElevation()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    graph.properties.gridSpacing = 10.0;
    graph.properties.verticalScale = 10.0;
    graph.groundSamples = {
        ProfileGroundSample{100.0, 23.5, L"100.0", 0},
        ProfileGroundSample{120.0, 36.0, L"120.0", 0},
    };

    const auto layout = ProfileGradeGraphLayout::calculate(graph);

    CHECK(layout.succeeded);
    CHECK(std::fabs(layout.minStation - 100.0) < 1e-9);
    CHECK(std::fabs(layout.maxStation - 120.0) < 1e-9);
    CHECK(std::fabs(layout.baseElevation - 20.0) < 1e-9);
    CHECK(std::fabs(ProfileGradeGraphLayout::mapX(layout, 115.0) - 15.0) < 1e-9);
    CHECK(std::fabs(ProfileGradeGraphLayout::mapY(layout, 23.5) - 35.0) < 1e-9);
}
```

Call them from `main()` before `alignmentCommandMetadataUsesExpectedNames()`:

```cpp
    profileDmxFileParsesStationsAndKeepsDuplicates();
    profileDmxFileRejectsTooFewValidSamples();
    profileGradeGraphLayoutMapsStationAndElevation();
```

Add these compile items to `tests/RoadProtoCoreTests.vcxproj`:

```xml
    <ClCompile Include="..\src\domain\profile\ProfileDmxFile.cpp" />
    <ClCompile Include="..\src\domain\profile\ProfileGradeGraphLayout.cpp" />
```

- [ ] **Step 2: Run core test build to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL because `domain/profile/ProfileDmxFile.h` and `ProfileGradeGraphLayout.h` do not exist.

- [ ] **Step 3: Create domain model and parser headers**

Create `src/domain/profile/ProfileGradeGraphModel.h`:

```cpp
#pragma once

#include <string>
#include <vector>

namespace roadproto::domain::profile {

enum class ProfileGroundSourceType {
    TerrainTin,
    DmxFile,
};

struct ProfileGroundSample {
    double station = 0.0;
    double elevation = 0.0;
    std::wstring rawStationText;
    int breakChainIndex = 0;
};

struct ProfileGradeGraphProperties {
    std::wstring graphName = L"拉坡图";
    int groundLineColorIndex = 4;
    double groundLineWidth = 1.0;
    double sampleInterval = 10.0;
    double verticalScale = 10.0;
    double gridSpacing = 10.0;
};

struct ProfileGradeGraphData {
    ProfileGroundSourceType sourceType = ProfileGroundSourceType::DmxFile;
    std::wstring roadCenterlineHandle;
    std::wstring terrainTinHandle;
    std::wstring dmxFilePath;
    ProfileGradeGraphProperties properties;
    std::vector<ProfileGroundSample> groundSamples;
};

} // namespace roadproto::domain::profile
```

Create `src/domain/profile/ProfileDmxFile.h`:

```cpp
#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

#include <string>

namespace roadproto::domain::profile {

struct ProfileDmxParseResult {
    bool succeeded = false;
    std::wstring errorMessage;
    std::vector<ProfileGroundSample> samples;
    std::size_t invalidLineCount = 0;
};

class ProfileDmxFile {
public:
    static ProfileDmxParseResult parseText(const std::wstring& content);
    static ProfileDmxParseResult read(const std::wstring& path);
};

} // namespace roadproto::domain::profile
```

Create `src/domain/profile/ProfileGradeGraphLayout.h`:

```cpp
#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

namespace roadproto::domain::profile {

struct ProfileGradeGraphLayoutResult {
    bool succeeded = false;
    std::wstring errorMessage;
    double minStation = 0.0;
    double maxStation = 0.0;
    double minElevation = 0.0;
    double maxElevation = 0.0;
    double baseElevation = 0.0;
    double graphWidth = 0.0;
    double graphHeight = 0.0;
};

class ProfileGradeGraphLayout {
public:
    static ProfileGradeGraphLayoutResult calculate(const ProfileGradeGraphData& graph);
    static double mapX(const ProfileGradeGraphLayoutResult& layout, double station);
    static double mapY(const ProfileGradeGraphLayoutResult& layout, double elevation, double verticalScale = 10.0);
    static double mapY(const ProfileGradeGraphData& graph, const ProfileGradeGraphLayoutResult& layout, double elevation);
};

} // namespace roadproto::domain::profile
```

- [ ] **Step 4: Implement parser and layout**

Create `src/domain/profile/ProfileDmxFile.cpp`:

```cpp
#include "domain/profile/ProfileDmxFile.h"

#include <cwctype>
#include <fstream>
#include <sstream>

namespace roadproto::domain::profile {
namespace {

std::wstring trim(const std::wstring& value)
{
    std::size_t first = 0;
    while (first < value.size() && std::iswspace(value[first])) {
        ++first;
    }
    std::size_t last = value.size();
    while (last > first && std::iswspace(value[last - 1])) {
        --last;
    }
    return value.substr(first, last - first);
}

bool parseStationToken(const std::wstring& token, double& station, int& breakChainIndex)
{
    const auto separator = token.find(L'_');
    const auto stationText = separator == std::wstring::npos ? token : token.substr(0, separator);
    try {
        std::size_t consumed = 0;
        station = std::stod(stationText, &consumed);
        if (consumed != stationText.size()) {
            return false;
        }
        breakChainIndex = 0;
        if (separator != std::wstring::npos) {
            const auto breakText = token.substr(separator + 1);
            std::size_t breakConsumed = 0;
            breakChainIndex = std::stoi(breakText, &breakConsumed);
            if (breakConsumed != breakText.size()) {
                return false;
            }
        }
        return true;
    } catch (...) {
        return false;
    }
}

bool parseLine(const std::wstring& rawLine, ProfileGroundSample& sample)
{
    const auto line = trim(rawLine);
    if (line.empty() || line.rfind(L"//", 0) == 0) {
        return false;
    }

    std::wistringstream stream(line);
    std::wstring stationToken;
    double elevation = 0.0;
    if (!(stream >> stationToken >> elevation)) {
        return false;
    }

    double station = 0.0;
    int breakChainIndex = 0;
    if (!parseStationToken(stationToken, station, breakChainIndex)) {
        return false;
    }

    sample.station = station;
    sample.elevation = elevation;
    sample.rawStationText = stationToken;
    sample.breakChainIndex = breakChainIndex;
    return true;
}

} // namespace

ProfileDmxParseResult ProfileDmxFile::parseText(const std::wstring& content)
{
    ProfileDmxParseResult result;
    std::wistringstream input(content);
    std::wstring line;
    while (std::getline(input, line)) {
        const auto trimmed = trim(line);
        if (trimmed.empty() || trimmed.rfind(L"//", 0) == 0) {
            continue;
        }

        ProfileGroundSample sample;
        if (parseLine(trimmed, sample)) {
            result.samples.push_back(sample);
        } else {
            ++result.invalidLineCount;
        }
    }

    result.succeeded = result.samples.size() >= 2;
    if (!result.succeeded) {
        result.errorMessage = L"DMX 有效地面线点少于 2 个。";
    }
    return result;
}

ProfileDmxParseResult ProfileDmxFile::read(const std::wstring& path)
{
    std::wifstream file(path);
    if (!file.is_open()) {
        ProfileDmxParseResult result;
        result.errorMessage = L"无法打开 DMX 文件。";
        return result;
    }

    std::wstringstream buffer;
    buffer << file.rdbuf();
    return parseText(buffer.str());
}

} // namespace roadproto::domain::profile
```

Create `src/domain/profile/ProfileGradeGraphLayout.cpp`:

```cpp
#include "domain/profile/ProfileGradeGraphLayout.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::profile {

ProfileGradeGraphLayoutResult ProfileGradeGraphLayout::calculate(const ProfileGradeGraphData& graph)
{
    ProfileGradeGraphLayoutResult result;
    if (graph.groundSamples.size() < 2) {
        result.errorMessage = L"纵断面地面线点少于 2 个。";
        return result;
    }

    const auto stationRange = std::minmax_element(
        graph.groundSamples.begin(),
        graph.groundSamples.end(),
        [](const auto& left, const auto& right) { return left.station < right.station; });
    const auto elevationRange = std::minmax_element(
        graph.groundSamples.begin(),
        graph.groundSamples.end(),
        [](const auto& left, const auto& right) { return left.elevation < right.elevation; });

    const auto gridSpacing = graph.properties.gridSpacing > 0.0 ? graph.properties.gridSpacing : 10.0;
    const auto verticalScale = graph.properties.verticalScale > 0.0 ? graph.properties.verticalScale : 10.0;
    result.minStation = stationRange.first->station;
    result.maxStation = stationRange.second->station;
    result.minElevation = elevationRange.first->elevation;
    result.maxElevation = elevationRange.second->elevation;
    result.baseElevation = std::floor(result.minElevation / gridSpacing) * gridSpacing;
    result.graphWidth = result.maxStation - result.minStation;
    const auto elevationSpanForHeight = std::max(gridSpacing, result.maxElevation - result.baseElevation);
    result.graphHeight = elevationSpanForHeight * verticalScale;
    result.succeeded = result.graphWidth >= 0.0 && result.graphHeight >= 0.0;
    if (!result.succeeded) {
        result.errorMessage = L"纵断面拉坡图范围无效。";
    }
    return result;
}

double ProfileGradeGraphLayout::mapX(const ProfileGradeGraphLayoutResult& layout, double station)
{
    return station - layout.minStation;
}

double ProfileGradeGraphLayout::mapY(
    const ProfileGradeGraphLayoutResult& layout,
    double elevation,
    double verticalScale)
{
    return (elevation - layout.baseElevation) * verticalScale;
}

double ProfileGradeGraphLayout::mapY(
    const ProfileGradeGraphData& graph,
    const ProfileGradeGraphLayoutResult& layout,
    double elevation)
{
    const auto verticalScale = graph.properties.verticalScale > 0.0 ? graph.properties.verticalScale : 10.0;
    return mapY(layout, elevation, verticalScale);
}

} // namespace roadproto::domain::profile
```

- [ ] **Step 5: Run tests to verify GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: build succeeds and executable prints `All core tests passed.`

- [ ] **Step 6: Commit Task 1**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/domain/profile/ProfileGradeGraphModel.h src/domain/profile/ProfileDmxFile.h src/domain/profile/ProfileDmxFile.cpp src/domain/profile/ProfileGradeGraphLayout.h src/domain/profile/ProfileGradeGraphLayout.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add profile ground data domain rules"
```

---

### Task 2: Application Service for Grade Graph Defaults

**Files:**
- Create: `src/application/profile/ProfileGradeGraphCreateService.h`
- Create: `src/application/profile/ProfileGradeGraphCreateService.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write failing service tests**

Add include:

```cpp
#include "application/profile/ProfileGradeGraphCreateService.h"
```

Add test:

```cpp
void profileGradeGraphCreateServiceBuildsDefaultGraphData()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileGradeGraphCreateInput input;
    input.sourceType = ProfileGroundSourceType::DmxFile;
    input.roadName = L"K1";
    input.roadCenterlineHandle = L"AA1";
    input.dmxFilePath = L"C:/data/K1.dmx";
    input.groundSamples = {
        ProfileGroundSample{0.0, 21.0, L"0.0", 0},
        ProfileGroundSample{10.0, 22.0, L"10.0", 0},
    };

    ProfileGradeGraphCreateService service;
    const auto result = service.build(input);

    CHECK(result.succeeded);
    CHECK(result.graph.sourceType == ProfileGroundSourceType::DmxFile);
    CHECK(result.graph.properties.graphName == L"K1拉坡图");
    CHECK(result.graph.roadCenterlineHandle == L"AA1");
    CHECK(result.graph.dmxFilePath == L"C:/data/K1.dmx");
    CHECK(result.graph.properties.groundLineColorIndex == 4);
    CHECK(std::fabs(result.graph.properties.groundLineWidth - 1.0) < 1e-9);
    CHECK(std::fabs(result.graph.properties.sampleInterval - 10.0) < 1e-9);
    CHECK(std::fabs(result.graph.properties.verticalScale - 10.0) < 1e-9);
    CHECK(std::fabs(result.graph.properties.gridSpacing - 10.0) < 1e-9);
}
```

Call it from `main()` after the Task 1 tests.

Add compile items to both C++ project files:

```xml
    <ClCompile Include="..\src\application\profile\ProfileGradeGraphCreateService.cpp" />
```

For `src/app/RoadProtoArx.vcxproj`, use:

```xml
    <ClCompile Include="..\application\profile\ProfileGradeGraphCreateService.cpp" />
```

- [ ] **Step 2: Run core test build to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL because `application/profile/ProfileGradeGraphCreateService.h` does not exist.

- [ ] **Step 3: Create service header**

Create `src/application/profile/ProfileGradeGraphCreateService.h`:

```cpp
#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

#include <string>

namespace roadproto::application::profile {

struct ProfileGradeGraphCreateInput {
    domain::profile::ProfileGroundSourceType sourceType = domain::profile::ProfileGroundSourceType::DmxFile;
    std::wstring roadName;
    std::wstring roadCenterlineHandle;
    std::wstring terrainTinHandle;
    std::wstring dmxFilePath;
    std::vector<domain::profile::ProfileGroundSample> groundSamples;
};

struct ProfileGradeGraphCreateResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::profile::ProfileGradeGraphData graph;
};

class ProfileGradeGraphCreateService {
public:
    ProfileGradeGraphCreateResult build(const ProfileGradeGraphCreateInput& input) const;
};

} // namespace roadproto::application::profile
```

- [ ] **Step 4: Implement service**

Create `src/application/profile/ProfileGradeGraphCreateService.cpp`:

```cpp
#include "application/profile/ProfileGradeGraphCreateService.h"

namespace roadproto::application::profile {

ProfileGradeGraphCreateResult ProfileGradeGraphCreateService::build(
    const ProfileGradeGraphCreateInput& input) const
{
    ProfileGradeGraphCreateResult result;
    if (input.groundSamples.size() < 2) {
        result.errorMessage = L"纵断面地面线点少于 2 个。";
        return result;
    }

    auto roadName = input.roadName.empty() ? L"道路" : input.roadName;
    result.graph.sourceType = input.sourceType;
    result.graph.roadCenterlineHandle = input.roadCenterlineHandle;
    result.graph.terrainTinHandle = input.terrainTinHandle;
    result.graph.dmxFilePath = input.dmxFilePath;
    result.graph.groundSamples = input.groundSamples;
    result.graph.properties.graphName = roadName + L"拉坡图";
    result.graph.properties.groundLineColorIndex = 4;
    result.graph.properties.groundLineWidth = 1.0;
    result.graph.properties.sampleInterval = 10.0;
    result.graph.properties.verticalScale = 10.0;
    result.graph.properties.gridSpacing = 10.0;
    result.succeeded = true;
    return result;
}

} // namespace roadproto::application::profile
```

- [ ] **Step 5: Run tests to verify GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all core tests pass.

- [ ] **Step 6: Commit Task 2**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj src/application/profile/ProfileGradeGraphCreateService.h src/application/profile/ProfileGradeGraphCreateService.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add profile grade graph create service"
```

---

### Task 3: PROFILE Module Registration

**Files:**
- Create: `src/modules/profile/ProfileModule.h`
- Create: `src/modules/profile/ProfileModule.cpp`
- Modify: `src/app/startup/Startup.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`

- [ ] **Step 1: Write failing command metadata test**

Add this test:

```cpp
void profileCommandMetadataUsesExpectedNames()
{
    roadproto::core::CommandRegistry commands;
    commands.registerCommand(roadproto::core::CommandDefinition{
        L"RD_PROFILE_GRADE_GRAPH_CREATE",
        L"纵断面拉坡图",
        L"PROFILE",
        L"Creates a longitudinal profile grade graph entity.",
        &noopCommand,
        true,
        true,
        L"docs/business/profile/纵断面拉坡图_创建.md",
        true});

    const auto found = commands.find(L"RD_PROFILE_GRADE_GRAPH_CREATE");
    CHECK(found.has_value());
    CHECK(found->moduleCode == L"PROFILE");
    CHECK(found->displayName == L"纵断面拉坡图");
    CHECK(found->businessDocPath == L"docs/business/profile/纵断面拉坡图_创建.md");
    CHECK(found->ribbonAttachable);
}
```

Call it from `main()` after `alignmentCommandMetadataUsesExpectedNames()`.

Add compile item:

```xml
    <ClCompile Include="..\src\modules\profile\ProfileModule.cpp" />
```

For `src/app/RoadProtoArx.vcxproj`, use:

```xml
    <ClCompile Include="..\modules\profile\ProfileModule.cpp" />
```

- [ ] **Step 2: Run core build to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL because `src/modules/profile/ProfileModule.cpp` does not exist or command procedures are undefined.

- [ ] **Step 3: Create module header**

Create `src/modules/profile/ProfileModule.h`:

```cpp
#pragma once

#include "core/module/ModuleRegistry.h"

namespace roadproto::modules::profile {

core::ModuleDefinition createProfileModule();

} // namespace roadproto::modules::profile
```

- [ ] **Step 4: Create module implementation**

Create `src/modules/profile/ProfileModule.cpp`:

```cpp
#include "modules/profile/ProfileModule.h"

#include "cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::modules::profile {
namespace {

void registerProfileCommands(core::CommandRegistry& commandRegistry)
{
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_GRADE_GRAPH_CREATE",
        L"纵断面拉坡图",
        L"PROFILE",
        L"Creates a longitudinal profile grade graph entity.",
        cad_adapter::objectarx::profileGradeGraphCreateCommandProcedure(),
        true,
        true,
        L"docs/business/profile/纵断面拉坡图_创建.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE",
        L"按 handle 编辑纵断面拉坡图",
        L"PROFILE",
        L"Internal double-click bridge command that edits a profile grade graph by handle.",
        cad_adapter::objectarx::profileGradeGraphEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/profile/纵断面拉坡图_属性编辑.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_PROFILE_APPLY_DIALOG_FILE",
        L"应用纵断面拉坡图对话框结果",
        L"PROFILE",
        L"Internal WPF bridge command that applies profile grade graph dialog response files.",
        cad_adapter::objectarx::profileApplyDialogFileCommandProcedure(),
        true,
        false,
        L"docs/business/profile/纵断面拉坡图_属性编辑.md",
        false});
}

void registerProfileRibbon(ui::RibbonModel& ribbonModel)
{
    ribbonModel.ensurePanel(L"PROFILE", L"纵断面设计");
}

} // namespace

core::ModuleDefinition createProfileModule()
{
    return core::ModuleDefinition{
        L"Profile",
        L"PROFILE",
        L"Longitudinal profile prototype module.",
        []() { return true; },
        []() { return true; },
        &registerProfileCommands,
        &registerProfileRibbon,
        L"docs/modules/profile.md"};
}

} // namespace roadproto::modules::profile
```

- [ ] **Step 5: Add command procedure stubs for test build**

Create `src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h`:

```cpp
#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx {

core::CommandProcedure profileGradeGraphCreateCommandProcedure();
core::CommandProcedure profileGradeGraphEditHandleCommandProcedure();
core::CommandProcedure profileApplyDialogFileCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx
```

Create `src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.cpp`:

```cpp
#include "cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h"

namespace roadproto::cad_adapter::objectarx {
namespace {

void runProfileGradeGraphCreateCommand() {}
void runProfileGradeGraphEditHandleCommand() {}
void runProfileApplyDialogFileCommand() {}

} // namespace

core::CommandProcedure profileGradeGraphCreateCommandProcedure()
{
    return &runProfileGradeGraphCreateCommand;
}

core::CommandProcedure profileGradeGraphEditHandleCommandProcedure()
{
    return &runProfileGradeGraphEditHandleCommand;
}

core::CommandProcedure profileApplyDialogFileCommandProcedure()
{
    return &runProfileApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx
```

Add `ObjectArxProfileGradeGraphCommand.cpp` to both C++ project files.

- [ ] **Step 6: Register the module in startup**

Modify `src/app/startup/Startup.cpp`:

```cpp
#include "modules/profile/ProfileModule.h"
```

Add in `registerBuiltInModules` after alignment:

```cpp
    moduleRegistry.registerModule(modules::profile::createProfileModule());
```

- [ ] **Step 7: Run tests to verify GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: all core tests pass.

- [ ] **Step 8: Commit Task 3**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj src/app/startup/Startup.cpp src/modules/profile/ProfileModule.h src/modules/profile/ProfileModule.cpp src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: register profile grade graph module"
```

---

### Task 4: Custom Profile Grade Graph Entity

**Files:**
- Create: `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h`
- Create: `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.cpp`
- Modify: `src/app/arx_entry/RoadProtoArxEntry.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write compile-level entity integration**

Add `DnProfileGradeGraphEntity.cpp` to `src/app/RoadProtoArx.vcxproj`:

```xml
    <ClCompile Include="..\cad_adapter\objectarx\profile\DnProfileGradeGraphEntity.cpp" />
```

Modify `src/app/arx_entry/RoadProtoArxEntry.cpp`:

```cpp
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
```

Initialize after road centerline:

```cpp
        cad_adapter::objectarx::initializeProfileGradeGraphEntityClass();
```

Uninitialize before road centerline:

```cpp
        cad_adapter::objectarx::uninitializeProfileGradeGraphEntityClass();
```

- [ ] **Step 2: Run ARX build to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL because `DnProfileGradeGraphEntity.h` does not exist.

- [ ] **Step 3: Create entity header**

Create `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h`:

```cpp
#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

#include "dbents.h"
#include "dbmain.h"

class DnProfileGradeGraphEntity : public AcDbEntity {
public:
    ACRX_DECLARE_MEMBERS(DnProfileGradeGraphEntity);

    DnProfileGradeGraphEntity();

    void setGraphData(const roadproto::domain::profile::ProfileGradeGraphData& data);
    const roadproto::domain::profile::ProfileGradeGraphData& graphData() const;
    void setInsertionPoint(const AcGePoint3d& point);
    AcGePoint3d insertionPoint() const;
    void setProperties(const roadproto::domain::profile::ProfileGradeGraphProperties& properties);
    bool replaceGroundSamples(const std::vector<roadproto::domain::profile::ProfileGroundSample>& samples);

    Acad::ErrorStatus dwgInFields(AcDbDwgFiler* filer) override;
    Acad::ErrorStatus dwgOutFields(AcDbDwgFiler* filer) const override;

protected:
    Adesk::Boolean subWorldDraw(AcGiWorldDraw* worldDraw) override;
    Acad::ErrorStatus subGetGeomExtents(AcDbExtents& extents) const override;
    Acad::ErrorStatus subTransformBy(const AcGeMatrix3d& transform) override;

private:
    roadproto::domain::profile::ProfileGradeGraphData graphData_;
    AcGePoint3d insertionPoint_;
};

namespace roadproto::cad_adapter::objectarx {

void initializeProfileGradeGraphEntityClass();
void uninitializeProfileGradeGraphEntityClass();

} // namespace roadproto::cad_adapter::objectarx
```

- [ ] **Step 4: Implement drawing and persistence**

Create `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.cpp` with:

```cpp
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"

#include "domain/alignment/StationFormatter.h"
#include "domain/profile/ProfileGradeGraphLayout.h"

#include "acgi.h"
#include "dbproxy.h"
#include "rxregsvc.h"

ACRX_DXF_DEFINE_MEMBERS(
    DnProfileGradeGraphEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNPROFILEGRADEGRAPHENTITY,
    RoadProto);

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;
constexpr double kTitleOffset = 8.0;
constexpr double kLabelTextHeight = 2.5;

void writeWideString(AcDbDwgFiler* filer, const std::wstring& value)
{
    filer->writeString(value.c_str());
}

std::wstring readWideString(AcDbDwgFiler* filer)
{
    ACHAR* raw = nullptr;
    filer->readString(&raw);
    std::wstring value = raw == nullptr ? L"" : raw;
    acutDelString(raw);
    return value;
}

void drawText(AcGiWorldDraw* worldDraw, const AcGePoint3d& point, const std::wstring& text, double height)
{
    if (text.empty()) {
        return;
    }
    worldDraw->geometry().text(point, AcGeVector3d::kZAxis, AcGeVector3d::kXAxis, height, 1.0, 0.0, text.c_str());
}

} // namespace

DnProfileGradeGraphEntity::DnProfileGradeGraphEntity() = default;

void DnProfileGradeGraphEntity::setGraphData(const roadproto::domain::profile::ProfileGradeGraphData& data)
{
    graphData_ = data;
}

const roadproto::domain::profile::ProfileGradeGraphData& DnProfileGradeGraphEntity::graphData() const
{
    return graphData_;
}

void DnProfileGradeGraphEntity::setInsertionPoint(const AcGePoint3d& point)
{
    insertionPoint_ = point;
}

AcGePoint3d DnProfileGradeGraphEntity::insertionPoint() const
{
    return insertionPoint_;
}

void DnProfileGradeGraphEntity::setProperties(
    const roadproto::domain::profile::ProfileGradeGraphProperties& properties)
{
    graphData_.properties = properties;
}

bool DnProfileGradeGraphEntity::replaceGroundSamples(
    const std::vector<roadproto::domain::profile::ProfileGroundSample>& samples)
{
    if (samples.size() < 2) {
        return false;
    }
    graphData_.groundSamples = samples;
    return true;
}
```

Continue the same file with DWG read/write for version, insertion point, source type, handles, DMX path, properties, and samples. Use the same primitive field order in `dwgInFields` and `dwgOutFields`.

Continue `subWorldDraw` with this drawing sequence:

```cpp
    const auto layout = roadproto::domain::profile::ProfileGradeGraphLayout::calculate(graphData_);
    if (!layout.succeeded) {
        return Adesk::kTrue;
    }

    const auto origin = insertionPoint_;
    const auto width = layout.graphWidth;
    const auto height = layout.graphHeight;

    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    worldDraw->subEntityTraits().setColor(8);
    AcGePoint3d frame[5] = {
        origin,
        AcGePoint3d(origin.x + width, origin.y, origin.z),
        AcGePoint3d(origin.x + width, origin.y + height, origin.z),
        AcGePoint3d(origin.x, origin.y + height, origin.z),
        origin,
    };
    worldDraw->geometry().polyline(5, frame);

    worldDraw->subEntityTraits().setColor(graphData_.properties.groundLineColorIndex);
    for (std::size_t i = 1; i < graphData_.groundSamples.size(); ++i) {
        const auto& a = graphData_.groundSamples[i - 1];
        const auto& b = graphData_.groundSamples[i];
        AcGePoint3d line[2] = {
            AcGePoint3d(origin.x + roadproto::domain::profile::ProfileGradeGraphLayout::mapX(layout, a.station),
                        origin.y + roadproto::domain::profile::ProfileGradeGraphLayout::mapY(graphData_, layout, a.elevation),
                        origin.z),
            AcGePoint3d(origin.x + roadproto::domain::profile::ProfileGradeGraphLayout::mapX(layout, b.station),
                        origin.y + roadproto::domain::profile::ProfileGradeGraphLayout::mapY(graphData_, layout, b.elevation),
                        origin.z)};
        worldDraw->geometry().polyline(2, line);
    }

    worldDraw->subEntityTraits().setColor(7);
    drawText(worldDraw, AcGePoint3d(origin.x + width / 2.0, origin.y + height + kTitleOffset, origin.z), graphData_.properties.graphName, kLabelTextHeight);
```

Add grid lines and y elevation labels in the same method using `graphData_.properties.gridSpacing` for elevation intervals and 100m for major station labels.

- [ ] **Step 5: Run ARX build to verify GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: ARX project builds.

- [ ] **Step 6: Commit Task 4**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/app/RoadProtoArx.vcxproj src/app/arx_entry/RoadProtoArxEntry.cpp src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add profile grade graph entity"
```

---

### Task 5: ObjectARX Create Command

**Files:**
- Modify: `src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h`
- Modify: `src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.cpp`

- [ ] **Step 1: Add command helpers and manual RED check**

Run current command in AutoCAD after loading the ARX. Expected before implementation: `RD_PROFILE_GRADE_GRAPH_CREATE` does nothing because Task 3 used stubs.

- [ ] **Step 2: Implement selection and append helpers**

In `ObjectArxProfileGradeGraphCommand.cpp`, replace stubs with helpers matching the alignment command pattern:

```cpp
bool appendEntityToModelSpace(AcDbEntity* entity, AcDbObjectId& entityId);
bool selectRoadCenterlineEntity(AcDbObjectId& entityId);
bool resolveObjectIdFromHandle(const std::wstring& handleText, AcDbObjectId& entityId);
std::wstring entityHandleText(AcDbEntity* entity);
bool promptDmxFilePath(std::wstring& path, roadproto::cad_adapter::IEditor& editor);
bool getPoint(const wchar_t* prompt, AcGePoint3d& point);
```

Use the existing implementations in `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.cpp` as the reference for selection, handle conversion, and model-space append.

- [ ] **Step 3: Implement terrain sampling**

Add:

```cpp
std::vector<roadproto::domain::profile::ProfileGroundSample> sampleGroundFromTerrain(
    const roadproto::domain::alignment::HorizontalAlignment& alignment,
    DnTerrainTinEntity& terrain,
    double interval,
    std::size_t& skippedCount)
{
    std::vector<roadproto::domain::profile::ProfileGroundSample> samples;
    skippedCount = 0;
    for (const auto& element : alignment.elements) {
        for (const auto& sample : element.samples) {
            const auto shouldKeep = samples.empty()
                || std::fabs(sample.station - samples.back().station) >= interval
                || std::fabs(sample.station - alignment.totalLength) < 1e-6;
            if (!shouldKeep) {
                continue;
            }

            double z = 0.0;
            if (terrain.SampleElevation(sample.point.x, sample.point.y, z)) {
                samples.push_back(roadproto::domain::profile::ProfileGroundSample{
                    sample.station,
                    z,
                    roadproto::domain::alignment::StationFormatter::format(sample.station),
                    0});
            } else {
                ++skippedCount;
            }
        }
    }
    return samples;
}
```

After implementation, verify the code compiles; if `element.samples` density does not include exact interval multiples, this first version keeps nearest existing alignment samples at or beyond interval spacing.

- [ ] **Step 4: Implement `runProfileGradeGraphCreateCommand`**

Use this flow:

```cpp
void runProfileGradeGraphCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_PROFILE_GRADE_GRAPH_CREATE: 请选择道路中线。");

    AcDbObjectId centerlineId;
    if (!selectRoadCenterlineEntity(centerlineId)) {
        editor.writeWarning(L"未选择道路中线实体。");
        return;
    }

    DnRoadCenterlineEntity* centerline = nullptr;
    if (acdbOpenObject(centerline, centerlineId, AcDb::kForRead) != Acad::eOk || centerline == nullptr) {
        editor.writeError(L"无法打开道路中线实体。");
        return;
    }

    const auto alignment = centerline->alignment();
    const auto centerlineHandle = entityHandleText(centerline);
    centerline->close();

    roadproto::application::profile::ProfileGradeGraphCreateInput input;
    input.roadName = alignment.properties.roadName;
    input.roadCenterlineHandle = centerlineHandle;

    if (alignment.properties.linkedTerrainEnabled && !alignment.properties.linkedTerrainHandle.empty()) {
        AcDbObjectId terrainId;
        if (resolveObjectIdFromHandle(alignment.properties.linkedTerrainHandle, terrainId)) {
            DnTerrainTinEntity* terrain = nullptr;
            if (acdbOpenObject(terrain, terrainId, AcDb::kForRead) == Acad::eOk && terrain != nullptr) {
                std::size_t skipped = 0;
                input.sourceType = roadproto::domain::profile::ProfileGroundSourceType::TerrainTin;
                input.terrainTinHandle = alignment.properties.linkedTerrainHandle;
                input.groundSamples = sampleGroundFromTerrain(alignment, *terrain, 10.0, skipped);
                terrain->close();
                if (skipped > 0) {
                    editor.writeWarning(L"部分路线采样点位于 TIN 范围外，已跳过。");
                }
            }
        }
    }

    if (input.groundSamples.empty()) {
        std::wstring dmxPath;
        if (!promptDmxFilePath(dmxPath, editor)) {
            editor.writeWarning(L"纵断面拉坡图创建已取消。");
            return;
        }
        const auto parsed = roadproto::domain::profile::ProfileDmxFile::read(dmxPath);
        if (!parsed.succeeded) {
            editor.writeError(parsed.errorMessage.empty() ? L"DMX 文件解析失败。" : parsed.errorMessage);
            return;
        }
        input.sourceType = roadproto::domain::profile::ProfileGroundSourceType::DmxFile;
        input.dmxFilePath = dmxPath;
        input.groundSamples = parsed.samples;
    }

    AcGePoint3d insertionPoint;
    if (!getPoint(L"\n请选择纵断面拉坡图放置点: ", insertionPoint)) {
        editor.writeWarning(L"纵断面拉坡图创建已取消。");
        return;
    }

    roadproto::application::profile::ProfileGradeGraphCreateService service;
    const auto created = service.build(input);
    if (!created.succeeded) {
        editor.writeError(created.errorMessage);
        return;
    }

    auto* entity = new DnProfileGradeGraphEntity();
    entity->setGraphData(created.graph);
    entity->setInsertionPoint(insertionPoint);
    AcDbObjectId profileId;
    if (!appendEntityToModelSpace(entity, profileId)) {
        delete entity;
        editor.writeError(L"插入 DnProfileGradeGraphEntity 失败。");
        return;
    }

    const auto handle = entityHandleText(entity);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"已创建纵断面拉坡图实体，handle: " + handle);
}
```

- [ ] **Step 5: Build ARX**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: ARX project builds.

- [ ] **Step 6: Commit Task 5**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: create profile grade graph from centerline"
```

---

### Task 6: WPF Bridge DTO, File Protocol, and Managed Tests

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileGradeGraphDialogDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileGradeGraphDialogFile.cs`
- Modify: `tests/RoadProtoManagedBridgeTests/RoadProtoManagedBridgeTests.csproj`
- Modify: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] **Step 1: Write failing managed bridge tests**

Add compile links to `tests/RoadProtoManagedBridgeTests/RoadProtoManagedBridgeTests.csproj`:

```xml
    <Compile Include="..\..\src\ui\wpf\RoadProto.Terrain.UI\Bridge\ProfileGradeGraphDialogDtos.cs" Link="Bridge\ProfileGradeGraphDialogDtos.cs" />
    <Compile Include="..\..\src\ui\wpf\RoadProto.Terrain.UI\Bridge\ProfileGradeGraphDialogFile.cs" Link="Bridge\ProfileGradeGraphDialogFile.cs" />
```

Add tests to `tests/RoadProtoManagedBridgeTests/Program.cs`:

```csharp
static void ProfileResponseWritesUpdateDmxAction()
{
    var path = NewTempFile();
    try
    {
        ProfileGradeGraphDialogFile.WriteResponse(path, new ProfileGradeGraphDialogResponse
        {
            Action = ProfileGradeGraphDialogAction.UpdateGroundLine,
            Accepted = true,
            Handle = "2B1",
            SourceType = ProfileGroundSourceType.DmxFile,
            GraphName = "K1拉坡图",
            DmxFilePath = "C:/data/K1.dmx",
            GroundLineColorIndex = 4,
            GroundLineWidth = 1,
            SampleInterval = 10,
            VerticalScale = 10,
            GridSpacing = 10,
        });

        var content = File.ReadAllText(path);
        Check(content.Contains("action=updateGroundLine"), "profile response should request DMX ground update");
        Check(content.Contains("handle=2B1"), "profile response should keep graph handle");
        Check(content.Contains("dmxFilePath=C:/data/K1.dmx"), "profile response should keep DMX path");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

static void ProfileRequestReadsDmxSource()
{
    var path = NewTempFile();
    try
    {
        File.WriteAllLines(path, new[]
        {
            "mode=full",
            "handle=2B1",
            "responsePath=C:/temp/profile.response",
            "sourceType=dmxFile",
            "graphName=K1拉坡图",
            "dmxFilePath=C:/data/K1.dmx",
            "groundLineColorIndex=4",
            "groundLineWidth=1",
            "sampleInterval=10",
            "verticalScale=10",
            "gridSpacing=10",
        });

        var request = ProfileGradeGraphDialogFile.ReadRequest(path);
        Check(request.SourceType == ProfileGroundSourceType.DmxFile, "profile request should read source type");
        Check(request.DmxFilePath == "C:/data/K1.dmx", "profile request should read DMX path");
        Check(request.CanUpdateGroundLine, "DMX source should enable ground update");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}
```

Call both tests before the final `Console.WriteLine`.

- [ ] **Step 2: Run managed tests to verify RED**

Run:

```powershell
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj
```

Expected: FAIL because profile bridge classes do not exist.

- [ ] **Step 3: Create DTO classes**

Create `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileGradeGraphDialogDtos.cs`:

```csharp
namespace RoadProto.Terrain.UI.Bridge;

public enum ProfileGroundSourceType
{
    TerrainTin,
    DmxFile,
}

public enum ProfileGradeGraphDialogAction
{
    Apply,
    UpdateGroundLine,
}

public sealed class ProfileGradeGraphDialogRequest
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public ProfileGroundSourceType SourceType { get; set; } = ProfileGroundSourceType.DmxFile;
    public string GraphName { get; set; } = "拉坡图";
    public string DmxFilePath { get; set; } = string.Empty;
    public int GroundLineColorIndex { get; set; } = 4;
    public double GroundLineWidth { get; set; } = 1.0;
    public double SampleInterval { get; set; } = 10.0;
    public double VerticalScale { get; set; } = 10.0;
    public double GridSpacing { get; set; } = 10.0;
    public bool CanUpdateGroundLine => SourceType == ProfileGroundSourceType.DmxFile;
}

public sealed class ProfileGradeGraphDialogResponse
{
    public ProfileGradeGraphDialogAction Action { get; set; } = ProfileGradeGraphDialogAction.Apply;
    public bool Accepted { get; set; }
    public string Handle { get; set; } = string.Empty;
    public ProfileGroundSourceType SourceType { get; set; } = ProfileGroundSourceType.DmxFile;
    public string GraphName { get; set; } = "拉坡图";
    public string DmxFilePath { get; set; } = string.Empty;
    public int GroundLineColorIndex { get; set; } = 4;
    public double GroundLineWidth { get; set; } = 1.0;
    public double SampleInterval { get; set; } = 10.0;
    public double VerticalScale { get; set; } = 10.0;
    public double GridSpacing { get; set; } = 10.0;
}
```

- [ ] **Step 4: Create file protocol**

Create `src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileGradeGraphDialogFile.cs` by copying the key-value escaping approach from `AlignmentDialogFile.cs` and using these keys:

```text
action
accepted
handle
responsePath
sourceType
graphName
dmxFilePath
groundLineColorIndex
groundLineWidth
sampleInterval
verticalScale
gridSpacing
```

Use source type text:

```csharp
private static ProfileGroundSourceType ParseSourceType(string value)
    => value.Equals("terrainTin", StringComparison.OrdinalIgnoreCase)
        ? ProfileGroundSourceType.TerrainTin
        : ProfileGroundSourceType.DmxFile;

private static string SourceTypeText(ProfileGroundSourceType sourceType)
    => sourceType == ProfileGroundSourceType.TerrainTin ? "terrainTin" : "dmxFile";
```

Use action text:

```csharp
private static ProfileGradeGraphDialogAction ParseAction(string value)
    => value.Equals("updateGroundLine", StringComparison.OrdinalIgnoreCase)
        ? ProfileGradeGraphDialogAction.UpdateGroundLine
        : ProfileGradeGraphDialogAction.Apply;

private static string ActionText(ProfileGradeGraphDialogAction action)
    => action == ProfileGradeGraphDialogAction.UpdateGroundLine ? "updateGroundLine" : "apply";
```

- [ ] **Step 5: Run managed tests to verify GREEN**

Run:

```powershell
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj
```

Expected: `All RoadProto managed bridge tests passed.`

- [ ] **Step 6: Commit Task 6**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add tests/RoadProtoManagedBridgeTests/RoadProtoManagedBridgeTests.csproj tests/RoadProtoManagedBridgeTests/Program.cs src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileGradeGraphDialogDtos.cs src/ui/wpf/RoadProto.Terrain.UI/Bridge/ProfileGradeGraphDialogFile.cs
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add profile grade graph WPF bridge protocol"
```

---

### Task 7: WPF Property Window and Dialog Command

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/ProfileGradeGraphWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/ProfileGradeGraphWindow.xaml.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AlignmentDialogCommands.cs`

- [ ] **Step 1: Add WPF command**

In `AlignmentDialogCommands.cs`, add a new command method:

```csharp
[CommandMethod("RD_PROFILE_SHOW_WPF_DIALOG")]
public void ShowProfileGradeGraphDialog()
{
    var document = CoreApplication.DocumentManager.MdiActiveDocument;
    if (document == null)
    {
        return;
    }

    try
    {
        var requestPath = GetCommandArgument();
        var request = ProfileGradeGraphDialogFile.ReadRequest(requestPath);
        var window = new ProfileGradeGraphWindow(request);
        CoreApplication.ShowModalWindow(window);
        var response = window.Response ?? new ProfileGradeGraphDialogResponse
        {
            Accepted = false,
            Handle = request.Handle,
            SourceType = request.SourceType,
        };
        ProfileGradeGraphDialogFile.WriteResponse(request.ResponsePath, response);
        var responseCommandPath = request.ResponsePath.Replace('\\', '/');
        document.SendStringToExecute($"RD_PROFILE_APPLY_DIALOG_FILE \"{responseCommandPath}\" ", true, false, true);
    }
    catch (System.Exception error)
    {
        document.Editor.WriteMessage($"\nRoadProto 纵断面拉坡图窗口打开失败: {error.Message}");
    }
}
```

Use the existing argument-reading helper in that file. If it is private to alignment naming, rename it to a neutral helper and keep alignment behavior unchanged.

- [ ] **Step 2: Create XAML window**

Create `ProfileGradeGraphWindow.xaml`:

```xml
<Window x:Class="RoadProto.Terrain.UI.ProfileGradeGraphWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="纵断面拉坡图"
        Width="420"
        Height="420"
        WindowStartupLocation="CenterOwner"
        ResizeMode="NoResize">
    <Grid Margin="16">
        <Grid.RowDefinitions>
            <RowDefinition Height="*" />
            <RowDefinition Height="Auto" />
        </Grid.RowDefinitions>
        <StackPanel>
            <TextBlock Text="拉坡图名称" />
            <TextBox x:Name="GraphNameBox" Margin="0,4,0,10" />
            <TextBlock Text="地面线颜色索引" />
            <TextBox x:Name="GroundLineColorBox" Margin="0,4,0,10" />
            <TextBlock Text="地面线宽度" />
            <TextBox x:Name="GroundLineWidthBox" Margin="0,4,0,10" />
            <TextBlock Text="地面线精度" />
            <TextBox x:Name="SampleIntervalBox" Margin="0,4,0,10" />
            <TextBlock Text="纵向比例" />
            <ComboBox x:Name="VerticalScaleBox" Margin="0,4,0,10">
                <ComboBoxItem Content="1:1" Tag="1" />
                <ComboBoxItem Content="1:10" Tag="10" />
                <ComboBoxItem Content="1:100" Tag="100" />
            </ComboBox>
            <TextBlock Text="网格线间距" />
            <TextBox x:Name="GridSpacingBox" Margin="0,4,0,10" />
            <TextBlock Text="DMX 文件路径" />
            <TextBox x:Name="DmxFilePathBox" Margin="0,4,0,10" IsReadOnly="True" />
        </StackPanel>
        <StackPanel Grid.Row="1" Orientation="Horizontal" HorizontalAlignment="Right">
            <Button x:Name="UpdateGroundLineButton" Content="更新地面线" Width="96" Margin="0,0,8,0" Click="OnUpdateGroundLine" />
            <Button Content="确定" Width="72" Margin="0,0,8,0" Click="OnOk" />
            <Button Content="取消" Width="72" Click="OnCancel" />
        </StackPanel>
    </Grid>
</Window>
```

- [ ] **Step 3: Create code-behind**

Create `ProfileGradeGraphWindow.xaml.cs`:

```csharp
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class ProfileGradeGraphWindow : Window
{
    private readonly ProfileGradeGraphDialogRequest _request;

    public ProfileGradeGraphWindow(ProfileGradeGraphDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        LoadRequest();
    }

    public ProfileGradeGraphDialogResponse? Response { get; private set; }

    private void LoadRequest()
    {
        GraphNameBox.Text = _request.GraphName;
        GroundLineColorBox.Text = _request.GroundLineColorIndex.ToString(CultureInfo.InvariantCulture);
        GroundLineWidthBox.Text = _request.GroundLineWidth.ToString("R", CultureInfo.InvariantCulture);
        SampleIntervalBox.Text = _request.SampleInterval.ToString("R", CultureInfo.InvariantCulture);
        GridSpacingBox.Text = _request.GridSpacing.ToString("R", CultureInfo.InvariantCulture);
        DmxFilePathBox.Text = _request.DmxFilePath;
        UpdateGroundLineButton.IsEnabled = _request.CanUpdateGroundLine;
        SelectVerticalScale(_request.VerticalScale);
    }

    private void SelectVerticalScale(double scale)
    {
        foreach (ComboBoxItem item in VerticalScaleBox.Items)
        {
            if (double.TryParse(item.Tag?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out var value)
                && Math.Abs(value - scale) < 1e-9)
            {
                VerticalScaleBox.SelectedItem = item;
                return;
            }
        }
        VerticalScaleBox.SelectedIndex = 1;
    }

    private double SelectedVerticalScale()
    {
        if (VerticalScaleBox.SelectedItem is ComboBoxItem item
            && double.TryParse(item.Tag?.ToString(), NumberStyles.Float, CultureInfo.InvariantCulture, out var value))
        {
            return value;
        }
        return 10.0;
    }

    private ProfileGradeGraphDialogResponse CreateResponse(ProfileGradeGraphDialogAction action, bool accepted)
        => new()
        {
            Action = action,
            Accepted = accepted,
            Handle = _request.Handle,
            SourceType = _request.SourceType,
            GraphName = GraphNameBox.Text.Trim(),
            DmxFilePath = _request.DmxFilePath,
            GroundLineColorIndex = ParseInt(GroundLineColorBox.Text, _request.GroundLineColorIndex),
            GroundLineWidth = ParseDouble(GroundLineWidthBox.Text, _request.GroundLineWidth),
            SampleInterval = ParseDouble(SampleIntervalBox.Text, _request.SampleInterval),
            VerticalScale = SelectedVerticalScale(),
            GridSpacing = ParseDouble(GridSpacingBox.Text, _request.GridSpacing),
        };

    private static int ParseInt(string text, int fallback)
        => int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var value) ? value : fallback;

    private static double ParseDouble(string text, double fallback)
        => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var value) ? value : fallback;

    private void OnUpdateGroundLine(object sender, RoutedEventArgs e)
    {
        Response = CreateResponse(ProfileGradeGraphDialogAction.UpdateGroundLine, true);
        DialogResult = true;
    }

    private void OnOk(object sender, RoutedEventArgs e)
    {
        Response = CreateResponse(ProfileGradeGraphDialogAction.Apply, true);
        DialogResult = true;
    }

    private void OnCancel(object sender, RoutedEventArgs e)
    {
        Response = CreateResponse(ProfileGradeGraphDialogAction.Apply, false);
        DialogResult = false;
    }
}
```

- [ ] **Step 4: Build WPF project**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: build succeeds.

- [ ] **Step 5: Commit Task 7**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/ui/wpf/RoadProto.Terrain.UI/ProfileGradeGraphWindow.xaml src/ui/wpf/RoadProto.Terrain.UI/ProfileGradeGraphWindow.xaml.cs src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AlignmentDialogCommands.cs
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add profile grade graph WPF dialog"
```

---

### Task 8: ObjectARX Edit, Apply, and DMX Update Flow

**Files:**
- Create: `src/cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.h`
- Create: `src/cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.cpp`
- Modify: `src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Build before implementation to verify RED for edit**

Load ARX and double-click is not wired yet; manually run:

```text
RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE
```

Expected before implementation: command stub does nothing.

- [ ] **Step 2: Create C++ bridge DTO**

Create `ProfileGradeGraphDialogBridge.h`:

```cpp
#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

#include <string>

namespace roadproto::cad_adapter::objectarx {

enum class ProfileGradeGraphDialogAction {
    Apply,
    UpdateGroundLine,
};

struct ProfileGradeGraphDialogRequest {
    std::wstring handle;
    std::wstring responsePath;
    domain::profile::ProfileGradeGraphData graph;
};

struct ProfileGradeGraphDialogResponse {
    ProfileGradeGraphDialogAction action = ProfileGradeGraphDialogAction::Apply;
    bool accepted = false;
    std::wstring handle;
    domain::profile::ProfileGroundSourceType sourceType = domain::profile::ProfileGroundSourceType::DmxFile;
    domain::profile::ProfileGradeGraphProperties properties;
    std::wstring dmxFilePath;
};

bool queueProfileGradeGraphWpfDialog(const ProfileGradeGraphDialogRequest& request, std::wstring& errorMessage);
bool readProfileGradeGraphDialogResponse(
    const std::wstring& responsePath,
    ProfileGradeGraphDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx
```

- [ ] **Step 3: Implement bridge file protocol**

Create `ProfileGradeGraphDialogBridge.cpp` by following `AlignmentDialogBridge.cpp`. It must:

- Write request file to `%TEMP%`.
- Include `handle`, `responsePath`, `sourceType`, `graphName`, `dmxFilePath`, `groundLineColorIndex`, `groundLineWidth`, `sampleInterval`, `verticalScale`, `gridSpacing`.
- Queue AutoCAD command `RD_PROFILE_SHOW_WPF_DIALOG "<requestPath>"`.
- Read response keys written by WPF.
- Normalize quoted response path using `normalizeAutoCadCommandArgument`.

- [ ] **Step 4: Implement edit handle command**

In `ObjectArxProfileGradeGraphCommand.cpp`, implement:

```cpp
void runProfileGradeGraphEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR handleBuffer[64] = {};
    if (acedGetString(Adesk::kFalse, L"\n纵断面拉坡图 handle: ", handleBuffer) != RTNORM) {
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handleBuffer, entityId)) {
        editor.writeWarning(L"未找到 handle 对应的纵断面拉坡图实体。");
        return;
    }

    DnProfileGradeGraphEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开纵断面拉坡图实体。");
        return;
    }

    ProfileGradeGraphDialogRequest request;
    request.handle = handleBuffer;
    request.graph = entity->graphData();
    entity->close();

    std::wstring errorMessage;
    if (!queueProfileGradeGraphWpfDialog(request, errorMessage)) {
        editor.writeError(L"打开纵断面拉坡图窗口失败: " + errorMessage);
    }
}
```

- [ ] **Step 5: Implement apply command and DMX update**

In `runProfileApplyDialogFileCommand`:

1. Read response file path with `acedGetString(Adesk::kTrue, ...)`.
2. Parse response.
3. Resolve handle to `DnProfileGradeGraphEntity`.
4. If `accepted == false`, return.
5. Open entity for write.
6. If action is `UpdateGroundLine`, require source type `DmxFile`, read `response.dmxFilePath`, parse with `ProfileDmxFile::read`, and call `replaceGroundSamples`.
7. Apply display properties with safe defaults:

```cpp
if (response.properties.groundLineWidth <= 0.0) {
    response.properties.groundLineWidth = 1.0;
}
if (response.properties.sampleInterval <= 0.0) {
    response.properties.sampleInterval = 10.0;
}
if (response.properties.verticalScale <= 0.0) {
    response.properties.verticalScale = 10.0;
}
if (response.properties.gridSpacing <= 0.0) {
    response.properties.gridSpacing = 10.0;
}
entity->setProperties(response.properties);
entity->recordGraphicsModified(true);
```

- [ ] **Step 6: Build ARX**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: ARX builds.

- [ ] **Step 7: Commit Task 8**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/app/RoadProtoArx.vcxproj src/cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.h src/cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.cpp src/cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.cpp
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: edit profile grade graph properties"
```

---

### Task 9: Ribbon and Double-Click Integration

**Files:**
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`

- [ ] **Step 1: Add Ribbon constants**

Add:

```csharp
private const string ProfilePanelId = "ROADPROTO_PROFILE_PANEL";
private const string ProfileGradeGraphButtonId = "ROADPROTO_RD_PROFILE_GRADE_GRAPH_CREATE";
private const string ProfileGradeGraphDxfName = "DNPROFILEGRADEGRAPHENTITY";
```

- [ ] **Step 2: Add Profile Ribbon panel and button**

In `TryCreateRibbon`, after the alignment panel, add:

```csharp
var profilePanel = tab.Panels.FirstOrDefault(item => item.Source.Id == ProfilePanelId);
if (profilePanel == null)
{
    var source = new RibbonPanelSource
    {
        Id = ProfilePanelId,
        Title = "纵断面设计",
    };
    profilePanel = new RibbonPanel { Source = source };
    tab.Panels.Add(profilePanel);
}

if (!profilePanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == ProfileGradeGraphButtonId))
{
    profilePanel.Source.Items.Add(CreateAlignmentCommandButton(
        ProfileGradeGraphButtonId,
        "纵断面拉坡图",
        "选择道路中线并创建纵断面拉坡图实体",
        "RD_PROFILE_GRADE_GRAPH_CREATE "));
}
```

- [ ] **Step 3: Add double-click forwarding**

In `OnBeginDoubleClick`, after road centerline detection:

```csharp
if (TryFindEntityByDxfName(document, e.Location, ProfileGradeGraphDxfName, out var profileGraphId))
{
    if (!SuppressDuplicateDoubleClick(profileGraphId))
    {
        QueueProfileGradeGraphEditByHandle(document, profileGraphId);
    }
}
```

Add:

```csharp
private static void QueueProfileGradeGraphEditByHandle(Document document, ObjectId entityId)
{
    var handle = entityId.Handle.ToString();
    if (string.IsNullOrWhiteSpace(handle))
    {
        return;
    }

    document.SendStringToExecute($"RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE {handle} ", true, false, true);
}
```

- [ ] **Step 4: Build WPF project**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: build succeeds.

- [ ] **Step 5: Commit Task 9**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "feat: add profile grade graph ribbon entry"
```

---

### Task 10: Documentation, Version Log, and Final Verification

**Files:**
- Create: `docs/business/profile/纵断面拉坡图_创建.md`
- Create: `docs/business/profile/纵断面拉坡图_属性编辑.md`
- Create: `docs/modules/profile.md`
- Modify: `docs/modules/module_index.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`

- [ ] **Step 1: Create business doc for creation**

Create `docs/business/profile/纵断面拉坡图_创建.md` with:

```markdown
# 纵断面拉坡图 / 创建

## 基本信息

- 功能名称：纵断面拉坡图创建
- 所属模块：纵断面设计
- 命令名称：`RD_PROFILE_GRADE_GRAPH_CREATE`
- 对应代码入口：`ObjectArxProfileGradeGraphCommand.cpp`、`ProfileGradeGraphCreateService.*`、`DnProfileGradeGraphEntity.*`
- 业务文档维护人：RoadProto
- 原型版本：v0.1.9
- 是否可复用：是

## 功能背景

纵断面拉坡图是竖曲线设计前的基础对象，用于沿道路中线展示地面线高程变化。

## 业务目标

- 从道路中线关联数模取高，或从 DMX 文件读取纵地面线。
- 在图纸中生成可保存、可双击编辑的纵断面拉坡图自定义实体。

## 适用场景

- 已有道路中线和关联 TIN 数模。
- 没有数模但已有 `.dmx` 纵地面线文件。

## 输入条件

- CAD 选择对象：`DnRoadCenterlineEntity`。
- 用户输入参数：DMX 文件路径或图面放置点。
- 已有设计实体：道路中线；可选 TIN 数模。
- 外部数据：`.dmx` 文件。

## 输出结果

- CAD 图形实体：`DnProfileGradeGraphEntity`。
- 领域实体：桩号-地面高程点列和拉坡图显示属性。
- 更新通知或重建请求：第一版不自动触发。

## 操作流程

1. 运行 `RD_PROFILE_GRADE_GRAPH_CREATE`。
2. 选择道路中线。
3. 有有效关联数模时沿道路中线取样并从 TIN 插值高程。
4. 没有有效关联数模时选择 DMX 文件。
5. 点选图纸中的拉坡图放置点。
6. 系统插入纵断面拉坡图实体。

## 关键业务规则

- x 方向按桩号 1:1 绘制。
- y 方向按纵向比例放大。
- DMX 同一桩号多条高程记录按文件顺序保留。
- 有效地面线点少于 2 个时不创建实体。

## 可复用性说明

- 可复用内容：DMX 解析、纵断面地面线数据、图形布局和自定义实体。
- 临时原型内容：文件式 WPF Bridge。
- 正式复用前需要改造的内容：统一关系持久化和自动重建。

## 与其他模块的依赖关系

- 上游模块：平面设计、地形数模。
- 下游模块：纵断面竖曲线设计。
- 实体联动行为：第一版保存 handle 和来源类型，后续接入统一关系管理。

## 后续对接正式 EICAD 功能的注意事项

- 竖曲线设计应复用同一拉坡图实体和地面线数据，不重写 DMX 解析。
```

- [ ] **Step 2: Create business doc for editing**

Create `docs/business/profile/纵断面拉坡图_属性编辑.md` with:

```markdown
# 纵断面拉坡图 / 属性编辑

## 基本信息

- 功能名称：纵断面拉坡图属性编辑
- 所属模块：纵断面设计
- 命令名称：`RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE`、`RD_PROFILE_APPLY_DIALOG_FILE`
- 对应代码入口：`ProfileGradeGraphDialogBridge.*`、`ProfileGradeGraphWindow.xaml`
- 业务文档维护人：RoadProto
- 原型版本：v0.1.9
- 是否可复用：部分可复用

## 功能背景

拉坡图创建后需要调整显示属性，并在 DMX 文件更新后刷新地面线数据。

## 业务目标

- 双击 `DnProfileGradeGraphEntity` 打开 WPF 属性窗口。
- 修改地面线颜色、线宽、精度、纵向比例和网格间距。
- DMX 来源支持重新读取文件更新地面线。

## 适用场景

- 调整纵断面拉坡图显示。
- 外部 DMX 文件修改后刷新图纸中的地面线。

## 输入条件

- CAD 选择对象：`DnProfileGradeGraphEntity`。
- 用户输入参数：显示属性和更新地面线动作。
- 已有设计实体：纵断面拉坡图实体。
- 外部数据：DMX 来源时读取实体保存的 `.dmx` 路径。

## 输出结果

- CAD 图形实体：刷新后的 `DnProfileGradeGraphEntity`。
- 领域实体：更新后的显示属性或地面线样本。
- 更新通知或重建请求：第一版不自动触发下游。

## 操作流程

1. 双击纵断面拉坡图实体。
2. WPF 窗口展示属性。
3. 用户修改属性并确认。
4. DMX 来源可点击“更新地面线”重新读取保存路径。
5. 数模来源“更新地面线”置灰。

## 关键业务规则

- WPF 不直接操作 ObjectARX 类型。
- 数模来源不使用 DMX 更新按钮。
- DMX 更新失败时不覆盖原地面线数据。
- 非法线宽、比例、精度和网格间距由 C++ 侧使用默认值兜底。

## 可复用性说明

- 可复用内容：属性 DTO、文件式 Bridge、DMX 更新动作。
- 临时原型内容：文件式请求/响应协议。
- 正式复用前需要改造的内容：统一 UI 通道和关系重建队列。

## 与其他模块的依赖关系

- 上游模块：纵断面拉坡图创建。
- 下游模块：纵断面竖曲线设计。
- 实体联动行为：后续接入统一关系管理。

## 后续对接正式 EICAD 功能的注意事项

- 竖曲线编辑窗口可复用拉坡图属性窗口中的比例和网格设置。
```

- [ ] **Step 3: Create module doc and update indexes**

Create `docs/modules/profile.md` with module code `PROFILE`, command table for the three commands, Ribbon position `RoadProto / 纵断面设计 / 纵断面拉坡图`, and code landing table.

Update `docs/modules/module_index.md` row for `纵断面设计` to point to `docs/modules/profile.md` and mark V0.1 as “纵断面拉坡图创建与属性编辑已实现”.

Update `docs/reuse/capability_catalog.md` with profile entries:

```markdown
| DMX 纵地面线解析 | V0.1.9 原型，支持普通桩号、断链桩号和同桩多高程保留 | `src/domain/profile/ProfileDmxFile.*` |
| 纵断面拉坡图自定义实体 | V0.1.9 原型，支持表头、网格、地面线折线、DWG 持久化和 WPF 属性编辑 | `src/cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.*` |
```

- [ ] **Step 4: Update version and test docs**

Update `docs/dev/version_log.md` with a `v0.1.9` entry containing ARX name `RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx`, new profile module, DMX parsing, custom entity, and known limitations.

Update `tests/README.md` to list DMX parser, profile layout, profile command metadata, and profile WPF Bridge tests.

- [ ] **Step 5: Run full verification**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" src\app\RoadProtoArx.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected:

- Core tests print `All core tests passed.`
- Managed bridge tests print `All RoadProto managed bridge tests passed.`
- WPF build succeeds.
- ARX build succeeds.

- [ ] **Step 6: Commit Task 10**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" add docs/business/profile/纵断面拉坡图_创建.md docs/business/profile/纵断面拉坡图_属性编辑.md docs/modules/profile.md docs/modules/module_index.md docs/reuse/capability_catalog.md docs/dev/version_log.md tests/README.md
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe" commit -m "docs: document profile grade graph feature"
```

---

## Manual AutoCAD Verification

After all automated builds pass:

1. Load the Debug ARX in AutoCAD 2021:

```text
ARXLOAD artifacts\x64\Debug\RoadProto_v0.1.9_20260511_ProfileGradeGraph.arx
NETLOAD artifacts\managed\Debug\net48\RoadProto.Terrain.UI.dll
```

2. Create or open a DWG containing a `DnRoadCenterlineEntity`.
3. Run `RD_PROFILE_GRADE_GRAPH_CREATE`.
4. For a road centerline with linked terrain, verify ground line comes from TIN.
5. For a road centerline without linked terrain, choose a `.dmx` file and verify graph creation.
6. Verify the graph title, frame, grid, y elevation labels, K labels, and cyan ground line display.
7. Double-click the graph and verify the property window opens.
8. Change vertical scale from `1:10` to `1:100` and verify the y direction stretches.
9. For DMX source, edit the `.dmx` file, click “更新地面线”, and verify the ground line changes.
10. For terrain source, verify “更新地面线” is disabled.
11. Save, close, reopen, and run `REGEN`; verify the graph persists.

## Self-Review Notes

Spec coverage:

- Architecture and module boundaries: Tasks 1-3.
- DMX parsing, duplicates, break-chain storage: Task 1.
- Terrain and DMX creation flows: Task 5.
- Custom entity drawing and persistence: Task 4.
- WPF edit/update UI: Tasks 6-8.
- Ribbon and double-click: Task 9.
- Required docs and version notes: Task 10.

Type consistency:

- C++ source enum: `ProfileGroundSourceType`.
- C# source enum: `ProfileGroundSourceType`.
- Entity class: `DnProfileGradeGraphEntity`.
- Create command: `RD_PROFILE_GRADE_GRAPH_CREATE`.
- Edit handle command: `RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE`.
- Apply command: `RD_PROFILE_APPLY_DIALOG_FILE`.
