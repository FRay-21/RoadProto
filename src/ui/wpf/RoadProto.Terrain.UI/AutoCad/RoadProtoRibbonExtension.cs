using System;
using System.Linq;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using Autodesk.AutoCAD.ApplicationServices;
using Autodesk.AutoCAD.DatabaseServices;
using Autodesk.AutoCAD.EditorInput;
using Autodesk.AutoCAD.Geometry;
using Autodesk.AutoCAD.Runtime;
using Autodesk.Windows;
using AutoCadApplication = Autodesk.AutoCAD.ApplicationServices.Application;
using AutoCadContextMenuExtension = Autodesk.AutoCAD.Windows.ContextMenuExtension;
using AutoCadMenuItem = Autodesk.AutoCAD.Windows.MenuItem;
using CoreApplication = Autodesk.AutoCAD.ApplicationServices.Core.Application;

[assembly: ExtensionApplication(typeof(RoadProto.Terrain.UI.AutoCad.RoadProtoRibbonExtension))]
[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.RoadProtoRibbonExtension))]
[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.SubgradeTemplateDialogCommands))]
[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.SlopeTemplateDialogCommands))]
[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.RoadModelDialogCommands))]
[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.RoadModelSectionViewerCommands))]
[assembly: CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.SectionDrawingConfigDialogCommands))]

namespace RoadProto.Terrain.UI.AutoCad;

public sealed class RoadProtoRibbonExtension : IExtensionApplication
{
    private const string TabId = "ROADPROTO_RIBBON_TAB";
    private const string TerrainPanelId = "ROADPROTO_TERRAIN_PANEL";
    private const string TerrainTinButtonId = "ROADPROTO_DN_TERRAIN_TIN_CREATE";
    private const string TerrainTinEditButtonId = "ROADPROTO_DN_TERRAIN_TIN_EDIT";
    private const string TerrainTinExportButtonId = "ROADPROTO_DN_TERRAIN_TIN_EXPORT";
    private const string TerrainTinImportButtonId = "ROADPROTO_DN_TERRAIN_TIN_IMPORT";
    private const string AlignmentPanelId = "ROADPROTO_ALIGNMENT_PANEL";
    private const string AlignmentCreateButtonId = "ROADPROTO_RD_ALIGN_CENTERLINE_CREATE";
    private const string AlignmentCurveEditButtonId = "ROADPROTO_RD_ALIGN_CURVE_PARAM_EDIT";
    private const string AlignmentExportIcdButtonId = "ROADPROTO_RD_ALIGN_CENTERLINE_EXPORT_ICD";
    private const string AlignmentImportIcdButtonId = "ROADPROTO_RD_ALIGN_CENTERLINE_IMPORT_ICD";
    private const string ProfilePanelId = "ROADPROTO_PROFILE_PANEL";
    private const string ProfileGradeGraphButtonId = "ROADPROTO_RD_PROFILE_GRADE_GRAPH_CREATE";
    private const string ProfileVerticalCurveButtonId = "ROADPROTO_RD_PROFILE_VERTICAL_CURVE_CREATE";
    private const string CrossSectionPanelId = "ROADPROTO_CROSS_SECTION_PANEL";
    private const string SubgradeTemplateButtonId = "ROADPROTO_RD_SECTION_SUBGRADE_TEMPLATE_CREATE";
    private const string SlopeTemplateButtonId = "ROADPROTO_RD_SECTION_SLOPE_TEMPLATE_CREATE";
    private const string PavementLayerTemplateButtonId = "ROADPROTO_RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE";
    private const string RoadModelCreateButtonId = "ROADPROTO_RD_SECTION_ROAD_MODEL_CREATE";
    private const string RoadModelEditButtonId = "ROADPROTO_RD_SECTION_ROAD_MODEL_EDIT";
    private const string RoadModelSectionViewerButtonId = "ROADPROTO_RD_SECTION_ROAD_MODEL_VIEW_SECTION";
    private const string TerrainTinDxfName = "DNTERRAINTINENTITY";
    private const string RoadCenterlineDxfName = "DNROADCENTERLINEENTITY";
    private const string ProfileGradeGraphDxfName = "DNPROFILEGRADEGRAPHENTITY";
    private const string ProfileVerticalCurveDxfName = "DNPROFILEVERTICALCURVEENTITY";
    private const string RoadModelDxfName = "DNROADMODELENTITY";
    private const string SubgradeTemplateDxfName = "DNSUBGRADETEMPLATEENTITY";
    private const string SlopeTemplateDxfName = "DNSLOPETEMPLATEENTITY";
    private const string PavementLayerTemplateDxfName = "DNPAVEMENTLAYERTEMPLATEENTITY";
    private static ObjectId _lastDoubleClickObjectId = ObjectId.Null;
    private static DateTime _lastDoubleClickUtc = DateTime.MinValue;
    private static bool _doubleClickHookAttached;
    private static AutoCadContextMenuExtension? _profileVerticalCurveContextMenu;
    private static RXClass? _profileVerticalCurveContextMenuClass;
    private static AutoCadMenuItem? _profileVerticalCurveAddPviMenuItem;
    private static AutoCadMenuItem? _profileVerticalCurveDeletePviMenuItem;

