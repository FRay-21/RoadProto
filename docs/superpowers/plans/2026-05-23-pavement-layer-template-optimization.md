# 路面结构层模板显示与编辑优化实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 优化路面结构层模板的标注、填充显示、颜色选择和当前层编辑体验，并保持道路模型结构层仍按现有颜色显示。

**Architecture:** 领域层新增显示方式和填充字段，桥接/XML/DWG 持久化同步读写；WPF 只负责当前层交互、索引色选择和预览绘制；`DnPavementLayerTemplateEntity` 自绘模板显示，不生成额外可选中实体。

**Tech Stack:** C++17, ObjectARX 2021, WPF .NET Framework 4.8, `RoadProtoCoreTests`, `RoadProtoManagedBridgeTests`.

---

## File Structure

- `src/domain/cross_section/PavementLayerTemplateModel.h/.cpp`: 新增 `PavementLayerTemplateDisplayMode`、填充名字段、默认值、编码转换和归一化。
- `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.cpp`: 请求/响应文件新增 `displayMode` 和 `layer.N.hatchPattern`。
- `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.cpp`: DWG 版本升级，读写新增字段；标注、加宽尺寸线、坡度标注和填充显示分支。
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogDtos.cs`: 新增显示方式枚举、填充列表和 DTO 字段。
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogFile.cs`: 托管请求/响应字段读写兼容旧数据。
- `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateXmlFile.cs`: XML 新增属性并兼容缺省值。
- `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml/.cs`: 右侧改为当前层编辑、预览点击选层、索引色对话框、hatch 预览、工程化标注。
- `tests/core_tests.cpp`: 核心领域和 ObjectARX 源码契约测试。
- `tests/RoadProtoManagedBridgeTests/Program.cs`: 托管 bridge、XML 和 WPF 源码契约测试。
- 文档和版本：README、业务文档、模块文档、复用说明、版本记录、测试说明、`build/RoadProto.Build.props`。

---

### Task 1: 领域模型和桥接字段

**Files:**
- Modify: `src/domain/cross_section/PavementLayerTemplateModel.h`
- Modify: `src/domain/cross_section/PavementLayerTemplateModel.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.cpp`
- Test: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing core tests**

Add tests that assert:

```cpp
CHECK(PavementLayerTemplateRules::displayModeCode(PavementLayerTemplateDisplayMode::Color) == std::wstring(L"Color"));
CHECK(PavementLayerTemplateRules::displayModeFromCode(L"Hatch") == PavementLayerTemplateDisplayMode::Hatch);
CHECK(PavementLayerTemplateRules::isSupportedHatchPattern(L"ANSI31"));
CHECK(!PavementLayerTemplateRules::isSupportedHatchPattern(L"NOT_A_PATTERN"));

auto data = PavementLayerTemplateDefaults::create();
CHECK(data.properties.displayMode == PavementLayerTemplateDisplayMode::Color);
CHECK(data.layers.front().hatchPattern == L"SOLID");
data.properties.displayMode = PavementLayerTemplateDisplayMode::HatchAndColor;
data.layers.front().hatchPattern = L"ANSI31";
CHECK(PavementLayerTemplateRules::normalize(data, errorMessage));
CHECK(data.properties.displayMode == PavementLayerTemplateDisplayMode::HatchAndColor);
CHECK(data.layers.front().hatchPattern == L"ANSI31");
```

Add source-contract checks that `PavementLayerTemplateDialogBridge.cpp` writes and reads `displayMode` and `hatchPattern`.

- [ ] **Step 2: Verify RED**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
```

Expected: build fails on missing display mode and hatch APIs.

- [ ] **Step 3: Implement domain additions**

Add:

```cpp
enum class PavementLayerTemplateDisplayMode {
    Color,
    Hatch,
    HatchAndColor
};

PavementLayerTemplateDisplayMode displayMode = PavementLayerTemplateDisplayMode::Color;
std::wstring hatchPattern = L"SOLID";
```

Add code conversion helpers and normalize unsupported hatch values to `SOLID`.

- [ ] **Step 4: Implement native bridge additions**

Write request key `displayMode` and each `layer.N.hatchPattern`. Read response `displayMode` with default `Color` and each hatch with default `SOLID`.

- [ ] **Step 5: Verify GREEN**

Run core build and test executable.

---

### Task 2: 托管 DTO、XML 和文件桥接

**Files:**
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogDtos.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogFile.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateXmlFile.cs`
- Test: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] **Step 1: Write failing managed tests**

Extend existing pavement template tests to require:

