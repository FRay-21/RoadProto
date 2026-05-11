# 平面布线交互与 WPF 参数窗口优化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 优化平面布线，使创建时可见预览、道路中线按元素着色、参数编辑使用 WPF、主点几何符合回旋线平曲线公式，并提供第一版参数夹点和引线桩号。

**Architecture:** `domain/alignment` 负责回旋线主点公式、五单元几何和可测试参数规则；`cad_adapter/objectarx/alignment` 负责点取预览、自定义实体绘制、夹点、DTO 文件桥接和内部应用命令；`src/ui/wpf/RoadProto.Terrain.UI` 负责 WPF 道路中线属性/参数窗口与托管命令。

**Tech Stack:** C++17、ObjectARX 2021、AutoCAD 2021、.NET Framework 4.8 WPF、MSBuild、RoadProto core tests。

---

### Task 1: Domain Geometry Contracts

**Files:**
- Modify: `src/domain/alignment/AlignmentGeometry.h`
- Modify: `src/domain/alignment/AlignmentGeometry.cpp`
- Modify: `src/domain/alignment/HorizontalAlignmentBuilder.cpp`
- Modify: `tests/core_tests.cpp`

- [ ] Add tests for spiral tangent offset `m`, shift `p`, and tangent length `T`.
- [ ] Add tests that PI feature point remains the tangent intersection control point and TS/ST lie on tangent lines.
- [ ] Implement `clothoidEndX`, `clothoidEndY`, `spiralTangentOffsetM`, `spiralShiftP`, and `defaultSpiralTangentLength`.
- [ ] Rebuild chain geometry from main-point formulas and remove unreachable legacy builder code.
- [ ] Run core tests and verify expected failures pass after implementation.

### Task 2: Entity Drawing, Labels, and Grips

**Files:**
- Modify: `src/domain/alignment/AlignmentGeometry.h`
- Modify: `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h`
- Modify: `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.cpp`

- [ ] Add curve/feature indices to alignment elements and feature points.
- [ ] Draw line cyan, first spiral red, arc yellow, second spiral magenta.
- [ ] Draw station labels as leader callouts.
- [ ] Map grip indices to control points or feature points.
- [ ] Implement first-version drag rules for TS/ST/SC/CS/ArcMid by adjusting `T1/T2/LS1/LS2/R`.

### Task 3: WPF Dialog Bridge

**Files:**
- Create: `src/cad_adapter/objectarx/alignment/AlignmentDialogBridge.h`
- Create: `src/cad_adapter/objectarx/alignment/AlignmentDialogBridge.cpp`
- Modify: `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.cpp`
- Modify: `src/modules/alignment/AlignmentModule.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`

- [ ] Write request/response DTO files in UTF-8 key-value format.
- [ ] Queue managed command `RD_ALIGN_SHOW_WPF_DIALOG` with request file path.
- [ ] Add internal C++ command `RD_ALIGN_APPLY_DIALOG_FILE` to apply WPF responses.
- [ ] Keep Core Console fallback non-UI-safe.

### Task 4: WPF Alignment Windows

**Files:**
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/AlignmentDialogDtos.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/AlignmentDialogFile.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AlignmentCenterlineWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AlignmentCenterlineWindow.xaml.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/StationLabelSettingsWindow.xaml`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/StationLabelSettingsWindow.xaml.cs`
- Create: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/AlignmentDialogCommands.cs`

- [ ] Implement road properties tab, curve parameter tab, and station settings child dialog.
- [ ] Implement terrain model selection button by hiding window, prompting for `DNTERRAINTINENTITY`, and storing handle.
- [ ] Register managed command `RD_ALIGN_SHOW_WPF_DIALOG`.
- [ ] On OK, write response DTO and send `RD_ALIGN_APPLY_DIALOG_FILE`.

### Task 5: Creation Preview and Command Flow

**Files:**
- Modify: `src/cad_adapter/objectarx/alignment/ObjectArxAlignmentCommand.cpp`
- Modify: `src/cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.cpp`

- [ ] Use `acedGetPoint` base-point rubber band for point selection.
- [ ] Insert/update a preview road centerline after the third point.
- [ ] On command completion, keep the preview as the real entity and open WPF properties.
- [ ] On invalid geometry, report error and delete preview.

### Task 6: Docs, Version, Verification

**Files:**
- Modify: `README.md`
- Modify: `build/RoadProto.Build.props`
- Modify: `docs/business/alignment/平面布线_道路中线.md`
- Modify: `docs/modules/alignment.md`
- Modify: `docs/reuse/alignment_centerline.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`

- [ ] Update version to `v0.1.8_20260509_AlignmentWpfPreview`.
- [ ] Document WPF dialog bridge and manual verification requirements.
- [ ] Run core tests, ARX Debug/Release builds, and WPF Debug/Release builds.