    public void Initialize()
    {
        AttachDoubleClickHook();
        AttachProfileVerticalCurveContextMenu();
        if (!TryCreateRibbon())
        {
            ComponentManager.ItemInitialized += OnComponentItemInitialized;
        }
    }

    public void Terminate()
    {
        ComponentManager.ItemInitialized -= OnComponentItemInitialized;
        DetachProfileVerticalCurveContextMenu();
        DetachDoubleClickHook();
    }

    [CommandMethod("DN_ROADPROTO_SHOW_RIBBON")]
    public void ShowRibbon()
    {
        TryCreateRibbon();
    }

    [CommandMethod("RD_PROFILE_VERTICAL_CURVE_CONTEXT_ADD_PVI")]
    public void ContextAddProfileVerticalCurvePvi()
    {
        CoreApplication.DocumentManager.MdiActiveDocument?
            .SendStringToExecute("RD_PROFILE_VERTICAL_CURVE_ADD_PVI ", true, false, true);
    }

    [CommandMethod("RD_PROFILE_VERTICAL_CURVE_CONTEXT_DELETE_PVI")]
    public void ContextDeleteProfileVerticalCurvePvi()
    {
        CoreApplication.DocumentManager.MdiActiveDocument?
            .SendStringToExecute("RD_PROFILE_VERTICAL_CURVE_DELETE_PVI ", true, false, true);
    }

    private static void OnComponentItemInitialized(object? sender, RibbonItemEventArgs e)
    {
        if (TryCreateRibbon())
        {
            ComponentManager.ItemInitialized -= OnComponentItemInitialized;
        }
    }

    private static bool TryCreateRibbon()
    {
        var ribbon = ComponentManager.Ribbon;
        if (ribbon == null)
        {
            return false;
        }

        var tab = ribbon.Tabs.FirstOrDefault(item => item.Id == TabId);
        if (tab == null)
        {
            tab = new RibbonTab
            {
                Id = TabId,
                Title = "RoadProto",
                IsVisible = true,
            };
            ribbon.Tabs.Add(tab);
        }
        tab.IsVisible = true;

        var panel = tab.Panels.FirstOrDefault(item => item.Source.Id == TerrainPanelId);
        if (panel == null)
        {
            var source = new RibbonPanelSource
            {
                Id = TerrainPanelId,
                Title = "数模",
            };
            panel = new RibbonPanel { Source = source };
            tab.Panels.Add(panel);
        }

        if (!panel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == TerrainTinButtonId))
        {
            panel.Source.Items.Add(CreateCommandButton(
                TerrainTinButtonId,
                "地形构网",
                "按样本对象图层和类型提取地形点并创建 TIN 地形数模",
                "DN_TERRAIN_TIN_CREATE "));
        }

