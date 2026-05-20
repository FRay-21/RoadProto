# Road Model Mesh Wireframe Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Redefine generated road models from longitudinal component lines into sampled cross-section mesh wireframes.

**Architecture:** Keep existing `componentLines` and `slopeLines` for compatibility, add `sections` and `wireLines` as the primary generated model. Domain code builds section node chains and derives mesh wire lines; ObjectARX persists and draws them; the viewer reads sections first and falls back to old interpolation.

**Tech Stack:** C++17 domain/application/ObjectARX, WPF .NET Framework 4.8, existing core and managed bridge tests.

---

### Task 1: Domain Data And Failing Tests

**Files:**
- Modify: `src/domain/cross_section/RoadModel.h`
- Modify: `tests/core_tests.cpp`

- [x] Add tests that expect `RoadModelData.sections` to contain sampled station sections.
- [x] Add tests that expect `RoadModelData.wireLines` to contain `SectionRib`, `OuterBoundary`, and `EndCap` lines.
- [x] Add a test where adjacent sections have different slope node counts and expect a `Transition` line.
- [x] Add a test that checks a slope wire line inherits the slope template component color.
- [x] Run `MSBuild tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64` and verify failure because the new types/fields do not exist.

### Task 2: Build Section Nodes And Mesh Wire Lines

**Files:**
- Modify: `src/domain/cross_section/RoadModel.h`
- Modify: `src/domain/cross_section/RoadModel.cpp`
- Test: `tests/core_tests.cpp`

- [x] Add `RoadModelSectionNodeKind`, `RoadModelWireLineKind`, `RoadModelSectionNode`, `RoadModelSection`, and `RoadModelWireLine`.
- [x] Add `sections` and `wireLines` to `RoadModelData`.
- [x] During `RoadModelBuilder::build`, create section nodes for each sampled station before interval line accumulation.
- [x] Derive `SectionRib`, `Longitudinal`, `OuterBoundary`, `Transition`, and `EndCap` from adjacent section chains.
- [x] Keep existing `componentLines` and `slopeLines` generation.
- [x] Run core tests and verify the new domain tests pass.

### Task 3: Entity Persistence And Drawing

**Files:**
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.cpp`
- Test: `tests/core_tests.cpp`

- [x] Add source-contract tests for new persistence version, `sections`, `wireLines`, and preferred `wireLines` drawing.
- [x] Run core tests and verify failure.
- [x] Bump `DnRoadModelEntity` version.
- [x] Read/write sections and wire lines with bounded counts and finite-point validation.
- [x] Draw `wireLines` first when present; fall back to old lines when empty.
- [x] Include new points in extents and transform.
- [x] Run core tests and ARX Debug build.

### Task 4: Viewer And Documentation

**Files:**
- Modify: `src/domain/cross_section/RoadModel.cpp`
- Modify: `docs/business/cross_section/横断面戴帽_道路模型创建.md`
- Modify: `docs/business/cross_section/查看横断面.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/reuse/road_model.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`
- Modify: `README.md`
- Modify: `build/RoadProto.Build.props`

- [x] Update `RoadModelSectionPreviewBuilder` to prefer `sections`.
- [x] Bump version to `v0.1.17` with stage `RoadModelMeshWireframe`.
- [x] Document the new section-node and mesh-wireframe model definition.
- [x] Run core tests, managed bridge tests, WPF Debug/Release builds, and ARX Debug/Release builds.
