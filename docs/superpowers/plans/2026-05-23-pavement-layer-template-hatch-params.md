# Pavement Layer Template Hatch Params Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Update pavement layer templates so DWG template entities show only a centered template title while WPF preview uses fixed-size labels and richer hatch parameters.

**Architecture:** Keep pavement layer data and validation in `domain/cross_section`, pass fields through the file/XML bridge, render user preview in WPF, and keep ObjectARX responsible only for DWG persistence and entity display. Road model structure layer display remains RGB color based.

**Tech Stack:** C++17/ObjectARX 2021, WPF .NET Framework 4.8, source-contract tests in `tests/core_tests.cpp`, managed bridge tests in `tests/RoadProtoManagedBridgeTests`.

---

### Task 1: Red Tests

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] **Step 1: Add failing domain and source-contract tests**

Check that hatch defaults include AutoCAD-style names, every layer carries `hatchAngle` and `hatchScale`, DWG entity version is bumped to `4`, and DWG custom entity source contains template-title centering helpers instead of per-layer dimension label rendering.

- [ ] **Step 2: Add failing managed bridge tests**

Check request/response/XML round-trip for `layer.N.hatchAngle` and `layer.N.hatchScale`, verify WPF exposes `HatchAngleBox` and `HatchScaleBox`, and verify the layer label font is fixed rather than derived from layer thickness.

- [ ] **Step 3: Run focused tests to verify RED**

Run:
`dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug`

Expected: failure mentioning missing hatch angle/scale or fixed label contract.

### Task 2: Data And Bridge

**Files:**
- Modify: `src/domain/cross_section/PavementLayerTemplateModel.h`
- Modify: `src/domain/cross_section/PavementLayerTemplateModel.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogDtos.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateDialogFile.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/PavementLayerTemplateXmlFile.cs`

- [ ] **Step 1: Add `hatchAngle` and `hatchScale` to the model**

Default angle is `0.0`, default scale is `1.0`; invalid scale normalizes to `1.0`.

- [ ] **Step 2: Pass the fields through request/response and XML**

Write/read `layer.N.hatchAngle`, `layer.N.hatchScale`, and XML attributes `hatchAngle`, `hatchScale`.

- [ ] **Step 3: Persist in DWG**

Bump `DnPavementLayerTemplateEntity` entity version to `4`, keep backwards compatibility for v3 by defaulting missing angle/scale.

### Task 3: WPF And DWG Display

**Files:**
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/PavementLayerTemplateWindow.xaml.cs`
- Modify: `src/cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.cpp`

- [ ] **Step 1: Add UI controls**

Add per-layer hatch angle and hatch scale text boxes near the hatch pattern dropdown; update preview when they change.

- [ ] **Step 2: Fix preview label sizing**

Use a fixed label font size for type/thickness labels and slope labels so thickness changes do not alter text size.

- [ ] **Step 3: Apply hatch angle and scale**

Use hatch scale for spacing and hatch angle for pattern line direction in WPF and DWG template entity preview.

- [ ] **Step 4: Replace DWG annotations**

Stop drawing layer dimension annotations in `DnPavementLayerTemplateEntity`; draw the template name above the section, centered by estimating text width and placing the text origin at `centerX - textWidth / 2`.

### Task 4: Docs, Version, Verification, Sync

**Files:**
- Modify: `build/RoadProto.Build.props`
- Modify: `README.md`
- Modify: `tests/README.md`
- Modify: `docs/dev/version_log.md`
- Modify: `docs/business/cross_section/路面结构层模板_*.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/reuse/pavement_layer_template.md`
- Modify: `docs/reuse/capability_catalog.md`

- [ ] **Step 1: Update version to `v0.1.22_20260523_PavementLayerTemplateHatchParams`**

Update load paths and verification notes consistently.

- [ ] **Step 2: Run verification**

Run Debug/Release core tests, managed bridge tests, WPF Release build, and full `RoadProto.sln` Release build.

- [ ] **Step 3: Commit, push, and sync main directory**

Commit/push the worktree branch, copy source/docs and Release artifacts back to `F:\0_GPT_道路设计原型功能项目`, then report main ARX and managed DLL paths.
