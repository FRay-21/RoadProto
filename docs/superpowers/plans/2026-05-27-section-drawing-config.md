# 横断面图配置 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the `横断面图配置` workflow so a selected section drawing can configure pavement layers for all section drawings from the same road model, support CSV import/export, allow grip editing of pavement-layer face vertices, and make pavement quantity statistics use edited section drawing geometry.

**Architecture:** Keep configuration in `DnRoadModelSectionDrawingEntity`, not in `DnRoadModelEntity`. Put station priority, component matching, and CSV parsing in `domain/cross_section`; keep CAD selection, template picking, entity scanning, DWG persistence, and grips in `cad_adapter/objectarx`; keep WPF as DTO editing only. Quantity calculation should map drawing faces into a domain-level sampler before falling back to road-model section data.

**Tech Stack:** C++17, ObjectARX 2021, WPF .NET Framework 4.8, RoadProto file-bridge commands, core tests in `tests/core_tests.cpp`, MSBuild from Visual Studio 2026 Insiders.

---

## File Structure

- Create `src/domain/cross_section/SectionDrawingConfigModel.h`: domain data model for section drawing config rows, component options, CSV import/export, row priority resolution.
- Create `src/domain/cross_section/SectionDrawingConfigModel.cpp`: implementation of normalization, matching, CSV parser/writer, and helpers.
- Modify `tests/core_tests.cpp`: include the new domain header and add tests for priority, component matching, CSV, drawing-face quantity sampling, and source contracts.
- Modify `tests/RoadProtoCoreTests.vcxproj`: compile `SectionDrawingConfigModel.cpp` and the new quantity sampler.
- Modify `src/app/RoadProtoArx.vcxproj`: compile new domain and ObjectARX bridge/command files.
- Create `src/domain/quantity/PavementQuantityDrawingFaceSampler.h/.cpp`: ObjectARX-free quantity sampler for current section drawing face polygons.
- Modify `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h/.cpp`: persist config data, face source fields, `manualEdited`, and grip-edit face vertices.
- Create `src/cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.h/.cpp`: C++ request/response file bridge.
- Create `src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.h/.cpp`: command entry points, selection, template picking, bulk update.
- Modify `src/modules/cross_section/CrossSectionModule.cpp`: register user, edit-handle, and apply-dialog commands.
- Create `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogDtos.cs`: WPF DTOs.
- Create `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogFile.cs`: WPF request/response and CSV file handling.
- Create `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SectionDrawingConfigDialogCommands.cs`: AutoCAD .NET command that opens the WPF window and sends the C++ apply command.
- Create `src/ui/wpf/RoadProto.Terrain.UI/SectionDrawingConfigWindow.xaml` and `.xaml.cs`: WPF configuration window.
- Modify `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`: add Ribbon button, command class registration, DXF name, and double-click hook.
- Modify `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.cpp`: sample from drawing faces first.
- Add `docs/business/cross_section/横断面图配置.md`.
- Modify `docs/business/cross_section/查看横断面.md`, `docs/business/drawing_quantity/路面工程量统计表.md`, `docs/modules/cross_section.md`, `docs/modules/module_index.md`, `README.md`, `tests/README.md`, and `docs/dev/version_log.md`.

---

### Task 1: Domain Config Model and CSV

**Files:**
- Create: `src/domain/cross_section/SectionDrawingConfigModel.h`
- Create: `src/domain/cross_section/SectionDrawingConfigModel.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write failing domain tests**

Add the include near the other cross-section includes:

```cpp
#include "domain/cross_section/SectionDrawingConfigModel.h"
```

Add these test functions in `tests/core_tests.cpp` near the existing cross-section domain tests:

```cpp
void sectionDrawingConfigRowsResolveByStationAndPriority()
{
    using namespace roadproto::domain::cross_section;

    SectionDrawingConfigData config;
    config.pavementRows.push_back(
        SectionPavementLayerConfigRow{
            20.0,
            40.0,
            {SectionDrawingComponentTypeSelection{SubgradeSide::Right, SubgradeComponentType::HardShoulder}},
            L"AA",
            L"右侧硬路肩结构层"});
    config.pavementRows.push_back(
        SectionPavementLayerConfigRow{
            0.0,
            100.0,
            {SectionDrawingComponentTypeSelection{SubgradeSide::Right, SubgradeComponentType::TravelLane}},
            L"BB",
            L"右侧行车道结构层"});

    std::wstring errorMessage;
    CHECK(SectionDrawingConfigRules::normalize(config, errorMessage));
    const auto firstMatch = SectionDrawingConfigRules::resolvePavementRow(config, 30.0);
    CHECK(firstMatch.has_value());
    CHECK(firstMatch->rowIndex == 0);
    CHECK(firstMatch->row.templateHandle == L"AA");

    const auto secondMatch = SectionDrawingConfigRules::resolvePavementRow(config, 50.0);
    CHECK(secondMatch.has_value());
    CHECK(secondMatch->rowIndex == 1);
    CHECK(secondMatch->row.templateHandle == L"BB");

    CHECK(!SectionDrawingConfigRules::resolvePavementRow(config, 120.0).has_value());
}

void sectionDrawingConfigComponentMatchingUsesSideAndType()
{
    using namespace roadproto::domain::cross_section;

    SectionPavementLayerConfigRow row;
    row.componentTypes = {
        SectionDrawingComponentTypeSelection{SubgradeSide::Left, SubgradeComponentType::TravelLane},
        SectionDrawingComponentTypeSelection{SubgradeSide::Right, SubgradeComponentType::HardShoulder},
    };

    CHECK(SectionDrawingConfigRules::matchesComponent(row, SubgradeSide::Left, SubgradeComponentType::TravelLane));
    CHECK(SectionDrawingConfigRules::matchesComponent(row, SubgradeSide::Right, SubgradeComponentType::HardShoulder));
    CHECK(!SectionDrawingConfigRules::matchesComponent(row, SubgradeSide::Right, SubgradeComponentType::TravelLane));
}

void sectionDrawingConfigCsvRoundTripsUtf8Rows()
{
    using namespace roadproto::domain::cross_section;

    SectionDrawingConfigData config;
    config.configPath = L"F:\\section_config.csv";
    config.pavementRows.push_back(
        SectionPavementLayerConfigRow{
            0.0,
            100.0,
            {
                SectionDrawingComponentTypeSelection{SubgradeSide::Left, SubgradeComponentType::TravelLane},
                SectionDrawingComponentTypeSelection{SubgradeSide::Right, SubgradeComponentType::HardShoulder},
            },
            L"1A2B",
            L"主线结构层"});

    const auto csv = SectionDrawingConfigCsv::write(config);
    CHECK(csv.find("\xEF\xBB\xBF") == 0);
    CHECK(csv.find("起点桩号,终点桩号,路基类型,模板Handle,模板名称") != std::string::npos);
    CHECK(csv.find("左侧行车道;右侧硬路肩") != std::string::npos);

    std::wstring errorMessage;
    const auto parsed = SectionDrawingConfigCsv::read(csv, L"F:\\section_config.csv", errorMessage);
    CHECK(parsed.has_value());
    if (parsed.has_value()) {
        CHECK(parsed->configPath == L"F:\\section_config.csv");
        CHECK(parsed->pavementRows.size() == 1);
        CHECK(parsed->pavementRows.front().startStation == 0.0);
        CHECK(parsed->pavementRows.front().endStation == 100.0);
        CHECK(parsed->pavementRows.front().componentTypes.size() == 2);
        CHECK(parsed->pavementRows.front().templateHandle == L"1A2B");
        CHECK(parsed->pavementRows.front().templateName == L"主线结构层");
    }
}
```

Call the tests from `main()` alongside other test calls:

```cpp
    sectionDrawingConfigRowsResolveByStationAndPriority();
    sectionDrawingConfigComponentMatchingUsesSideAndType();
    sectionDrawingConfigCsvRoundTripsUtf8Rows();
```

- [ ] **Step 2: Run test build to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL because `domain/cross_section/SectionDrawingConfigModel.h` does not exist.

- [ ] **Step 3: Add the domain header**

Create `src/domain/cross_section/SectionDrawingConfigModel.h`:

```cpp
#pragma once

#include "domain/cross_section/SubgradeTemplateModel.h"

#include <optional>
#include <string>
#include <vector>

namespace roadproto::domain::cross_section {

struct SectionDrawingComponentTypeSelection {
    SubgradeSide side = SubgradeSide::Right;
    SubgradeComponentType componentType = SubgradeComponentType::TravelLane;
};

struct SectionPavementLayerConfigRow {
    double startStation = 0.0;
    double endStation = 0.0;
    std::vector<SectionDrawingComponentTypeSelection> componentTypes;
    std::wstring templateHandle;
    std::wstring templateName;
};

struct SectionDrawingConfigData {
    std::wstring configPath;
    std::vector<SectionPavementLayerConfigRow> pavementRows;
    int version = 1;
};

struct SectionDrawingResolvedPavementRow {
    std::size_t rowIndex = 0;
    SectionPavementLayerConfigRow row;
};

class SectionDrawingConfigRules {
public:
    static bool normalize(SectionDrawingConfigData& data, std::wstring& errorMessage);
    static std::optional<SectionDrawingResolvedPavementRow> resolvePavementRow(
        const SectionDrawingConfigData& data,
        double station);
    static bool matchesComponent(
        const SectionPavementLayerConfigRow& row,
        SubgradeSide side,
        SubgradeComponentType componentType);
    static std::wstring componentSelectionCode(const SectionDrawingComponentTypeSelection& selection);
    static std::wstring componentSelectionDisplayName(const SectionDrawingComponentTypeSelection& selection);
    static std::optional<SectionDrawingComponentTypeSelection> componentSelectionFromText(
        const std::wstring& text);
};

class SectionDrawingConfigCsv {
public:
    static std::string write(const SectionDrawingConfigData& data);
    static std::optional<SectionDrawingConfigData> read(
        const std::string& csv,
        const std::wstring& configPath,
        std::wstring& errorMessage);
};

} // namespace roadproto::domain::cross_section
```

- [ ] **Step 4: Add the domain implementation**

Create `src/domain/cross_section/SectionDrawingConfigModel.cpp`:

```cpp
#include "domain/cross_section/SectionDrawingConfigModel.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <unordered_map>

namespace roadproto::domain::cross_section {
namespace {

constexpr double kStationTolerance = 1.0e-7;

bool isFinite(double value)
{
    return std::isfinite(value);
}

std::wstring trim(std::wstring value)
{
    while (!value.empty() && std::iswspace(value.front()) != 0) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::iswspace(value.back()) != 0) {
        value.pop_back();
    }
    return value;
}

std::string wideToUtf8(const std::wstring& value)
{
    std::string output;
    for (const auto ch : value) {
        if (ch <= 0x7F) {
            output.push_back(static_cast<char>(ch));
        } else if (ch <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0 | ((ch >> 6) & 0x1F)));
            output.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        } else {
            output.push_back(static_cast<char>(0xE0 | ((ch >> 12) & 0x0F)));
            output.push_back(static_cast<char>(0x80 | ((ch >> 6) & 0x3F)));
            output.push_back(static_cast<char>(0x80 | (ch & 0x3F)));
        }
    }
    return output;
}

