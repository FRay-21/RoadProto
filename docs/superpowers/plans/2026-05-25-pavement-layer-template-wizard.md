# Pavement Layer Template Wizard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the pavement layer create wizard, two new layer types, and document-based initial parameter presets without changing double-click edit behavior.

**Architecture:** Keep native ObjectARX responsible for entity creation and request queuing. Add a `ShowCreateWizard` request flag so the managed command can optionally run a WPF wizard before opening the existing editor window. Keep all document presets in a managed preset factory used by the wizard, while native and XML layers learn the two new layer type codes for persistence.

**Tech Stack:** C++17/ObjectARX bridge, WPF .NET Framework 4.8, managed bridge DTO/file tests, C++ core source-contract tests.

---

### Task 1: Failing Contract Tests

**Files:**
- Modify: `tests/RoadProtoManagedBridgeTests/Program.cs`
- Modify: `tests/core_tests.cpp`

- [ ] Add managed bridge tests proving request files read `showCreateWizard`, enum values include `AsphaltSeal` and `ApproachSlab`, the preset factory returns the DOCX mainline and bridge-transition defaults, and the WPF command/window source contains the wizard flow.
- [ ] Add core tests proving the native layer type code/display-name functions support `AsphaltSeal` and `ApproachSlab`, and the create command sets `showCreateWizard = true` only on the create path.
- [ ] Run `dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug`; expected result before implementation: compile/test failure.

### Task 2: Layer Types And Bridge Flag

**Files:**
- Modify: `src/domain/cross_section/PavementLayerTemplateModel.h`
- Modify: `src/domain/cross_section/PavementLayerTemplateModel.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.h`
- Modify: `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxPavementLayerTemplateCommand.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogDtos.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogFile.cs`

- [ ] Implement `AsphaltSeal` and `ApproachSlab` in native and managed enums, labels, codes, defaults, and bridge parsing.
- [ ] Add `ShowCreateWizard` to the request DTO and native request file writer.
- [ ] Set the flag only in `runPavementLayerTemplateCreateCommand()`.
- [ ] Re-run managed bridge tests; expected result: remaining failures only around missing preset factory/window.

### Task 3: Preset Factory

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplatePresetFactory.cs`
- Modify: `tests/RoadProtoManagedBridgeTests/RoadProtoManagedBridgeTests.csproj`

- [ ] Add `PavementLayerTemplatePresetFactory` with pavement type and road segment option labels.
- [ ] Encode DOCX presets for all ten road segment types, omitting blank/zero layers and applying width, structure code, pavement type, display mode, RGB, hatch, thickness, widening, and slope defaults.
- [ ] Link the new factory file into managed bridge tests.
- [ ] Re-run managed bridge tests; expected result: remaining failures only around missing wizard window/flow.

### Task 4: Wizard Window And Command Flow

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateCreateWizardWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateCreateWizardWindow.xaml.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/PavementLayerTemplateDialogCommands.cs`

- [ ] Build the wizard UI with pavement type, road segment type, and editable name/thickness/widening/slope rows.
- [ ] On OK, return a preset template and then open the existing `PavementLayerTemplateWindow`.
- [ ] On wizard cancel, write a cancelled response and avoid opening the existing editor.
- [ ] Ensure double-click/edit requests skip the wizard because `ShowCreateWizard` is false.
- [ ] Run managed bridge tests and WPF build.

### Task 5: Verification

**Files:**
- All changed files above.

- [ ] Run `dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug`.
- [ ] Run `dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Debug`.
- [ ] If MSBuild is available, build and run `tests\RoadProtoCoreTests.vcxproj` Debug.
- [ ] Inspect `git diff --stat` and summarize only the files changed for this request.
