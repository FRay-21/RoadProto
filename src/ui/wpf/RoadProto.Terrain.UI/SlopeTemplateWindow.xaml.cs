using System;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Linq;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Shapes;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class SlopeTemplateWindow : Window
{
    private readonly SlopeTemplateDialogRequest _request;
    private readonly ObservableCollection<SlopeComponentDto> _components = new();
    private bool _loading = true;

    public SlopeTemplateWindow(SlopeTemplateDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        Response = null;
        LoadComboBoxes();
        LoadRequest();
    }

    public SlopeTemplateDialogResponse? Response { get; private set; }

    private SlopeComponentDto? SelectedComponent
        => ComponentListBox.SelectedIndex >= 0 && ComponentListBox.SelectedIndex < _components.Count
            ? _components[ComponentListBox.SelectedIndex]
            : null;

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

        KindComboBox.ItemsSource = Enum.GetValues(typeof(SlopeTemplateKind))
            .Cast<SlopeTemplateKind>()
            .Select(value => new ComboOption<SlopeTemplateKind>(SlopeTemplateLabels.KindLabel(value), value))
            .ToList();
        KindComboBox.DisplayMemberPath = nameof(ComboOption<SlopeTemplateKind>.Label);

        var typeOptions = Enum.GetValues(typeof(SlopeComponentType))
            .Cast<SlopeComponentType>()
            .Select(value => new ComboOption<SlopeComponentType>(SlopeTemplateLabels.ComponentTypeLabel(value), value))
            .ToList();
        AddTypeComboBox.ItemsSource = typeOptions;
        AddTypeComboBox.DisplayMemberPath = nameof(ComboOption<SlopeComponentType>.Label);
        ComponentTypeComboBox.ItemsSource = typeOptions;
        ComponentTypeComboBox.DisplayMemberPath = nameof(ComboOption<SlopeComponentType>.Label);

        ConstraintModeComboBox.ItemsSource = Enum.GetValues(typeof(SlopeGeometryConstraintMode))
            .Cast<SlopeGeometryConstraintMode>()
            .Select(value => new ComboOption<SlopeGeometryConstraintMode>(SlopeTemplateLabels.ConstraintModeLabel(value), value))
            .ToList();
        ConstraintModeComboBox.DisplayMemberPath = nameof(ComboOption<SlopeGeometryConstraintMode>.Label);
    }

    private void LoadRequest()
    {
        _loading = true;
        TemplateNameBox.Text = string.IsNullOrWhiteSpace(_request.TemplateName) ? "边坡模板1" : _request.TemplateName;
        SelectComboValue(DisplayScaleComboBox, _request.DisplayScale);
        SelectComboValue(KindComboBox, _request.Kind);
        SelectComboValue(AddTypeComboBox, _request.Kind == SlopeTemplateKind.Cut ? SlopeComponentType.CutSlope : SlopeComponentType.FillSlope);
        StopAtGroundCheckBox.IsChecked = _request.StopAtGround;
        RepeatLastGroupCheckBox.IsChecked = _request.RepeatLastGroupUntilGround;

        _components.Clear();
        var source = _request.Components.Count > 0
            ? _request.Components.Select(item => item.Clone()).ToList()
            : BuildDefaults(_request.Kind);
        foreach (var component in source)
        {
            _components.Add(component);
        }
        ComponentListBox.ItemsSource = _components;
        ComponentListBox.SelectedIndex = _components.Count > 0 ? 0 : -1;
        RefreshSelectedComponentInputs();
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
        RefreshSelectedComponentInputs();
        ComponentListBox.Items.Refresh();
        DrawPreview();
    }

    private void AddComponent_Click(object sender, RoutedEventArgs e)
    {
        var type = SelectedValue(AddTypeComboBox, SlopeComponentType.FillSlope);
        var component = DefaultComponent(type);
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
        ComponentListBox.SelectedIndex = Math.Min(index, _components.Count - 1);
        DrawPreview();
    }

    private void MoveUp_Click(object sender, RoutedEventArgs e)
        => MoveSelected(-1);

    private void MoveDown_Click(object sender, RoutedEventArgs e)
        => MoveSelected(1);

    private void MoveSelected(int delta)
    {
        var index = ComponentListBox.SelectedIndex;
        var target = index + delta;
        if (index < 0 || target < 0 || target >= _components.Count)
        {
            return;
        }
        var item = _components[index];
        _components.RemoveAt(index);
        _components.Insert(target, item);
        ComponentListBox.SelectedIndex = target;
        DrawPreview();
    }

    private void RefreshSelectedComponentInputs()
    {
        _loading = true;
        var component = SelectedComponent;
        var hasSelection = component != null;
        ComponentTypeComboBox.IsEnabled = hasSelection;
        ConstraintModeComboBox.IsEnabled = hasSelection;
        SlopeBox.IsEnabled = hasSelection;
        HeightBox.IsEnabled = hasSelection;
        WidthBox.IsEnabled = hasSelection;
        GroundSearchIncrementBox.IsEnabled = hasSelection;
        ColorRBox.IsEnabled = hasSelection;
        ColorGBox.IsEnabled = hasSelection;
        ColorBBox.IsEnabled = hasSelection;

        if (component == null)
        {
            _loading = false;
            return;
        }

        ResolveGeometry(component);
        SelectComboValue(ComponentTypeComboBox, component.Type);
        SelectComboValue(ConstraintModeComboBox, component.ConstraintMode);
        SlopeBox.Text = component.Slope.ToString("0.###", CultureInfo.InvariantCulture);
        HeightBox.Text = component.Height.ToString("0.###", CultureInfo.InvariantCulture);
        WidthBox.Text = component.Width.ToString("0.###", CultureInfo.InvariantCulture);
        GroundSearchIncrementBox.Text = component.GroundSearchHeightIncrement.ToString("0.###", CultureInfo.InvariantCulture);
        ColorRBox.Text = component.ColorR.ToString(CultureInfo.InvariantCulture);
        ColorGBox.Text = component.ColorG.ToString(CultureInfo.InvariantCulture);
        ColorBBox.Text = component.ColorB.ToString(CultureInfo.InvariantCulture);
        ColorPreviewBorder.Background = new SolidColorBrush(Color.FromRgb(ClampByte(component.ColorR), ClampByte(component.ColorG), ClampByte(component.ColorB)));

        SlopeBox.IsEnabled = component.ConstraintMode != SlopeGeometryConstraintMode.HeightAndWidth;
        HeightBox.IsEnabled = component.ConstraintMode != SlopeGeometryConstraintMode.SlopeAndWidth;
        WidthBox.IsEnabled = component.ConstraintMode != SlopeGeometryConstraintMode.SlopeAndHeight;
        _loading = false;
    }

    private void ApplyInputsToSelectedComponent()
    {
        var component = SelectedComponent;
        if (component == null)
        {
            return;
        }
        component.Type = SelectedValue(ComponentTypeComboBox, component.Type);
        component.ConstraintMode = SelectedValue(ConstraintModeComboBox, component.ConstraintMode);
        component.Slope = ReadDouble(SlopeBox.Text, component.Slope);
        component.Height = Math.Max(0.0, ReadDouble(HeightBox.Text, component.Height));
        component.Width = Math.Max(0.0, ReadDouble(WidthBox.Text, component.Width));
        component.GroundSearchHeightIncrement = Math.Max(0.0, ReadDouble(GroundSearchIncrementBox.Text, component.GroundSearchHeightIncrement));
        component.ColorR = ClampColor(ReadInt(ColorRBox.Text, component.ColorR));
        component.ColorG = ClampColor(ReadInt(ColorGBox.Text, component.ColorG));
        component.ColorB = ClampColor(ReadInt(ColorBBox.Text, component.ColorB));
        ResolveGeometry(component);
    }

    private static void ResolveGeometry(SlopeComponentDto component)
    {
        const double tolerance = 1.0e-9;
        switch (component.ConstraintMode)
        {
            case SlopeGeometryConstraintMode.SlopeAndHeight:
                if (Math.Abs(component.Slope) > tolerance)
                {
                    component.Width = component.Height / Math.Abs(component.Slope);
                }
                break;
            case SlopeGeometryConstraintMode.SlopeAndWidth:
                component.Height = Math.Abs(component.Width * component.Slope);
                break;
            case SlopeGeometryConstraintMode.HeightAndWidth:
                if (component.Width > tolerance)
                {
                    var sign = component.Type switch
                    {
                        SlopeComponentType.CutSlope => 1.0,
                        SlopeComponentType.FillSlope => -1.0,
                        _ => Math.Abs(component.Slope) > tolerance ? Math.Sign(component.Slope) : -1.0,
                    };
                    component.Slope = sign * component.Height / component.Width;
                }
                break;
        }
    }

    private void DrawPreview()
    {
        if (PreviewCanvas.ActualWidth <= 1 || PreviewCanvas.ActualHeight <= 1)
        {
            return;
        }
        PreviewCanvas.Children.Clear();

        var segments = _components.Select(component =>
        {
            ResolveGeometry(component);
            return component;
        }).ToList();

        var points = new System.Collections.Generic.List<Point> { new(0, 0) };
        var x = 0.0;
        var y = 0.0;
        foreach (var component in segments)
        {
            x += component.Width;
            y += component.Width * component.Slope;
            points.Add(new Point(x, y));
        }
        if (points.Count < 2)
        {
            return;
        }

        var minX = points.Min(p => p.X);
        var maxX = points.Max(p => p.X);
        var minY = points.Min(p => p.Y);
        var maxY = points.Max(p => p.Y);
        var pad = 36.0;
        var sx = (PreviewCanvas.ActualWidth - pad * 2) / Math.Max(1.0, maxX - minX);
        var sy = (PreviewCanvas.ActualHeight - pad * 2) / Math.Max(1.0, maxY - minY);
        var scale = Math.Max(0.1, Math.Min(sx, sy));

        Point Map(Point p)
            => new(
                pad + (p.X - minX) * scale,
                PreviewCanvas.ActualHeight - pad - (p.Y - minY) * scale);

        var axis = new Line
        {
            X1 = pad,
            X2 = PreviewCanvas.ActualWidth - pad,
            Y1 = Map(new Point(0, 0)).Y,
            Y2 = Map(new Point(0, 0)).Y,
            Stroke = new SolidColorBrush(Color.FromRgb(210, 216, 224)),
            StrokeThickness = 1,
        };
        PreviewCanvas.Children.Add(axis);

        for (var i = 1; i < points.Count; i++)
        {
            var component = segments[i - 1];
            var start = Map(points[i - 1]);
            var end = Map(points[i]);
            PreviewCanvas.Children.Add(new Line
            {
                X1 = start.X,
                Y1 = start.Y,
                X2 = end.X,
                Y2 = end.Y,
                Stroke = new SolidColorBrush(Color.FromRgb(ClampByte(component.ColorR), ClampByte(component.ColorG), ClampByte(component.ColorB))),
                StrokeThickness = ComponentListBox.SelectedIndex == i - 1 ? 3.0 : 2.0,
                StrokeStartLineCap = PenLineCap.Round,
                StrokeEndLineCap = PenLineCap.Round,
            });
        }
    }

    private void Ok_Click(object sender, RoutedEventArgs e)
    {
        ApplyInputsToSelectedComponent();
        Response = new SlopeTemplateDialogResponse
        {
            Accepted = true,
            Handle = _request.Handle,
            InsertionX = _request.InsertionX,
            InsertionY = _request.InsertionY,
            InsertionZ = _request.InsertionZ,
            TemplateName = string.IsNullOrWhiteSpace(TemplateNameBox.Text) ? "边坡模板1" : TemplateNameBox.Text.Trim(),
            DisplayScale = SelectedValue(DisplayScaleComboBox, 10.0),
            Kind = SelectedValue(KindComboBox, SlopeTemplateKind.Fill),
            StopAtGround = StopAtGroundCheckBox.IsChecked == true,
            RepeatLastGroupUntilGround = RepeatLastGroupCheckBox.IsChecked == true,
            Components = _components.Select(component => component.Clone()).ToList(),
        };
        DialogResult = true;
    }

    private static System.Collections.Generic.List<SlopeComponentDto> BuildDefaults(SlopeTemplateKind kind)
        => new()
        {
            DefaultSlope(kind),
            DefaultBerm(kind),
            DefaultSlope(kind),
            DefaultBerm(kind),
            DefaultSlope(kind),
        };

    private static SlopeComponentDto DefaultSlope(SlopeTemplateKind kind)
        => new()
        {
            Type = kind == SlopeTemplateKind.Cut ? SlopeComponentType.CutSlope : SlopeComponentType.FillSlope,
            ConstraintMode = SlopeGeometryConstraintMode.SlopeAndHeight,
            Slope = kind == SlopeTemplateKind.Cut ? 1.0 / 1.5 : -1.0 / 1.5,
            Height = 4.0,
            Width = 6.0,
            GroundSearchHeightIncrement = 2.0,
            ColorR = kind == SlopeTemplateKind.Cut ? 190 : 30,
            ColorG = kind == SlopeTemplateKind.Cut ? 90 : 132,
            ColorB = kind == SlopeTemplateKind.Cut ? 45 : 88,
        };

    private static SlopeComponentDto DefaultBerm(SlopeTemplateKind kind)
        => new()
        {
            Type = SlopeComponentType.Berm,
            ConstraintMode = SlopeGeometryConstraintMode.SlopeAndWidth,
            Slope = kind == SlopeTemplateKind.Cut ? 0.03 : -0.03,
            Height = 0.03,
            Width = 1.0,
            ColorR = 90,
            ColorG = 110,
            ColorB = 130,
        };

    private SlopeComponentDto DefaultComponent(SlopeComponentType type)
    {
        if (type == SlopeComponentType.Berm)
        {
            return DefaultBerm(SelectedValue(KindComboBox, SlopeTemplateKind.Fill));
        }
        return type == SlopeComponentType.CutSlope
            ? DefaultSlope(SlopeTemplateKind.Cut)
            : DefaultSlope(SlopeTemplateKind.Fill);
    }

    private static double ReadDouble(string text, double fallback)
        => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var value) ? value : fallback;

    private static int ReadInt(string text, int fallback)
        => int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out var value) ? value : fallback;

    private static int ClampColor(int value)
        => Math.Max(0, Math.Min(255, value));

    private static byte ClampByte(int value)
        => (byte)ClampColor(value);

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
}
