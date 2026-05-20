# Slope Template Road Model Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build slope template authoring and use those templates in cross-section road-model generation.

**Architecture:** Add a domain slope template model and reuse the existing cross-section module pattern for commands, WPF bridge files, custom entities, and `DnRoadModelEntity` persistence. Road-model generation keeps subgrade lines and slope lines in the same entity but separate data collections.

**Tech Stack:** C++17, ObjectARX 2021, WPF .NET Framework 4.8, existing C++ core tests and managed bridge tests.

---

### Task 1: Domain Slope Template Model

**Files:**
- Create: `src/domain/cross_section/SlopeTemplateModel.h`
- Create: `src/domain/cross_section/SlopeTemplateModel.cpp`
- Modify: `tests/core_tests.cpp`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] Write failing tests for fill/cut defaults, geometry constraint calculation, repeat-group validation, and code/display conversions.
- [ ] Run `RoadProtoCoreTests` and verify the tests fail because `SlopeTemplateModel` does not exist.
- [ ] Implement enums, data structs, defaults, geometry resolution, normalize rules, and string conversion functions.
- [ ] Add new `.cpp` to both C++ projects.
- [ ] Run core tests and verify the new domain tests pass.

### Task 2: Road Model Slope Data And Builder

**Files:**
- Modify: `src/domain/cross_section/RoadModel.h`
- Modify: `src/domain/cross_section/RoadModel.cpp`
- Modify: `src/application/cross_section/RoadModelBuildService.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] Write failing tests for left/right search widths, slope template group priority, subgrade outer-edge start points, TIN cross-section intersection, stop-at-ground, repeat-until-ground failure, and slope line output.
- [ ] Run core tests and verify the tests fail on missing fields and behavior.
- [ ] Extend `RoadModelConfig`, `RoadModelBuildInput`, and `RoadModelData` with slope template groups, slope template sources, TIN surface data, and slope lines.
- [ ] Implement TIN cross-section cache and section ground-line cutting with triangle AABB filtering.
- [ ] Implement slope template application and success/failure logic.
- [ ] Run core tests and verify slope builder behavior passes.

### Task 3: Native Slope Template Entity And Commands

**Files:**
- Create: `src/application/cross_section/SlopeTemplateCreateService.h`
- Create: `src/application/cross_section/SlopeTemplateCreateService.cpp`
- Create: `src/cad_adapter/objectarx/cross_section/DnSlopeTemplateEntity.h`
- Create: `src/cad_adapter/objectarx/cross_section/DnSlopeTemplateEntity.cpp`
- Create: `src/cad_adapter/objectarx/cross_section/SlopeTemplateDialogBridge.h`
- Create: `src/cad_adapter/objectarx/cross_section/SlopeTemplateDialogBridge.cpp`
- Create: `src/cad_adapter/objectarx/cross_section/ObjectArxSlopeTemplateCommand.h`
- Create: `src/cad_adapter/objectarx/cross_section/ObjectArxSlopeTemplateCommand.cpp`
- Modify: `src/app/startup/CrossSectionStartupRegistration.cpp`
- Modify: `src/modules/cross_section/CrossSectionModule.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `tests/RoadProtoCoreTests.vcxproj`
- Modify: `tests/core_tests.cpp`

- [ ] Write failing source-contract tests for class declaration, DWG persistence, drawing, double-click bridge, command metadata, startup registration, and project inclusion.
- [ ] Run core tests and verify the contract tests fail.
- [ ] Implement create service, custom entity, bridge file reader/writer, and ObjectARX commands.
- [ ] Register commands and entity class.
- [ ] Run core tests and verify native source-contract tests pass.

### Task 4: WPF Slope Template Window And Road Model UI

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SlopeTemplateDialogDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/SlopeTemplateDialogFile.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/SlopeTemplateDialogCommands.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/SlopeTemplateWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/SlopeTemplateWindow.xaml.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/SlopeTemplateGroupWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/SlopeTemplateGroupWindow.xaml.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelDialogDtos.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelDialogFile.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelWindow.xaml`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelWindow.xaml.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadModelDialogCommands.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadProtoRibbonExtension.cs`
- Modify: `tests/RoadProtoManagedBridgeTests/RoadProtoManagedBridgeTests.csproj`
- Modify: `tests/RoadProtoManagedBridgeTests/Program.cs`

- [ ] Write failing managed bridge tests for slope template request/response fields and road-model slope template groups.
- [ ] Run managed bridge tests and verify they fail on missing DTO/file support.
- [ ] Implement WPF DTOs, bridge files, commands, windows, Ribbon button, double-click hook, road-model global search-width fields, and slope template group UI.
- [ ] Run managed bridge tests and WPF build.

### Task 5: ObjectARX Road Model Integration

**Files:**
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/RoadModelDialogBridge.h`
- Modify: `src/cad_adapter/objectarx/cross_section/RoadModelDialogBridge.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.h`
- Modify: `src/cad_adapter/objectarx/cross_section/DnRoadModelEntity.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] Write failing source-contract tests for slope groups, slope template picking, TIN handle reading, slope line persistence, and slope line drawing.
- [ ] Run core tests and verify failures.
- [ ] Extend native bridge parsing/writing for slope fields.
- [ ] Read linked terrain TIN from the road centerline and pass TIN points/triangles to `RoadModelBuildService`.
- [ ] Read slope template handles from CAD and pass slope sources to the builder.
- [ ] Persist and draw slope lines in `DnRoadModelEntity`.
- [ ] Run core tests.

### Task 6: Documentation And Version

**Files:**
- Create: `docs/business/cross_section/边坡模板_创建.md`
- Create: `docs/business/cross_section/边坡模板_编辑.md`
- Create: `docs/business/cross_section/边坡模板_WPF桥接回写.md`
- Create: `docs/business/cross_section/横断面戴帽_边坡模板.md`
- Create: `docs/reuse/slope_template.md`
- Modify: `docs/reuse/road_model.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`
- Modify: `README.md`
- Modify: `build/RoadProto.Build.props`

- [ ] Add business docs and reuse docs.
- [ ] Update module index, README, tests README, version log, and build version metadata.
- [ ] Run documentation/source path checks through core tests.

### Task 7: Verification

**Files:**
- All modified source and docs.

- [ ] Run `RoadProtoCoreTests`.
- [ ] Run `RoadProtoManagedBridgeTests`.
- [ ] Build WPF Release.
- [ ] Build ARX Release.
- [ ] Report any AutoCAD manual-test gaps that cannot be executed in this environment.