std::wstring utf8ToWide(const std::string& value)
{
    std::wstring output;
    for (std::size_t i = 0; i < value.size();) {
        const auto ch = static_cast<unsigned char>(value[i]);
        if (ch < 0x80) {
            output.push_back(static_cast<wchar_t>(ch));
            ++i;
        } else if ((ch & 0xE0) == 0xC0 && i + 1 < value.size()) {
            const auto next = static_cast<unsigned char>(value[i + 1]);
            output.push_back(static_cast<wchar_t>(((ch & 0x1F) << 6) | (next & 0x3F)));
            i += 2;
        } else if ((ch & 0xF0) == 0xE0 && i + 2 < value.size()) {
            const auto second = static_cast<unsigned char>(value[i + 1]);
            const auto third = static_cast<unsigned char>(value[i + 2]);
            output.push_back(static_cast<wchar_t>(((ch & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F)));
            i += 3;
        } else {
            ++i;
        }
    }
    return output;
}

std::string csvEscape(const std::wstring& value)
{
    const auto utf8 = wideToUtf8(value);
    if (utf8.find_first_of(",\"\r\n") == std::string::npos) {
        return utf8;
    }
    std::string escaped = "\"";
    for (const auto ch : utf8) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped += "\"";
    return escaped;
}

std::vector<std::string> parseCsvLine(const std::string& line)
{
    std::vector<std::string> cells;
    std::string cell;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const auto ch = line[i];
        if (quoted && ch == '"' && i + 1 < line.size() && line[i + 1] == '"') {
            cell.push_back('"');
            ++i;
        } else if (ch == '"') {
            quoted = !quoted;
        } else if (!quoted && ch == ',') {
            cells.push_back(cell);
            cell.clear();
        } else {
            cell.push_back(ch);
        }
    }
    cells.push_back(cell);
    return cells;
}

std::vector<std::wstring> splitWide(const std::wstring& text, wchar_t separator)
{
    std::vector<std::wstring> result;
    std::wstring current;
    for (const auto ch : text) {
        if (ch == separator) {
            if (!trim(current).empty()) {
                result.push_back(trim(current));
            }
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    if (!trim(current).empty()) {
        result.push_back(trim(current));
    }
    return result;
}

std::wstring sideCode(SubgradeSide side)
{
    return side == SubgradeSide::Left ? L"Left" : L"Right";
}

std::wstring sideDisplayName(SubgradeSide side)
{
    return side == SubgradeSide::Left ? L"左侧" : L"右侧";
}

std::optional<SubgradeSide> sideFromText(const std::wstring& text)
{
    if (text == L"Left" || text == L"左侧") {
        return SubgradeSide::Left;
    }
    if (text == L"Right" || text == L"右侧") {
        return SubgradeSide::Right;
    }
    return std::nullopt;
}

} // namespace

bool SectionDrawingConfigRules::normalize(SectionDrawingConfigData& data, std::wstring& errorMessage)
{
    errorMessage.clear();
    data.version = std::max(1, data.version);
    for (auto& row : data.pavementRows) {
        if (!isFinite(row.startStation) || !isFinite(row.endStation)) {
            errorMessage = L"横断面图配置桩号必须是有限数值。";
            return false;
        }
        if (row.endStation < row.startStation) {
            std::swap(row.startStation, row.endStation);
        }
        row.templateHandle = trim(row.templateHandle);
        row.templateName = trim(row.templateName);
        std::vector<SectionDrawingComponentTypeSelection> unique;
        for (const auto& selection : row.componentTypes) {
            const auto exists = std::any_of(unique.begin(), unique.end(), [&](const auto& candidate) {
                return candidate.side == selection.side && candidate.componentType == selection.componentType;
            });
            if (!exists) {
                unique.push_back(selection);
            }
        }
        row.componentTypes = std::move(unique);
    }
    return true;
}

std::optional<SectionDrawingResolvedPavementRow> SectionDrawingConfigRules::resolvePavementRow(
    const SectionDrawingConfigData& data,
    double station)
{
    if (!isFinite(station)) {
        return std::nullopt;
    }
    for (std::size_t i = 0; i < data.pavementRows.size(); ++i) {
        const auto& row = data.pavementRows[i];
        if (row.templateHandle.empty()) {
            continue;
        }
        if (station >= row.startStation - kStationTolerance && station <= row.endStation + kStationTolerance) {
            return SectionDrawingResolvedPavementRow{i, row};
        }
    }
    return std::nullopt;
}

bool SectionDrawingConfigRules::matchesComponent(
    const SectionPavementLayerConfigRow& row,
    SubgradeSide side,
    SubgradeComponentType componentType)
{
    return std::any_of(row.componentTypes.begin(), row.componentTypes.end(), [&](const auto& selection) {
        return selection.side == side && selection.componentType == componentType;
    });
}

std::wstring SectionDrawingConfigRules::componentSelectionCode(const SectionDrawingComponentTypeSelection& selection)
{
    return sideCode(selection.side) + L":" + std::wstring(subgradeComponentTypeCode(selection.componentType));
}

std::wstring SectionDrawingConfigRules::componentSelectionDisplayName(const SectionDrawingComponentTypeSelection& selection)
{
    return sideDisplayName(selection.side) + std::wstring(subgradeComponentTypeDisplayName(selection.componentType));
}

std::optional<SectionDrawingComponentTypeSelection> SectionDrawingConfigRules::componentSelectionFromText(
    const std::wstring& text)
{
    const auto value = trim(text);
    const auto separator = value.find(L':');
    if (separator != std::wstring::npos) {
        const auto side = sideFromText(value.substr(0, separator));
        const auto type = subgradeComponentTypeFromCode(value.substr(separator + 1));
        if (side.has_value()) {
            return SectionDrawingComponentTypeSelection{*side, type};
        }
    }

    const std::pair<SubgradeSide, std::wstring> sides[] = {
        {SubgradeSide::Left, L"左侧"},
        {SubgradeSide::Right, L"右侧"},
    };
    for (const auto& side : sides) {
        if (value.rfind(side.second, 0) != 0) {
            continue;
        }
        const auto typeName = value.substr(side.second.size());
        const auto allTypes = {
            SubgradeComponentType::Median,
            SubgradeComponentType::TravelLane,
            SubgradeComponentType::HardShoulder,
            SubgradeComponentType::EarthShoulder,
            SubgradeComponentType::SideMedian,
            SubgradeComponentType::Sidewalk,
            SubgradeComponentType::BikeLane,
            SubgradeComponentType::CurbStrip,
        };
        for (const auto type : allTypes) {
            if (typeName == subgradeComponentTypeDisplayName(type)) {
                return SectionDrawingComponentTypeSelection{side.first, type};
            }
        }
    }
    return std::nullopt;
}

std::string SectionDrawingConfigCsv::write(const SectionDrawingConfigData& data)
{
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << "\xEF\xBB\xBF起点桩号,终点桩号,路基类型,模板Handle,模板名称\n";
    for (const auto& row : data.pavementRows) {
        std::wstring components;
        for (std::size_t i = 0; i < row.componentTypes.size(); ++i) {
            if (i > 0) {
                components += L";";
            }
            components += SectionDrawingConfigRules::componentSelectionDisplayName(row.componentTypes[i]);
        }
        stream << std::setprecision(17) << row.startStation << ','
               << std::setprecision(17) << row.endStation << ','
               << csvEscape(components) << ','
               << csvEscape(row.templateHandle) << ','
               << csvEscape(row.templateName) << '\n';
    }
    return stream.str();
}

std::optional<SectionDrawingConfigData> SectionDrawingConfigCsv::read(
    const std::string& csv,
    const std::wstring& configPath,
    std::wstring& errorMessage)
{
    errorMessage.clear();
    SectionDrawingConfigData data;
    data.configPath = configPath;

    std::istringstream stream(csv);
    std::string line;
    bool headerRead = false;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!headerRead) {
            if (line.rfind("\xEF\xBB\xBF", 0) == 0) {
                line = line.substr(3);
            }
            headerRead = true;
            continue;
        }
        if (line.empty()) {
            continue;
        }
        const auto cells = parseCsvLine(line);
        if (cells.size() < 5) {
            errorMessage = L"横断面图配置 CSV 列数不足。";
            return std::nullopt;
        }

        SectionPavementLayerConfigRow row;
        row.startStation = std::strtod(cells[0].c_str(), nullptr);
        row.endStation = std::strtod(cells[1].c_str(), nullptr);
        for (const auto& componentText : splitWide(utf8ToWide(cells[2]), L';')) {
            const auto selection = SectionDrawingConfigRules::componentSelectionFromText(componentText);
            if (selection.has_value()) {
                row.componentTypes.push_back(*selection);
            }
        }
        row.templateHandle = utf8ToWide(cells[3]);
        row.templateName = utf8ToWide(cells[4]);
        data.pavementRows.push_back(std::move(row));
    }

    if (!SectionDrawingConfigRules::normalize(data, errorMessage)) {
        return std::nullopt;
    }
    return data;
}

} // namespace roadproto::domain::cross_section
```

- [ ] **Step 5: Add new C++ files to projects**

In `tests/RoadProtoCoreTests.vcxproj`, add:

```xml
    <ClCompile Include="..\src\domain\cross_section\SectionDrawingConfigModel.cpp" />
```

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\domain\cross_section\SectionDrawingConfigModel.cpp" />
```

- [ ] **Step 6: Run tests to verify GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: build succeeds and tests pass.

- [ ] **Step 7: Commit**

```powershell
git add src/domain/cross_section/SectionDrawingConfigModel.h src/domain/cross_section/SectionDrawingConfigModel.cpp tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj
git commit -m "feat: add section drawing config domain rules"
```

---

### Task 2: Quantity Sampler for Current Drawing Faces

