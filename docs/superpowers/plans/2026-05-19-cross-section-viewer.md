# Cross Section Viewer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a read-only cross-section viewer that previews road model subgrade lines, slope lines, and ground line by sampled station.

**Architecture:** Persist sampled stations in `RoadModelData`, derive per-station 2D preview segments in domain code, and show those segments through a WPF window launched by an ObjectARX command. CAD access stays in `cad_adapter`, WPF receives plain DTOs through the existing key-value Bridge style.

**Tech Stack:** C++17 domain/application/ObjectARX, WPF .NET Framework 4.8, existing RoadProto core tests and managed bridge tests.

---

### Task 1: Domain Preview Model

**Files:**
- Modify: `src/domain/cross_section/RoadModel.h`
- Modify: `src/domain/cross_section/RoadModel.cpp`
- Test: `tests/core_tests.cpp`

- [ ] Add `sampledStations` to `RoadModelData`.
- [ ] Add `RoadModelSectionPreviewSegment`, `RoadModelSectionPreview`, `RoadModelSectionPreviewRequest`, and `RoadModelSectionPreviewBuilder`.
- [ ] Write failing tests for sampled station persistence in builder output and preview generation at one station.
- [ ] Run core build and confirm the tests fail because preview types and sampled station storage are missing.
- [ ] Implement sampled station assignment and preview generation by station interpolation.
- [ ] Run core tests and confirm the new tests pass.

### Task 2: Bridge DTO And WPF Window

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewerDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewerFile.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelSectionViewerWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelSectionViewerWindow.xaml.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadModelSectionViewerCommands.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Modify: `tests/RoadProtoManagedBridgeTests/RoadProtoManagedBridgeTests.csproj`
- Test: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] Write failing managed tests for request file round-trip and window XAML containing station list, preview canvas, and legend labels.
- [ ] Run managed bridge tests and confirm they fail.
- [ ] Implement DTOs and key-value file reader.
- [ ] Implement WPF viewer with station list, Canvas drawing, simple scaling, and status text.
- [ ] Add AutoCAD managed command that reads the pending request and opens the viewer.
- [ ] Add Ribbon button `查看横断面`.
- [ ] Run managed bridge tests and WPF build.

### Task 3: ObjectARX Command And Native Bridge

**Files:**
- Create: `src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.h`
- Create: `src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h`
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- Modify: `src/modules/cross_section/CrossSectionModule.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Test: `tests/core_tests.cpp`

- [ ] Write failing core source-contract tests for `RD_SECTION_ROAD_MODEL_VIEW_SECTION`, bridge queue function, and managed WPF command name.
- [ ] Run core build and confirm failure.
- [ ] Add native bridge writer for viewer request files.
- [ ] Add command procedure that selects `DnRoadModelEntity`, builds previews, and queues WPF viewer.
- [ ] Register command metadata and Ribbon attachability in the cross-section module.
- [ ] Update project files to compile the native bridge.
- [ ] Run core tests and ARX build.

### Task 4: Documentation And Version

**Files:**
- Create: `docs/business/cross_section/查看横断面.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/reuse/road_model.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`
- Modify: `README.md`
- Modify: `build/RoadProto.Build.props`

- [ ] Document the new command, UI behavior, and limitations.
- [ ] Bump version to `v0.1.16` with stage `RoadModelSectionViewer`.
- [ ] Run core tests, managed bridge tests, WPF Debug/Release builds, and ARX Debug/Release builds.
- [ ] Report the final ARX and managed DLL paths.
