# 查看横断面预览与绘制优化 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 优化 `查看横断面` 窗口预览交互，并支持将所有采样桩号横断面批量绘制到模型空间。

**Architecture:** WPF 继续只负责展示和触发动作；Bridge 使用请求/响应文件传递 `drawSections` 动作；C++ ObjectARX 读取道路模型、点取基点并创建一桩号一个 `DnRoadModelSectionDrawingEntity` 自定义实体。结构层颜色和填充参数优先从 `DnPavementLayerTemplateEntity` 读取，旧数据降级为道路模型保存 RGB。

**Tech Stack:** C++17, ObjectARX 2021, WPF .NET Framework 4.8, existing key-value Bridge files, RoadProto core tests and managed bridge tests.

---

### Task 1: Bridge And WPF Contracts

**Files:**
- Modify: `tests/RoadProtoManagedBridgeTests/Program.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewerDtos.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/Bridge/RoadModelSectionViewerFile.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/AutoCad/RoadModelSectionViewerCommands.cs`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelSectionViewerWindow.xaml`
- Modify: `src/ui/wpf/RoadProto.Terrain.UI/RoadModelSectionViewerWindow.xaml.cs`

- [ ] **Step 1: Write failing managed bridge/source-contract tests**

Add checks that the viewer request reads `responsePath`, writes a `drawSections` response, the window contains `绘制横断面`, the canvas wires `MouseWheel` and mouse drag handlers, and the AutoCAD command sends `RD_SECTION_ROAD_MODEL_VIEW_SECTION_APPLY_DIALOG_FILE`.

- [ ] **Step 2: Run managed bridge tests and verify failure**

Run:

```powershell
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
```

Expected: FAIL because `responsePath`, draw action, and preview mouse handlers do not exist yet.

- [ ] **Step 3: Implement WPF request/response DTOs and file I/O**

Add `RoadModelSectionViewerAction`, `RoadModelSectionViewerResponse`, `ResponsePath` on request, and `WriteResponse(...)` that writes:

```text
action=drawSections
accepted=1
handle=<road model handle>
```

- [ ] **Step 4: Implement WPF UI controls and interaction**

Add `绘制横断面` button, `Response` property, draw button handler, `_previewZoom`, `_previewPan`, mouse wheel zoom, mouse drag pan, transform helper, `ClipToBounds`, and structure-layer polygon fill before line drawing.

- [ ] **Step 5: Implement AutoCAD managed command forwarding**

After `ShowDialog()`, if `window.Response` is present, write response and send:

```csharp
document.SendStringToExecute($"RD_SECTION_ROAD_MODEL_VIEW_SECTION_APPLY_DIALOG_FILE \"{responseCommandPath}\"\n", true, false, true);
```

- [ ] **Step 6: Re-run managed bridge tests and verify pass**

Run the same `dotnet run` command. Expected: PASS.

### Task 2: Native Bridge And Command Contract

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.h`
- Modify: `src/cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h`
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`
- Modify: `src/modules/cross_section/CrossSectionModule.cpp`

- [ ] **Step 1: Write failing native source-contract tests**

Add checks for `RoadModelSectionViewerAction::DrawSections`, `RoadModelSectionViewerResponse`, `readRoadModelSectionViewerResponse`, `responsePath`, `RD_SECTION_ROAD_MODEL_VIEW_SECTION_APPLY_DIALOG_FILE`, and module registration of the internal command.

- [ ] **Step 2: Run core tests and verify failure**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
```

Expected: FAIL on missing native Bridge/action command contract.

- [ ] **Step 3: Implement native viewer response parsing**

Generate `responsePath` in `queueRoadModelSectionViewerWpfDialog`, write it into the request file, add response parsing, and remove response files after reading.

- [ ] **Step 4: Register internal apply command**

Add `roadModelViewSectionApplyDialogFileCommandProcedure()` and register `RD_SECTION_ROAD_MODEL_VIEW_SECTION_APPLY_DIALOG_FILE` as non-Ribbon internal command pointing to `docs/business/cross_section/查看横断面.md`.

- [ ] **Step 5: Re-run core tests and verify pass for this contract**

Run core tests. Expected: command/Bridge source-contract tests pass.

### Task 3: Section Drawing Entity And Batch Draw Command

**Files:**
- Modify: `tests/core_tests.cpp`
- Modify: `src/app/RoadProtoArx.vcxproj`
- Modify: `src/app/arx_entry/RoadProtoArxEntry.cpp`
- Create: `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h`
- Create: `src/cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.cpp`
- Modify: `src/cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.cpp`

- [ ] **Step 1: Write failing core source-contract tests**

Add checks that the new entity source/header exist, the ARX project compiles them, app entry initializes/uninitializes the entity class, and the command creates `DnRoadModelSectionDrawingEntity` after prompting an insertion point.

- [ ] **Step 2: Run core tests and verify failure**

Run the core test command. Expected: FAIL because the entity files and initialization are missing.

- [ ] **Step 3: Implement `DnRoadModelSectionDrawingEntity`**

The entity stores insertion point, width, height, road model handle, station, station label, line segments, structure-layer faces, hatch pattern, hatch angle, hatch scale, and future info fields. World draw order is: structure-layer fill, hatch lines, normal lines, outer frame, station text.

- [ ] **Step 4: Implement batch draw command path**

When response action is `DrawSections`, reread the road model, build all previews, read pavement template hatch settings by `pavementLayerTemplateHandle` and `layerIndex`, prompt `acedGetPoint`, lay out each section upward without overlap, append one entity per station, and report success/skipped counts.

- [ ] **Step 5: Re-run core tests**

Run core tests. Expected: PASS.

### Task 4: Documentation, Versioning, And Verification

**Files:**
- Modify: `README.md`
- Modify: `build/RoadProto.Build.props`
- Modify: `docs/business/cross_section/查看横断面.md`
- Modify: `docs/modules/cross_section.md`
- Modify: `docs/modules/module_index.md`
- Modify: `docs/reuse/road_model.md`
- Modify: `docs/reuse/capability_catalog.md`
- Modify: `docs/dev/version_log.md`
- Modify: `tests/README.md`

- [ ] **Step 1: Write/update source-contract documentation tests**

Update existing doc/version tests to expect the new version stage and new viewer drawing notes.

- [ ] **Step 2: Run tests and verify failure**

Run core tests. Expected: FAIL until docs/version strings are updated.

- [ ] **Step 3: Update docs and version metadata**

Bump to `v0.1.25_20260525_CrossSectionViewDrawing`, update ARX filename references, describe preview pan/zoom and `绘制横断面`.

- [ ] **Step 4: Run full verification**

Run:

```powershell
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' tests\RoadProtoCoreTests.vcxproj /p:Configuration=Debug /p:Platform=x64
artifacts\x64\Debug\RoadProtoCoreTests.exe
dotnet run --project tests\RoadProtoManagedBridgeTests\RoadProtoManagedBridgeTests.csproj -c Debug
dotnet build src\ui\wpf\RoadProto.Terrain.UI\RoadProto.Terrain.UI.csproj -c Release
& 'D:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe' RoadProto.sln /p:Configuration=Release /p:Platform=x64
```

Expected: all commands pass and Release artifacts are generated under `artifacts/x64/Release/` and `artifacts/managed/Release/net48/`.