        if (!panel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == TerrainTinEditButtonId))
        {
            panel.Source.Items.Add(CreateCommandButton(
                TerrainTinEditButtonId,
                "编辑数模",
                "修改 TIN 地形数模的显示模式和构网参数",
                "DN_TERRAIN_TIN_EDIT "));
        }

        if (!panel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == TerrainTinExportButtonId))
        {
            panel.Source.Items.Add(CreateCommandButton(
                TerrainTinExportButtonId,
                "\u5bfc\u51fa\u6570\u6a21",
                "\u5c06\u9009\u4e2d\u7684 TIN \u5730\u5f62\u6570\u6a21\u5bfc\u51fa\u4e3a .rmesh \u6587\u4ef6",
                "DN_TERRAIN_TIN_EXPORT "));
        }

        if (!panel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == TerrainTinImportButtonId))
        {
            panel.Source.Items.Add(CreateCommandButton(
                TerrainTinImportButtonId,
                "\u5bfc\u5165\u6570\u6a21",
                "\u4ece .rmesh \u6587\u4ef6\u5bfc\u5165 TIN \u5730\u5f62\u6570\u6a21\u5b9e\u4f53",
                "DN_TERRAIN_TIN_IMPORT "));
        }

        var alignmentPanel = tab.Panels.FirstOrDefault(item => item.Source.Id == AlignmentPanelId);
        if (alignmentPanel == null)
        {
            var source = new RibbonPanelSource
            {
                Id = AlignmentPanelId,
                Title = "平面设计",
            };
            alignmentPanel = new RibbonPanel { Source = source };
            tab.Panels.Add(alignmentPanel);
        }

        if (!alignmentPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == AlignmentCreateButtonId))
        {
            alignmentPanel.Source.Items.Add(CreateAlignmentCommandButton(
                AlignmentCreateButtonId,
                "平面布线",
                "点取控制点并创建道路中线自定义实体",
                "RD_ALIGN_CENTERLINE_CREATE "));
        }

        if (!alignmentPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == AlignmentCurveEditButtonId))
        {
            alignmentPanel.Source.Items.Add(CreateAlignmentCommandButton(
                AlignmentCurveEditButtonId,
                "编辑平曲线参数",
                "选择道路中线并编辑 T1、T2、LS1、R、LS2",
                "RD_ALIGN_CURVE_PARAM_EDIT "));
        }

        if (!alignmentPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == AlignmentExportIcdButtonId))
        {
            alignmentPanel.Source.Items.Add(CreateAlignmentCommandButton(
                AlignmentExportIcdButtonId,
                "导出中线 ICD",
                "将选中的道路中线实体导出为 .icd 文件",
                "RD_ALIGN_CENTERLINE_EXPORT_ICD "));
        }

        if (!alignmentPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == AlignmentImportIcdButtonId))
        {
            alignmentPanel.Source.Items.Add(CreateAlignmentCommandButton(
                AlignmentImportIcdButtonId,
                "导入中线 ICD",
                "从 .icd 文件导入道路中线实体",
                "RD_ALIGN_CENTERLINE_IMPORT_ICD "));
        }

        var profilePanel = tab.Panels.FirstOrDefault(item => item.Source.Id == ProfilePanelId);
        if (profilePanel == null)
        {
            var source = new RibbonPanelSource
            {
                Id = ProfilePanelId,
                Title = "纵断面设计",
            };
            profilePanel = new RibbonPanel { Source = source };
            tab.Panels.Add(profilePanel);
        }

        if (!profilePanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == ProfileGradeGraphButtonId))
        {
            profilePanel.Source.Items.Add(CreateProfileCommandButton(
                ProfileGradeGraphButtonId,
                "纵断面拉坡图",
                "选择道路中线并创建纵断面拉坡图",
                "RD_PROFILE_GRADE_GRAPH_CREATE "));
        }

        if (!profilePanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == ProfileVerticalCurveButtonId))
        {
            profilePanel.Source.Items.Add(CreateProfileCommandButton(
                ProfileVerticalCurveButtonId,
                "创建竖曲线",
                "选择纵断面拉坡图并创建竖曲线",
                "RD_PROFILE_VERTICAL_CURVE_CREATE "));
        }

        var crossSectionPanel = tab.Panels.FirstOrDefault(item => item.Source.Id == CrossSectionPanelId);
        if (crossSectionPanel == null)
        {
            var source = new RibbonPanelSource
            {
                Id = CrossSectionPanelId,
                Title = "\u6a2a\u65ad\u9762\u8bbe\u8ba1",
            };
            crossSectionPanel = new RibbonPanel { Source = source };
            tab.Panels.Add(crossSectionPanel);
        }

        if (!crossSectionPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == SubgradeTemplateButtonId))
        {
            crossSectionPanel.Source.Items.Add(CreateCrossSectionCommandButton(
                SubgradeTemplateButtonId,
                "\u521b\u5efa\u8def\u57fa\u6a21\u677f",
                "\u70b9\u53d6\u63d2\u5165\u70b9\u5e76\u521b\u5efa\u72ec\u7acb\u8def\u57fa\u6a21\u677f",
                "RD_SECTION_SUBGRADE_TEMPLATE_CREATE "));
        }

        if (!crossSectionPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == SlopeTemplateButtonId))
        {
            crossSectionPanel.Source.Items.Add(CreateCrossSectionCommandButton(
                SlopeTemplateButtonId,
                "创建边坡模板",
                "点取插入点并创建独立边坡模板",
                "RD_SECTION_SLOPE_TEMPLATE_CREATE "));
        }

        if (!crossSectionPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == PavementLayerTemplateButtonId))
        {
            crossSectionPanel.Source.Items.Add(CreateCrossSectionCommandButton(
                PavementLayerTemplateButtonId,
                "创建路面结构层模板",
                "点取插入点并创建独立路面结构层模板",
                "RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE "));
        }

        if (!crossSectionPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == RoadModelCreateButtonId))
        {
            crossSectionPanel.Source.Items.Add(CreateCrossSectionCommandButton(
                RoadModelCreateButtonId,
                "横断面戴帽",
                "创建横断面戴帽道路模型",
                "RD_SECTION_ROAD_MODEL_CREATE "));
        }

        if (!crossSectionPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == RoadModelEditButtonId))
        {
            crossSectionPanel.Source.Items.Add(CreateCrossSectionCommandButton(
                RoadModelEditButtonId,
                "编辑道路模型",
                "编辑已有道路模型",
                "RD_SECTION_ROAD_MODEL_EDIT "));
        }

        if (!crossSectionPanel.Source.Items.OfType<RibbonButton>().Any(item => item.Id == RoadModelSectionViewerButtonId))
        {
            crossSectionPanel.Source.Items.Add(CreateCrossSectionCommandButton(
                RoadModelSectionViewerButtonId,
                "查看横断面",
                "按采样桩号查看道路模型横断面预览",
                "RD_SECTION_ROAD_MODEL_VIEW_SECTION "));
        }

        tab.IsActive = true;

        return true;
    }

    private static void AttachDoubleClickHook()
    {
        if (_doubleClickHookAttached)
        {
            return;
        }

        CoreApplication.BeginDoubleClick += OnBeginDoubleClick;
        _doubleClickHookAttached = true;
    }

    private static void DetachDoubleClickHook()
    {
        if (!_doubleClickHookAttached)
        {
            return;
        }

        CoreApplication.BeginDoubleClick -= OnBeginDoubleClick;
        _doubleClickHookAttached = false;
    }

    private static void AttachProfileVerticalCurveContextMenu()
    {
        if (_profileVerticalCurveContextMenu != null)
        {
            return;
        }

        var entityClass = RXObject.GetClass(typeof(Entity));
        if (entityClass == null)
        {
            WriteEditorMessage("\nRoadProto 竖曲线右键菜单注册失败: 无法获取 AcDbEntity 类。");
            return;
        }

        var addItem = new AutoCadMenuItem("新增竖曲线变坡点");
        addItem.Click += (_, _) => SendActiveDocumentCommand("RD_PROFILE_VERTICAL_CURVE_CONTEXT_ADD_PVI ");

        var deleteItem = new AutoCadMenuItem("删除竖曲线变坡点");
        deleteItem.Click += (_, _) => SendActiveDocumentCommand("RD_PROFILE_VERTICAL_CURVE_CONTEXT_DELETE_PVI ");

        var menu = new AutoCadContextMenuExtension
        {
            Title = "RoadProto 竖曲线",
        };
        menu.MenuItems.Add(addItem);
        menu.MenuItems.Add(deleteItem);
        menu.Popup += OnProfileVerticalCurveContextMenuPopup;

        AutoCadApplication.AddObjectContextMenuExtension(entityClass, menu);
        _profileVerticalCurveContextMenuClass = entityClass;
        _profileVerticalCurveContextMenu = menu;
        _profileVerticalCurveAddPviMenuItem = addItem;
        _profileVerticalCurveDeletePviMenuItem = deleteItem;
    }

    private static void DetachProfileVerticalCurveContextMenu()
    {
        if (_profileVerticalCurveContextMenu == null || _profileVerticalCurveContextMenuClass == null)
        {
            return;
        }

        _profileVerticalCurveContextMenu.Popup -= OnProfileVerticalCurveContextMenuPopup;
        AutoCadApplication.RemoveObjectContextMenuExtension(
            _profileVerticalCurveContextMenuClass,
            _profileVerticalCurveContextMenu);
        _profileVerticalCurveContextMenu = null;
        _profileVerticalCurveContextMenuClass = null;
        _profileVerticalCurveAddPviMenuItem = null;
        _profileVerticalCurveDeletePviMenuItem = null;
    }

    private static void OnProfileVerticalCurveContextMenuPopup(object? sender, EventArgs e)
    {
        var isProfileVerticalCurveSelection = TryGetImpliedProfileVerticalCurve(out _);
        SetProfileVerticalCurveContextMenuItemState(_profileVerticalCurveAddPviMenuItem, isProfileVerticalCurveSelection);
        SetProfileVerticalCurveContextMenuItemState(_profileVerticalCurveDeletePviMenuItem, isProfileVerticalCurveSelection);
    }

    private static void SetProfileVerticalCurveContextMenuItemState(AutoCadMenuItem? item, bool isVisible)
    {
        if (item == null)
        {
            return;
        }

        item.Visible = isVisible;
        item.Enabled = isVisible;
    }

    private static bool TryGetImpliedProfileVerticalCurve(out ObjectId entityId)
    {
        var document = CoreApplication.DocumentManager.MdiActiveDocument;
        var implied = document?.Editor.SelectImplied();
        if (implied?.Status == PromptStatus.OK
            && TryFindEntityByDxfName(implied.Value, ProfileVerticalCurveDxfName, out entityId))
        {
            return true;
        }

        entityId = ObjectId.Null;
        return false;
    }

    private static void OnBeginDoubleClick(object? sender, BeginDoubleClickEventArgs e)
    {
        try
        {
            var document = CoreApplication.DocumentManager.MdiActiveDocument;
            if (document == null)
            {
                return;
            }

            if (TryFindTerrainTinEntity(document, e.Location, out var entityId))
            {
                if (!SuppressDuplicateDoubleClick(entityId))
                {
                    QueueTerrainTinEditByHandle(document, entityId);
                }
                return;
            }

            if (TryFindEntityByDxfName(document, e.Location, RoadCenterlineDxfName, out var roadCenterlineId))
            {
                if (!SuppressDuplicateDoubleClick(roadCenterlineId))
                {
                    QueueRoadCenterlineEditByHandle(document, roadCenterlineId);
                }
                return;
            }

            if (TryFindEntityByDxfName(document, e.Location, ProfileGradeGraphDxfName, out var profileGradeGraphId))
            {
                if (!SuppressDuplicateDoubleClick(profileGradeGraphId))
                {
                    QueueProfileGradeGraphEditByHandle(document, profileGradeGraphId);
                }
                return;
            }

            if (TryFindEntityByDxfName(document, e.Location, ProfileVerticalCurveDxfName, out var profileVerticalCurveId))
            {
                if (!SuppressDuplicateDoubleClick(profileVerticalCurveId))
                {
                    QueueProfileVerticalCurveEditByHandle(document, profileVerticalCurveId);
                }
                return;
            }

            if (TryFindEntityByDxfName(document, e.Location, RoadModelDxfName, out var roadModelId))
            {
                if (!SuppressDuplicateDoubleClick(roadModelId))
                {
                    QueueRoadModelEditByHandle(document, roadModelId);
                }
                return;
            }

            if (TryFindEntityByDxfName(document, e.Location, SubgradeTemplateDxfName, out var subgradeTemplateId))
            {
                if (!SuppressDuplicateDoubleClick(subgradeTemplateId))
                {
                    QueueSubgradeTemplateEditByHandle(document, subgradeTemplateId);
                }
                return;
            }

            if (TryFindEntityByDxfName(document, e.Location, SlopeTemplateDxfName, out var slopeTemplateId))
            {
                if (!SuppressDuplicateDoubleClick(slopeTemplateId))
                {
                    QueueSlopeTemplateEditByHandle(document, slopeTemplateId);
                }
                return;
            }

            if (TryFindEntityByDxfName(document, e.Location, PavementLayerTemplateDxfName, out var pavementLayerTemplateId))
            {
                if (!SuppressDuplicateDoubleClick(pavementLayerTemplateId))
                {
                    QueuePavementLayerTemplateEditByHandle(document, pavementLayerTemplateId);
                }
            }
        }
        catch (System.Exception error)
        {
            WriteEditorMessage($"\nRoadProto 数模双击编辑触发失败: {error.Message}");
        }
    }

    private static bool TryFindTerrainTinEntity(Document document, Point3d location, out ObjectId entityId)
    {
        var editor = document.Editor;
        var implied = editor.SelectImplied();
        if (implied.Status == PromptStatus.OK && TryFindTerrainTinEntity(implied.Value, out entityId))
        {
            return true;
        }

        var tolerance = GetPickTolerance(editor);
        var min = new Point3d(location.X - tolerance, location.Y - tolerance, location.Z);
        var max = new Point3d(location.X + tolerance, location.Y + tolerance, location.Z);
        var filter = new SelectionFilter(new[]
        {
            new TypedValue((int)DxfCode.Start, TerrainTinDxfName),
        });

        var picked = editor.SelectCrossingWindow(min, max, filter);
        if (picked.Status == PromptStatus.OK && TryFindTerrainTinEntity(picked.Value, out entityId))
        {
            return true;
        }

        entityId = ObjectId.Null;
        return false;
    }

    private static bool TryFindEntityByDxfName(Document document, Point3d location, string dxfName, out ObjectId entityId)
    {
        var editor = document.Editor;
        var implied = editor.SelectImplied();
        if (implied.Status == PromptStatus.OK && TryFindEntityByDxfName(implied.Value, dxfName, out entityId))
        {
            return true;
        }

        var tolerance = GetPickTolerance(editor);
        var min = new Point3d(location.X - tolerance, location.Y - tolerance, location.Z);
        var max = new Point3d(location.X + tolerance, location.Y + tolerance, location.Z);
        var filter = new SelectionFilter(new[]
        {
            new TypedValue((int)DxfCode.Start, dxfName),
        });

        var picked = editor.SelectCrossingWindow(min, max, filter);
        if (picked.Status == PromptStatus.OK && TryFindEntityByDxfName(picked.Value, dxfName, out entityId))
        {
            return true;
        }

        entityId = ObjectId.Null;
        return false;
    }

    private static bool TryFindTerrainTinEntity(SelectionSet? selectionSet, out ObjectId entityId)
    {
        if (selectionSet != null)
        {
            foreach (var objectId in selectionSet.GetObjectIds())
            {
                if (IsTerrainTinEntity(objectId))
                {
                    entityId = objectId;
                    return true;
                }
            }
        }

        entityId = ObjectId.Null;
        return false;
    }

    private static bool TryFindEntityByDxfName(SelectionSet? selectionSet, string dxfName, out ObjectId entityId)
    {
        if (selectionSet != null)
        {
            foreach (var objectId in selectionSet.GetObjectIds())
            {
                if (IsEntityByDxfName(objectId, dxfName))
                {
                    entityId = objectId;
                    return true;
                }
            }
        }

        entityId = ObjectId.Null;
        return false;
    }

    private static bool IsTerrainTinEntity(ObjectId objectId)
    {
        if (objectId.IsNull)
        {
            return false;
        }

        var objectClass = objectId.ObjectClass;
        return string.Equals(objectClass?.DxfName, TerrainTinDxfName, StringComparison.OrdinalIgnoreCase)
            || string.Equals(objectClass?.Name, TerrainTinDxfName, StringComparison.OrdinalIgnoreCase);
    }

    private static bool IsEntityByDxfName(ObjectId objectId, string dxfName)
    {
        if (objectId.IsNull)
        {
            return false;
        }

        var objectClass = objectId.ObjectClass;
        return string.Equals(objectClass?.DxfName, dxfName, StringComparison.OrdinalIgnoreCase)
            || string.Equals(objectClass?.Name, dxfName, StringComparison.OrdinalIgnoreCase);
    }

    private static double GetPickTolerance(Editor editor)
    {
        try
        {
            using var view = editor.GetCurrentView();
            return Math.Max(Math.Max(view.Width, view.Height) / 500.0, 0.01);
        }
        catch
        {
            return 0.1;
        }
    }

    private static bool SuppressDuplicateDoubleClick(ObjectId entityId)
    {
        var now = DateTime.UtcNow;
        if (entityId == _lastDoubleClickObjectId && now - _lastDoubleClickUtc < TimeSpan.FromMilliseconds(750))
        {
            return true;
        }

        _lastDoubleClickObjectId = entityId;
        _lastDoubleClickUtc = now;
        return false;
    }

    private static void QueueTerrainTinEditByHandle(Document document, ObjectId entityId)
    {
        var handle = entityId.Handle.ToString();
        if (string.IsNullOrWhiteSpace(handle))
        {
            return;
        }

        document.SendStringToExecute($"DN_TERRAIN_TIN_EDIT_HANDLE {handle} ", true, false, true);
    }

    private static void QueueRoadCenterlineEditByHandle(Document document, ObjectId entityId)
    {
        var handle = entityId.Handle.ToString();
        if (string.IsNullOrWhiteSpace(handle))
        {
            return;
        }

        document.SendStringToExecute($"RD_ALIGN_CENTERLINE_EDIT_HANDLE {handle} ", true, false, true);
    }

    private static void QueueProfileGradeGraphEditByHandle(Document document, ObjectId entityId)
    {
        var handle = entityId.Handle.ToString();
        if (string.IsNullOrWhiteSpace(handle))
        {
            return;
        }

        document.SendStringToExecute($"RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE {handle} ", true, false, true);
    }

    private static void QueueProfileVerticalCurveEditByHandle(Document document, ObjectId entityId)
    {
        var handle = entityId.Handle.ToString();
        if (string.IsNullOrWhiteSpace(handle))
        {
            return;
        }

        document.SendStringToExecute($"RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE {handle} ", true, false, true);
    }

    private static void QueueSubgradeTemplateEditByHandle(Document document, ObjectId entityId)
    {
        var handle = entityId.Handle.ToString();
        if (string.IsNullOrWhiteSpace(handle))
        {
            return;
        }

        document.SendStringToExecute($"RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE {handle}\n", true, false, true);
    }

    private static void QueueSlopeTemplateEditByHandle(Document document, ObjectId entityId)
    {
        var handle = entityId.Handle.ToString();
        if (string.IsNullOrWhiteSpace(handle))
        {
            return;
        }

        document.SendStringToExecute($"RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE {handle}\n", true, false, true);
    }

    private static void QueuePavementLayerTemplateEditByHandle(Document document, ObjectId entityId)
    {
        var handle = entityId.Handle.ToString();
        if (string.IsNullOrWhiteSpace(handle))
        {
            return;
        }

        document.SendStringToExecute($"RD_SECTION_PAVEMENT_LAYER_TEMPLATE_EDIT_HANDLE {handle}\n", true, false, true);
    }

    private static void QueueRoadModelEditByHandle(Document document, ObjectId entityId)
    {
        var handle = entityId.Handle.ToString();
        if (string.IsNullOrWhiteSpace(handle))
        {
            return;
        }

        document.SendStringToExecute($"RD_SECTION_ROAD_MODEL_EDIT_HANDLE {handle}\n", true, false, true);
    }

    private static void SendActiveDocumentCommand(string command)
    {
        CoreApplication.DocumentManager.MdiActiveDocument?
            .SendStringToExecute(command, true, false, true);
    }

    private static void WriteEditorMessage(string message)
    {
        try
        {
            CoreApplication.DocumentManager.MdiActiveDocument?.Editor.WriteMessage(message);
        }
        catch
        {
        }
    }

    private static RibbonButton CreateCommandButton(string id, string text, string toolTip, string command)
    {
        var icon = CreateTerrainIcon();
        return new RibbonButton
        {
            Id = id,
            Text = text,
            ShowText = true,
            ShowImage = true,
            Image = icon,
            LargeImage = icon,
            Size = RibbonItemSize.Standard,
            Orientation = Orientation.Horizontal,
            ToolTip = toolTip,
            CommandParameter = command,
            CommandHandler = new SendCommandHandler(),
        };
    }

    private static RibbonButton CreateAlignmentCommandButton(string id, string text, string toolTip, string command)
    {
        var icon = CreateAlignmentIcon();
        return new RibbonButton
        {
            Id = id,
            Text = text,
            ShowText = true,
            ShowImage = true,
            Image = icon,
            LargeImage = icon,
            Size = RibbonItemSize.Standard,
            Orientation = Orientation.Horizontal,
            ToolTip = toolTip,
            CommandParameter = command,
            CommandHandler = new SendCommandHandler(),
        };
    }

    private static RibbonButton CreateProfileCommandButton(string id, string text, string toolTip, string command)
    {
        var icon = CreateProfileIcon();
        return new RibbonButton
        {
            Id = id,
            Text = text,
            ShowText = true,
            ShowImage = true,
            Image = icon,
            LargeImage = icon,
            Size = RibbonItemSize.Standard,
            Orientation = Orientation.Horizontal,
            ToolTip = toolTip,
            CommandParameter = command,
            CommandHandler = new SendCommandHandler(),
        };
    }

    private static RibbonButton CreateCrossSectionCommandButton(string id, string text, string toolTip, string command)
    {
        var icon = CreateCrossSectionIcon();
        return new RibbonButton
        {
            Id = id,
            Text = text,
            ShowText = true,
            ShowImage = true,
            Image = icon,
            LargeImage = icon,
            Size = RibbonItemSize.Standard,
            Orientation = Orientation.Horizontal,
            ToolTip = toolTip,
            CommandParameter = command,
            CommandHandler = new SendCommandHandler(),
        };
    }

    private static ImageSource CreateTerrainIcon()
    {
        var drawingGroup = new DrawingGroup();
        drawingGroup.Children.Add(new GeometryDrawing(
            new SolidColorBrush(Color.FromRgb(28, 115, 220)),
            null,
            Geometry.Parse("M 2,14 L 8,7 L 13,11 L 20,3 L 22,14 Z")));
        drawingGroup.Children.Add(new GeometryDrawing(
            new SolidColorBrush(Color.FromRgb(52, 174, 91)),
            null,
            Geometry.Parse("M 2,14 L 22,14 L 22,22 L 2,22 Z")));
        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            new Pen(new SolidColorBrush(Color.FromRgb(250, 216, 61)), 1.5),
            Geometry.Parse("M 3,18 C 7,15 10,21 14,17 S 20,14 22,18")));
        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            new Pen(new SolidColorBrush(Color.FromRgb(245, 112, 48)), 1.2),
            Geometry.Parse("M 5,12 L 10,16 L 15,12 L 20,15")));
        drawingGroup.Freeze();
        return new DrawingImage(drawingGroup);
    }

    private static ImageSource CreateAlignmentIcon()
    {
        var drawingGroup = new DrawingGroup();
        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            new Pen(new SolidColorBrush(Color.FromRgb(95, 103, 116)), 1.2),
            Geometry.Parse("M 3,18 L 9,18 M 15,6 L 21,6")));
        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            new Pen(new SolidColorBrush(Color.FromRgb(28, 115, 220)), 2.0),
            Geometry.Parse("M 3,18 C 8,18 8,6 13,6 S 17,6 21,6")));
        drawingGroup.Children.Add(new GeometryDrawing(
            new SolidColorBrush(Color.FromRgb(250, 216, 61)),
            null,
            Geometry.Parse("M 7,16 L 10,18 L 7,20 Z M 14,4 L 17,6 L 14,8 Z")));
        drawingGroup.Freeze();
        return new DrawingImage(drawingGroup);
    }

    private static ImageSource CreateProfileIcon()
    {
        var drawingGroup = new DrawingGroup();
        var gridPen = new Pen(new SolidColorBrush(Color.FromRgb(160, 168, 181)), 0.7);
        var groundPen = new Pen(new SolidColorBrush(Color.FromRgb(52, 174, 91)), 1.4);
        var gradePen = new Pen(new SolidColorBrush(Color.FromRgb(245, 112, 48)), 1.8);

        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            gridPen,
            Geometry.Parse("M 4,5 L 4,21 M 10,5 L 10,21 M 16,5 L 16,21 M 22,5 L 22,21 M 2,9 L 23,9 M 2,15 L 23,15 M 2,21 L 23,21")));
        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            groundPen,
            Geometry.Parse("M 2,14 C 5,11 8,16 11,13 S 17,8 22,12")));
        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            gradePen,
            Geometry.Parse("M 3,19 L 9,16 L 14,17 L 21,7")));
        drawingGroup.Children.Add(new GeometryDrawing(
            new SolidColorBrush(Color.FromRgb(28, 115, 220)),
            null,
            Geometry.Parse("M 8,15 L 10,16 L 8,17 Z M 13,16 L 15,17 L 13,18 Z M 20,6 L 22,7 L 20,8 Z")));
        drawingGroup.Freeze();
        return new DrawingImage(drawingGroup);
    }

    private static ImageSource CreateCrossSectionIcon()
    {
        var drawingGroup = new DrawingGroup();
        var centerPen = new Pen(new SolidColorBrush(Color.FromRgb(80, 88, 101)), 1.2)
        {
            DashStyle = DashStyles.Dash,
        };

        drawingGroup.Children.Add(new GeometryDrawing(
            null,
            centerPen,
            Geometry.Parse("M 12,3 L 12,22")));
        drawingGroup.Children.Add(new GeometryDrawing(
            new SolidColorBrush(Color.FromRgb(0, 120, 0)),
            null,
            Geometry.Parse("M 7,11 L 12,11 L 12,15 L 7,15 Z M 12,11 L 17,11 L 17,15 L 12,15 Z")));
        drawingGroup.Children.Add(new GeometryDrawing(
            new SolidColorBrush(Color.FromRgb(0, 90, 180)),
            null,
            Geometry.Parse("M 3,13 L 7,11 L 7,15 L 3,17 Z M 17,11 L 21,13 L 21,17 L 17,15 Z")));
        drawingGroup.Children.Add(new GeometryDrawing(
            new SolidColorBrush(Color.FromRgb(120, 120, 120)),
            null,
            Geometry.Parse("M 1,16 L 3,13 L 3,17 L 1,19 Z M 21,13 L 23,16 L 23,19 L 21,17 Z")));
        drawingGroup.Freeze();
        return new DrawingImage(drawingGroup);
    }

    private sealed class SendCommandHandler : ICommand
    {
        public event EventHandler? CanExecuteChanged
        {
            add { }
            remove { }
        }

        public bool CanExecute(object? parameter) => true;

        public void Execute(object? parameter)
        {
            var command = ResolveCommand(parameter);
            if (string.IsNullOrWhiteSpace(command))
            {
                return;
            }

            var document = CoreApplication.DocumentManager.MdiActiveDocument;
            var commandText = command!.Trim();
            document?.SendStringToExecute("\x03\x03" + commandText + " ", true, false, true);
        }

        private static string? ResolveCommand(object? parameter)
        {
            if (parameter is string command)
            {
                return command;
            }

            var commandParameter = parameter?.GetType().GetProperty("CommandParameter");
            return commandParameter?.GetValue(parameter) as string;
        }
    }
}
