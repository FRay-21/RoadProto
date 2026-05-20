using System;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
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
    private bool _loading = true;
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

        _layers.Clear();
        var source = _request.Layers.Count > 0 ? _request.Layers : PavementLayerTemplateLabels.DefaultLayers();
        foreach (var layer in source)
        {
            _layers.Add(layer.Clone());
        }
        LayerCountBox.Text = _layers.Count.ToString(CultureInfo.InvariantCulture);
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

        count = Math.Max(1, Math.Min(100, count));
        while (_layers.Count < count)
        {
            var type = (PavementLayerType)(Math.Min(_layers.Count, _typeOptions.Count - 1));
            _layers.Add(new PavementLayerTemplateLayerDto
            {
                Type = type,
                Name = PavementLayerTemplateLabels.LayerTypeLabel(type),
            });
        }
        while (_layers.Count > count)
        {
            _layers.RemoveAt(_layers.Count - 1);
        }

        RefreshLayerGroups();
        DrawPreview();
    }

    private void LayerInput_Changed(object sender, EventArgs e)
    {
        if (_loading)
        {
            return;
        }

        ApplyLayerInputs();
        RefreshThicknessVisibility();
        DrawPreview();
    }

    private void RefreshLayerGroups()
    {
        _loading = true;
        _layerControls.Clear();
        LayersPanel.Children.Clear();
        for (var i = 0; i < _layers.Count; i++)
        {
            LayersPanel.Children.Add(CreateLayerGroup(i, _layers[i]));
        }
        _loading = false;
        RefreshThicknessVisibility();
    }

    private Border CreateLayerGroup(int index, PavementLayerTemplateLayerDto layer)
    {
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
        var uniformBox = new CheckBox { Content = "内外厚度是否一致", IsChecked = layer.UniformThickness, Margin = new Thickness(0, 4, 0, 4) };
        var thicknessBox = new TextBox { Height = 26, Text = Format(layer.Thickness) };
        var innerThicknessBox = new TextBox { Height = 26, Text = Format(layer.InnerThickness) };
        var outerThicknessBox = new TextBox { Height = 26, Text = Format(layer.OuterThickness) };
        var innerWideningBox = new TextBox { Height = 26, Text = Format(layer.InnerWidening) };
        var outerWideningBox = new TextBox { Height = 26, Text = Format(layer.OuterWidening) };
        var innerSlopeBox = new TextBox { Height = 26, Text = Format(layer.InnerSlope) };
        var outerSlopeBox = new TextBox { Height = 26, Text = Format(layer.OuterSlope) };

        panel.Children.Add(FieldRow("类型", typeBox));
        panel.Children.Add(FieldRow("名称", nameBox));
        panel.Children.Add(uniformBox);
        var thicknessRow = FieldRow("厚度", thicknessBox);
        var innerThicknessRow = FieldRow("内侧厚度", innerThicknessBox);
        var outerThicknessRow = FieldRow("外侧厚度", outerThicknessBox);
        panel.Children.Add(thicknessRow);
        panel.Children.Add(innerThicknessRow);
        panel.Children.Add(outerThicknessRow);
        panel.Children.Add(FieldRow("内侧加宽", innerWideningBox));
        panel.Children.Add(FieldRow("外侧加宽", outerWideningBox));
        panel.Children.Add(FieldRow("内侧坡度", innerSlopeBox));
        panel.Children.Add(FieldRow("外侧坡度", outerSlopeBox));

        typeBox.SelectionChanged += LayerInput_Changed;
        nameBox.TextChanged += LayerInput_Changed;
        uniformBox.Checked += LayerInput_Changed;
        uniformBox.Unchecked += LayerInput_Changed;
        thicknessBox.TextChanged += LayerInput_Changed;
        innerThicknessBox.TextChanged += LayerInput_Changed;
        outerThicknessBox.TextChanged += LayerInput_Changed;
        innerWideningBox.TextChanged += LayerInput_Changed;
        outerWideningBox.TextChanged += LayerInput_Changed;
        innerSlopeBox.TextChanged += LayerInput_Changed;
        outerSlopeBox.TextChanged += LayerInput_Changed;

        _layerControls.Add(new LayerControls(
            typeBox,
            nameBox,
            uniformBox,
            thicknessBox,
            innerThicknessBox,
            outerThicknessBox,
            innerWideningBox,
            outerWideningBox,
            innerSlopeBox,
            outerSlopeBox,
            thicknessRow,
            innerThicknessRow,
            outerThicknessRow));
        return border;
    }

    private static FrameworkElement FieldRow(string label, Control input)
    {
        var grid = new Grid { Margin = new Thickness(0, 0, 0, 6) };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(108) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.Children.Add(new TextBlock { Text = label, VerticalAlignment = VerticalAlignment.Center });
        Grid.SetColumn(input, 1);
        grid.Children.Add(input);
        return grid;
    }

    private void RefreshThicknessVisibility()
    {
        for (var i = 0; i < _layerControls.Count && i < _layers.Count; i++)
        {
            var uniform = _layerControls[i].UniformBox.IsChecked == true;
            _layerControls[i].ThicknessRow.Visibility = uniform ? Visibility.Visible : Visibility.Collapsed;
            _layerControls[i].InnerThicknessRow.Visibility = uniform ? Visibility.Collapsed : Visibility.Visible;
            _layerControls[i].OuterThicknessRow.Visibility = uniform ? Visibility.Collapsed : Visibility.Visible;
        }
    }

    private void ApplyLayerInputs()
    {
        for (var i = 0; i < _layerControls.Count && i < _layers.Count; i++)
        {
            var controls = _layerControls[i];
            var layer = _layers[i];
            layer.Type = SelectedValue(controls.TypeBox, layer.Type);
            layer.Name = string.IsNullOrWhiteSpace(controls.NameBox.Text)
                ? PavementLayerTemplateLabels.LayerTypeLabel(layer.Type)
                : controls.NameBox.Text.Trim();
            layer.UniformThickness = controls.UniformBox.IsChecked == true;
            layer.Thickness = Math.Max(0.001, ReadDouble(controls.ThicknessBox.Text, layer.Thickness));
            layer.InnerThickness = Math.Max(0.001, ReadDouble(controls.InnerThicknessBox.Text, layer.InnerThickness));
            layer.OuterThickness = Math.Max(0.001, ReadDouble(controls.OuterThicknessBox.Text, layer.OuterThickness));
            if (layer.UniformThickness)
            {
                layer.InnerThickness = layer.Thickness;
                layer.OuterThickness = layer.Thickness;
                controls.InnerThicknessBox.Text = Format(layer.InnerThickness);
                controls.OuterThicknessBox.Text = Format(layer.OuterThickness);
            }
            else
            {
                layer.Thickness = Math.Max(layer.InnerThickness, layer.OuterThickness);
            }
            layer.InnerWidening = Math.Max(0.0, ReadDouble(controls.InnerWideningBox.Text, layer.InnerWidening));
            layer.OuterWidening = Math.Max(0.0, ReadDouble(controls.OuterWideningBox.Text, layer.OuterWidening));
            layer.InnerSlope = ReadDouble(controls.InnerSlopeBox.Text, layer.InnerSlope);
            layer.OuterSlope = ReadDouble(controls.OuterSlopeBox.Text, layer.OuterSlope);
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

        var minX = geometry.Min(item => Math.Min(Math.Min(item.TopInner.X, item.TopOuter.X), Math.Min(item.BottomInner.X, item.BottomOuter.X)));
        var maxX = geometry.Max(item => Math.Max(Math.Max(item.TopInner.X, item.TopOuter.X), Math.Max(item.BottomInner.X, item.BottomOuter.X)));
        var minY = geometry.Min(item => Math.Min(Math.Min(item.TopInner.Y, item.TopOuter.Y), Math.Min(item.BottomInner.Y, item.BottomOuter.Y)));
        var maxY = geometry.Max(item => Math.Max(Math.Max(item.TopInner.Y, item.TopOuter.Y), Math.Max(item.BottomInner.Y, item.BottomOuter.Y)));
        var pad = 58.0;
        var sx = (PreviewCanvas.ActualWidth - pad * 2) / Math.Max(0.1, maxX - minX);
        var sy = (PreviewCanvas.ActualHeight - pad * 2) / Math.Max(0.1, maxY - minY);
        var scale = Math.Max(4.0, Math.Min(sx, sy)) * _previewZoom;

        Point Map(Point point)
            => new(
                pad + (point.X - minX) * scale + _previewPan.X,
                PreviewCanvas.ActualHeight - pad - (point.Y - minY) * scale + _previewPan.Y);

        DrawGrid();
        for (var i = 0; i < geometry.Count; i++)
        {
            DrawLayer(geometry[i], i, Map);
        }
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
            var item = new LayerPreviewGeometry(
                layer,
                new Point(topInnerX, topInnerY),
                new Point(topOuterX, topOuterY),
                new Point(topInnerX - layer.InnerWidening, topInnerY - innerThickness),
                new Point(topOuterX + layer.OuterWidening, topOuterY - outerThickness));
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

    private void DrawLayer(LayerPreviewGeometry geometry, int index, Func<Point, Point> map)
    {
        var color = LayerColor(index);
        var topInner = map(geometry.TopInner);
        var topOuter = map(geometry.TopOuter);
        var bottomInner = map(geometry.BottomInner);
        var bottomOuter = map(geometry.BottomOuter);
        var fill = new SolidColorBrush(Color.FromArgb(74, color.R, color.G, color.B));
        var stroke = new SolidColorBrush(color);
        var polygon = new Polygon
        {
            Points = new PointCollection { topInner, topOuter, bottomOuter, bottomInner },
            Fill = fill,
            Stroke = stroke,
            StrokeThickness = 1.4,
        };
        PreviewCanvas.Children.Add(polygon);

        AddEdge(topInner, topOuter, stroke, 2.2);
        AddEdge(topOuter, bottomOuter, stroke, 1.6);
        AddEdge(bottomOuter, bottomInner, stroke, 2.0);
        AddEdge(bottomInner, topInner, stroke, 1.6);

        var layer = geometry.Layer;
        var midX = (topInner.X + topOuter.X + bottomInner.X + bottomOuter.X) / 4.0;
        var midY = (topInner.Y + topOuter.Y + bottomInner.Y + bottomOuter.Y) / 4.0;
        AddLabel(layer.Name, new Point(midX - 38, midY - 10), Brushes.White);
        AddLabel($"厚 {Format(layer.UniformThickness ? layer.Thickness : layer.InnerThickness)}/{Format(layer.UniformThickness ? layer.Thickness : layer.OuterThickness)}", new Point(midX - 38, midY + 10), Brushes.White);
        AddLabel($"内侧加宽 {Format(layer.InnerWidening)}", new Point(bottomInner.X - 18, bottomInner.Y + 14), Brushes.LightSkyBlue);
        AddLabel($"外侧加宽 {Format(layer.OuterWidening)}", new Point(bottomOuter.X + 10, bottomOuter.Y + 14), Brushes.LightSkyBlue);
        if (Math.Abs(layer.InnerSlope) > 1.0e-9)
        {
            AddLabel($"内侧坡度 {Format(layer.InnerSlope)}", new Point(bottomInner.X - 20, bottomInner.Y + 34), Brushes.Khaki);
        }
        if (Math.Abs(layer.OuterSlope) > 1.0e-9)
        {
            AddLabel($"外侧坡度 {Format(layer.OuterSlope)}", new Point(bottomOuter.X + 10, bottomOuter.Y + 34), Brushes.Khaki);
        }
    }

    private void AddEdge(Point start, Point end, Brush stroke, double thickness)
        => PreviewCanvas.Children.Add(new Line
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

    private void AddLabel(string text, Point position, Brush foreground)
    {
        var label = new TextBlock
        {
            Text = text,
            Foreground = foreground,
            FontSize = 12,
            Background = new SolidColorBrush(Color.FromArgb(150, 21, 26, 32)),
            Padding = new Thickness(3, 1, 3, 1),
        };
        Canvas.SetLeft(label, position.X);
        Canvas.SetTop(label, position.Y);
        PreviewCanvas.Children.Add(label);
    }

    private void PreviewCanvas_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        _previewZoom = Math.Max(0.25, Math.Min(8.0, _previewZoom * (e.Delta > 0 ? 1.12 : 1.0 / 1.12)));
        DrawPreview();
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
        _layers.Clear();
        foreach (var layer in template.Layers)
        {
            _layers.Add(layer.Clone());
        }
        if (_layers.Count == 0)
        {
            foreach (var layer in PavementLayerTemplateLabels.DefaultLayers())
            {
                _layers.Add(layer);
            }
        }
        LayerCountBox.Text = _layers.Count.ToString(CultureInfo.InvariantCulture);
        RefreshLayerGroups();
        _loading = false;
        DrawPreview();
    }

    private void Apply_Click(object sender, RoutedEventArgs e)
    {
        var response = BuildResponse(true);
        Response = response;
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
        => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var value) ? value : fallback;

    private static string Format(double value)
        => value.ToString("0.###", CultureInfo.InvariantCulture);

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

    private static Color LayerColor(int index)
    {
        var colors = new[]
        {
            Color.FromRgb(65, 174, 221),
            Color.FromRgb(79, 203, 137),
            Color.FromRgb(250, 197, 83),
            Color.FromRgb(236, 132, 80),
            Color.FromRgb(177, 138, 230),
            Color.FromRgb(142, 164, 180),
        };
        return colors[index % colors.Length];
    }

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

    private sealed class LayerControls
    {
        public LayerControls(
            ComboBox typeBox,
            TextBox nameBox,
            CheckBox uniformBox,
            TextBox thicknessBox,
            TextBox innerThicknessBox,
            TextBox outerThicknessBox,
            TextBox innerWideningBox,
            TextBox outerWideningBox,
            TextBox innerSlopeBox,
            TextBox outerSlopeBox,
            FrameworkElement thicknessRow,
            FrameworkElement innerThicknessRow,
            FrameworkElement outerThicknessRow)
        {
            TypeBox = typeBox;
            NameBox = nameBox;
            UniformBox = uniformBox;
            ThicknessBox = thicknessBox;
            InnerThicknessBox = innerThicknessBox;
            OuterThicknessBox = outerThicknessBox;
            InnerWideningBox = innerWideningBox;
            OuterWideningBox = outerWideningBox;
            InnerSlopeBox = innerSlopeBox;
            OuterSlopeBox = outerSlopeBox;
            ThicknessRow = thicknessRow;
            InnerThicknessRow = innerThicknessRow;
            OuterThicknessRow = outerThicknessRow;
        }

        public ComboBox TypeBox { get; }
        public TextBox NameBox { get; }
        public CheckBox UniformBox { get; }
        public TextBox ThicknessBox { get; }
        public TextBox InnerThicknessBox { get; }
        public TextBox OuterThicknessBox { get; }
        public TextBox InnerWideningBox { get; }
        public TextBox OuterWideningBox { get; }
        public TextBox InnerSlopeBox { get; }
        public TextBox OuterSlopeBox { get; }
        public FrameworkElement ThicknessRow { get; }
        public FrameworkElement InnerThicknessRow { get; }
        public FrameworkElement OuterThicknessRow { get; }
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
        }

        public PavementLayerTemplateLayerDto Layer { get; }
        public Point TopInner { get; }
        public Point TopOuter { get; }
        public Point BottomInner { get; }
        public Point BottomOuter { get; }
    }
}
