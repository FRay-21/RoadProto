using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class SubgradeTemplateWindow : Window
{
    private const SubgradeRoadGrade CreateModeDefaultRoadGrade = SubgradeRoadGrade.Expressway;
    private readonly SubgradeTemplateDialogRequest _request;
    private readonly ObservableCollection<SubgradeComponentDto> _components = new();
    private bool _loading = true;

    public SubgradeTemplateWindow(SubgradeTemplateDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        Response = null;
        LoadComboBoxes();
        LoadRequest();
    }

    public SubgradeTemplateDialogResponse? Response { get; private set; }

    private SubgradeComponentDto? SelectedComponent
        => ComponentListBox.SelectedIndex >= 0 && ComponentListBox.SelectedIndex < _components.Count
            ? _components[ComponentListBox.SelectedIndex]
            : null;

    private bool IsCreateMode => string.IsNullOrWhiteSpace(_request.Handle);
    private bool RequestHasPersistedComponents => _request.Components.Count > 0;

    private void LoadComboBoxes()
    {
        DisplayScaleComboBox.ItemsSource = new[]
        {
            new ComboOption<double>("1:1", 1.0),
            new ComboOption<double>("1:10", 10.0),
            new ComboOption<double>("1:20", 20.0),
            new ComboOption<double>("1:50", 50.0),
            new ComboOption<double>("1:100", 100.0),
        };
        DisplayScaleComboBox.DisplayMemberPath = nameof(ComboOption<double>.Label);

        RoadGradeComboBox.ItemsSource = Enum.GetValues(typeof(SubgradeRoadGrade))
            .Cast<SubgradeRoadGrade>()
            .Select(value => new ComboOption<SubgradeRoadGrade>(SubgradeTemplateLabels.RoadGradeLabel(value), value))
            .ToList();
        RoadGradeComboBox.DisplayMemberPath = nameof(ComboOption<SubgradeRoadGrade>.Label);

        var sideOptions = new[]
        {
            new ComboOption<SubgradeSide>("左侧", SubgradeSide.Left),
            new ComboOption<SubgradeSide>("右侧", SubgradeSide.Right),
        };
        AddSideComboBox.ItemsSource = sideOptions;
        AddSideComboBox.DisplayMemberPath = nameof(ComboOption<SubgradeSide>.Label);

        var typeOptions = Enum.GetValues(typeof(SubgradeComponentType))
            .Cast<SubgradeComponentType>()
            .Select(value => new ComboOption<SubgradeComponentType>(SubgradeTemplateLabels.ComponentTypeLabel(value), value))
            .ToList();
        AddTypeComboBox.ItemsSource = typeOptions;
        AddTypeComboBox.DisplayMemberPath = nameof(ComboOption<SubgradeComponentType>.Label);
        ComponentTypeComboBox.ItemsSource = typeOptions;
        ComponentTypeComboBox.DisplayMemberPath = nameof(ComboOption<SubgradeComponentType>.Label);

        SlopeModeComboBox.ItemsSource = Enum.GetValues(typeof(SubgradeSlopeMode))
            .Cast<SubgradeSlopeMode>()
            .Select(value => new ComboOption<SubgradeSlopeMode>(SubgradeTemplateLabels.SlopeModeLabel(value), value))
            .ToList();
        SlopeModeComboBox.DisplayMemberPath = nameof(ComboOption<SubgradeSlopeMode>.Label);
    }

    private void LoadRequest()
    {
        _loading = true;
        var initialRoadGrade = IsCreateMode ? CreateModeDefaultRoadGrade : _request.RoadGrade;
        TemplateNameBox.Text = string.IsNullOrWhiteSpace(_request.TemplateName) ? "默认路基模板" : _request.TemplateName;
        SelectComboValue(DisplayScaleComboBox, _request.DisplayScale);
        SelectComboValue(RoadGradeComboBox, initialRoadGrade);
        SelectComboValue(AddSideComboBox, SubgradeSide.Right);
        SelectComboValue(AddTypeComboBox, SubgradeComponentType.TravelLane);

        _components.Clear();
        var source = !IsCreateMode && RequestHasPersistedComponents
            ? _request.Components.Select(item => item.Clone()).ToList()
            : BuildDefaults(initialRoadGrade);
        foreach (var component in source)
        {
            _components.Add(component);
        }
        ComponentListBox.ItemsSource = _components;

        if (_components.Count > 0)
        {
            ComponentListBox.SelectedIndex = _request.PickComponentIndex >= 0 && _request.PickComponentIndex < _components.Count
                ? _request.PickComponentIndex
                : 0;
        }
        RefreshSelectedComponentInputs();
        _loading = false;
        DrawPreview();
    }

    private void Window_Loaded(object sender, RoutedEventArgs e)
    {
        DrawPreview();
    }

    private void PreviewCanvas_SizeChanged(object sender, SizeChangedEventArgs e)
    {
        DrawPreview();
    }

    private void GlobalInput_Changed(object sender, EventArgs e)
    {
        if (!_loading)
        {
            DrawPreview();
        }
    }

    private void RoadGradeComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_loading)
        {
            return;
        }

        var grade = SelectedValue(RoadGradeComboBox, SubgradeRoadGrade.Expressway);
        _components.Clear();
        foreach (var component in BuildDefaults(grade))
        {
            _components.Add(component);
        }

        ComponentListBox.SelectedIndex = _components.Count > 0 ? 0 : -1;
        RefreshSelectedComponentInputs();
        DrawPreview();
    }

    private void ComponentListBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (!_loading)
        {
            RefreshSelectedComponentInputs();
            DrawPreview();
        }
    }

    private void ComponentInput_Changed(object sender, EventArgs e)
    {
        if (_loading)
        {
            return;
        }

        ApplyInputsToSelectedComponent();
        UpdateSlopeModeInputState();
        ComponentListBox.Items.Refresh();
        DrawPreview();
    }

    private void PavementLayerCheckBox_Changed(object sender, RoutedEventArgs e)
    {
        if (_loading)
        {
            return;
        }

        ApplyInputsToSelectedComponent();
        UpdatePavementLayerInputState();
        DrawPreview();
    }

    private void PreviousComponent_Click(object sender, RoutedEventArgs e)
        => MoveSelectionToward(-1);

    private void NextComponent_Click(object sender, RoutedEventArgs e)
        => MoveSelectionToward(1);

    private void MoveSelectionToward(int direction)
    {
        if (_components.Count == 0)
        {
            return;
        }

        var spatialOrder = SpatialComponentOrder();
        if (spatialOrder.Count == 0)
        {
            return;
        }

        var currentPosition = spatialOrder.IndexOf(ComponentListBox.SelectedIndex);
        var targetPosition = currentPosition < 0
            ? (direction < 0 ? 0 : spatialOrder.Count - 1)
            : Math.Max(0, Math.Min(spatialOrder.Count - 1, currentPosition + Math.Sign(direction)));

        ComponentListBox.SelectedIndex = spatialOrder[targetPosition];
        ComponentListBox.ScrollIntoView(ComponentListBox.SelectedItem);
    }

    private List<int> SpatialComponentOrder()
    {
        var positions = new List<(int Index, double CenterX)>();
        AddSide(SubgradeSide.Left);
        AddSide(SubgradeSide.Right);
        return positions
            .OrderBy(item => item.CenterX)
            .ThenBy(item => item.Index)
            .Select(item => item.Index)
            .ToList();

        void AddSide(SubgradeSide side)
        {
            var x = 0.0;
            var sign = side == SubgradeSide.Left ? -1.0 : 1.0;
            for (var index = 0; index < _components.Count; index++)
            {
                var component = _components[index];
                if (component.Side != side)
                {
                    continue;
                }

                var width = Math.Max(0.05, component.Width);
                var x2 = x + sign * width;
                positions.Add((index, (x + x2) * 0.5));
                x = x2;
            }
        }
    }

    private void AddComponent_Click(object sender, RoutedEventArgs e)
    {
        var side = SelectedValue(AddSideComboBox, SubgradeSide.Right);
        var type = SelectedValue(AddTypeComboBox, SubgradeComponentType.TravelLane);
        var component = new SubgradeComponentDto
        {
            Side = side,
            Type = type,
            Width = 1.0,
            ColorR = DefaultColor(type).R,
            ColorG = DefaultColor(type).G,
            ColorB = DefaultColor(type).B,
        };
        _components.Add(component);
        ComponentListBox.SelectedIndex = _components.Count - 1;
        ComponentListBox.ScrollIntoView(component);
        DrawPreview();
    }

    private void DeleteComponent_Click(object sender, RoutedEventArgs e)
    {
        var index = ComponentListBox.SelectedIndex;
        if (index < 0 || index >= _components.Count)
        {
            return;
        }

        _components.RemoveAt(index);
        if (_components.Count > 0)
        {
            ComponentListBox.SelectedIndex = Math.Min(index, _components.Count - 1);
        }
        RefreshSelectedComponentInputs();
        DrawPreview();
    }

    private void WideningTable_Click(object sender, RoutedEventArgs e)
    {
        var component = SelectedComponent;
        if (component == null)
        {
            return;
        }

        var window = new StationValueTableWindow("变宽数据表", "加宽值", component.WideningTable)
        {
            Owner = this,
        };
        if (window.ShowDialog() == true)
        {
            component.WideningTable = window.Rows;
        }
    }

    private void SlopeTable_Click(object sender, RoutedEventArgs e)
    {
        var component = SelectedComponent;
        if (component == null)
        {
            return;
        }

        var window = new StationValueTableWindow("坡度变化数据表", "坡度", component.VariableSlopeTable)
        {
            Owner = this,
        };
        if (window.ShowDialog() == true)
        {
            component.VariableSlopeTable = window.Rows;
            component.SlopeMode = SubgradeSlopeMode.VariableByStation;
            RefreshSelectedComponentInputs();
            DrawPreview();
        }
    }

    private void PickPavementLayerTemplate_Click(object sender, RoutedEventArgs e)
    {
        var index = ComponentListBox.SelectedIndex;
        if (index < 0 || index >= _components.Count)
        {
            return;
        }

        ApplyInputsToSelectedComponent();
        Response = BuildResponse(false, SubgradeTemplateDialogAction.PickPavementLayerTemplate, index);
        DialogResult = false;
    }

    private void ClearPavementLayerTemplate_Click(object sender, RoutedEventArgs e)
    {
        var component = SelectedComponent;
        if (component == null)
        {
            return;
        }

        component.PavementLayerLinked = false;
        component.PavementLayerHandle = string.Empty;
        component.PavementLayerName = string.Empty;
        component.PavementLayerThickness = 0.0;
        RefreshSelectedComponentInputs();
        DrawPreview();
    }

    private void Ok_Click(object sender, RoutedEventArgs e)
    {
        ApplyInputsToSelectedComponent();
        Response = BuildResponse(true, SubgradeTemplateDialogAction.None, -1);
        DialogResult = true;
    }

    private SubgradeTemplateDialogResponse BuildResponse(
        bool accepted,
        SubgradeTemplateDialogAction action,
        int pickComponentIndex)
        => new()
        {
            Action = action,
            PickComponentIndex = pickComponentIndex,
            Accepted = accepted,
            Handle = _request.Handle,
            InsertionX = _request.InsertionX,
            InsertionY = _request.InsertionY,
            InsertionZ = _request.InsertionZ,
            TemplateName = string.IsNullOrWhiteSpace(TemplateNameBox.Text) ? "默认路基模板" : TemplateNameBox.Text.Trim(),
            DisplayScale = SelectedValue(DisplayScaleComboBox, 10.0),
            RoadGrade = SelectedValue(RoadGradeComboBox, SubgradeRoadGrade.Expressway),
            RoadCenterlineHandle = _request.RoadCenterlineHandle,
            Components = _components.Select(component => component.Clone()).ToList(),
        };

    private void RefreshSelectedComponentInputs()
    {
        _loading = true;
        var component = SelectedComponent;
        var hasComponent = component != null;
        ComponentTypeComboBox.IsEnabled = hasComponent;
        WidthBox.IsEnabled = hasComponent;
        HeightBox.IsEnabled = hasComponent;
        SlopeModeComboBox.IsEnabled = hasComponent;
        FixedSlopeBox.IsEnabled = hasComponent;
        ColorRBox.IsEnabled = hasComponent;
        ColorGBox.IsEnabled = hasComponent;
        ColorBBox.IsEnabled = hasComponent;
        PavementLayerCheckBox.IsEnabled = hasComponent;
        PickPavementLayerTemplateButtonState(hasComponent);

        if (component == null)
        {
            SelectedSideText.Text = "";
            ComponentTypeComboBox.SelectedIndex = -1;
            WidthBox.Text = "";
            HeightBox.Text = "";
            FixedSlopeBox.Text = "";
            ColorRBox.Text = "";
            ColorGBox.Text = "";
            ColorBBox.Text = "";
            PavementLayerCheckBox.IsChecked = false;
            PavementLayerNameBox.Text = "";
            PavementLayerHandleBox.Text = "";
            ColorPreviewBorder.Background = Brushes.Transparent;
            _loading = false;
            UpdateSlopeModeInputState();
            UpdatePavementLayerInputState();
            return;
        }

        SelectedSideText.Text = component.Side == SubgradeSide.Left ? "左侧" : "右侧";
        SelectComboValue(ComponentTypeComboBox, component.Type);
        SelectComboValue(SlopeModeComboBox, component.SlopeMode);
        WidthBox.Text = Format(component.Width);
        HeightBox.Text = Format(component.Height);
        FixedSlopeBox.Text = Format(component.FixedSlope);
        ColorRBox.Text = component.ColorR.ToString(CultureInfo.InvariantCulture);
        ColorGBox.Text = component.ColorG.ToString(CultureInfo.InvariantCulture);
        ColorBBox.Text = component.ColorB.ToString(CultureInfo.InvariantCulture);
        PavementLayerCheckBox.IsChecked = component.PavementLayerLinked;
        PavementLayerNameBox.Text = component.PavementLayerName;
        PavementLayerHandleBox.Text = component.PavementLayerHandle;
        ColorPreviewBorder.Background = BrushFor(component);
        _loading = false;
        UpdateSlopeModeInputState();
        UpdatePavementLayerInputState();
    }

    private void ApplyInputsToSelectedComponent()
    {
        var component = SelectedComponent;
        if (component == null)
        {
            return;
        }

        component.Type = SelectedValue(ComponentTypeComboBox, component.Type);
        component.Width = Math.Max(0.0, ReadDouble(WidthBox.Text, component.Width));
        component.Height = ReadDouble(HeightBox.Text, component.Height);
        component.SlopeMode = SelectedValue(SlopeModeComboBox, component.SlopeMode);
        component.FixedSlope = component.SlopeMode == SubgradeSlopeMode.Fixed
            ? ReadDouble(FixedSlopeBox.Text, component.FixedSlope)
            : 0.0;
        component.ColorR = ClampColor(ReadInt(ColorRBox.Text, component.ColorR));
        component.ColorG = ClampColor(ReadInt(ColorGBox.Text, component.ColorG));
        component.ColorB = ClampColor(ReadInt(ColorBBox.Text, component.ColorB));
        component.PavementLayerLinked = PavementLayerCheckBox.IsChecked == true;
        component.PavementLayerHandle = component.PavementLayerLinked ? PavementLayerHandleBox.Text.Trim() : string.Empty;
        component.PavementLayerName = component.PavementLayerLinked ? PavementLayerNameBox.Text.Trim() : string.Empty;
        ColorPreviewBorder.Background = BrushFor(component);
    }

    private void UpdateSlopeModeInputState()
    {
        var component = SelectedComponent;
        var fixedSlopeEnabled = component != null
            && SelectedValue(SlopeModeComboBox, component.SlopeMode) == SubgradeSlopeMode.Fixed;
        FixedSlopeBox.IsEnabled = fixedSlopeEnabled;
        FixedSlopeBox.Opacity = fixedSlopeEnabled ? 1.0 : 0.55;
    }

    private void UpdatePavementLayerInputState()
    {
        var hasComponent = SelectedComponent != null;
        PavementLayerNameBox.IsEnabled = hasComponent;
        PavementLayerHandleBox.IsEnabled = hasComponent;
        PickPavementLayerTemplateButtonState(hasComponent);
    }

    private void PickPavementLayerTemplateButtonState(bool enabled)
    {
        var pickButton = FindName("PickPavementLayerTemplateButton") as Button;
        var clearButton = FindName("ClearPavementLayerTemplateButton") as Button;
        if (pickButton != null)
        {
            pickButton.IsEnabled = enabled;
        }
        if (clearButton != null)
        {
            clearButton.IsEnabled = enabled;
        }
    }

    private void DrawPreview()
    {
        if (PreviewCanvas == null)
        {
            return;
        }

        PreviewCanvas.Children.Clear();
        var width = PreviewCanvas.ActualWidth > 10 ? PreviewCanvas.ActualWidth : 720.0;
        var height = PreviewCanvas.ActualHeight > 10 ? PreviewCanvas.ActualHeight : 420.0;
        var bounds = CalculatePreviewBounds();
        var modelWidth = Math.Max(1.0, bounds.MaxX - bounds.MinX);
        var modelHeight = Math.Max(1.0, bounds.MaxY - bounds.MinY);
        var factor = Math.Max(1.0, Math.Min((width - 60.0) / modelWidth, (height - 70.0) / modelHeight));
        var originX = width * 0.5;
        var originY = height * 0.55;

        double MapX(double x) => originX + x * factor;

        var centerLine = new Line
        {
            X1 = MapX(0.0),
            X2 = MapX(0.0),
            Y1 = 24,
            Y2 = height - 28,
            Stroke = Brushes.Firebrick,
            StrokeThickness = 1.3,
            StrokeDashArray = new DoubleCollection { 5, 4 },
        };
        PreviewCanvas.Children.Add(centerLine);
        AddCanvasText("中线", MapX(0.0) + 6, 24, Brushes.Firebrick, 13);

        DrawSide(SubgradeSide.Left, factor, originX, originY);
        DrawSide(SubgradeSide.Right, factor, originX, originY);
        AddCanvasText(TemplateNameBox.Text, 18, 12, Brushes.Black, 14);

        void DrawSide(SubgradeSide side, double scale, double ox, double oy)
        {
            var x = 0.0;
            var y = 0.0;
            var sign = side == SubgradeSide.Left ? -1.0 : 1.0;
            for (var index = 0; index < _components.Count; index++)
            {
                var component = _components[index];
                if (component.Side != side)
                {
                    continue;
                }

                var w = Math.Max(0.05, component.Width);
                var topStartY = y + component.Height;
                var topEndY = topStartY + DisplaySlope(component) * w;
                var x2 = x + sign * w;
                var bottomOffset = 0.35;
                var points = new PointCollection
                {
                    new(ox + x * scale, oy - topStartY * scale),
                    new(ox + x2 * scale, oy - topEndY * scale),
                    new(ox + x2 * scale, oy - (topEndY - bottomOffset) * scale),
                    new(ox + x * scale, oy - (topStartY - bottomOffset) * scale),
                };
                var polygon = new Polygon
                {
                    Points = points,
                    Fill = BrushFor(component),
                    Stroke = index == ComponentListBox.SelectedIndex ? Brushes.OrangeRed : Brushes.White,
                    StrokeThickness = index == ComponentListBox.SelectedIndex ? 2.2 : 1.0,
                    Tag = index,
                    Cursor = Cursors.Hand,
                };
                polygon.MouseLeftButtonDown += PreviewComponent_MouseLeftButtonDown;
                PreviewCanvas.Children.Add(polygon);

                var labelX = Math.Min(ox + x * scale, ox + x2 * scale);
                var labelWidth = Math.Max(24.0, Math.Abs((x2 - x) * scale));
                var labelY = oy - (Math.Max(topStartY, topEndY) + 0.55) * scale;
                AddCanvasText(
                    SubgradeTemplateLabels.ComponentTypeLabel(component.Type),
                    labelX,
                    labelY,
                    Brushes.Black,
                    11,
                    labelWidth,
                    TextAlignment.Center);
                var widthY = oy - (Math.Min(topStartY, topEndY) - 0.06) * scale;
                AddCanvasText(WidthText(component), labelX, widthY, Brushes.Black, 10, labelWidth, TextAlignment.Center);
                AddCanvasText(SlopeText(component), labelX, widthY + 12.0, Brushes.Black, 10, labelWidth, TextAlignment.Center);
                PreviewCanvas.Children.Add(CreatePreviewComponentHitTarget(points, index));
                x = x2;
                y = topEndY;
            }
        }
    }

    private Polygon CreatePreviewComponentHitTarget(PointCollection points, int index)
    {
        var hitTarget = new Polygon
        {
            Points = new PointCollection(points),
            Fill = Brushes.Transparent,
            Stroke = Brushes.Transparent,
            StrokeThickness = 8.0,
            Tag = index,
            Cursor = Cursors.Hand,
        };
        hitTarget.MouseLeftButtonDown += PreviewComponent_MouseLeftButtonDown;
        return hitTarget;
    }

    private void PreviewComponent_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (sender is FrameworkElement { Tag: int index }
            && index >= 0
            && index < _components.Count)
        {
            ComponentListBox.SelectedIndex = index;
            ComponentListBox.ScrollIntoView(ComponentListBox.SelectedItem);
        }
    }

    private PreviewBounds CalculatePreviewBounds()
    {
        var bounds = new PreviewBounds(-2.0, 2.0, -2.0, 2.0);
        Walk(SubgradeSide.Left);
        Walk(SubgradeSide.Right);
        return bounds;

        void Walk(SubgradeSide side)
        {
            var x = 0.0;
            var y = 0.0;
            var sign = side == SubgradeSide.Left ? -1.0 : 1.0;
            foreach (var component in _components.Where(item => item.Side == side))
            {
                var w = Math.Max(0.05, component.Width);
                var topStartY = y + component.Height;
                var topEndY = topStartY + DisplaySlope(component) * w;
                var x2 = x + sign * w;
                bounds.Include(x, topStartY);
                bounds.Include(x2, topEndY);
                bounds.Include(x, topStartY - 0.5);
                bounds.Include(x2, topEndY - 0.5);
                x = x2;
                y = topEndY;
            }
        }
    }

    private void AddCanvasText(string text, double x, double y, Brush brush, double size)
        => AddCanvasText(text, x, y, brush, size, double.NaN, TextAlignment.Left);

    private void AddCanvasText(
        string text,
        double x,
        double y,
        Brush brush,
        double size,
        double maxWidth,
        TextAlignment alignment)
    {
        if (string.IsNullOrWhiteSpace(text))
        {
            return;
        }

        var block = new TextBlock
        {
            Text = text,
            Foreground = brush,
            FontSize = size,
            TextAlignment = alignment,
            TextTrimming = TextTrimming.CharacterEllipsis,
            IsHitTestVisible = false,
        };
        if (!double.IsNaN(maxWidth) && maxWidth > 0)
        {
            block.Width = maxWidth;
            block.MaxWidth = maxWidth;
        }
        Canvas.SetLeft(block, x);
        Canvas.SetTop(block, y);
        PreviewCanvas.Children.Add(block);
    }

    private static double DisplaySlope(SubgradeComponentDto component)
    {
        if (component.SlopeMode == SubgradeSlopeMode.Fixed)
        {
            return component.FixedSlope;
        }

        return component.VariableSlopeTable.Count > 0
            ? component.VariableSlopeTable[0].Value
            : 0.0;
    }

    private static string WidthText(SubgradeComponentDto component)
        => $"宽 {Format(component.Width)}";

    private static string SlopeText(SubgradeComponentDto component)
        => $"坡 {Format(DisplaySlope(component))}";

    private static List<SubgradeComponentDto> BuildDefaults(SubgradeRoadGrade grade)
    {
        return grade switch
        {
            SubgradeRoadGrade.Expressway => Symmetric(new[]
            {
                (SubgradeComponentType.Median, 1.5),
                (SubgradeComponentType.CurbStrip, 0.75),
                (SubgradeComponentType.TravelLane, 7.5),
                (SubgradeComponentType.HardShoulder, 3.0),
                (SubgradeComponentType.EarthShoulder, 0.75),
            }),
            SubgradeRoadGrade.FirstClass => Symmetric(new[]
            {
                (SubgradeComponentType.Median, 1.0),
                (SubgradeComponentType.CurbStrip, 0.5),
                (SubgradeComponentType.TravelLane, 3.75),
                (SubgradeComponentType.TravelLane, 3.75),
                (SubgradeComponentType.HardShoulder, 2.5),
                (SubgradeComponentType.EarthShoulder, 0.75),
            }),
            SubgradeRoadGrade.SecondClass => Symmetric(new[]
            {
                (SubgradeComponentType.TravelLane, 3.75),
                (SubgradeComponentType.HardShoulder, 1.5),
                (SubgradeComponentType.EarthShoulder, 0.75),
            }),
            SubgradeRoadGrade.ThirdClass => Symmetric(new[]
            {
                (SubgradeComponentType.TravelLane, 3.5),
                (SubgradeComponentType.HardShoulder, 0.75),
                (SubgradeComponentType.EarthShoulder, 0.75),
            }),
            SubgradeRoadGrade.FourthClass => Symmetric(new[]
            {
                (SubgradeComponentType.TravelLane, 3.0),
                (SubgradeComponentType.HardShoulder, 0.25),
                (SubgradeComponentType.EarthShoulder, 0.5),
            }),
            SubgradeRoadGrade.UrbanExpressway => Symmetric(new[]
            {
                (SubgradeComponentType.Median, 1.0),
                (SubgradeComponentType.TravelLane, 7.5),
                (SubgradeComponentType.SideMedian, 1.0),
                (SubgradeComponentType.BikeLane, 3.0),
                (SubgradeComponentType.Sidewalk, 4.0),
            }),
            SubgradeRoadGrade.UrbanArterial => Symmetric(new[]
            {
                (SubgradeComponentType.Median, 1.5),
                (SubgradeComponentType.CurbStrip, 0.5),
                (SubgradeComponentType.TravelLane, 3.5),
                (SubgradeComponentType.TravelLane, 3.5),
                (SubgradeComponentType.SideMedian, 1.5),
                (SubgradeComponentType.BikeLane, 2.5),
                (SubgradeComponentType.Sidewalk, 3.0),
            }),
            SubgradeRoadGrade.UrbanSubArterial => Symmetric(new[]
            {
                (SubgradeComponentType.CurbStrip, 0.25),
                (SubgradeComponentType.TravelLane, 3.5),
                (SubgradeComponentType.TravelLane, 3.5),
                (SubgradeComponentType.BikeLane, 2.5),
                (SubgradeComponentType.Sidewalk, 3.0),
            }),
            SubgradeRoadGrade.UrbanBranch => Symmetric(new[]
            {
                (SubgradeComponentType.TravelLane, 3.25),
                (SubgradeComponentType.CurbStrip, 0.25),
                (SubgradeComponentType.Sidewalk, 2.0),
            }),
            _ => new List<SubgradeComponentDto>(),
        };
    }

    private static List<SubgradeComponentDto> Symmetric((SubgradeComponentType Type, double Width)[] items)
    {
        var result = new List<SubgradeComponentDto>();
        foreach (var side in new[] { SubgradeSide.Left, SubgradeSide.Right })
        {
            foreach (var item in items)
            {
                var color = DefaultColor(item.Type);
                result.Add(new SubgradeComponentDto
                {
                    Side = side,
                    Type = item.Type,
                    Width = item.Width,
                    ColorR = color.R,
                    ColorG = color.G,
                    ColorB = color.B,
                });
            }
        }

        return result;
    }

    private static Rgb DefaultColor(SubgradeComponentType type)
        => type switch
        {
            SubgradeComponentType.Median or SubgradeComponentType.SideMedian => new Rgb(0, 120, 0),
            SubgradeComponentType.TravelLane or SubgradeComponentType.HardShoulder or SubgradeComponentType.BikeLane or SubgradeComponentType.CurbStrip => new Rgb(0, 90, 180),
            _ => new Rgb(120, 120, 120),
        };

    private static SolidColorBrush BrushFor(SubgradeComponentDto component)
        => new(Color.FromRgb(
            ToByte(component.ColorR),
            ToByte(component.ColorG),
            ToByte(component.ColorB)));

    private static byte ToByte(int value)
        => (byte)ClampColor(value);

    private static int ClampColor(int value)
        => Math.Max(0, Math.Min(255, value));

    private static double ReadDouble(string text, double fallback)
        => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var value)
            || double.TryParse(text, NumberStyles.Float, CultureInfo.CurrentCulture, out value)
            ? value
            : fallback;

    private static int ReadInt(string text, int fallback)
        => int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var value)
            || int.TryParse(text, NumberStyles.Integer, CultureInfo.CurrentCulture, out value)
            ? value
            : fallback;

    private static string Format(double value)
        => value.ToString("0.###", CultureInfo.InvariantCulture);

    private static void SelectComboValue<T>(ComboBox comboBox, T value)
    {
        foreach (var item in comboBox.Items.OfType<ComboOption<T>>())
        {
            if (EqualityComparer<T>.Default.Equals(item.Value, value))
            {
                comboBox.SelectedItem = item;
                return;
            }
        }
    }

    private static T SelectedValue<T>(ComboBox comboBox, T fallback)
        => comboBox.SelectedItem is ComboOption<T> option ? option.Value : fallback;

    private sealed class ComboOption<T>
    {
        public ComboOption(string label, T value)
        {
            Label = label;
            Value = value;
        }

        public string Label { get; }
        public T Value { get; }
    }

    private readonly struct Rgb
    {
        public Rgb(int r, int g, int b)
        {
            R = r;
            G = g;
            B = b;
        }

        public int R { get; }
        public int G { get; }
        public int B { get; }
    }

    private sealed class PreviewBounds
    {
        public PreviewBounds(double minX, double maxX, double minY, double maxY)
        {
            MinX = minX;
            MaxX = maxX;
            MinY = minY;
            MaxY = maxY;
        }

        public double MinX { get; private set; }
        public double MaxX { get; private set; }
        public double MinY { get; private set; }
        public double MaxY { get; private set; }

        public void Include(double x, double y)
        {
            MinX = Math.Min(MinX, x);
            MaxX = Math.Max(MaxX, x);
            MinY = Math.Min(MinY, y);
            MaxY = Math.Max(MaxY, y);
        }
    }
}
