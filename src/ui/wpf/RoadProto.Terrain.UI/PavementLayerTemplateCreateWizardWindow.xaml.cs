using System;
using System.Collections.Generic;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class PavementLayerTemplateCreateWizardWindow : Window
{
    private readonly PavementLayerTemplateDialogRequest _request;
    private readonly List<LayerRowControls> _layerRows = new();
    private bool _loading;
    private PavementLayerTemplateDto _template = new();

    public PavementLayerTemplateCreateWizardWindow(PavementLayerTemplateDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        LoadOptions();
    }

    public PavementLayerTemplateDto? SelectedTemplate { get; private set; }

    private void LoadOptions()
    {
        _loading = true;
        PavementTypeBox.ItemsSource = PavementLayerTemplatePresetFactory.PavementTypeOptions;
        PavementTypeBox.DisplayMemberPath = nameof(PavementLayerTemplatePresetOption<PavementSurfaceType>.Label);
        SelectComboValue(PavementTypeBox, _request.PavementType);
        RefreshRoadSegmentOptions();
        _loading = false;
        RefreshTemplate();
    }

    private void PavementTypeBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (_loading)
        {
            return;
        }

        RefreshRoadSegmentOptions();
        RefreshTemplate();
    }

    private void RoadSegmentTypeBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        if (!_loading)
        {
            RefreshTemplate();
        }
    }

    private void RefreshRoadSegmentOptions()
    {
        var pavementType = SelectedValue(PavementTypeBox, PavementSurfaceType.Asphalt);
        RoadSegmentTypeBox.ItemsSource = PavementLayerTemplatePresetFactory.RoadSegmentOptions(pavementType);
        RoadSegmentTypeBox.DisplayMemberPath = nameof(PavementLayerTemplatePresetOption<PavementLayerTemplateRoadSegmentType>.Label);
        RoadSegmentTypeBox.SelectedIndex = 0;
    }

    private void RefreshTemplate()
    {
        var pavementType = SelectedValue(PavementTypeBox, PavementSurfaceType.Asphalt);
        var roadSegmentType = SelectedValue(RoadSegmentTypeBox, PavementLayerTemplateRoadSegmentType.MainlineLane);
        _template = PavementLayerTemplatePresetFactory.Create(pavementType, roadSegmentType);
        _template.DisplayScale = _request.DisplayScale <= 0.0 ? 10.0 : _request.DisplayScale;
        RebuildLayerRows();
    }

    private void RebuildLayerRows()
    {
        _layerRows.Clear();
        WizardLayersPanel.Children.Clear();
        for (var i = 0; i < _template.Layers.Count; i++)
        {
            var layer = _template.Layers[i];
            var row = CreateLayerRow(layer);
            WizardLayersPanel.Children.Add(row.Element);
            _layerRows.Add(row);
        }
    }

    private LayerRowControls CreateLayerRow(PavementLayerTemplateLayerDto layer)
    {
        var grid = new Grid { Margin = new Thickness(0, 0, 0, 8) };
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(86) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(66) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(88) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(66) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(88) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(66) });
        grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(88) });

        grid.Children.Add(new TextBlock
        {
            Text = PavementLayerTemplateLabels.LayerTypeLabel(layer.Type),
            VerticalAlignment = VerticalAlignment.Center,
        });

        var nameBox = new TextBox { Height = 28, Text = layer.Name, Margin = new Thickness(0, 0, 14, 0) };
        var thicknessBox = new TextBox { Height = 28, Text = Format(layer.Thickness) };
        var wideningBox = new TextBox { Height = 28, Text = Format(layer.InnerWidening) };
        var slopeBox = new TextBox { Height = 28, Text = Format(layer.InnerSlope) };

        AddCell(grid, nameBox, 1);
        AddLabel(grid, "厚度", 2);
        AddCell(grid, thicknessBox, 3);
        AddLabel(grid, "加宽", 4);
        AddCell(grid, wideningBox, 5);
        AddLabel(grid, "坡度", 6);
        AddCell(grid, slopeBox, 7);

        return new LayerRowControls(layer, grid, nameBox, thicknessBox, wideningBox, slopeBox);
    }

    private static void AddLabel(Grid grid, string text, int column)
    {
        var label = new TextBlock
        {
            Text = text,
            VerticalAlignment = VerticalAlignment.Center,
            HorizontalAlignment = HorizontalAlignment.Right,
            Margin = new Thickness(0, 0, 8, 0),
        };
        Grid.SetColumn(label, column);
        grid.Children.Add(label);
    }

    private static void AddCell(Grid grid, Control control, int column)
    {
        Grid.SetColumn(control, column);
        grid.Children.Add(control);
    }

    private void Ok_Click(object sender, RoutedEventArgs e)
    {
        ApplyLayerRows();
        SelectedTemplate = PavementLayerTemplateLabels.CloneTemplate(_template);
        DialogResult = true;
    }

    private void Cancel_Click(object sender, RoutedEventArgs e)
    {
        SelectedTemplate = null;
        DialogResult = false;
    }

    private void ApplyLayerRows()
    {
        foreach (var row in _layerRows)
        {
            var layer = row.Layer;
            layer.Name = string.IsNullOrWhiteSpace(row.NameBox.Text)
                ? PavementLayerTemplateLabels.LayerTypeLabel(layer.Type)
                : row.NameBox.Text.Trim();
            layer.Thickness = Math.Max(0.001, ReadDouble(row.ThicknessBox.Text, layer.Thickness));
            layer.InnerThickness = layer.Thickness;
            layer.OuterThickness = layer.Thickness;
            layer.InnerWidening = ReadDouble(row.WideningBox.Text, layer.InnerWidening);
            layer.OuterWidening = layer.InnerWidening;
            layer.InnerSlope = ReadDouble(row.SlopeBox.Text, layer.InnerSlope);
            layer.OuterSlope = layer.InnerSlope;
        }
    }

    private static double ReadDouble(string text, double fallback)
        => double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out var value)
            && !double.IsNaN(value)
            && !double.IsInfinity(value)
                ? value
                : fallback;

    private static string Format(double value)
        => value.ToString("0.###", CultureInfo.InvariantCulture);

    private static void SelectComboValue<T>(ComboBox comboBox, T value)
    {
        for (var i = 0; i < comboBox.Items.Count; i++)
        {
            if (comboBox.Items[i] is PavementLayerTemplatePresetOption<T> option && Equals(option.Value, value))
            {
                comboBox.SelectedIndex = i;
                return;
            }
        }
        comboBox.SelectedIndex = comboBox.Items.Count > 0 ? 0 : -1;
    }

    private static T SelectedValue<T>(ComboBox comboBox, T fallback)
        => comboBox.SelectedItem is PavementLayerTemplatePresetOption<T> option ? option.Value : fallback;

    private sealed class LayerRowControls
    {
        public LayerRowControls(
            PavementLayerTemplateLayerDto layer,
            FrameworkElement element,
            TextBox nameBox,
            TextBox thicknessBox,
            TextBox wideningBox,
            TextBox slopeBox)
        {
            Layer = layer;
            Element = element;
            NameBox = nameBox;
            ThicknessBox = thicknessBox;
            WideningBox = wideningBox;
            SlopeBox = slopeBox;
        }

        public PavementLayerTemplateLayerDto Layer { get; }
        public FrameworkElement Element { get; }
        public TextBox NameBox { get; }
        public TextBox ThicknessBox { get; }
        public TextBox WideningBox { get; }
        public TextBox SlopeBox { get; }
    }
}