```csharp
Check(request.DisplayMode == PavementLayerTemplateDisplayMode.HatchAndColor, "display mode should parse");
Check(request.Layers[0].HatchPattern == "ANSI31", "hatch pattern should parse");
Check(content.Contains("displayMode=HatchAndColor"), "response should write display mode");
Check(content.Contains("layer.0.hatchPattern=ANSI31"), "response should write hatch pattern");
Check(roundTrip.DisplayMode == PavementLayerTemplateDisplayMode.HatchAndColor, "XML should round-trip display mode");
Check(roundTrip.Layers[0].HatchPattern == "ANSI31", "XML should round-trip hatch pattern");
```

- [ ] **Step 2: Verify RED**

Run:

```powershell
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
```

Expected: build fails on missing DTO fields and enum.

- [ ] **Step 3: Implement DTO and file bridge**

Add managed display mode enum, `DisplayMode` property, `HatchPattern` property, `HatchPatternOptions`, and default/clone support.

- [ ] **Step 4: Implement XML support**

Write `displayMode` on `Properties`, write each layer `hatchPattern`, and default missing values to `Color` / `SOLID`.

- [ ] **Step 5: Verify GREEN**

Run managed tests.

---

### Task 3: WPF 当前层编辑、索引色和预览标注

**Files:**
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml.cs`
- Test: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] **Step 1: Write failing WPF source-contract tests**

Require the window source to contain:

- `CurrentLayerBox`
- `PreviousLayerButton`
- `NextLayerButton`
- `DisplayModeBox`
- `HatchPatternBox`
- `ColorIndexDialog`
- `PreviewCanvas_MouseLeftButtonDown`
- `DrawWideningDimension`
- `FormatSlopeLabel`
- `DrawHatchPattern`

- [ ] **Step 2: Verify RED**

Run managed tests; expected contract failures.

- [ ] **Step 3: Update XAML**

Replace the all-layer `LayersPanel` with a current-layer editor panel:

- common grid: preview width, structure layer count, display mode
- layer navigator: current layer textbox + up/down icon buttons
- current layer fields: type, name, RGB + clickable color preview, hatch pattern, thickness/widening/slope fields

- [ ] **Step 4: Update code-behind**

Maintain `_selectedLayerIndex`, bind controls from `_layers[_selectedLayerIndex]`, handle layer count changes, input layer number changes, previous/next buttons, and preview left-click hit testing.

- [ ] **Step 5: Implement preview drawing**

Draw layer fill by display mode, draw hatch lines for non-`SOLID` patterns, draw selected layer highlight, draw smaller model-scaled labels, layer name + thickness on one line, CAD-style widening dimensions, and slope text `1:n`.

- [ ] **Step 6: Implement color index dialog**

Use a lightweight WPF modal window built in code with common ACI colors; selected color updates RGB boxes.

- [ ] **Step 7: Verify GREEN**

Run managed tests and WPF Debug build.

---

### Task 4: DWG 实体显示和持久化

**Files:**
- Modify: `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.cpp`
- Test: `tests/core_tests.cpp`

- [ ] **Step 1: Write failing source-contract tests**

Require `DnPavementLayerTemplateEntity.cpp` to contain:

- `kEntityVersion = 3`
- `displayMode`
- `hatchPattern`
- `drawLayerHatch`
- `drawWideningDimension`
- `formatSlopeLabel`
- layer label text built as layer name + thickness in one text call

- [ ] **Step 2: Verify RED**

Run core build/tests; expected contract failures.

- [ ] **Step 3: Update DWG read/write**

Bump entity version to 3. Read `displayMode` and per-layer `hatchPattern` only for version >= 3; default old data through `PavementLayerTemplateRules::normalize`.

- [ ] **Step 4: Update drawing**

Branch fill/hatch by display mode; add self-drawn hatch lines; replace text widening labels with dimension lines; replace slope text with `1:n`; shrink text heights.

- [ ] **Step 5: Verify GREEN**

Run core build and tests.

---

### Task 5: 文档、版本和最终验证

**Files:**
- Modify: `build/RoadProto.Build.props`
- Modify: `README.md`
- Modify: `docs/business/cross_section/路面结构层模板_创建.md`
- Modify: `docs/business/cross_section/路面结构层模板_编辑.md`
- Modify: `docs/business/cross_section/路面结构层模板_WPF桥接回写.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/reuse/pavement_layer_template.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`

- [ ] **Step 1: Update docs and version**

Use version `v0.1.21`, build date `20260523`, stage `PavementLayerTemplateAnnotation`.

- [ ] **Step 2: Run verification**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

Expected: all tests and Release builds pass, producing:

- `artifacts\x64\Release\RoadProto_v0.1.21_20260523_PavementLayerTemplateAnnotation.arx`
- `artifacts\managed\Release\net48\RoadProto.Terrain.UI.dll`

