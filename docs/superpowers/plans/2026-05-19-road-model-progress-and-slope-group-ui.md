# Road Model Progress And Slope Group UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add visible progress feedback while generating road models and make slope template group management obvious in the cross-section road model UI.

**Architecture:** Domain road model building exposes a lightweight progress callback that reports percent and status text without depending on ObjectARX. The ObjectARX command adapts that callback to AutoCAD's status bar progress meter. WPF keeps template group management as UI-only selection and response-file state management.

**Tech Stack:** C++17 domain/application/ObjectARX, WPF .NET Framework 4.8, existing key-value Bridge files, existing core and managed bridge tests.

---

### Task 1: Progress Callback Contract

**Files:**
- Modify: `src/domain/cross_section/RoadModel.h`
- Modify: `src/domain/cross_section/RoadModel.cpp`
- Test: `tests/core_tests.cpp`

- [ ] Add `RoadModelProgressCallback` to `RoadModelBuildInput`.
- [ ] Write a core test that builds a road model and verifies progress callbacks are emitted and end at 100.
- [ ] Run the core test to verify it fails before implementation.
- [ ] Emit progress during validation, sampling, per-station build, and finalization.
- [ ] Run the core test to verify it passes.

### Task 2: ObjectARX Status Bar Progress

**Files:**
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- Test: `tests/core_tests.cpp`

- [ ] Add a source-contract test for `StatusProgressMeter` and callback wiring in the road model command.
- [ ] Run the core test to verify it fails before implementation.
- [ ] Add a small RAII `StatusProgressMeter` in the road model command and pass a callback to `RoadModelBuildInput`.
- [ ] Run the core test and ARX build.

### Task 3: Slope Group Management UI

**Files:**
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelWindow.xaml`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelWindow.xaml.cs`
- Modify: `docs/prototypes/slope_template_ui_prototype.html`
- Test: `tests/core_tests.cpp`
- Test: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] Add tests that require visible `管理模板组` entry points in WPF.
- [ ] Run tests to verify they fail before implementation.
- [ ] Add left/right DataGrid management buttons and handlers that select the row and focus the group management controls.
- [ ] Keep existing add/delete/move/pick/clear behavior.
- [ ] Update the HTML prototype to show progress during model generation.
- [ ] Run tests and WPF build.

### Task 4: Docs And Verification

**Files:**
- Modify: `docs/business/cross_section/横断面戴帽_边坡模板.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`

- [ ] Document progress feedback and template group management behavior.
- [ ] Run core tests, managed bridge tests, WPF Debug/Release builds, and ARX Debug/Release builds.
- [ ] Report the generated ARX and DLL paths.