**Files:**
- Create: `src/domain/quantity/PavementQuantityDrawingFaceSampler.h`
- Create: `src/domain/quantity/PavementQuantityDrawingFaceSampler.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write failing quantity sampler test**

Add include:

```cpp
#include "domain/quantity/PavementQuantityDrawingFaceSampler.h"
```

Add this test near existing quantity tests:

```cpp
void pavementQuantityDrawingFaceSamplerUsesEditedPolygonGeometry()
{
    using namespace roadproto::domain::quantity;

    std::vector<PavementQuantityDrawingFace> faces;
    faces.push_back(
        PavementQuantityDrawingFace{
            L"上面层",
            L"右侧行车道",
            {
                PavementQuantityDrawingPoint{0.0, 1.0},
                PavementQuantityDrawingPoint{4.0, 1.0},
                PavementQuantityDrawingPoint{5.0, 0.0},
                PavementQuantityDrawingPoint{0.0, 0.0},
            }});

    const auto sample = PavementQuantityDrawingFaceSampler::sampleAtStation(25.0, faces);
    CHECK(sample.has_value());
    if (sample.has_value()) {
        CHECK(sample->station == 25.0);
        CHECK(sample->layers.size() == 1);
        CHECK(sample->layers.front().layerName == L"上面层");
        CHECK(sample->layers.front().componentName == L"右侧行车道");
        CHECK(std::fabs(sample->layers.front().projectedWidth - 5.0) < 1.0e-9);
        CHECK(std::fabs(sample->layers.front().sectionArea - 4.5) < 1.0e-9);
    }
}
```

Call it from `main()`:

```cpp
    pavementQuantityDrawingFaceSamplerUsesEditedPolygonGeometry();
```

- [ ] **Step 2: Run test build to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: FAIL because `domain/quantity/PavementQuantityDrawingFaceSampler.h` does not exist.

- [ ] **Step 3: Add sampler header**

Create `src/domain/quantity/PavementQuantityDrawingFaceSampler.h`:

```cpp
#pragma once

#include "domain/quantity/PavementQuantityTable.h"

#include <optional>
#include <string>
#include <vector>

namespace roadproto::domain::quantity {

struct PavementQuantityDrawingPoint {
    double x = 0.0;
    double y = 0.0;
};

struct PavementQuantityDrawingFace {
    std::wstring layerName;
    std::wstring componentName;
    std::vector<PavementQuantityDrawingPoint> points;
};

class PavementQuantityDrawingFaceSampler {
public:
    static std::optional<PavementQuantitySectionSample> sampleAtStation(
        double station,
        const std::vector<PavementQuantityDrawingFace>& faces);
};

} // namespace roadproto::domain::quantity
```

- [ ] **Step 4: Add sampler implementation**

Create `src/domain/quantity/PavementQuantityDrawingFaceSampler.cpp`:

```cpp
#include "domain/quantity/PavementQuantityDrawingFaceSampler.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace roadproto::domain::quantity {
namespace {

struct Totals {
    double projectedWidth = 0.0;
    double sectionArea = 0.0;
};

using LayerKey = std::pair<std::wstring, std::wstring>;

std::wstring normalizeLayerName(const std::wstring& value)
{
    return value.empty() ? L"路面结构层" : value;
}

std::wstring normalizeComponentName(const std::wstring& value)
{
    return value.empty() ? L"未分部件" : value;
}

double polygonArea(const std::vector<PavementQuantityDrawingPoint>& points)
{
    if (points.size() < 3) {
        return 0.0;
    }
    double sum = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& first = points[i];
        const auto& second = points[(i + 1) % points.size()];
        sum += first.x * second.y - second.x * first.y;
    }
    return std::fabs(sum) * 0.5;
}

} // namespace

std::optional<PavementQuantitySectionSample> PavementQuantityDrawingFaceSampler::sampleAtStation(
    double station,
    const std::vector<PavementQuantityDrawingFace>& faces)
{
    std::map<LayerKey, Totals> totalsByLayer;
    for (const auto& face : faces) {
        if (face.points.size() < 3) {
            continue;
        }

        auto minX = face.points.front().x;
        auto maxX = face.points.front().x;
        for (const auto& point : face.points) {
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
                minX = maxX = 0.0;
                break;
            }
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
        }
        const auto area = polygonArea(face.points);
        if (area <= 0.0 || maxX <= minX) {
            continue;
        }

        auto& totals = totalsByLayer[LayerKey{normalizeComponentName(face.componentName), normalizeLayerName(face.layerName)}];
        totals.projectedWidth += maxX - minX;
        totals.sectionArea += area;
    }

    if (totalsByLayer.empty()) {
        return std::nullopt;
    }

    PavementQuantitySectionSample sample;
    sample.station = station;
    for (const auto& item : totalsByLayer) {
        sample.layers.push_back(
            PavementQuantityLayerSample{
                item.first.second,
                item.second.projectedWidth,
                item.second.sectionArea,
                item.first.first});
    }
    return sample;
}

} // namespace roadproto::domain::quantity
```

- [ ] **Step 5: Add source file to projects**

In `tests/RoadProtoCoreTests.vcxproj`, add:

```xml
    <ClCompile Include="..\src\domain\quantity\PavementQuantityDrawingFaceSampler.cpp" />
```

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\domain\quantity\PavementQuantityDrawingFaceSampler.cpp" />
```

- [ ] **Step 6: Run tests to verify GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: build succeeds and tests pass.

- [ ] **Step 7: Commit**

```powershell
git add src/domain/quantity/PavementQuantityDrawingFaceSampler.h src/domain/quantity/PavementQuantityDrawingFaceSampler.cpp tests/core_tests.cpp tests/RoadProtoCoreTests.vcxproj src/app/RoadProtoArx.vcxproj
git commit -m "feat: sample pavement quantities from drawing faces"
```

---

### Task 3: Section Drawing Entity Persistence and Grips

**Files:**
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h`
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing source-contract test**

Add this test near the existing `DnRoadModelSectionDrawingEntity` source-contract test:

```cpp
void sectionDrawingEntityPersistsConfigAndEditableFaceContracts()
{
    const auto root = findRepositoryRootForTests();
    const auto header = readTextFileForTests(
        root / "src" / "cad_adapter" / "objectarx" / "cross_section" / "DnRoadModelSectionDrawingEntity.h");
    const auto source = readTextFileForTests(
        root / "src" / "cad_adapter" / "objectarx" / "cross_section" / "DnRoadModelSectionDrawingEntity.cpp");

    CHECK(header.find("SectionDrawingConfigData") != std::string::npos);
    CHECK(header.find("faceId") != std::string::npos);
    CHECK(header.find("sourceTemplateHandle") != std::string::npos);
    CHECK(header.find("sourceConfigRowIndex") != std::string::npos);
    CHECK(header.find("manualEdited") != std::string::npos);
    CHECK(header.find("subGetGripPoints") != std::string::npos);
    CHECK(header.find("subMoveGripPointsAt") != std::string::npos);

    CHECK(source.find("kEntityVersion = 4") != std::string::npos);
    CHECK(source.find("writeSectionDrawingConfig") != std::string::npos);
    CHECK(source.find("readSectionDrawingConfig") != std::string::npos);
    CHECK(source.find("manualEdited = true") != std::string::npos);
    CHECK(source.find("faceGripIndex") != std::string::npos);
}
```

Call it from `main()`:

```cpp
    sectionDrawingEntityPersistsConfigAndEditableFaceContracts();
```

- [ ] **Step 2: Run tests to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL because the header/source do not contain the new fields and grip overrides.

- [ ] **Step 3: Extend the entity header**

In `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h`, include the config model:

```cpp
#include "domain/cross_section/SectionDrawingConfigModel.h"
```

Extend `RoadModelSectionDrawingFace`:

```cpp
    std::wstring faceId;
    std::wstring sourceTemplateHandle;
    int sourceConfigRowIndex = -1;
    bool manualEdited = false;
```

Extend `RoadModelSectionDrawingData`:

```cpp
    roadproto::domain::cross_section::SectionDrawingConfigData config;
```

Add public mutation helpers:

```cpp
    Acad::ErrorStatus setSectionDrawingConfig(
        const roadproto::domain::cross_section::SectionDrawingConfigData& config);
    Acad::ErrorStatus replaceFaces(std::vector<roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingFace> faces);
```

Add grip overrides:

```cpp
    Acad::ErrorStatus subGetGripPoints(
        AcGePoint3dArray& gripPoints,
        AcDbIntArray& osnapModes,
        AcDbIntArray& geomIds) const override;
    Acad::ErrorStatus subMoveGripPointsAt(const AcDbIntArray& indices, const AcGeVector3d& offset) override;
```

- [ ] **Step 4: Extend validation and persistence**

In `DnRoadModelSectionDrawingEntity.cpp`:

Add:

```cpp
#include <optional>
```

Change:

```cpp
constexpr Adesk::Int16 kEntityVersion = 3;
```

to:

```cpp
constexpr Adesk::Int16 kEntityVersion = 4;
```

Add helpers near `writeWideString`:

```cpp
void writeSectionDrawingConfig(
    AcDbDwgFiler* filer,
    const roadproto::domain::cross_section::SectionDrawingConfigData& config)
{
    filer->writeInt32(config.version);
    writeWideString(filer, config.configPath);
    filer->writeInt32(static_cast<Adesk::Int32>(config.pavementRows.size()));
    for (const auto& row : config.pavementRows) {
        filer->writeDouble(row.startStation);
        filer->writeDouble(row.endStation);
        filer->writeInt32(static_cast<Adesk::Int32>(row.componentTypes.size()));
        for (const auto& component : row.componentTypes) {
            filer->writeInt32(static_cast<Adesk::Int32>(component.side));
            filer->writeInt32(static_cast<Adesk::Int32>(component.componentType));
        }
        writeWideString(filer, row.templateHandle);
        writeWideString(filer, row.templateName);
    }
}

bool readSectionDrawingConfig(
    AcDbDwgFiler* filer,
    roadproto::domain::cross_section::SectionDrawingConfigData& config,
    std::wstring& errorMessage)
{
    using roadproto::domain::cross_section::SectionDrawingComponentTypeSelection;
    using roadproto::domain::cross_section::SectionPavementLayerConfigRow;
    using roadproto::domain::cross_section::SubgradeComponentType;
    using roadproto::domain::cross_section::SubgradeSide;

    Adesk::Int32 version = 1;
    filer->readInt32(&version);
    config.version = static_cast<int>(version);
    config.configPath = readWideString(filer);

    Adesk::Int32 rowCount = 0;
    if (!readCount(filer, 10000, rowCount)) {
        return false;
    }
    config.pavementRows.reserve(static_cast<std::size_t>(rowCount));
    for (Adesk::Int32 i = 0; i < rowCount; ++i) {
        SectionPavementLayerConfigRow row;
        filer->readDouble(&row.startStation);
        filer->readDouble(&row.endStation);

        Adesk::Int32 componentCount = 0;
        if (!readCount(filer, 1000, componentCount)) {
            return false;
        }
        for (Adesk::Int32 j = 0; j < componentCount; ++j) {
            Adesk::Int32 side = 0;
            Adesk::Int32 componentType = 0;
            filer->readInt32(&side);
            filer->readInt32(&componentType);
            row.componentTypes.push_back(
                SectionDrawingComponentTypeSelection{
                    static_cast<SubgradeSide>(side),
                    static_cast<SubgradeComponentType>(componentType)});
        }
        row.templateHandle = readWideString(filer);
        row.templateName = readWideString(filer);
        config.pavementRows.push_back(std::move(row));
    }
    return roadproto::domain::cross_section::SectionDrawingConfigRules::normalize(config, errorMessage);
}
```

In `dwgInFields`, after reading `face.componentName` for version 3, read version 4 face fields:

```cpp
        if (version >= 4) {
            face.faceId = readWideString(filer);
            face.sourceTemplateHandle = readWideString(filer);
            Adesk::Int32 rowIndex = -1;
            filer->readInt32(&rowIndex);
            face.sourceConfigRowIndex = static_cast<int>(rowIndex);
            Adesk::Int32 manualEdited = 0;
            filer->readInt32(&manualEdited);
            face.manualEdited = manualEdited != 0;
        }
