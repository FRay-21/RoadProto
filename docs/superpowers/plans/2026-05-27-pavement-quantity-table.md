# Pavement Quantity Table Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the `RD_DRAWING_PAVEMENT_QUANTITY_TABLE` command and generate `.xls` pavement quantity tables from drawn cross-section entities.

**Architecture:** Keep quantity calculation in `domain/quantity`, register a new `DRAWING_QUANTITY` module, and keep ObjectARX selection/file prompts in `cad_adapter/objectarx/drawing_quantity`. Use SpreadsheetML `.xls` output to avoid Excel COM and third-party dependencies.

**Tech Stack:** C++17, ObjectARX 2021 adapter, existing command/module/Ribbon registries, WPF managed Ribbon extension, RoadProto core tests.

---

### Task 1: Domain Quantity Calculator

**Files:**
- Create: `src/domain/quantity/PavementQuantityTable.h`
- Create: `src/domain/quantity/PavementQuantityTable.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`

- [x] **Step 1: Write failing tests** for construct-range splitting, boundary interpolation, average-section volume, and dynamic `.xls` columns.
- [x] **Step 2: Run test build to verify red** with missing quantity headers.
- [x] **Step 3: Implement calculator and SpreadsheetML writer**.
- [x] **Step 4: Run core tests and keep them green**.

### Task 2: Module, Startup, And Ribbon

**Files:**
- Create: `src/modules/drawing_quantity/DrawingQuantityModule.h`
- Create: `src/modules/drawing_quantity/DrawingQuantityModule.cpp`
- Create: `src/app/startup/DrawingQuantityStartupRegistration.h`
- Create: `src/app/startup/DrawingQuantityStartupRegistration.cpp`
- Modify: `src/app/startup/Startup.cpp`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`

- [x] **Step 1: Write failing metadata tests** for command registration and managed Ribbon source.
- [x] **Step 2: Implement module registration** through `ModuleRegistry`.
- [x] **Step 3: Add visible `出图出表` Ribbon panel and button**.
- [x] **Step 4: Build WPF project**.

### Task 3: ObjectARX Command Adapter

**Files:**
- Create: `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.h`
- Create: `src/cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.*`
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`

- [x] **Step 1: Register command procedure with test-build stub**.
- [x] **Step 2: Select `DnRoadModelSectionDrawingEntity` objects and validate one road model handle**.
- [x] **Step 3: Read road model section nodes and structure ranges**.
- [x] **Step 4: Prompt `.xls` path and call the domain writer**.
- [x] **Step 5: Build ARX Debug and Release**.

### Task 4: Documentation And Versioning

**Files:**
- Create: `docs/business/drawing_quantity/路面工程量统计表.md`
- Create: `docs/modules/drawing_quantity.md`
- Create: `docs/reuse/pavement_quantity_table.md`
- Modify: `docs/modules/module_index.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `README.md`
- Modify: `tests/README.md`
- Modify: `build/RoadProto.Build.props`

- [x] **Step 1: Add business, module, and reuse documentation**.
- [x] **Step 2: Bump version to `v0.1.28_20260527_PavementQuantityTable`**.
- [x] **Step 3: Update verification status after successful builds**.
