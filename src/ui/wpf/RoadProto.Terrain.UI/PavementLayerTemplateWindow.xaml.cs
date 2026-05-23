using System;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;
using Microsoft.Win32;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class PavementLayerTemplateWindow : Window
{
    private readonly PavementLayerTemplateDialogRequest _request;
    private readonly ObservableCollection<PavementLayerTemplateLayerDto> _layers = new();
    private readonly System.Collections.Generic.List<LayerControls> _layerControls = new();
    private readonly System.Collections.Generic.List<ComboOption<PavementLayerType>> _typeOptions;
    private readonly System.Collections.Generic.List<ComboOption<PavementLayerTemplateDisplayMode>> _displayModeOptions;
    private int _selectedLayerIndex;
    private bool _loading = true;
    private bool _applyingLayerInputs;
    private bool _updatingCurrentLayerBox;
    private bool _panning;
    private Point _lastPanPoint;
    private double _previewZoom = 1.0;
    private Vector _previewPan;

    public PavementLayerTemplateWindow(PavementLayerTemplateDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        _typeOptions = Enum.GetValues(typeof(PavementLayerType))
            .Cast<PavementLayerType>()
            .Select(value => new ComboOption<PavementLayerType>(PavementLayerTemplateLabels.LayerTypeLabel(value), value))
            .ToList();
        _displayModeOptions = Enum.GetValues(typeof(PavementLayerTemplateDisplayMode))
            .Cast<PavementLayerTemplateDisplayMode>()
            .Select(value => new ComboOption<PavementLayerTemplateDisplayMode>(DisplayModeLabel(value), value))
            .ToList();
        Response = null;
        LoadRequest();
    }

    public event EventHandler<PavementLayerTemplateDialogResponse>? ApplyRequested;

    public PavementLayerTemplateDialogResponse? Response { get; private set; }

    private void LoadRequest()
    {
        _loading = true;
        TemplateNameBox.Text = string.IsNullOrWhiteSpace(_request.TemplateName) ? "路面结构层模板" : _request.TemplateName;
        DisplayScaleBox.Text = Format(_request.DisplayScale <= 0 ? 10.0 : _request.DisplayScale);
        PreviewWidthBox.Text = Format(_request.PreviewWidth <= 0 ? 3.75 : _request.PreviewWidth);
        DisplayModeBox.ItemsSource = _displayModeOptions;
        DisplayModeBox.DisplayMemberPath = nameof(ComboOption<PavementLayerTemplateDisplayMode>.Label);
        SelectComboValue(DisplayModeBox, _request.DisplayMode);

        _layers.Clear();
        var source = _request.Layers.Count > 0 ? _request.Layers : PavementLayerTemplateLabels.DefaultLayers();
        foreach (var layer in source)
        {
            var copy = layer.Clone();
            EnsureLayerColor(copy, _layers.Count);
            copy.HatchPattern = PavementLayerTemplateLabels.NormalizeHatchPattern(copy.HatchPattern);
            _layers.Add(copy);
        }
        _selectedLayerIndex = 0;
        LayerCountBox.Text = _layers.Count.ToString(CultureInfo.InvariantCulture);
        UpdateCurrentLayerBox();
        RefreshLayerGroups();
        _loading = false;
        DrawPreview();
    }

    private void Window_Loaded(object sender, RoutedEventArgs e)
        => DrawPreview();

    private void PreviewCanvas_SizeChanged(object sender, SizeChangedEventArgs e)
        => DrawPreview();

    private void GlobalInput_Changed(object sender, EventArgs e)
    {
        if (!_loading)
        {
            DrawPreview();
        }
    }

    private void LayerCountBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (_loading)
        {
            return;
        }

        if (!int.TryParse(LayerCountBox.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var count))
        {
            return;
        }

        ApplyLayerInputs();
        count = Math.Max(1, Math.Min(100, count));
        while (_layers.Count < count)
        {
            var type = (PavementLayerType)(Math.Min(_layers.Count, _typeOptions.Count - 1));
            var color = PavementLayerTemplateLabels.DefaultColorForLayerIndex(_layers.Count);
            _layers.Add(new PavementLayerTemplateLayerDto
            {
                Type = type,
                Name = PavementLayerTemplateLabels.LayerTypeLabel(type),
                ColorR = color.R,
                ColorG = color.G,
                ColorB = color.B,
                HatchPattern = "SOLID",
            });
        }
        while (_layers.Count > count)
        {
            _layers.RemoveAt(_layers.Count - 1);
        }

        _selectedLayerIndex = Math.Max(0, Math.Min(_selectedLayerIndex, _layers.Count - 1));
        UpdateCurrentLayerBox();
        RefreshLayerGroups();
        DrawPreview();
    }

    private void CurrentLayerBox_TextChanged(object sender, TextChangedEventArgs e)
    {
        if (_loading || _updatingCurrentLayerBox)
        {
            return;
        }

        if (!int.TryParse(CurrentLayerBox.Text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var layerNumber))
        {
            return;
        }

        SelectLayer(layerNumber - 1);
    }

    private void PreviousLayerButton_Click(object sender, RoutedEventArgs e)
        => SelectLayer(_selectedLayerIndex - 1);

    private void NextLayerButton_Click(object sender, RoutedEventArgs e)
        => SelectLayer(_selectedLayerIndex + 1);

    private void SelectLayer(int index)
    {
        if (_layers.Count == 0)
        {
            _selectedLayerIndex = 0;
            return;
        }

        ApplyLayerInputs();
        var clamped = Math.Max(0, Math.Min(index, _layers.Count - 1));
        if (clamped == _selectedLayerIndex && _layerControls.Count > 0)
        {
            UpdateCurrentLayerBox();
            return;
        }

        _selectedLayerIndex = clamped;
        UpdateCurrentLayerBox();
        RefreshLayerGroups();
        DrawPreview();
    }

    private void UpdateCurrentLayerBox()
    {
        _updatingCurrentLayerBox = true;
        try
        {
            CurrentLayerBox.Text = _layers.Count == 0
                ? "0"
                : (_selectedLayerIndex + 1).ToString(CultureInfo.InvariantCulture);
        }
        finally
        {
            _updatingCurrentLayerBox = false;
        }
    }

    private void LayerInput_Changed(object sender, EventArgs e)
    {
        if (_loading || _applyingLayerInputs)
        {
            return;
        }

        ApplyLayerInputs();
        RefreshLayerFieldVisibility();
        DrawPreview();
    }

    private void RefreshLayerGroups()
    {
        _loading = true;
        _layerControls.Clear();
        LayersPanel.Children.Clear();
        _selectedLayerIndex = Math.Max(0, Math.Min(_selectedLayerIndex, Math.Max(0, _layers.Count - 1)));
        if (_layers.Count > 0)
        {
            LayersPanel.Children.Add(CreateLayerGroup(_selectedLayerIndex, _layers[_selectedLayerIndex]));
        }
        _loading = false;
        RefreshLayerFieldVisibility();
    }

    private Border CreateLayerGroup(int index, PavementLayerTemplateLayerDto layer)
    {
        EnsureLayerColor(layer, index);
        var border = new Border
        {
            BorderBrush = new SolidColorBrush(Color.FromRgb(190, 198, 210)),
            BorderThickness = new Thickness(1),
            Padding = new Thickness(10),
            Margin = new Thickness(0, 0, 0, 10),
            Background = Brushes.White,
        };

        var panel = new StackPanel();
        border.Child = panel;
        panel.Children.Add(new TextBlock
        {
            Text = $"结构层 {index + 1}",
            FontWeight = FontWeights.SemiBold,
            Margin = new Thickness(0, 0, 0, 8),
        });

        var typeBox = new ComboBox
        {
            Height = 26,
            ItemsSource = _typeOptions,
            DisplayMemberPath = nameof(ComboOption<PavementLayerType>.Label),
            IsEditable = false,
        };
        SelectComboValue(typeBox, layer.Type);
        var nameBox = new TextBox { Height = 26, Text = layer.Name };
        var colorRBox = new TextBox { Height = 26, Width = 45, Text = ClampColor(layer.ColorR).ToString(CultureInfo.InvariantCulture) };
        var colorGBox = new TextBox { Height = 26, Width = 45, Text = ClampColor(layer.ColorG).ToString(CultureInfo.InvariantCulture) };
        var colorBBox = new TextBox { Height = 26, Width = 45, Text = ClampColor(layer.ColorB).ToString(CultureInfo.InvariantCulture) };
        var colorPreview = new Border
        {
            Width = 34,
            Height = 24,
            Margin = new Thickness(8, 0, 0, 0),
            BorderBrush = new SolidColorBrush(Color.FromRgb(145, 154, 168)),
            BorderThickness = new Thickness(1),
            Background = new SolidColorBrush(LayerColor(layer, index)),
            Cursor = Cursors.Hand,
            ToolTip = "选择索引颜色",
        };
        colorPreview.MouseLeftButtonDown += ColorPreview_MouseLeftButtonDown;
        var hatchPatternBox = new ComboBox
        {
            Height = 26,
            ItemsSource = PavementLayerTemplateLabels.HatchPatternOptions,
            IsEditable = false,
            SelectedItem = PavementLayerTemplateLabels.NormalizeHatchPattern(layer.HatchPattern),
        };
        var uniformBox = new CheckBox { Content = "内外厚度是否一致", IsChecked = layer.UniformThickness, Margin = new Thickness(0, 4, 0, 4) };
        var thicknessBox = new TextBox { Height = 26, Text = Format(layer.Thickness) };
        var innerThicknessBox = new TextBox { Height = 26, Text = Format(layer.InnerThickness) };
        var outerThicknessBox = new TextBox { Height = 26, Text = Format(layer.OuterThickness) };
        var uniformWideningBox = new CheckBox { Content = "内外加宽是否一致", IsChecked = NearlyEqual(layer.InnerWidening, layer.OuterWidening), Margin = new Thickness(0, 4, 0, 4) };
        var wideningBox = new TextBox { Height = 26, Text = Format(layer.InnerWidening) };
        var innerWideningBox = new TextBox { Height = 26, Text = Format(layer.InnerWidening) };
        var outerWideningBox = new TextBox { Height = 26, Text = Format(layer.OuterWidening) };
        var uniformSlopeBox = new CheckBox { Content = "内外坡度是否一致", IsChecked = NearlyEqual(layer.InnerSlope, layer.OuterSlope), Margin = new Thickness(0, 4, 0, 4) };
        var slopeBox = new TextBox { Height = 26, Text = Format(layer.InnerSlope) };
        var innerSlopeBox = new TextBox { Height = 26, Text = Format(layer.InnerSlope) };
        var outerSlopeBox = new TextBox { Height = 26, Text = Format(layer.OuterSlope) };

        panel.Children.Add(FieldRow("类型", typeBox));
        panel.Children.Add(FieldRow("名称", nameBox));
        panel.Children.Add(ColorRow("颜色 RGB", colorRBox, colorGBox, colorBBox, colorPreview));
        panel.Children.Add(FieldRow("填充类型", hatchPatternBox));
        panel.Children.Add(uniformBox);
        var thicknessRow = FieldRow("厚度", thicknessBox);
        var innerThicknessRow = FieldRow("内侧厚度", innerThicknessBox);
        var outerThicknessRow = FieldRow("外侧厚度", outerThicknessBox);
        panel.Children.Add(thicknessRow);
        panel.Children.Add(innerThicknessRow);
        panel.Children.Add(outerThicknessRow);
        panel.Children.Add(uniformWideningBox);
        var wideningRow = FieldRow("加宽", wideningBox);
        var innerWideningRow = FieldRow("内侧加宽", innerWideningBox);
        var outerWideningRow = FieldRow("外侧加宽", outerWideningBox);
        panel.Children.Add(wideningRow);
        panel.Children.Add(innerWideningRow);
        panel.Children.Add(outerWideningRow);
        panel.Children.Add(uniformSlopeBox);
        var slopeRow = FieldRow("坡度", slopeBox);
        var innerSlopeRow = FieldRow("内侧坡度", innerSlopeBox);
        var outerSlopeRow = FieldRow("外侧坡度", outerSlopeBox);
        panel.Children.Add(slopeRow);
        panel.Children.Add(innerSlopeRow);
        panel.Children.Add(outerSlopeRow);

        typeBox.SelectionChanged += LayerInput_Changed;
        nameBox.TextChanged += LayerInput_Changed;
        colorRBox.TextChanged += LayerInput_Changed;
        colorGBox.TextChanged += LayerInput_Changed;
        colorBBox.TextChanged += LayerInput_Changed;
        hatchPatternBox.SelectionChanged += LayerInput_Changed;
        uniformBox.Checked += LayerInput_Changed;
        uniformBox.Unchecked += LayerInput_Changed;
        thicknessBox.TextChanged += LayerInput_Changed;
        innerThicknessBox.TextChanged += LayerInput_Changed;
        outerThicknessBox.TextChanged += LayerInput_Changed;
        uniformWideningBox.Checked += LayerInput_Changed;
        uniformWideningBox.Unchecked += LayerInput_Changed;
        wideningBox.TextChanged += LayerInput_Changed;
        innerWideningBox.TextChanged += LayerInput_Changed;
        outerWideningBox.TextChanged += LayerInput_Changed;
        uniformSlopeBox.Checked += LayerInput_Changed;
        uniformSlopeBox.Unchecked += LayerInput_Changed;
        slopeBox.TextChanged += LayerInput_Changed;
        innerSlopeBox.TextChanged += LayerInput_Changed;
        outerSlopeBox.TextChanged += LayerInput_Changed;

        _layerControls.Add(new LayerControls(
            index,
            typeBox,
            nameBox,
            colorRBox,
            colorGBox,
            colorBBox,
            colorPreview,
            hatchPatternBox,
            uniformBox,
            thicknessBox,
            innerThicknessBox,
            outerThicknessBox,
            uniformWideningBox,
            wideningBox,
            innerWideningBox,
            outerWideningBox,
            uniformSlopeBox,
            slopeBox,
            innerSlopeBox,
            outerSlopeBox,
            thicknessRow,
            innerThicknessRow,
            outerThicknessRow,
            wideningRow,
            innerWideningRow,
            outerWideningRow,
            slopeRow,
            innerSlopeRow,
            outerSlopeRow));
        return border;
    }

    private static FrameworkElement FieldRow(string label, FrameworkElement input)
    {
        var grid = new Grid { Margin = new Thickness(0, 0, 0, 6) };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(108) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center });
        Grid.SetColumn(input, 1);
        grid.Children.Add(input);
        return grid;
    }

    private static FrameworkElement ColorRow(
        string label,
        TextBox redBox,
        TextBox greenBox,
        TextBox blueBox,
        Border preview)
    {
        var panel = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            HorizontalAlignment = HorizontalAlignment.Left,
        };
        panel.Children.Add(redBox);
        panel.Children.Add(new TextBlock { Text = ",", Margin = new Thickness(4, 0, 4, 0), VerticalAlignment = VerticalAlignment.Center });
        panel.Children.Add(greenBox);
        panel.Children.Add(new TextBlock { Text = ",", Margin = new Thickness(4, 0, 4, 0), VerticalAlignment = VerticalAlignment.Center });
        panel.Children.Add(blueBox);
        panel.Children.Add(preview);
        return FieldRow(label, panel);
    }

    private void RefreshLayerFieldVisibility()
    {
        for (var i = 0; i < _layerControls.Count; i++)
        {
            var controls = _layerControls[i];
            var uniformThickness = controls.UniformBox.IsChecked == true;
            controls.ThicknessRow.Visibility = uniformThickness ? Visibility.Visible : Visibility.Collapsed;
            controls.InnerThicknessRow.Visibility = uniformThickness ? Visibility.Collapsed : Visibility.Visible;
            controls.OuterThicknessRow.Visibility = uniformThickness ? Visibility.Collapsed : Visibility.Visible;

            var uniformWidening = controls.UniformWideningBox.IsChecked == true;
            controls.WideningRow.Visibility = uniformWidening ? Visibility.Visible : Visibility.Collapsed;
            controls.InnerWideningRow.Visibility = uniformWidening ? Visibility.Collapsed : Visibility.Visible;
            controls.OuterWideningRow.Visibility = uniformWidening ? Visibility.Collapsed : Visibility.Visible;

            var uniformSlope = controls.UniformSlopeBox.IsChecked == true;
            controls.SlopeRow.Visibility = uniformSlope ? Visibility.Visible : Visibility.Collapsed;
            controls.InnerSlopeRow.Visibility = uniformSlope ? Visibility.Collapsed : Visibility.Visible;
            controls.OuterSlopeRow.Visibility = uniformSlope ? Visibility.Collapsed : Visibility.Visible;
        }
    }

    private void ApplyLayerInputs()
    {
        if (_applyingLayerInputs)
        {
            return;
        }

        _applyingLayerInputs = true;
        try
        {
            for (var i = 0; i < _layerControls.Count; i++)
            {
                var controls = _layerControls[i];
                if (controls.LayerIndex < 0 || controls.LayerIndex >= _layers.Count)
                {
                    continue;
                }

                var layer = _layers[controls.LayerIndex];
                layer.Type = SelectedValue(controls.TypeBox, layer.Type);
                layer.Name = string.IsNullOrWhiteSpace(controls.NameBox.Text)
                    ? PavementLayerTemplateLabels.LayerTypeLabel(layer.Type)
                    : controls.NameBox.Text.Trim();
                layer.ColorR = ReadColorChannel(controls.ColorRBox.Text, layer.ColorR);
                layer.ColorG = ReadColorChannel(controls.ColorGBox.Text, layer.ColorG);
                layer.ColorB = ReadColorChannel(controls.ColorBBox.Text, layer.ColorB);
                SetTextBoxValue(controls.ColorRBox, layer.ColorR);
                SetTextBoxValue(controls.ColorGBox, layer.ColorG);
                SetTextBoxValue(controls.ColorBBox, layer.ColorB);
                controls.ColorPreview.Background = new SolidColorBrush(LayerColor(layer, controls.LayerIndex));
                layer.HatchPattern = controls.HatchPatternBox.SelectedItem is string hatchPattern
                    ? PavementLayerTemplateLabels.NormalizeHatchPattern(hatchPattern)
                    : "SOLID";
                layer.UniformThickness = controls.UniformBox.IsChecked == true;
                layer.Thickness = Math.Max(0.001, ReadDouble(controls.ThicknessBox.Text, layer.Thickness));
                layer.InnerThickness = Math.Max(0.001, ReadDouble(controls.InnerThicknessBox.Text, layer.InnerThickness));
                layer.OuterThickness = Math.Max(0.001, ReadDouble(controls.OuterThicknessBox.Text, layer.OuterThickness));
                if (layer.UniformThickness)
                {
                    layer.InnerThickness = layer.Thickness;
                    layer.OuterThickness = layer.Thickness;
                    SetTextBoxValue(controls.InnerThicknessBox, layer.InnerThickness);
                    SetTextBoxValue(controls.OuterThicknessBox, layer.OuterThickness);
                }
                else
                {
                    layer.Thickness = Math.Max(layer.InnerThickness, layer.OuterThickness);
                    SetTextBoxValue(controls.ThicknessBox, layer.Thickness);
                }

                if (controls.UniformWideningBox.IsChecked == true)
                {
                    var widening = ReadDouble(controls.WideningBox.Text, layer.InnerWidening);
                    layer.InnerWidening = widening;
                    layer.OuterWidening = widening;
                    SetTextBoxValue(controls.InnerWideningBox, widening);
                    SetTextBoxValue(controls.OuterWideningBox, widening);
                }
                else
                {
                    layer.InnerWidening = ReadDouble(controls.InnerWideningBox.Text, layer.InnerWidening);
                    layer.OuterWidening = ReadDouble(controls.OuterWideningBox.Text, layer.OuterWidening);
                    SetTextBoxValue(controls.WideningBox, layer.InnerWidening);
                }

                if (controls.UniformSlopeBox.IsChecked == true)
                {
                    var slope = ReadSlope(controls.SlopeBox.Text, layer.InnerSlope);
                    layer.InnerSlope = slope;
                    layer.OuterSlope = slope;
                    SetTextBoxValue(controls.InnerSlopeBox, slope);
                    SetTextBoxValue(controls.OuterSlopeBox, slope);
                }
                else
                {
                    layer.InnerSlope = ReadSlope(controls.InnerSlopeBox.Text, layer.InnerSlope);
                    layer.OuterSlope = ReadSlope(controls.OuterSlopeBox.Text, layer.OuterSlope);
                    SetTextBoxValue(controls.SlopeBox, layer.InnerSlope);
                }
            }
        }
        finally
        {
            _applyingLayerInputs = false;
        }
    }

    private void DrawPreview()
    {
        if (PreviewCanvas.ActualWidth <= 1 || PreviewCanvas.ActualHeight <= 1)
        {
            return;
        }

        ApplyLayerInputs();
        PreviewCanvas.Children.Clear();
        var previewWidth = Math.Max(0.1, ReadDouble(PreviewWidthBox.Text, 3.75));
        var geometry = BuildPreviewGeometry(previewWidth);
        if (geometry.Count == 0)
        {
            return;
        }

        var transform = CreatePreviewTransform(geometry, _previewZoom, _previewPan);
        if (transform == null)
        {
            return;
        }

        DrawGrid();
        var displayMode = CurrentDisplayMode();
        for (var i = 0; i < geometry.Count; i++)
        {
            DrawLayerFill(geometry[i], i, displayMode, transform.WorldToScreen);
        }
        for (var i = 0; i < geometry.Count; i++)
        {
            DrawHatchPattern(geometry[i], i, displayMode, transform.WorldToScreen);
        }
        for (var i = 0; i < geometry.Count; i++)
        {
            DrawLayerEdgesAndLabels(geometry, i, transform.WorldToScreen);
        }
    }

    private PreviewTransform? CreatePreviewTransform(
        System.Collections.Generic.IReadOnlyList<LayerPreviewGeometry> geometry,
        double zoom,
        Vector pan)
    {
        if (PreviewCanvas.ActualWidth <= 1 || PreviewCanvas.ActualHeight <= 1 || geometry.Count == 0)
        {
            return null;
        }

        var minX = geometry.Min(item => item.Points.Min(point => point.X));
        var maxX = geometry.Max(item => item.Points.Max(point => point.X));
        var minY = geometry.Min(item => item.Points.Min(point => point.Y));
        var maxY = geometry.Max(item => item.Points.Max(point => point.Y));
        var pad = 58.0;
        var width = Math.Max(0.1, maxX - minX);
        var height = Math.Max(0.1, maxY - minY);
        var usableWidth = Math.Max(1.0, PreviewCanvas.ActualWidth - pad * 2);
        var usableHeight = Math.Max(1.0, PreviewCanvas.ActualHeight - pad * 2);
        var baseScale = Math.Max(4.0, Math.Min(usableWidth / width, usableHeight / height));
        return new PreviewTransform(
            PreviewCanvas.ActualWidth,
            PreviewCanvas.ActualHeight,
            minX,
            maxY,
            width,
            height,
            baseScale * zoom,
            pan);
    }

    private System.Collections.Generic.List<LayerPreviewGeometry> BuildPreviewGeometry(double previewWidth)
    {
        var output = new System.Collections.Generic.List<LayerPreviewGeometry>();
        var topInnerX = 0.0;
        var topOuterX = previewWidth;
        var topInnerY = 0.0;
        var topOuterY = 0.0;

        foreach (var layer in _layers)
        {
            var innerThickness = layer.UniformThickness ? layer.Thickness : layer.InnerThickness;
            var outerThickness = layer.UniformThickness ? layer.Thickness : layer.OuterThickness;
            var inheritedTopWidth = topOuterX - topInnerX;
            if (inheritedTopWidth <= 1.0e-9)
            {
                break;
            }

            var inheritedTopGrade = (topOuterY - topInnerY) / inheritedTopWidth;
            var currentTopInner = new Point(
                topInnerX - layer.InnerWidening,
                topInnerY - layer.InnerWidening * inheritedTopGrade);
            var currentTopOuter = new Point(
                topOuterX + layer.OuterWidening,
                topOuterY + layer.OuterWidening * inheritedTopGrade);
            var currentTopWidth = currentTopOuter.X - currentTopInner.X;
            if (currentTopWidth <= 1.0e-9)
            {
                break;
            }

            var innerInset = SlopeInset(innerThickness, layer.InnerSlope);
            var outerInset = SlopeInset(outerThickness, layer.OuterSlope);
            ClampSideInsets(currentTopWidth, ref innerInset, ref outerInset);
            var bottomInnerX = currentTopInner.X + innerInset;
            var bottomOuterX = currentTopOuter.X - outerInset;
            var bottomInnerY = currentTopInner.Y - innerThickness;
            var bottomOuterY = currentTopOuter.Y - outerThickness;
            var item = new LayerPreviewGeometry(
                layer,
                currentTopInner,
                currentTopOuter,
                new Point(bottomInnerX, bottomInnerY),
                new Point(bottomOuterX, bottomOuterY));
            output.Add(item);
            topInnerX = item.BottomInner.X;
            topOuterX = item.BottomOuter.X;
            topInnerY = item.BottomInner.Y;
            topOuterY = item.BottomOuter.Y;
        }
        return output;
    }

    private void DrawGrid()
    {
        var pen = new SolidColorBrush(Color.FromRgb(45, 52, 60));
        for (var x = 0.0; x < PreviewCanvas.ActualWidth; x += 60.0)
        {
            PreviewCanvas.Children.Add(new Line { X1 = x, X2 = x, Y1 = 0, Y2 = PreviewCanvas.ActualHeight, Stroke = pen, StrokeThickness = 1 });
        }
        for (var y = 0.0; y < PreviewCanvas.ActualHeight; y += 60.0)
        {
            PreviewCanvas.Children.Add(new Line { X1 = 0, X2 = PreviewCanvas.ActualWidth, Y1 = y, Y2 = y, Stroke = pen, StrokeThickness = 1 });
        }
    }

    private void DrawLayerFill(
        LayerPreviewGeometry geometry,
        int index,
        PavementLayerTemplateDisplayMode displayMode,
        Func<Point, Point> map)
    {
        if (displayMode == PavementLayerTemplateDisplayMode.Hatch)
        {
            return;
        }

        var color = LayerColor(geometry.Layer, index);
        var topInner = map(geometry.TopInner);
        var topOuter = map(geometry.TopOuter);
        var bottomInner = map(geometry.BottomInner);
        var bottomOuter = map(geometry.BottomOuter);
        var fill = new SolidColorBrush(Color.FromArgb(74, color.R, color.G, color.B));
        var polygon = new Polygon
        {
            Points = new PointCollection { topInner, topOuter, bottomOuter, bottomInner },
            Fill = fill,
        };
        PreviewCanvas.Children.Add(polygon);
    }

    private void DrawHatchPattern(
        LayerPreviewGeometry geometry,
        int index,
        PavementLayerTemplateDisplayMode displayMode,
        Func<Point, Point> map)
    {
        if (displayMode == PavementLayerTemplateDisplayMode.Color)
        {
            return;
        }

        var pattern = PavementLayerTemplateLabels.NormalizeHatchPattern(geometry.Layer.HatchPattern);
        if (pattern == "SOLID")
        {
            return;
        }

        var points = new[] { map(geometry.TopInner), map(geometry.TopOuter), map(geometry.BottomOuter), map(geometry.BottomInner) };
        var minX = points.Min(point => point.X);
        var maxX = points.Max(point => point.X);
        var minY = points.Min(point => point.Y);
        var maxY = points.Max(point => point.Y);
        var color = displayMode == PavementLayerTemplateDisplayMode.HatchAndColor
            ? LayerColor(geometry.Layer, index)
            : Color.FromRgb(205, 213, 222);
        var stroke = new SolidColorBrush(Color.FromArgb(170, color.R, color.G, color.B));
        var spacing = pattern == "DOTS" ? 12.0 : 10.0;
        var hatchCanvas = new Canvas
        {
            Width = PreviewCanvas.ActualWidth,
            Height = PreviewCanvas.ActualHeight,
            Clip = CreatePolygonClip(points),
        };
        PreviewCanvas.Children.Add(hatchCanvas);

        if (pattern == "DOTS")
        {
            for (var x = minX; x <= maxX; x += spacing)
            {
                for (var y = minY; y <= maxY; y += spacing)
                {
                    if (PointInPolygon(new Point(x, y), points))
                    {
                        var dot = new Ellipse
                        {
                            Width = 2.0,
                            Height = 2.0,
                            Fill = stroke,
                        };
                        Canvas.SetLeft(dot, x - 1.0);
                        Canvas.SetTop(dot, y - 1.0);
                        hatchCanvas.Children.Add(dot);
                    }
                }
            }
            return;
        }

        for (var d = minX - (maxY - minY); d <= maxX; d += spacing)
        {
            var start = new Point(d, maxY);
            var end = new Point(d + (maxY - minY), minY);
            AddHatchLine(hatchCanvas, start, end, stroke);
        }

        if (pattern is "CROSS" or "ANSI32" or "ANSI37" or "ANSI38")
        {
            for (var d = minX; d <= maxX + (maxY - minY); d += spacing)
            {
                var start = new Point(d, minY);
                var end = new Point(d - (maxY - minY), maxY);
                AddHatchLine(hatchCanvas, start, end, stroke);
            }
        }
    }

    private static Geometry CreatePolygonClip(System.Collections.Generic.IReadOnlyList<Point> points)
    {
        var figure = new PathFigure
        {
            StartPoint = points[0],
            IsClosed = true,
            IsFilled = true,
        };
        for (var i = 1; i < points.Count; i++)
        {
            figure.Segments.Add(new LineSegment(points[i], true));
        }
        return new PathGeometry(new[] { figure });
    }

    private static void AddHatchLine(Canvas canvas, Point start, Point end, Brush stroke)
    {
        canvas.Children.Add(new Line
        {
            X1 = start.X,
            Y1 = start.Y,
            X2 = end.X,
            Y2 = end.Y,
            Stroke = stroke,
            StrokeThickness = 0.8,
            StrokeStartLineCap = PenLineCap.Flat,
            StrokeEndLineCap = PenLineCap.Flat,
        });
    }

    private void DrawLayerEdgesAndLabels(
        System.Collections.Generic.IReadOnlyList<LayerPreviewGeometry> geometry,
        int index,
        Func<Point, Point> map)
    {
        var current = geometry[index];
        var color = LayerColor(current.Layer, index);
        var stroke = new SolidColorBrush(color);
        var topInner = map(current.TopInner);
        var topOuter = map(current.TopOuter);
        var bottomInner = map(current.BottomInner);
        var bottomOuter = map(current.BottomOuter);
        var drawTopEdge = index == 0 ||
            !SamePoint(current.TopInner, geometry[index - 1].BottomInner) ||
            !SamePoint(current.TopOuter, geometry[index - 1].BottomOuter);

        if (drawTopEdge)
        {
            AddEdge(topInner, topOuter, stroke, 2.2);
        }
        var edgeThickness = index == _selectedLayerIndex ? 2.8 : 1.6;
        AddEdge(topOuter, bottomOuter, stroke, edgeThickness);
        AddEdge(bottomOuter, bottomInner, stroke, index == _selectedLayerIndex ? 3.0 : 2.0);
        AddEdge(bottomInner, topInner, stroke, edgeThickness);

        var layer = current.Layer;
        var midX = (topInner.X + topOuter.X + bottomInner.X + bottomOuter.X) / 4.0;
        var midY = (topInner.Y + topOuter.Y + bottomInner.Y + bottomOuter.Y) / 4.0;
        var layerHeight = Math.Max(1.0, Math.Abs(((topInner.Y + topOuter.Y) - (bottomInner.Y + bottomOuter.Y)) * 0.5));
        var labelFontSize = Math.Max(7.0, layerHeight * 0.22);
        AddLabel(LayerLabel(layer), new Point(midX - 42, midY - labelFontSize * 0.7), Brushes.White, labelFontSize);
        if (Math.Abs(layer.InnerWidening) > 1.0e-9)
        {
            var inheritedInner = index == 0
                ? map(InheritedInnerTop(current))
                : map(geometry[index - 1].BottomInner);
            DrawWideningDimension(inheritedInner, topInner, Format(layer.InnerWidening), Brushes.LightSkyBlue);
        }
        if (Math.Abs(layer.OuterWidening) > 1.0e-9)
        {
            var inheritedOuter = index == 0
                ? map(InheritedOuterTop(current))
                : map(geometry[index - 1].BottomOuter);
            DrawWideningDimension(inheritedOuter, topOuter, Format(layer.OuterWidening), Brushes.LightSkyBlue);
        }
        if (Math.Abs(layer.InnerSlope) > 1.0e-9)
        {
            AddLabel(FormatSlopeLabel(layer.InnerSlope), Midpoint(topInner, bottomInner, -28.0), Brushes.Khaki, labelFontSize * 0.9);
        }
        if (Math.Abs(layer.OuterSlope) > 1.0e-9)
        {
            AddLabel(FormatSlopeLabel(layer.OuterSlope), Midpoint(topOuter, bottomOuter, 10.0), Brushes.Khaki, labelFontSize * 0.9);
        }
    }

    private static string LayerLabel(PavementLayerTemplateLayerDto layer)
    {
        var inner = layer.UniformThickness ? layer.Thickness : layer.InnerThickness;
        var outer = layer.UniformThickness ? layer.Thickness : layer.OuterThickness;
        var thickness = NearlyEqual(inner, outer) ? Format(inner) : $"{Format(inner)}/{Format(outer)}";
        return $"{layer.Name}  厚 {thickness}";
    }

    private static Point InheritedInnerTop(LayerPreviewGeometry geometry)
    {
        var grade = TopGrade(geometry);
        return new Point(
            geometry.TopInner.X + geometry.Layer.InnerWidening,
            geometry.TopInner.Y + geometry.Layer.InnerWidening * grade);
    }

    private static Point InheritedOuterTop(LayerPreviewGeometry geometry)
    {
        var grade = TopGrade(geometry);
        return new Point(
            geometry.TopOuter.X - geometry.Layer.OuterWidening,
            geometry.TopOuter.Y - geometry.Layer.OuterWidening * grade);
    }

    private static double TopGrade(LayerPreviewGeometry geometry)
    {
        var width = geometry.TopOuter.X - geometry.TopInner.X;
        return Math.Abs(width) <= 1.0e-9 ? 0.0 : (geometry.TopOuter.Y - geometry.TopInner.Y) / width;
    }

    private static Point Midpoint(Point a, Point b, double offsetX)
        => new((a.X + b.X) * 0.5 + offsetX, (a.Y + b.Y) * 0.5 - 8.0);

    private void DrawWideningDimension(Point from, Point to, string text, Brush stroke)
    {
        if (SamePoint(from, to))
        {
            return;
        }

        var offset = new Vector(0.0, -18.0);
        var a = from + offset;
        var b = to + offset;
        AddEdge(from, a, stroke, 0.9);
        AddEdge(to, b, stroke, 0.9);
        AddEdge(a, b, stroke, 1.0);
        DrawArrow(a, b, stroke);
        DrawArrow(b, a, stroke);
        AddLabel(text, new Point((a.X + b.X) * 0.5 - 10.0, (a.Y + b.Y) * 0.5 - 18.0), stroke, 9.0);
    }

    private void DrawArrow(Point tip, Point tail, Brush stroke)
    {
        var direction = tail - tip;
        if (direction.Length <= 1.0e-9)
        {
            return;
        }

        direction.Normalize();
        var normal = new Vector(-direction.Y, direction.X);
        var p1 = tip + direction * 8.0 + normal * 3.5;
        var p2 = tip + direction * 8.0 - normal * 3.5;
        PreviewCanvas.Children.Add(new Polygon
        {
            Points = new PointCollection { tip, p1, p2 },
            Fill = stroke,
        });
    }

    private static string FormatSlopeLabel(double slope)
        => $"1:{Format(slope)}";

    private void AddEdge(Point start, Point end, Brush stroke, double thickness)
    {
        if (SamePoint(start, end))
        {
            return;
        }

        PreviewCanvas.Children.Add(new Line
        {
            X1 = start.X,
            Y1 = start.Y,
            X2 = end.X,
            Y2 = end.Y,
            Stroke = stroke,
            StrokeThickness = thickness,
            StrokeStartLineCap = PenLineCap.Round,
            StrokeEndLineCap = PenLineCap.Round,
        });
    }

    private void AddLabel(string text, Point position, Brush foreground)
        => AddLabel(text, position, foreground, 10.0);

    private void AddLabel(string text, Point position, Brush foreground, double fontSize)
    {
        var label = new TextBlock
        {
            Text = text,
            Foreground = foreground,
            FontSize = fontSize,
            FontWeight = FontWeights.SemiBold,
            FontFamily = new FontFamily("Consolas"),
            Padding = new Thickness(1, 0, 1, 0),
        };
        Canvas.SetLeft(label, position.X);
        Canvas.SetTop(label, position.Y);
        PreviewCanvas.Children.Add(label);
    }

    private void PreviewCanvas_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        ApplyLayerInputs();
        var previewWidth = Math.Max(0.1, ReadDouble(PreviewWidthBox.Text, 3.75));
        var geometry = BuildPreviewGeometry(previewWidth);
        var oldTransform = CreatePreviewTransform(geometry, _previewZoom, _previewPan);
        var mouse = e.GetPosition(PreviewCanvas);
        var newZoom = Math.Max(0.25, Math.Min(8.0, _previewZoom * (e.Delta > 0 ? 1.12 : 1.0 / 1.12)));
        if (oldTransform != null)
        {
            var worldPoint = oldTransform.ScreenToWorld(mouse);
            var newTransform = CreatePreviewTransform(geometry, newZoom, _previewPan);
            if (newTransform != null)
            {
                _previewPan += mouse - newTransform.WorldToScreen(worldPoint);
            }
        }

        _previewZoom = newZoom;
        DrawPreview();
        e.Handled = true;
    }

    private void PreviewCanvas_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        ApplyLayerInputs();
        var previewWidth = Math.Max(0.1, ReadDouble(PreviewWidthBox.Text, 3.75));
        var geometry = BuildPreviewGeometry(previewWidth);
        var transform = CreatePreviewTransform(geometry, _previewZoom, _previewPan);
        if (transform == null)
        {
            return;
        }

        var mouse = e.GetPosition(PreviewCanvas);
        for (var i = geometry.Count - 1; i >= 0; --i)
        {
            var points = geometry[i].Points.Select(transform.WorldToScreen).ToArray();
            if (PointInPolygon(mouse, points))
            {
                SelectLayer(i);
                e.Handled = true;
                return;
            }
        }
    }

    private void ColorPreview_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (_layerControls.Count == 0)
        {
            return;
        }

        ApplyLayerInputs();
        var controls = _layerControls[0];
        if (controls.LayerIndex < 0 || controls.LayerIndex >= _layers.Count)
        {
            return;
        }

        var initial = LayerColor(_layers[controls.LayerIndex], controls.LayerIndex);
        if (ColorIndexDialog.ShowDialog(this, initial) is { } selected)
        {
            SetTextBoxValue(controls.ColorRBox, selected.R);
            SetTextBoxValue(controls.ColorGBox, selected.G);
            SetTextBoxValue(controls.ColorBBox, selected.B);
            LayerInput_Changed(sender, EventArgs.Empty);
        }
        e.Handled = true;
    }

    private void PreviewCanvas_MouseMiddleButtonDown(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton != MouseButton.Middle)
        {
            return;
        }
        _panning = true;
        _lastPanPoint = e.GetPosition(PreviewCanvas);
        PreviewCanvas.CaptureMouse();
    }

    private void PreviewCanvas_MouseMiddleButtonUp(object sender, MouseButtonEventArgs e)
    {
        if (e.ChangedButton != MouseButton.Middle)
        {
            return;
        }
        _panning = false;
        PreviewCanvas.ReleaseMouseCapture();
    }

    private void PreviewCanvas_MouseMove(object sender, MouseEventArgs e)
    {
        if (!_panning)
        {
            return;
        }
        var point = e.GetPosition(PreviewCanvas);
        _previewPan += point - _lastPanPoint;
        _lastPanPoint = point;
        DrawPreview();
    }

    private void ImportXml_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "RoadProto pavement XML (*.rpavement.xml)|*.rpavement.xml",
        };
        if (dialog.ShowDialog(this) != true)
        {
            return;
        }

        try
        {
            var template = PavementLayerTemplateXmlFile.Read(dialog.FileName);
            LoadTemplate(template);
        }
        catch (Exception error)
        {
            MessageBox.Show(this, error.Message, "导入 XML", MessageBoxButton.OK, MessageBoxImage.Warning);
        }
    }

    private void SaveXml_Click(object sender, RoutedEventArgs e)
    {
        ApplyLayerInputs();
        var dialog = new SaveFileDialog
        {
            Filter = "RoadProto pavement XML (*.rpavement.xml)|*.rpavement.xml",
            DefaultExt = PavementLayerTemplateXmlFile.Extension,
            AddExtension = true,
            FileName = EnsureXmlFileName(TemplateNameBox.Text),
        };
        if (dialog.ShowDialog(this) != true)
        {
            return;
        }

        var path = dialog.FileName.EndsWith(PavementLayerTemplateXmlFile.Extension, StringComparison.OrdinalIgnoreCase)
            ? dialog.FileName
            : dialog.FileName + PavementLayerTemplateXmlFile.Extension;
        PavementLayerTemplateXmlFile.Write(path, BuildTemplateDto());
    }

    private void LoadTemplate(PavementLayerTemplateDto template)
    {
        _loading = true;
        TemplateNameBox.Text = template.TemplateName;
        DisplayScaleBox.Text = Format(template.DisplayScale);
        PreviewWidthBox.Text = Format(template.PreviewWidth);
        SelectComboValue(DisplayModeBox, template.DisplayMode);
        _layers.Clear();
        foreach (var layer in template.Layers)
        {
            var copy = layer.Clone();
            EnsureLayerColor(copy, _layers.Count);
            copy.HatchPattern = PavementLayerTemplateLabels.NormalizeHatchPattern(copy.HatchPattern);
            _layers.Add(copy);
        }
        if (_layers.Count == 0)
        {
            foreach (var layer in PavementLayerTemplateLabels.DefaultLayers())
            {
                _layers.Add(layer);
            }
        }
        _selectedLayerIndex = 0;
        LayerCountBox.Text = _layers.Count.ToString(CultureInfo.InvariantCulture);
        UpdateCurrentLayerBox();
        RefreshLayerGroups();
        _loading = false;
        DrawPreview();
    }

    private void Apply_Click(object sender, RoutedEventArgs e)
    {
        var response = BuildResponse(true);
        ApplyRequested?.Invoke(this, response);
    }

    private void Ok_Click(object sender, RoutedEventArgs e)
    {
        Response = BuildResponse(true);
        DialogResult = true;
    }

    private void Cancel_Click(object sender, RoutedEventArgs e)
    {
        Response = new PavementLayerTemplateDialogResponse
        {
            Accepted = false,
            Handle = _request.Handle,
        };
        DialogResult = false;
    }

    private PavementLayerTemplateDialogResponse BuildResponse(bool accepted)
    {
        var template = BuildTemplateDto();
        return new PavementLayerTemplateDialogResponse
        {
            Accepted = accepted,
            Handle = _request.Handle,
            InsertionX = _request.InsertionX,
            InsertionY = _request.InsertionY,
            InsertionZ = _request.InsertionZ,
            TemplateName = template.TemplateName,
            DisplayScale = template.DisplayScale,
            PreviewWidth = template.PreviewWidth,
            DisplayMode = template.DisplayMode,
            Layers = template.Layers,
        };
    }

    private PavementLayerTemplateDto BuildTemplateDto()
    {
        ApplyLayerInputs();
        return new PavementLayerTemplateDto
        {
            TemplateName = string.IsNullOrWhiteSpace(TemplateNameBox.Text) ? "路面结构层模板" : TemplateNameBox.Text.Trim(),
            DisplayScale = Math.Max(0.1, ReadDouble(DisplayScaleBox.Text, 10.0)),
            PreviewWidth = Math.Max(0.1, ReadDouble(PreviewWidthBox.Text, 3.75)),
            DisplayMode = CurrentDisplayMode(),
            Layers = _layers.Select(layer => layer.Clone()).ToList(),
        };
    }

    private static string EnsureXmlFileName(string value)
    {
        var name = string.IsNullOrWhiteSpace(value) ? "路面结构层模板" : value.Trim();
        foreach (var invalid in System.IO.Path.GetInvalidFileNameChars())
        {
            name = name.Replace(invalid, '_');
        }
        return name + PavementLayerTemplateXmlFile.Extension;
    }

    private static double ReadDouble(string text, double fallback)
        => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var value) && IsFinite(value)
            ? value
            : fallback;

    private static double ReadSlope(string text, double fallback)
    {
        var trimmed = text.Trim();
        var ratio = trimmed.Replace('：', ':').Split(':');
        if (ratio.Length == 2 &&
            double.TryParse(ratio[0], NumberStyles.Float, CultureInfo.InvariantCulture, out var numerator) &&
            double.TryParse(ratio[1], NumberStyles.Float, CultureInfo.InvariantCulture, out var denominator) &&
            numerator > 0.0 &&
            IsFinite(numerator) &&
            IsFinite(denominator))
        {
            return denominator / numerator;
        }

        return ReadDouble(trimmed, fallback);
    }

    private static int ReadColorChannel(string text, int fallback)
        => int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var value)
            ? ClampColor(value)
            : ClampColor(fallback);

    private static double SlopeInset(double thickness, double slope)
        => thickness > 0.0 && IsFinite(slope) ? -thickness * slope : 0.0;

    private static bool IsFinite(double value)
        => !double.IsNaN(value) && !double.IsInfinity(value);

    private static void ClampSideInsets(double topWidth, ref double innerInset, ref double outerInset)
    {
        var totalInset = innerInset + outerInset;
        if (topWidth <= 0.0 || totalInset <= topWidth)
        {
            return;
        }

        var scale = topWidth / totalInset;
        innerInset *= scale;
        outerInset *= scale;
    }

    private static string Format(double value)
        => value.ToString("0.###", CultureInfo.InvariantCulture);

    private static bool NearlyEqual(double first, double second)
        => Math.Abs(first - second) < 1.0e-9;

    private static bool SamePoint(Point first, Point second)
        => NearlyEqual(first.X, second.X) && NearlyEqual(first.Y, second.Y);

    private PavementLayerTemplateDisplayMode CurrentDisplayMode()
        => SelectedValue(DisplayModeBox, PavementLayerTemplateDisplayMode.Color);

    private static string DisplayModeLabel(PavementLayerTemplateDisplayMode mode)
        => mode switch
        {
            PavementLayerTemplateDisplayMode.Hatch => "\u6309\u586b\u5145",
            PavementLayerTemplateDisplayMode.HatchAndColor => "\u6309\u586b\u5145+\u989c\u8272\u663e\u793a",
            _ => "\u6309\u989c\u8272",
        };

    private static bool PointInPolygon(Point point, System.Collections.Generic.IReadOnlyList<Point> polygon)
    {
        var inside = false;
        for (int i = 0, j = polygon.Count - 1; i < polygon.Count; j = i++)
        {
            var current = polygon[i];
            var previous = polygon[j];
            var crosses = current.Y > point.Y != previous.Y > point.Y;
            if (crosses)
            {
                var x = (previous.X - current.X) * (point.Y - current.Y) / (previous.Y - current.Y) + current.X;
                if (point.X < x)
                {
                    inside = !inside;
                }
            }
        }
        return inside;
    }

    private static void SetTextBoxValue(TextBox box, double value)
    {
        var text = Format(value);
        if (box.Text != text)
        {
            box.Text = text;
        }
    }

    private static void SetTextBoxValue(TextBox box, int value)
    {
        var text = value.ToString(CultureInfo.InvariantCulture);
        if (box.Text != text)
        {
            box.Text = text;
        }
    }

    private static void SelectComboValue<T>(ComboBox comboBox, T value)
    {
        for (var i = 0; i < comboBox.Items.Count; i++)
        {
            if (comboBox.Items[i] is ComboOption<T> option && Equals(option.Value, value))
            {
                comboBox.SelectedIndex = i;
                return;
            }
        }
        comboBox.SelectedIndex = comboBox.Items.Count > 0 ? 0 : -1;
    }

    private static T SelectedValue<T>(ComboBox comboBox, T fallback)
        => comboBox.SelectedItem is ComboOption<T> option ? option.Value : fallback;

    private static Color LayerColor(PavementLayerTemplateLayerDto layer, int index)
    {
        if (layer.ColorR < 0 || layer.ColorG < 0 || layer.ColorB < 0)
        {
            var fallback = PavementLayerTemplateLabels.DefaultColorForLayerIndex(index);
            return Color.FromRgb((byte)fallback.R, (byte)fallback.G, (byte)fallback.B);
        }

        return Color.FromRgb(ToByte(layer.ColorR), ToByte(layer.ColorG), ToByte(layer.ColorB));
    }

    private static void EnsureLayerColor(PavementLayerTemplateLayerDto layer, int index)
    {
        if (layer.ColorR < 0 || layer.ColorG < 0 || layer.ColorB < 0)
        {
            var color = PavementLayerTemplateLabels.DefaultColorForLayerIndex(index);
            layer.ColorR = color.R;
            layer.ColorG = color.G;
            layer.ColorB = color.B;
            return;
        }

        layer.ColorR = ClampColor(layer.ColorR);
        layer.ColorG = ClampColor(layer.ColorG);
        layer.ColorB = ClampColor(layer.ColorB);
    }

    private static int ClampColor(int value)
        => Math.Max(0, Math.Min(255, value));

    private static byte ToByte(int value)
        => (byte)ClampColor(value);

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

    private static class ColorIndexDialog
    {
        private static readonly (int Index, Color Color)[] Colors =
        {
            (1, Color.FromRgb(255, 0, 0)),
            (2, Color.FromRgb(255, 255, 0)),
            (3, Color.FromRgb(0, 255, 0)),
            (4, Color.FromRgb(0, 255, 255)),
            (5, Color.FromRgb(0, 0, 255)),
            (6, Color.FromRgb(255, 0, 255)),
            (7, Color.FromRgb(255, 255, 255)),
            (8, Color.FromRgb(128, 128, 128)),
            (9, Color.FromRgb(192, 192, 192)),
            (10, Color.FromRgb(255, 127, 127)),
            (30, Color.FromRgb(255, 127, 0)),
            (50, Color.FromRgb(255, 255, 127)),
            (70, Color.FromRgb(127, 255, 0)),
            (90, Color.FromRgb(127, 255, 127)),
            (110, Color.FromRgb(0, 255, 127)),
            (130, Color.FromRgb(127, 255, 255)),
            (150, Color.FromRgb(0, 127, 255)),
            (170, Color.FromRgb(127, 127, 255)),
            (190, Color.FromRgb(127, 0, 255)),
            (210, Color.FromRgb(255, 127, 255)),
            (230, Color.FromRgb(255, 0, 127)),
            (250, Color.FromRgb(51, 51, 51)),
            (251, Color.FromRgb(91, 91, 91)),
            (252, Color.FromRgb(132, 132, 132)),
            (253, Color.FromRgb(173, 173, 173)),
            (254, Color.FromRgb(214, 214, 214)),
            (255, Color.FromRgb(255, 255, 255)),
        };

        public static Color? ShowDialog(Window owner, Color initial)
        {
            Color? selected = null;
            var grid = new UniformGrid
            {
                Columns = 9,
                Margin = new Thickness(12),
            };

            foreach (var (index, color) in Colors)
            {
                var button = new Button
                {
                    Width = 32,
                    Height = 32,
                    Margin = new Thickness(3),
                    Background = new SolidColorBrush(color),
                    BorderBrush = SameColor(color, initial) ? Brushes.White : Brushes.DimGray,
                    BorderThickness = SameColor(color, initial) ? new Thickness(3) : new Thickness(1),
                    ToolTip = $"ACI {index}",
                    Tag = color,
                };
                button.Click += (sender, _) =>
                {
                    selected = (Color)((Button)sender!).Tag;
                    Window.GetWindow(button)!.DialogResult = true;
                };
                grid.Children.Add(button);
            }

            var dialog = new Window
            {
                Title = "\u9009\u62e9\u7d22\u5f15\u989c\u8272",
                Owner = owner,
                WindowStartupLocation = WindowStartupLocation.CenterOwner,
                ResizeMode = ResizeMode.NoResize,
                SizeToContent = SizeToContent.WidthAndHeight,
                Content = grid,
            };

            return dialog.ShowDialog() == true ? selected : null;
        }

        private static bool SameColor(Color first, Color second)
            => first.R == second.R && first.G == second.G && first.B == second.B;
    }

    private sealed class LayerControls
    {
        public LayerControls(
            int layerIndex,
            ComboBox typeBox,
            TextBox nameBox,
            TextBox colorRBox,
            TextBox colorGBox,
            TextBox colorBBox,
            Border colorPreview,
            ComboBox hatchPatternBox,
            CheckBox uniformBox,
            TextBox thicknessBox,
            TextBox innerThicknessBox,
            TextBox outerThicknessBox,
            CheckBox uniformWideningBox,
            TextBox wideningBox,
            TextBox innerWideningBox,
            TextBox outerWideningBox,
            CheckBox uniformSlopeBox,
            TextBox slopeBox,
            TextBox innerSlopeBox,
            TextBox outerSlopeBox,
            FrameworkElement thicknessRow,
            FrameworkElement innerThicknessRow,
            FrameworkElement outerThicknessRow,
            FrameworkElement wideningRow,
            FrameworkElement innerWideningRow,
            FrameworkElement outerWideningRow,
            FrameworkElement slopeRow,
            FrameworkElement innerSlopeRow,
            FrameworkElement outerSlopeRow)
        {
            LayerIndex = layerIndex;
            TypeBox = typeBox;
            NameBox = nameBox;
            ColorRBox = colorRBox;
            ColorGBox = colorGBox;
            ColorBBox = colorBBox;
            ColorPreview = colorPreview;
            HatchPatternBox = hatchPatternBox;
            UniformBox = uniformBox;
            ThicknessBox = thicknessBox;
            InnerThicknessBox = innerThicknessBox;
            OuterThicknessBox = outerThicknessBox;
            UniformWideningBox = uniformWideningBox;
            WideningBox = wideningBox;
            InnerWideningBox = innerWideningBox;
            OuterWideningBox = outerWideningBox;
            UniformSlopeBox = uniformSlopeBox;
            SlopeBox = slopeBox;
            InnerSlopeBox = innerSlopeBox;
            OuterSlopeBox = outerSlopeBox;
            ThicknessRow = thicknessRow;
            InnerThicknessRow = innerThicknessRow;
            OuterThicknessRow = outerThicknessRow;
            WideningRow = wideningRow;
            InnerWideningRow = innerWideningRow;
            OuterWideningRow = outerWideningRow;
            SlopeRow = slopeRow;
            InnerSlopeRow = innerSlopeRow;
            OuterSlopeRow = outerSlopeRow;
        }

        public int LayerIndex { get; }
        public ComboBox TypeBox { get; }
        public TextBox NameBox { get; }
        public TextBox ColorRBox { get; }
        public TextBox ColorGBox { get; }
        public TextBox ColorBBox { get; }
        public Border ColorPreview { get; }
        public ComboBox HatchPatternBox { get; }
        public CheckBox UniformBox { get; }
        public TextBox ThicknessBox { get; }
        public TextBox InnerThicknessBox { get; }
        public TextBox OuterThicknessBox { get; }
        public CheckBox UniformWideningBox { get; }
        public TextBox WideningBox { get; }
        public TextBox InnerWideningBox { get; }
        public TextBox OuterWideningBox { get; }
        public CheckBox UniformSlopeBox { get; }
        public TextBox SlopeBox { get; }
        public TextBox InnerSlopeBox { get; }
        public TextBox OuterSlopeBox { get; }
        public FrameworkElement ThicknessRow { get; }
        public FrameworkElement InnerThicknessRow { get; }
        public FrameworkElement OuterThicknessRow { get; }
        public FrameworkElement WideningRow { get; }
        public FrameworkElement InnerWideningRow { get; }
        public FrameworkElement OuterWideningRow { get; }
        public FrameworkElement SlopeRow { get; }
        public FrameworkElement InnerSlopeRow { get; }
        public FrameworkElement OuterSlopeRow { get; }
    }

    private sealed class PreviewTransform
    {
        private readonly double _canvasWidth;
        private readonly double _canvasHeight;
        private readonly double _minX;
        private readonly double _maxY;
        private readonly double _contentWidth;
        private readonly double _contentHeight;
        private readonly double _scale;
        private readonly Vector _pan;

        public PreviewTransform(
            double canvasWidth,
            double canvasHeight,
            double minX,
            double maxY,
            double worldWidth,
            double worldHeight,
            double scale,
            Vector pan)
        {
            _canvasWidth = canvasWidth;
            _canvasHeight = canvasHeight;
            _minX = minX;
            _maxY = maxY;
            _contentWidth = worldWidth * scale;
            _contentHeight = worldHeight * scale;
            _scale = scale;
            _pan = pan;
        }

        public Point WorldToScreen(Point point)
            => new(
                (_canvasWidth - _contentWidth) / 2.0 + (point.X - _minX) * _scale + _pan.X,
                (_canvasHeight - _contentHeight) / 2.0 + (_maxY - point.Y) * _scale + _pan.Y);

        public Point ScreenToWorld(Point point)
            => new(
                _minX + (point.X - _pan.X - (_canvasWidth - _contentWidth) / 2.0) / _scale,
                _maxY - (point.Y - _pan.Y - (_canvasHeight - _contentHeight) / 2.0) / _scale);
    }

    private sealed class LayerPreviewGeometry
    {
        public LayerPreviewGeometry(
            PavementLayerTemplateLayerDto layer,
            Point topInner,
            Point topOuter,
            Point bottomInner,
            Point bottomOuter)
        {
            Layer = layer;
            TopInner = topInner;
            TopOuter = topOuter;
            BottomInner = bottomInner;
            BottomOuter = bottomOuter;
            Points = new[] { topInner, topOuter, bottomOuter, bottomInner };
        }

        public PavementLayerTemplateLayerDto Layer { get; }
        public Point TopInner { get; }
        public Point TopOuter { get; }
        public Point BottomInner { get; }
        public Point BottomOuter { get; }
        public System.Collections.Generic.IReadOnlyList<Point> Points { get; }
    }
}