```

After reading lines and before axes for version 4:

```cpp
    if (version >= 4) {
        std::wstring configError;
        if (!readSectionDrawingConfig(filer, data.config, configError)) {
            return Acad::eInvalidInput;
        }
    }
```

In `dwgOutFields`, write the new face fields immediately after `componentName`:

```cpp
        writeWideString(filer, face.faceId);
        writeWideString(filer, face.sourceTemplateHandle);
        filer->writeInt32(static_cast<Adesk::Int32>(face.sourceConfigRowIndex));
        filer->writeInt32(face.manualEdited ? 1 : 0);
```

After writing lines and before axes:

```cpp
    writeSectionDrawingConfig(filer, data_.config);
```

- [ ] **Step 5: Add mutation helpers and grips**

Add methods before `dwgInFields`:

```cpp
Acad::ErrorStatus DnRoadModelSectionDrawingEntity::setSectionDrawingConfig(
    const roadproto::domain::cross_section::SectionDrawingConfigData& config)
{
    assertWriteEnabled();
    auto updated = data_;
    updated.config = config;
    std::wstring errorMessage;
    if (!roadproto::domain::cross_section::SectionDrawingConfigRules::normalize(updated.config, errorMessage)) {
        return Acad::eInvalidInput;
    }
    data_ = std::move(updated);
    recordGraphicsModified(true);
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::replaceFaces(std::vector<RoadModelSectionDrawingFace> faces)
{
    assertWriteEnabled();
    auto updated = data_;
    updated.faces = std::move(faces);
    if (!validateDrawingData(updated)) {
        return Acad::eInvalidInput;
    }
    data_ = std::move(updated);
    recordGraphicsModified(true);
    return Acad::eOk;
}
```

Add helper near `sectionPoint`:

```cpp
struct FaceGripIndex {
    std::size_t faceIndex = 0;
    std::size_t pointIndex = 0;
};

std::optional<FaceGripIndex> faceGripIndex(int gripIndex, const RoadModelSectionDrawingData& data)
{
    if (gripIndex <= 0) {
        return std::nullopt;
    }
    auto remaining = static_cast<std::size_t>(gripIndex - 1);
    for (std::size_t faceIndex = 0; faceIndex < data.faces.size(); ++faceIndex) {
        const auto count = data.faces[faceIndex].points.size();
        if (remaining < count) {
            return FaceGripIndex{faceIndex, remaining};
        }
        remaining -= count;
    }
    return std::nullopt;
}
```

Add grip overrides after `subTransformBy`:

```cpp
Acad::ErrorStatus DnRoadModelSectionDrawingEntity::subGetGripPoints(
    AcGePoint3dArray& gripPoints,
    AcDbIntArray&,
    AcDbIntArray&) const
{
    assertReadEnabled();
    if (!validateDrawingData(data_) || !areAxesUsable(xAxis_, yAxis_)) {
        return Acad::eOk;
    }

    gripPoints.append(data_.insertionPoint);
    for (const auto& face : data_.faces) {
        for (const auto& point : face.points) {
            gripPoints.append(sectionPoint(data_.insertionPoint, xAxis_, yAxis_, point.x, point.y));
        }
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::subMoveGripPointsAt(
    const AcDbIntArray& indices,
    const AcGeVector3d& offset)
{
    assertWriteEnabled();
    if (indices.length() == 0 || offset.isZeroLength()) {
        return Acad::eOk;
    }

    const auto localDx = offset.dotProduct(xAxis_) / xAxis_.lengthSqrd();
    const auto localDy = offset.dotProduct(yAxis_) / yAxis_.lengthSqrd();
    for (int i = 0; i < indices.length(); ++i) {
        const auto index = indices.at(i);
        if (index == 0) {
            data_.insertionPoint += offset;
            continue;
        }
        const auto faceIndex = faceGripIndex(index, data_);
        if (!faceIndex.has_value()) {
            continue;
        }
        auto& face = data_.faces[faceIndex->faceIndex];
        auto& point = face.points[faceIndex->pointIndex];
        point.x += localDx;
        point.y += localDy;
        face.manualEdited = true;
    }
    recordGraphicsModified(true);
    return Acad::eOk;
}
```

- [ ] **Step 6: Run tests to verify GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: tests pass.

- [ ] **Step 7: Commit**

```powershell
git add src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.cpp tests/core_tests.cpp
git commit -m "feat: persist editable section drawing faces"
```

---

### Task 4: Bridge DTOs and WPF File Round-Trip

**Files:**
- Create: `src/cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.h`
- Create: `src/cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.cpp`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogFile.cs`
- Modify: `tests/core_tests.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] **Step 1: Write failing source-contract test**

Add:

```cpp
void sectionDrawingConfigBridgeSourceContracts()
{
    const auto root = findRepositoryRootForTests();
    const auto cppHeader = readTextFileForTests(
        root / "src" / "cad_adapter" / "objectarx" / "cross_section" / "SectionDrawingConfigDialogBridge.h");
    const auto cppSource = readTextFileForTests(
        root / "src" / "cad_adapter" / "objectarx" / "cross_section" / "SectionDrawingConfigDialogBridge.cpp");
    const auto dtoSource = readTextFileForTests(
        root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "Bridge" / "SectionDrawingConfigDialogDtos.cs");
    const auto fileSource = readTextFileForTests(
        root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "Bridge" / "SectionDrawingConfigDialogFile.cs");

    CHECK(cppHeader.find("SectionDrawingConfigDialogRequest") != std::string::npos);
    CHECK(cppHeader.find("SectionDrawingConfigDialogResponse") != std::string::npos);
    CHECK(cppHeader.find("PickTemplate") != std::string::npos);
    CHECK(cppSource.find("RD_SECTION_DRAWING_CONFIG_SHOW_WPF_DIALOG") != std::string::npos);
    CHECK(cppSource.find("componentOptionCount") != std::string::npos);
    CHECK(cppSource.find("pavementRowCount") != std::string::npos);

    CHECK(dtoSource.find("SectionDrawingConfigAction") != std::string::npos);
    CHECK(dtoSource.find("Draw") != std::string::npos);
    CHECK(dtoSource.find("PickTemplate") != std::string::npos);
    CHECK(dtoSource.find("ComponentOptions") != std::string::npos);
    CHECK(fileSource.find("ReadRequest") != std::string::npos);
    CHECK(fileSource.find("WriteResponse") != std::string::npos);
    CHECK(fileSource.find("ImportCsv") != std::string::npos);
    CHECK(fileSource.find("ExportCsv") != std::string::npos);
}
```

Call it from `main()`:

```cpp
    sectionDrawingConfigBridgeSourceContracts();
```

- [ ] **Step 2: Run tests to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL because the bridge files do not exist.

- [ ] **Step 3: Add C++ bridge header**

Create `src/cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.h`:

```cpp
#pragma once

#include "domain/cross_section/SectionDrawingConfigModel.h"

#include <string>
#include <vector>

namespace roadproto::cad_adapter::objectarx::cross_section {

struct SectionDrawingConfigComponentOption {
    std::wstring code;
    std::wstring displayName;
    roadproto::domain::cross_section::SectionDrawingComponentTypeSelection selection;
};

struct SectionDrawingConfigDialogRequest {
    std::wstring drawingHandle;
    std::wstring roadModelHandle;
    std::wstring responsePath;
    roadproto::domain::cross_section::SectionDrawingConfigData config;
    std::vector<SectionDrawingConfigComponentOption> componentOptions;
};

enum class SectionDrawingConfigDialogAction {
    None,
    Draw,
    PickTemplate
};

struct SectionDrawingConfigDialogResponse {
    SectionDrawingConfigDialogAction action = SectionDrawingConfigDialogAction::None;
    bool accepted = false;
    std::wstring drawingHandle;
    int pickRowIndex = -1;
    roadproto::domain::cross_section::SectionDrawingConfigData config;
};

bool queueSectionDrawingConfigWpfDialog(
    const SectionDrawingConfigDialogRequest& request,
    std::wstring& errorMessage);

bool readSectionDrawingConfigDialogResponse(
    const std::wstring& responsePath,
    SectionDrawingConfigDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
```

- [ ] **Step 4: Add C++ bridge implementation**

Create `src/cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.cpp` using the same key/value escaping helpers as `RoadModelSectionViewerBridge.cpp`. Use these required keys:

```cpp
// Request keys:
// drawingHandle, roadModelHandle, responsePath, configPath,
// componentOptionCount,
// componentOption.{i}.code,
// componentOption.{i}.displayName,
// pavementRowCount,
// pavementRow.{i}.startStation,
// pavementRow.{i}.endStation,
// pavementRow.{i}.componentCodes,
// pavementRow.{i}.templateHandle,
// pavementRow.{i}.templateName

// Response keys:
// action = none | draw | pickTemplate
// accepted = 0 | 1
// drawingHandle
// pickRowIndex
// configPath
// pavementRowCount and pavementRow.{i}.* fields
```

The implementation must queue:

```cpp
const std::wstring command = L"RD_SECTION_DRAWING_CONFIG_SHOW_WPF_DIALOG\n";
```

and write the pending request path to:

```cpp
RoadProtoSectionDrawingConfig_<process-id>.pending
```

- [ ] **Step 5: Add WPF DTOs**

Create `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogDtos.cs`:

```csharp
using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public enum SectionDrawingConfigAction
{
    None,
    Draw,
    PickTemplate,
}

public sealed class SectionDrawingConfigComponentOptionDto
{
    public string Code { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;
}

public sealed class SectionDrawingConfigRowDto
{
    public double StartStation { get; set; }
    public double EndStation { get; set; }
    public List<string> ComponentCodes { get; set; } = new();
    public string ComponentDisplayText { get; set; } = string.Empty;
    public string TemplateHandle { get; set; } = string.Empty;
    public string TemplateName { get; set; } = string.Empty;
}

public sealed class SectionDrawingConfigRequest
{
    public string DrawingHandle { get; set; } = string.Empty;
    public string RoadModelHandle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public string ConfigPath { get; set; } = string.Empty;
    public List<SectionDrawingConfigComponentOptionDto> ComponentOptions { get; set; } = new();
    public List<SectionDrawingConfigRowDto> PavementRows { get; set; } = new();
}

public sealed class SectionDrawingConfigResponse
{
    public SectionDrawingConfigAction Action { get; set; } = SectionDrawingConfigAction.None;
    public bool Accepted { get; set; }
    public string DrawingHandle { get; set; } = string.Empty;
    public int PickRowIndex { get; set; } = -1;
    public string ConfigPath { get; set; } = string.Empty;
    public List<SectionDrawingConfigRowDto> PavementRows { get; set; } = new();
}
```

- [ ] **Step 6: Add WPF file reader/writer and CSV helpers**

Create `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogFile.cs` following the existing `RoadModelSectionViewerFile` style. It must expose:

```csharp
public static SectionDrawingConfigRequest ReadRequest(string path)
public static void WriteResponse(string path, SectionDrawingConfigResponse response)
public static List<SectionDrawingConfigRowDto> ImportCsv(string path, IReadOnlyList<SectionDrawingConfigComponentOptionDto> options)
public static void ExportCsv(string path, IReadOnlyList<SectionDrawingConfigRowDto> rows, IReadOnlyList<SectionDrawingConfigComponentOptionDto> options)
```

Use UTF-8 with BOM for CSV:

```csharp
private static readonly UTF8Encoding Utf8Bom = new(encoderShouldEmitUTF8Identifier: true);
```

Write the exact CSV header:

```csharp
"起点桩号,终点桩号,路基类型,模板Handle,模板名称"
```

- [ ] **Step 7: Add bridge source to ARX project**

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\cad_adapter\objectarx\cross_section\SectionDrawingConfigDialogBridge.cpp" />
```

- [ ] **Step 8: Run tests and WPF build to verify GREEN**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: core tests pass and WPF builds.

- [ ] **Step 9: Commit**

```powershell
git add src/cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.h src/cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.cpp src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogDtos.cs src/ui/wpf/RoadProto.Terrain.UI/Bridge/SectionDrawingConfigDialogFile.cs tests/core_tests.cpp src/app/RoadProtoArx.vcxproj
git commit -m "feat: add section drawing config bridge"
```

---

### Task 5: WPF Configuration Window

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/SectionDrawingConfigWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/SectionDrawingConfigWindow.xaml.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SectionDrawingConfigDialogCommands.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing WPF source-contract test**

Add:

```cpp
void sectionDrawingConfigWpfWindowSourceContracts()
{
    const auto root = findRepositoryRootForTests();
    const auto xaml = readTextFileForTests(
        root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "SectionDrawingConfigWindow.xaml");
    const auto code = readTextFileForTests(
        root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "SectionDrawingConfigWindow.xaml.cs");
    const auto commands = readTextFileForTests(
        root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "AutoCad" / "SectionDrawingConfigDialogCommands.cs");
    const auto ribbon = readTextFileForTests(
        root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "AutoCad" / "RoadProtoRibbonExtension.cs");

    CHECK(xaml.find("路面结构层") != std::string::npos);
    CHECK(xaml.find("导入") != std::string::npos);
    CHECK(xaml.find("导出") != std::string::npos);
    CHECK(xaml.find("绘制") != std::string::npos);
    CHECK(xaml.find("取消") != std::string::npos);
    CHECK(xaml.find("ComponentDisplayText") != std::string::npos);
    CHECK(xaml.find("TemplateName") != std::string::npos);

    CHECK(code.find("ImportCsv") != std::string::npos);
    CHECK(code.find("ExportCsv") != std::string::npos);
    CHECK(code.find("PickTemplate") != std::string::npos);
    CHECK(code.find("Draw") != std::string::npos);
    CHECK(commands.find("RD_SECTION_DRAWING_CONFIG_SHOW_WPF_DIALOG") != std::string::npos);
    CHECK(commands.find("RD_SECTION_DRAWING_CONFIG_APPLY_DIALOG_FILE") != std::string::npos);
    CHECK(ribbon.find("CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.SectionDrawingConfigDialogCommands))") != std::string::npos);
}
```

Call from `main()`:

```cpp
    sectionDrawingConfigWpfWindowSourceContracts();
```

- [ ] **Step 2: Run tests to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL because WPF window and command files do not exist.

- [ ] **Step 3: Add WPF AutoCAD command**

Create `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SectionDrawingConfigDialogCommands.cs`:

```csharp
using System.Diagnostics;
using System.IO;
using System.Text;
using Autodesk.AutoCAD.Runtime;
using RoadProto.Terrain.UI.Bridge;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class SectionDrawingConfigDialogCommands
{
    private static string PendingRequestPath
        => Path.Combine(Path.GetTempPath(), $"RoadProtoSectionDrawingConfig_{Process.GetCurrentProcess().Id}.pending");

    [CommandMethod("RD_SECTION_DRAWING_CONFIG_SHOW_WPF_DIALOG", CommandFlags.Session)]
    public void ShowSectionDrawingConfigDialog()
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var editor = document?.Editor;
        if (document == null || editor == null)
        {
            return;
        }

        if (!File.Exists(PendingRequestPath))
        {
            editor.WriteMessage("\nRoadProto section drawing config pending request path is missing.");
            return;
        }

        try
        {
            var requestPath = File.ReadAllText(PendingRequestPath, Encoding.UTF8).Trim().Trim('"');
            File.Delete(PendingRequestPath);
            if (string.IsNullOrWhiteSpace(requestPath))
            {
                editor.WriteMessage("\nRoadProto section drawing config request path is empty.");
                return;
            }

            var request = SectionDrawingConfigDialogFile.ReadRequest(requestPath);
            var window = new SectionDrawingConfigWindow(request);
            window.ShowDialog();
            if (window.Response == null || string.IsNullOrWhiteSpace(request.ResponsePath))
            {
                return;
            }

            SectionDrawingConfigDialogFile.WriteResponse(request.ResponsePath, window.Response);
            var responseCommandPath = request.ResponsePath.Replace('\\', '/');
            document.SendStringToExecute(
                $"RD_SECTION_DRAWING_CONFIG_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n",
                true,
                false,
                true);
        }
        catch (System.Exception error)
        {
            editor.WriteMessage($"\nRoadProto section drawing config failed: {error.Message}");
        }
    }
}
```

- [ ] **Step 4: Add WPF window XAML**

Create `src/ui/wpf/RoadProto.Terrain.UI/SectionDrawingConfigWindow.xaml`:

```xml
<Window x:Class="RoadProto.Terrain.UI.SectionDrawingConfigWindow"
        xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        Title="横断面图配置"
        Width="860"
        Height="560"
        WindowStartupLocation="CenterScreen">
    <DockPanel Margin="12">
        <Grid DockPanel.Dock="Top" Margin="0,0,0,10">
            <Grid.ColumnDefinitions>
                <ColumnDefinition Width="Auto" />
                <ColumnDefinition Width="*" />
                <ColumnDefinition Width="Auto" />
                <ColumnDefinition Width="Auto" />
            </Grid.ColumnDefinitions>
            <TextBlock Text="配置文件" VerticalAlignment="Center" Margin="0,0,8,0" />
            <TextBox Grid.Column="1" Text="{Binding ConfigPath, UpdateSourceTrigger=PropertyChanged}" MinWidth="280" />
            <Button Grid.Column="2" Content="导入" Margin="8,0,0,0" Padding="14,4" Click="OnImportClick" />
            <Button Grid.Column="3" Content="导出" Margin="8,0,0,0" Padding="14,4" Click="OnExportClick" />
        </Grid>

        <StackPanel DockPanel.Dock="Bottom" Orientation="Horizontal" HorizontalAlignment="Right" Margin="0,10,0,0">
            <Button Content="绘制" Width="90" Margin="0,0,8,0" Click="OnDrawClick" />
            <Button Content="取消" Width="90" Click="OnCancelClick" />
        </StackPanel>

        <TabControl>
            <TabItem Header="路面结构层">
                <DockPanel Margin="0,10,0,0">
                    <StackPanel DockPanel.Dock="Top" Orientation="Horizontal" HorizontalAlignment="Right" Margin="0,0,0,8">
                        <Button Content="新增" Width="72" Margin="0,0,6,0" Click="OnAddRowClick" />
                        <Button Content="删除" Width="72" Margin="0,0,6,0" Click="OnRemoveRowClick" />
                        <Button Content="上移" Width="72" Margin="0,0,6,0" Click="OnMoveUpClick" />
                        <Button Content="下移" Width="72" Click="OnMoveDownClick" />
                    </StackPanel>
                    <DataGrid ItemsSource="{Binding PavementRows}"
                              SelectedItem="{Binding SelectedRow}"
                              AutoGenerateColumns="False"
                              CanUserAddRows="False"
                              HeadersVisibility="Column"
                              GridLinesVisibility="Horizontal">
                        <DataGrid.Columns>
                            <DataGridTextColumn Header="起点桩号" Binding="{Binding StartStation}" Width="120" />
                            <DataGridTextColumn Header="终点桩号" Binding="{Binding EndStation}" Width="120" />
                            <DataGridTemplateColumn Header="路基类型" Width="260">
                                <DataGridTemplateColumn.CellTemplate>
                                    <DataTemplate>
                                        <Button Content="{Binding ComponentDisplayText}" Click="OnComponentTypesClick" HorizontalContentAlignment="Left" />
                                    </DataTemplate>
                                </DataGridTemplateColumn.CellTemplate>
                            </DataGridTemplateColumn>
                            <DataGridTemplateColumn Header="模板" Width="*">
                                <DataGridTemplateColumn.CellTemplate>
                                    <DataTemplate>
                                        <DockPanel>
                                            <Button DockPanel.Dock="Right" Content="点选" Padding="10,2" Click="OnPickTemplateClick" />
                                            <TextBlock Text="{Binding TemplateName}" VerticalAlignment="Center" Margin="0,0,8,0" />
                                        </DockPanel>
                                    </DataTemplate>
                                </DataGridTemplateColumn.CellTemplate>
                            </DataGridTemplateColumn>
                        </DataGrid.Columns>
                    </DataGrid>
                </DockPanel>
            </TabItem>
        </TabControl>
    </DockPanel>
</Window>
```

- [ ] **Step 5: Add WPF window code-behind**

Create `src/ui/wpf/RoadProto.Terrain.UI/SectionDrawingConfigWindow.xaml.cs` with:

```csharp
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows;
using Microsoft.Win32;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class SectionDrawingConfigWindow : Window, INotifyPropertyChanged
{
    private readonly SectionDrawingConfigRequest _request;
    private SectionDrawingConfigRowDto? _selectedRow;

    public SectionDrawingConfigWindow(SectionDrawingConfigRequest request)
    {
        InitializeComponent();
        _request = request;
        ConfigPath = request.ConfigPath;
        ComponentOptions = new ObservableCollection<SectionDrawingConfigComponentOptionDto>(request.ComponentOptions);
        PavementRows = new ObservableCollection<SectionDrawingConfigRowDto>(request.PavementRows);
        foreach (var row in PavementRows)
        {
            RefreshComponentDisplayText(row);
        }
        DataContext = this;
    }

    public event PropertyChangedEventHandler? PropertyChanged;
    public SectionDrawingConfigResponse? Response { get; private set; }
    public string ConfigPath { get; set; }
    public ObservableCollection<SectionDrawingConfigComponentOptionDto> ComponentOptions { get; }
    public ObservableCollection<SectionDrawingConfigRowDto> PavementRows { get; }

    public SectionDrawingConfigRowDto? SelectedRow
    {
        get => _selectedRow;
        set
        {
            _selectedRow = value;
            OnPropertyChanged();
        }
    }

    private void OnImportClick(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog { Filter = "CSV 配置 (*.csv)|*.csv|所有文件 (*.*)|*.*" };
        if (dialog.ShowDialog(this) != true)
        {
            return;
        }
        PavementRows.Clear();
        foreach (var row in SectionDrawingConfigDialogFile.ImportCsv(dialog.FileName, ComponentOptions.ToList()))
        {
            RefreshComponentDisplayText(row);
            PavementRows.Add(row);
        }
        ConfigPath = dialog.FileName;
        OnPropertyChanged(nameof(ConfigPath));
    }

    private void OnExportClick(object sender, RoutedEventArgs e)
    {
        var dialog = new SaveFileDialog
        {
            Filter = "CSV 配置 (*.csv)|*.csv|所有文件 (*.*)|*.*",
            FileName = string.IsNullOrWhiteSpace(ConfigPath) ? "section_drawing_config.csv" : ConfigPath,
        };
        if (dialog.ShowDialog(this) != true)
        {
            return;
        }
        SectionDrawingConfigDialogFile.ExportCsv(dialog.FileName, PavementRows.ToList(), ComponentOptions.ToList());
        ConfigPath = dialog.FileName;
        OnPropertyChanged(nameof(ConfigPath));
    }

    private void OnAddRowClick(object sender, RoutedEventArgs e)
    {
        var row = new SectionDrawingConfigRowDto();
        RefreshComponentDisplayText(row);
        PavementRows.Add(row);
        SelectedRow = row;
    }

    private void OnRemoveRowClick(object sender, RoutedEventArgs e)
    {
        if (SelectedRow != null)
        {
            PavementRows.Remove(SelectedRow);
        }
    }

    private void OnMoveUpClick(object sender, RoutedEventArgs e)
    {
        MoveSelectedRow(-1);
    }

    private void OnMoveDownClick(object sender, RoutedEventArgs e)
    {
        MoveSelectedRow(1);
    }

    private void OnComponentTypesClick(object sender, RoutedEventArgs e)
    {
        var row = SelectedRow;
        if (row == null)
        {
            return;
        }
        var dialog = new Window
        {
            Title = "选择路基类型",
            Owner = this,
            Width = 320,
            Height = 420,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
        };
        var list = new System.Windows.Controls.ListBox { SelectionMode = System.Windows.Controls.SelectionMode.Multiple, Margin = new Thickness(10) };
        foreach (var option in ComponentOptions)
        {
            list.Items.Add(option);
        }
        list.DisplayMemberPath = nameof(SectionDrawingConfigComponentOptionDto.DisplayName);
        foreach (var item in list.Items.OfType<SectionDrawingConfigComponentOptionDto>())
        {
            if (row.ComponentCodes.Contains(item.Code))
            {
                list.SelectedItems.Add(item);
            }
        }
        var ok = new System.Windows.Controls.Button { Content = "确定", Width = 72, Margin = new Thickness(0, 8, 0, 0), HorizontalAlignment = HorizontalAlignment.Right };
        var panel = new System.Windows.Controls.DockPanel();
        System.Windows.Controls.DockPanel.SetDock(ok, System.Windows.Controls.Dock.Bottom);
        panel.Children.Add(ok);
        panel.Children.Add(list);
        dialog.Content = panel;
        ok.Click += (_, _) => dialog.DialogResult = true;
        if (dialog.ShowDialog() == true)
        {
            row.ComponentCodes = list.SelectedItems.OfType<SectionDrawingConfigComponentOptionDto>().Select(item => item.Code).ToList();
            RefreshComponentDisplayText(row);
            OnPropertyChanged(nameof(PavementRows));
        }
    }

    private void OnPickTemplateClick(object sender, RoutedEventArgs e)
    {
        if (SelectedRow == null)
        {
            return;
        }
        Response = CreateResponse(SectionDrawingConfigAction.PickTemplate);
        Response.PickRowIndex = PavementRows.IndexOf(SelectedRow);
        DialogResult = true;
    }

    private void OnDrawClick(object sender, RoutedEventArgs e)
    {
        Response = CreateResponse(SectionDrawingConfigAction.Draw);
        DialogResult = true;
    }

    private void OnCancelClick(object sender, RoutedEventArgs e)
    {
        Response = new SectionDrawingConfigResponse
        {
            Action = SectionDrawingConfigAction.None,
            Accepted = false,
            DrawingHandle = _request.DrawingHandle,
        };
        DialogResult = false;
    }

    private void MoveSelectedRow(int offset)
    {
        if (SelectedRow == null)
        {
            return;
        }
        var oldIndex = PavementRows.IndexOf(SelectedRow);
        var newIndex = oldIndex + offset;
        if (oldIndex < 0 || newIndex < 0 || newIndex >= PavementRows.Count)
        {
            return;
        }
        PavementRows.Move(oldIndex, newIndex);
    }

    private SectionDrawingConfigResponse CreateResponse(SectionDrawingConfigAction action)
        => new()
        {
            Action = action,
            Accepted = true,
            DrawingHandle = _request.DrawingHandle,
            ConfigPath = ConfigPath,
            PavementRows = PavementRows.ToList(),
        };

    private void RefreshComponentDisplayText(SectionDrawingConfigRowDto row)
    {
        row.ComponentDisplayText = string.Join("、",
            row.ComponentCodes
                .Select(code => ComponentOptions.FirstOrDefault(option => option.Code == code)?.DisplayName ?? code)
                .Where(text => !string.IsNullOrWhiteSpace(text)));
        if (string.IsNullOrWhiteSpace(row.ComponentDisplayText))
        {
            row.ComponentDisplayText = "未选择";
        }
    }

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
}
```

- [ ] **Step 6: Register WPF command class**

In `RoadProtoRibbonExtension.cs`, add assembly command class:

```csharp
[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.SectionDrawingConfigDialogCommands))]
```

- [ ] **Step 7: Run tests and WPF build**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: core tests pass and WPF builds.

- [ ] **Step 8: Commit**

```powershell
git add src/ui/wpf/RoadProto.Terrain.UI/SectionDrawingConfigWindow.xaml src/ui/wpf/RoadProto.Terrain.UI/SectionDrawingConfigWindow.xaml.cs src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SectionDrawingConfigDialogCommands.cs src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs tests/core_tests.cpp
git commit -m "feat: add section drawing config WPF window"
```

---

### Task 6: ObjectARX Commands, Bulk Apply, Ribbon, and Double Click

**Files:**
- Create: `src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.h`
- Create: `src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.cpp`
- Modify: `src/modules/cross_section/CrossSectionModule.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing command metadata and source-contract tests**

In the existing cross-section module test, add:

```cpp
    const auto sectionDrawingConfigCommand = commands.find(L"RD_SECTION_DRAWING_CONFIG");
    CHECK(sectionDrawingConfigCommand.has_value());
    if (sectionDrawingConfigCommand.has_value()) {
        CHECK(sectionDrawingConfigCommand->moduleCode == L"CROSS_SECTION");
        CHECK(sectionDrawingConfigCommand->displayName == L"横断面图配置");
        CHECK(sectionDrawingConfigCommand->businessDocPath == L"docs/business/cross_section/横断面图配置.md");
        CHECK(sectionDrawingConfigCommand->ribbonAttachable);
        CHECK(sectionDrawingConfigCommand->isPrototype);
        CHECK(sectionDrawingConfigCommand->reusable);
    }

    const auto sectionDrawingConfigEditCommand = commands.find(L"RD_SECTION_DRAWING_CONFIG_EDIT_HANDLE");
    CHECK(sectionDrawingConfigEditCommand.has_value());
    if (sectionDrawingConfigEditCommand.has_value()) {
        CHECK(sectionDrawingConfigEditCommand->moduleCode == L"CROSS_SECTION");
        CHECK(sectionDrawingConfigEditCommand->businessDocPath == L"docs/business/cross_section/横断面图配置.md");
        CHECK(!sectionDrawingConfigEditCommand->ribbonAttachable);
    }

    const auto sectionDrawingConfigApplyCommand = commands.find(L"RD_SECTION_DRAWING_CONFIG_APPLY_DIALOG_FILE");
    CHECK(sectionDrawingConfigApplyCommand.has_value());
    if (sectionDrawingConfigApplyCommand.has_value()) {
        CHECK(sectionDrawingConfigApplyCommand->moduleCode == L"CROSS_SECTION");
        CHECK(sectionDrawingConfigApplyCommand->businessDocPath == L"docs/business/cross_section/横断面图配置.md");
        CHECK(!sectionDrawingConfigApplyCommand->ribbonAttachable);
    }
```

Add source-contract test:

```cpp
void sectionDrawingConfigObjectArxCommandSourceContracts()
{
    const auto root = findRepositoryRootForTests();
    const auto header = readTextFileForTests(
        root / "src" / "cad_adapter" / "objectarx" / "cross_section" / "ObjectArxSectionDrawingConfigCommand.h");
    const auto source = readTextFileForTests(
        root / "src" / "cad_adapter" / "objectarx" / "cross_section" / "ObjectArxSectionDrawingConfigCommand.cpp");
    const auto ribbon = readTextFileForTests(
        root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI" / "AutoCad" / "RoadProtoRibbonExtension.cs");

    CHECK(header.find("sectionDrawingConfigCommandProcedure") != std::string::npos);
    CHECK(header.find("sectionDrawingConfigEditHandleCommandProcedure") != std::string::npos);
    CHECK(header.find("sectionDrawingConfigApplyDialogFileCommandProcedure") != std::string::npos);
    CHECK(source.find("collectSectionDrawingsForRoadModel") != std::string::npos);
    CHECK(source.find("collectComponentOptions") != std::string::npos);
    CHECK(source.find("promptPavementLayerTemplate") != std::string::npos);
    CHECK(source.find("applySectionDrawingConfigToAllDrawings") != std::string::npos);
    CHECK(source.find("manualEdited") != std::string::npos);
    CHECK(ribbon.find("SectionDrawingConfigButtonId") != std::string::npos);
    CHECK(ribbon.find("DNROADMODELSECTIONDRAWINGENTITY") != std::string::npos);
    CHECK(ribbon.find("RD_SECTION_DRAWING_CONFIG_EDIT_HANDLE") != std::string::npos);
}
```

Call it from `main()`:

```cpp
    sectionDrawingConfigObjectArxCommandSourceContracts();
```

- [ ] **Step 2: Run tests to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL because command files and metadata do not exist.

- [ ] **Step 3: Add command header**

Create `src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.h`:

```cpp
#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::cross_section {

core::CommandProcedure sectionDrawingConfigCommandProcedure();
core::CommandProcedure sectionDrawingConfigEditHandleCommandProcedure();
core::CommandProcedure sectionDrawingConfigApplyDialogFileCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::cross_section
```

- [ ] **Step 4: Add command implementation skeleton and flow**

Create `src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.cpp`. Use `#ifndef ROADPROTO_TEST_BUILD` around ObjectARX code like the existing command files. Include:

```cpp
#include "cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h"
#include "cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"

#include <algorithm>
#include <sstream>
#include <unordered_map>
#include <vector>
#endif
```

Implement these functions:

```cpp
// selectSectionDrawingEntity: implied selection first, then acedSSGet.
// collectSectionDrawingsForRoadModel: scan model space for DnRoadModelSectionDrawingEntity with same roadModelHandle.
// collectComponentOptions: read DnRoadModelEntity componentLines and create side/type options.
// promptPavementLayerTemplate: use acedEntSel and validate DnPavementLayerTemplateEntity.
// queueConfigDialogForDrawing: build SectionDrawingConfigDialogRequest from selected drawing.
// applySectionDrawingConfigToAllDrawings: write config to each drawing and rebuild non-manual faces.
```

For the first green pass, implement `applySectionDrawingConfigToAllDrawings` so it writes config and preserves current faces. Add the pavement face regeneration in Task 7 after face generation helpers are in place. This keeps command registration and bridge flow testable without changing geometry yet.

At the bottom:

```cpp
core::CommandProcedure sectionDrawingConfigCommandProcedure()
{
    return &runSectionDrawingConfigCommand;
}

core::CommandProcedure sectionDrawingConfigEditHandleCommandProcedure()
{
    return &runSectionDrawingConfigEditHandleCommand;
}

core::CommandProcedure sectionDrawingConfigApplyDialogFileCommandProcedure()
{
    return &runSectionDrawingConfigApplyDialogFileCommand;
}
```

- [ ] **Step 5: Register commands**

In `src/modules/cross_section/CrossSectionModule.cpp`, include:

```cpp
#include "cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.h"
```

Register:

```cpp
    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_DRAWING_CONFIG",
        L"横断面图配置",
        L"CROSS_SECTION",
        L"Configures pavement layers for model-space road model section drawings.",
        cad_adapter::objectarx::cross_section::sectionDrawingConfigCommandProcedure(),
        true,
        true,
        L"docs/business/cross_section/横断面图配置.md",
        true});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_DRAWING_CONFIG_EDIT_HANDLE",
        L"按 handle 编辑横断面图配置",
        L"CROSS_SECTION",
        L"Internal double-click bridge command that edits section drawing config by handle.",
        cad_adapter::objectarx::cross_section::sectionDrawingConfigEditHandleCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/横断面图配置.md",
        false});

    commandRegistry.registerCommand(core::CommandDefinition{
        L"RD_SECTION_DRAWING_CONFIG_APPLY_DIALOG_FILE",
        L"应用横断面图配置对话框结果",
        L"CROSS_SECTION",
        L"Internal WPF bridge command that applies section drawing config response files.",
        cad_adapter::objectarx::cross_section::sectionDrawingConfigApplyDialogFileCommandProcedure(),
        true,
        false,
        L"docs/business/cross_section/横断面图配置.md",
        false});
```

- [ ] **Step 6: Add command file to projects**

In `src/app/RoadProtoArx.vcxproj`, add:

```xml
    <ClCompile Include="..\cad_adapter\objectarx\cross_section\ObjectArxSectionDrawingConfigCommand.cpp" />
```

In `tests/RoadProtoCoreTests.vcxproj`, add:

```xml
    <ClCompile Include="..\src\cad_adapter\objectarx\cross_section\ObjectArxSectionDrawingConfigCommand.cpp" />
```

- [ ] **Step 7: Add Ribbon button and double-click**

In `RoadProtoRibbonExtension.cs`, add constants:

```csharp
private const string SectionDrawingConfigButtonId = "ROADPROTO_RD_SECTION_DRAWING_CONFIG";
private const string SectionDrawingDxfName = "DNROADMODELSECTIONDRAWINGENTITY";
```

Add Ribbon button in the cross-section panel:

```csharp
if (!crossSectionPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == SectionDrawingConfigButtonId))
{
    crossSectionPanel.Source.Items.Add(CreateCrossSectionCommandButton(
        SectionDrawingConfigButtonId,
        "横断面图配置",
        "选择横断面图并配置路面结构层",
        "RD_SECTION_DRAWING_CONFIG "));
}
```

In `OnBeginDoubleClick`, before template checks:

```csharp
if (TryFindEntityByDxfName(document, e.Location, SectionDrawingDxfName, out var sectionDrawingId))
{
    if (!SuppressDuplicateDoubleClick(sectionDrawingId))
    {
        QueueSectionDrawingConfigEditByHandle(document, sectionDrawingId);
    }
    return;
}
```

Add:

```csharp
private static void QueueSectionDrawingConfigEditByHandle(Document document, ObjectId entityId)
{
    var handle = entityId.Handle.ToString();
    if (string.IsNullOrWhiteSpace(handle))
    {
        return;
    }

    document.SendStringToExecute($"RD_SECTION_DRAWING_CONFIG_EDIT_HANDLE {handle}\n", true, false, true);
}
```

- [ ] **Step 8: Run tests and builds**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
```

Expected: tests pass and WPF builds.

- [ ] **Step 9: Commit**

```powershell
git add src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.h src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.cpp src/modules/cross_section/CrossSectionModule.cpp src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs src/app/RoadProtoArx.vcxproj tests/RoadProtoCoreTests.vcxproj tests/core_tests.cpp
git commit -m "feat: register section drawing config command"
```

---

### Task 7: Generate Configured Pavement Faces and Preserve Manual Edits

**Files:**
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing source-contract test**

Add:

```cpp
void sectionDrawingConfigFaceGenerationContracts()
{
    const auto root = findRepositoryRootForTests();
    const auto source = readTextFileForTests(
        root / "src" / "cad_adapter" / "objectarx" / "cross_section" / "ObjectArxSectionDrawingConfigCommand.cpp");

    CHECK(source.find("buildConfiguredPavementFaces") != std::string::npos);
    CHECK(source.find("PavementLayerTemplateRules::buildSection") != std::string::npos);
    CHECK(source.find("SectionDrawingConfigRules::resolvePavementRow") != std::string::npos);
    CHECK(source.find("SectionDrawingConfigRules::matchesComponent") != std::string::npos);
    CHECK(source.find("preserveManualEditedFaces") != std::string::npos);
    CHECK(source.find("sourceConfigRowIndex") != std::string::npos);
    CHECK(source.find("sourceTemplateHandle") != std::string::npos);
}
```

Call it from `main()`:

```cpp
    sectionDrawingConfigFaceGenerationContracts();
```

- [ ] **Step 2: Run tests to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL because face generation helpers are missing.

- [ ] **Step 3: Implement face generation helpers**

In `ObjectArxSectionDrawingConfigCommand.cpp`, add:

```cpp
std::vector<RoadModelSectionDrawingFace> preserveManualEditedFaces(
    const std::vector<RoadModelSectionDrawingFace>& faces)
{
    std::vector<RoadModelSectionDrawingFace> preserved;
    for (const auto& face : faces) {
        if (face.manualEdited) {
            preserved.push_back(face);
        }
    }
    return preserved;
}
```

Add `buildConfiguredPavementFaces` with these inputs:

```cpp
std::vector<RoadModelSectionDrawingFace> buildConfiguredPavementFaces(
    const RoadModelSectionDrawingData& drawing,
    const RoadModelData& roadModel,
    const std::unordered_map<std::wstring, PavementLayerTemplateData>& pavementTemplates,
    std::vector<std::wstring>& warnings)
```

Implementation rules:

- Start with `preserveManualEditedFaces(drawing.faces)`.
- Resolve config row by `SectionDrawingConfigRules::resolvePavementRow(drawing.config, drawing.station)`.
- If no row, return preserved faces.
- Find the pavement template by `row.templateHandle`; if missing, push a warning and return preserved faces.
- Find the `RoadModelSection` matching `drawing.station`.
- For each subgrade component span on left and right, match with `SectionDrawingConfigRules::matchesComponent`.
- Use `PavementLayerTemplateRules::buildSection(templateData, componentWidth, side, topInnerElevation, topOuterElevation)` to create layer polygons.
- Convert generated offset/elevation points into drawing-local x/y using the same min offset/elevation basis used by the existing section drawing. If the original drawing does not store that basis, derive it from current `lines` and `faces` extents and keep all new face points inside the existing frame.
- Set `face.layerName`, `face.componentName`, RGB, hatch fields, `sourceTemplateHandle`, `sourceConfigRowIndex`, and `manualEdited=false`.

- [ ] **Step 4: Wire generation into apply**

In `applySectionDrawingConfigToAllDrawings`, replace the first-pass “config only” behavior:

```cpp
entity->setSectionDrawingConfig(response.config);
entity->replaceFaces(buildConfiguredPavementFaces(entity->drawingData(), roadModelData, pavementTemplates, warnings));
```

After applying all drawings, write a command-line message containing updated count and skipped manual-edited count.

- [ ] **Step 5: Run tests**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: tests pass.

- [ ] **Step 6: Commit**

```powershell
git add src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.cpp tests/core_tests.cpp
git commit -m "feat: draw section configured pavement layers"
```

---

### Task 8: Make Quantity Command Prefer Edited Drawing Faces

**Files:**
- Modify: `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing source-contract test**

Add:

```cpp
void pavementQuantityCommandPrefersDrawingFacesContract()
{
    const auto root = findRepositoryRootForTests();
    const auto source = readTextFileForTests(
        root / "src" / "cad_adapter" / "objectarx" / "drawing_quantity" / "ObjectArxPavementQuantityTableCommand.cpp");

    const auto drawingFacePosition = source.find("PavementQuantityDrawingFaceSampler::sampleAtStation");
    const auto roadModelPosition = source.find("sampleFromRoadModelSection");
    CHECK(drawingFacePosition != std::string::npos);
    CHECK(roadModelPosition != std::string::npos);
    CHECK(drawingFacePosition < roadModelPosition);
    CHECK(source.find("drawingFacesFromSectionDrawing") != std::string::npos);
}
```

Call it from `main()`:

```cpp
    pavementQuantityCommandPrefersDrawingFacesContract();
```

- [ ] **Step 2: Run tests to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL because quantity command still tries road-model sampling first.

- [ ] **Step 3: Include new sampler**

In `ObjectArxPavementQuantityTableCommand.cpp`, add:

```cpp
#include "domain/quantity/PavementQuantityDrawingFaceSampler.h"
```

Add using:

```cpp
using roadproto::domain::quantity::PavementQuantityDrawingFace;
using roadproto::domain::quantity::PavementQuantityDrawingFaceSampler;
using roadproto::domain::quantity::PavementQuantityDrawingPoint;
```

- [ ] **Step 4: Replace local face sampling with domain sampler**

Add mapper:

```cpp
std::vector<PavementQuantityDrawingFace> drawingFacesFromSectionDrawing(
    const RoadModelSectionDrawingData& drawingData)
{
    std::vector<PavementQuantityDrawingFace> faces;
    faces.reserve(drawingData.faces.size());
    for (const auto& face : drawingData.faces) {
        PavementQuantityDrawingFace mapped;
        mapped.layerName = face.layerName;
        mapped.componentName = face.componentName;
        mapped.points.reserve(face.points.size());
        for (const auto& point : face.points) {
            mapped.points.push_back(PavementQuantityDrawingPoint{point.x, point.y});
        }
        faces.push_back(std::move(mapped));
    }
    return faces;
}
```

Change the sample loop to:

```cpp
    for (const auto& drawing : drawings) {
        auto sample = PavementQuantityDrawingFaceSampler::sampleAtStation(
            drawing.station,
            drawingFacesFromSectionDrawing(drawing));
        if (!sample.has_value() && hasRoadModelData) {
            sample = sampleFromRoadModelSection(drawing.station, roadModelData);
        }
        if (sample.has_value()) {
            samples.push_back(*sample);
        }
    }
```

Remove or leave unused local `sampleFromDrawingFaces`; if leaving it causes warnings, delete it.

- [ ] **Step 5: Run tests and Debug build**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" RoadProto.sln /p:Configuration=Debug /p:Platform=x64
```

Expected: tests pass and Debug solution builds.

- [ ] **Step 6: Commit**

```powershell
git add src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.cpp tests/core_tests.cpp
git commit -m "fix: calculate pavement quantities from edited drawings"
```

---

### Task 9: Business Docs, Module Docs, Version, and Test Notes

**Files:**
- Create: `docs/business/cross_section/横断面图配置.md`
- Modify: `docs/business/cross_section/查看横断面.md`
- Modify: `docs/business/drawing_quantity/路面工程量统计表.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/modules/module_index.md`
- Modify: `README.md`
- Modify: `tests/README.md`
- Modify: `docs/dev/version_log.md`
- Modify: `build/RoadProto.Build.props`
- Modify: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing documentation contract test**

Add:

```cpp
void sectionDrawingConfigDocumentationContracts()
{
    const auto root = findRepositoryRootForTests();
    const auto businessDoc = readTextFileForTests(root / "docs" / "business" / "cross_section" / "横断面图配置.md");
    const auto crossSectionModule = readTextFileForTests(root / "docs" / "modules" / "cross_section.md");
    const auto moduleIndex = readTextFileForTests(root / "docs" / "modules" / "module_index.md");
    const auto readme = readTextFileForTests(root / "README.md");
    const auto testsReadme = readTextFileForTests(root / "tests" / "README.md");
    const auto versionLog = readTextFileForTests(root / "docs" / "dev" / "version_log.md");
    const auto buildProps = readTextFileForTests(root / "build" / "RoadProto.Build.props");
    const auto quantityDoc = readTextFileForTests(root / "docs" / "business" / "drawing_quantity" / "路面工程量统计表.md");

    CHECK(businessDoc.find("RD_SECTION_DRAWING_CONFIG") != std::string::npos);
    CHECK(businessDoc.find("CSV") != std::string::npos);
    CHECK(businessDoc.find("manualEdited") != std::string::npos);
    CHECK(crossSectionModule.find("横断面图配置") != std::string::npos);
    CHECK(moduleIndex.find("横断面图配置") != std::string::npos);
    CHECK(readme.find("RD_SECTION_DRAWING_CONFIG") != std::string::npos);
    CHECK(testsReadme.find("横断面图配置") != std::string::npos);
    CHECK(versionLog.find("SectionDrawingConfig") != std::string::npos);
    CHECK(buildProps.find("<RoadProtoStage>SectionDrawingConfig</RoadProtoStage>") != std::string::npos);
    CHECK(quantityDoc.find("横断面图实体当前面域") != std::string::npos);
}
```

Call it from `main()`:

```cpp
    sectionDrawingConfigDocumentationContracts();
```

- [ ] **Step 2: Run tests to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL because docs/version have not been updated.

- [ ] **Step 3: Add business document**

Create `docs/business/cross_section/横断面图配置.md` using the project template. Include:

```markdown
# 横断面图配置

## 基本信息

- 功能名称：横断面图配置
- 所属模块：横断面设计
- 命令名称：`RD_SECTION_DRAWING_CONFIG`
- 内部命令：`RD_SECTION_DRAWING_CONFIG_EDIT_HANDLE`、`RD_SECTION_DRAWING_CONFIG_APPLY_DIALOG_FILE`
- 对应代码入口：`src/cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.cpp`
- 业务文档维护人：RoadProto 项目
- 原型版本：`v0.1.31`
- 是否可复用：部分可复用

## 功能背景

横断面图配置面向 `查看横断面` 已绘制出的 `DnRoadModelSectionDrawingEntity` 成果图。它用于在成果图阶段按桩号范围和路基部件类型配置路面结构层，并允许用户在图面拖动结构层顶点修正面积。

## 业务目标

- 选择一张横断面图后打开 WPF 配置窗口。
- 使用 CSV 导入、导出配置。
- 在 `路面结构层` tab 中维护起点桩号、终点桩号、路基类型和路面结构层模板。
- 点击 `绘制` 后更新同一道路模型下所有已绘制横断面图。
- 双击横断面图可重新打开配置窗口。
- 结构层面域支持顶点夹点编辑，编辑后 face 标记 `manualEdited=true`。
- 路面工程量统计表按横断面图实体当前面域计算。

## 关键业务规则

- 表格上方行优先级高。
- 桩号范围为闭区间。
- 路基类型按侧别和部件类型去重，多选后可同时命中多个部件。
- CSV 列为 `起点桩号,终点桩号,路基类型,模板Handle,模板名称`。
- CSV 只保存配置，不保存手动拖动后的结构层几何。
- `绘制` 默认保留 `manualEdited=true` 的面域。
```

- [ ] **Step 4: Update docs and version metadata**

Update:

- `build/RoadProto.Build.props`: set `RoadProtoVersion` to `v0.1.31`, `RoadProtoBuildDate` to `20260527`, and `RoadProtoStage` to `SectionDrawingConfig`.
- `README.md`: add `RD_SECTION_DRAWING_CONFIG` to command list and current version summary.
- `docs/modules/cross_section.md`: add command row, Ribbon entry, code落点, and update status.
- `docs/modules/module_index.md`: mention 横断面图配置 and editable section drawing faces.
- `docs/business/cross_section/查看横断面.md`: state generated section drawings can be configured and edited.
- `docs/business/drawing_quantity/路面工程量统计表.md`: state statistics prefer 横断面图实体当前面域.
- `tests/README.md`: add automated and manual validation scope.
- `docs/dev/version_log.md`: add entry containing `SectionDrawingConfig`.

- [ ] **Step 5: Run tests**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: tests pass.

- [ ] **Step 6: Commit**

```powershell
git add docs/business/cross_section/横断面图配置.md docs/business/cross_section/查看横断面.md docs/business/drawing_quantity/路面工程量统计表.md docs/modules/cross_section.md docs/modules/module_index.md README.md tests/README.md docs/dev/version_log.md build/RoadProto.Build.props tests/core_tests.cpp
git commit -m "docs: document section drawing config"
```

---

### Task 10: Full Verification and Main Directory Sync

**Files:**
- Build outputs under `artifacts/`
- No source edits unless verification finds a defect.

- [ ] **Step 1: Run full core tests**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Release /p:Platform=x64
artifacts\x64\Release\RoadProtoCoreTests.exe
```

Expected: both Debug and Release tests pass.

- [ ] **Step 2: Build WPF plugin**

Run:

```powershell
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
```

Expected: both builds succeed.

- [ ] **Step 3: Build full solution**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" RoadProto.sln /p:Configuration=Debug /p:Platform=x64
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

Expected: Debug and Release solution builds succeed.

- [ ] **Step 4: Manual AutoCAD verification**

Load:

```text
ARXLOAD artifacts\x64\Release\<current RoadProto ARX name>.arx
NETLOAD artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll
```

Verify:

- `横断面设计 / 横断面图配置` button exists.
- Command selects `DnRoadModelSectionDrawingEntity`.
- WPF window opens with path, import, export, `路面结构层` tab, grid, draw and cancel buttons.
- Import/export CSV opens in Excel with the expected five columns.
- Template point-pick fills handle and name.
- Draw updates all section drawings for the same road model.
- Double-clicking section drawing reopens the same window.
- Dragging pavement layer face vertices changes shape and persists after save/reopen.
- Pavement quantity table changes after dragging a face vertex.

- [ ] **Step 5: Check process cleanup after AutoCAD testing**

Run:

```powershell
Get-Process acad,accoreconsole,taskhostw,Codex,devenv -ErrorAction SilentlyContinue |
    Select-Object ProcessName,Id,WorkingSet64,PrivateMemorySize64,StartTime
```

Expected: no unexpected background `acad.exe` or `accoreconsole.exe` remains after GUI testing.

- [ ] **Step 6: If implementing inside `.worktrees`, sync source and artifacts to main project directory**

If current path is under `.worktrees/<branch>`, copy the required source/doc paths back to `F:\0_GPT_道路设计原型功能项目` and copy:

```text
artifacts/x64/Debug/
artifacts/x64/Release/
artifacts/managed/Debug/net48/
artifacts/managed/Release/net48/
```

Expected: main project directory has matching source/docs and latest loadable ARX/DLL/PDB artifacts.

- [ ] **Step 7: Final commit**

If verification required fixes, commit them:

```powershell
git add .
git commit -m "test: verify section drawing config"
```

If no fixes were needed, do not create an empty commit.
