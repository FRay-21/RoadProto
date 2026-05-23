# Pavement Layer Template General Parameters Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add hidden-by-default general pavement template data fields that persist through WPF, Bridge, XML, and DWG without changing preview or road-model display.

**Architecture:** Extend the domain template properties as the source of truth, then map those fields through native Bridge, managed DTO/file/XML layers, and WPF controls. ObjectARX entity persistence gets a version bump with backward-compatible defaults.

**Tech Stack:** C++17 domain/ObjectARX adapter, WPF .NET Framework 4.8, UTF-8 key-value Bridge files, LINQ XML, MSBuild, RoadProto core and managed bridge tests.

---

### Task 1: Add Failing Contract Tests

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] **Step 1: Write the failing core tests**

Add assertions that `PavementLayerTemplateProperties` exposes the new fields, that defaults keep advanced general parameters hidden, that option code/display conversions exist for moisture, pavement type, and soil group, and that `DnPavementLayerTemplateEntity.cpp` uses the next entity version and writes the new property fields.

- [ ] **Step 2: Write the failing managed tests**

Add managed bridge assertions that request/response files and `.rpavement.xml` round-trip `showAllGeneralParameters`, `structureCode`, moisture types, pavement type, soil groups, design deflection, and cumulative axle loads. Add source-contract checks for the WPF checkbox and advanced parameter controls.

- [ ] **Step 3: Run tests to verify RED**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
```

Expected: compilation or test failures because fields and controls do not exist yet.

### Task 2: Extend Domain and Native Persistence

**Files:**
- Modify: `src/domain/cross_section/PavementLayerTemplateModel.h`
- Modify: `src/domain/cross_section/PavementLayerTemplateModel.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.cpp`

- [ ] **Step 1: Add enums and properties**

Add `PavementSubgradeMoistureType`, `PavementSurfaceType`, and `PavementSubgradeSoilGroup` enums plus vectors/text fields to `PavementLayerTemplateProperties`.

- [ ] **Step 2: Add code/display helpers**

Implement stable code helpers and default/normalize behavior. Unknown list codes are ignored by WPF managed parsing, while C++ normalization preserves only supported enum values.

- [ ] **Step 3: Wire native Bridge**

Write and read new top-level keys in request/response files. Use semicolon-separated enum code lists for multi-select values.

- [ ] **Step 4: Wire DWG persistence**

Bump `DnPavementLayerTemplateEntity` version and read/write the new property fields after display mode so old drawings still load with defaults.

- [ ] **Step 5: Run core tests to verify GREEN for native/domain**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: core tests pass after implementation.

### Task 3: Extend Managed DTO, XML, and WPF UI

**Files:**
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogDtos.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogFile.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateXmlFile.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml.cs`

- [ ] **Step 1: Add managed enums and DTO fields**

Add managed equivalents for moisture type, pavement type, and soil group. Add list/text/show fields to template DTOs and cloning.

- [ ] **Step 2: Wire request/response and XML**

Read/write top-level fields in Bridge files and `Properties` attributes in XML. Use invariant culture and existing escaping helpers.

- [ ] **Step 3: Add WPF controls**

Add `ShowAllGeneralParametersBox` and an `AdvancedGeneralParametersPanel`. Keep existing four common fields always visible, and bind visibility to the checkbox. Use checkboxes for multi-select values and a ComboBox for pavement type.

- [ ] **Step 4: Update BuildTemplateDto and LoadTemplate**

Populate controls from requests/XML and write values back without touching preview geometry or DWG drawing behavior.

- [ ] **Step 5: Run managed tests to verify GREEN**

Run:

```powershell
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
```

Expected: managed bridge tests pass.

### Task 4: Update Docs, Version, Build, and Sync

**Files:**
- Modify: `build/RoadProto.Build.props`
- Modify: `README.md`
- Modify: `docs/business/cross_section/路面结构层模板_创建.md`
- Modify: `docs/business/cross_section/路面结构层模板_编辑.md`
- Modify: `docs/business/cross_section/路面结构层模板_WPF桥接回写.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/modules/module_index.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/reuse/pavement_layer_template.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`

- [ ] **Step 1: Bump version metadata**

Set version to `v0.1.23`, date `20260523`, and stage `PavementLayerTemplateGeneralParams`.

- [ ] **Step 2: Update docs**

Document the new data-only general parameters, default-hidden checkbox, and persistence scope.

- [ ] **Step 3: Run full verification**

Run:

```powershell
& "D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" RoadProto.sln /p:Configuration=Release /p:Platform=x64 /m
artifacts\x64\Release\RoadProtoCoreTests.exe
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
```

Expected: Release solution builds with 0 errors, core tests pass, managed bridge tests pass.

- [ ] **Step 4: Commit, push, and sync main directory**

Commit on `codex/pavement-layer-template-annotation`, push to origin, copy changed source/docs/tests and Release ARX/DLL/PDB files back to `F:\0_GPT_道路设计原型功能项目`, and verify synced file hashes.
