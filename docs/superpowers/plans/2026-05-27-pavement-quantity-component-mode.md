# Pavement Quantity Component Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an export-time aggregation mode so pavement quantity tables can be generated either by component+layer or by layer type only.

**Architecture:** Keep aggregation in `src/domain/quantity`, carry component names through road model section nodes and drawn section faces, and keep the output-path UI choice in the ObjectARX adapter. Preserve the existing by-layer behavior as an explicit mode.

**Tech Stack:** C++17, ObjectARX 2021 adapter, Windows file save dialog customization, RoadProto core tests.

---

### Task 1: Domain Aggregation Mode

**Files:**
- Modify: `src/domain/quantity/PavementQuantityTable.h`
- Modify: `src/domain/quantity/PavementQuantityTable.cpp`
- Modify: `tests/core_tests.cpp`

- [x] **Step 1: Write failing tests** for component+layer columns and old by-layer aggregation.
- [x] **Step 2: Run Debug core test build and verify the new tests fail**.
- [x] **Step 3: Add `PavementQuantityAggregationMode` and component-aware sample normalization**.
- [x] **Step 4: Run Debug core tests and verify they pass**.

### Task 2: Carry Component Names Through Road Model And Drawn Sections

**Files:**
- Modify: `src/domain/cross_section/RoadModel.h`
- Modify: `src/domain/cross_section/RoadModel.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h`
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewerDtos.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewerFile.cs`
- Modify: `tests/core_tests.cpp`

- [x] **Step 1: Add failing source-contract tests** for `componentName` persistence and viewer bridge fields.
- [x] **Step 2: Add component names to generated pavement layer section nodes and preview segments**.
- [x] **Step 3: Persist component names in road model and drawn section entities with version-compatible reads**.
- [x] **Step 4: Run Debug core tests**.

### Task 3: Export-Time Mode Selection

**Files:**
- Modify: `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.cpp`
- Modify: `tests/core_tests.cpp`

- [x] **Step 1: Add failing source-contract tests** for a save dialog mode option.
- [x] **Step 2: Add a save-file dialog customization with `按部件和结构层` and `按结构层类型` choices**.
- [x] **Step 3: Pass the selected aggregation mode to `PavementQuantityTableCalculator::build`**.
- [x] **Step 4: Run Debug core tests**.

### Task 4: Documentation, Versioning, And Verification

**Files:**
- Modify: `build/RoadProto.Build.props`
- Modify: `README.md`
- Modify: `docs/business/drawing_quantity/路面工程量统计表.md`
- Modify: `docs/reuse/pavement_quantity_table.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`

- [x] **Step 1: Bump version to `v0.1.29_20260527_PavementQuantityComponentMode`**.
- [x] **Step 2: Update business, reuse, README, test, and version docs**.
- [x] **Step 3: Build and run core tests in Debug and Release**.
- [x] **Step 4: Build `RoadProto.sln` in Debug and Release**.
