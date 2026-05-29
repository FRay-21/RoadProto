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
        for (var i = 0; i < 6; i++)
        {
            grid.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(82) });
        }

        grid.Children.Add(new TextBlock
        {
            Text = PavementLayerTemplateLabels.LayerTypeLabel(layer.Type),
            VerticalAlignment = VerticalAlignment.Center,
        });

        var nameBox = new TextBox { Height = 28, Text = layer.Name, Margin = new Thickness(0, 0, 14, 0) };
        var innerThicknessBox = new TextBox { Height = 28, Text = Format(layer.InnerThickness) };
        var outerThicknessBox = new TextBox { Height = 28, Text = Format(layer.OuterThickness) };
        var innerWideningBox = new TextBox { Height = 28, Text = Format(layer.InnerWidening) };
        var outerWideningBox = new TextBox { Height = 28, Text = Format(layer.OuterWidening) };
        var innerSlopeBox = new TextBox { Height = 28, Text = Format(layer.InnerSlope) };
        var outerSlopeBox = new TextBox { Height = 28, Text = Format(layer.OuterSlope) };

        AddCell(grid, nameBox, 1);
        AddCell(grid, innerThicknessBox, 2);
        AddCell(grid, outerThicknessBox, 3);
        AddCell(grid, innerWideningBox, 4);
        AddCell(grid, outerWideningBox, 5);
        AddCell(grid, innerSlopeBox, 6);
        AddCell(grid, outerSlopeBox, 7);

        return new LayerRowControls(
            layer,
            grid,
            nameBox,
            innerThicknessBox,
            outerThicknessBox,
            innerWideningBox,
            outerWideningBox,
            innerSlopeBox,
            outerSlopeBox);
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
            layer.InnerThickness = Math.Max(0.001, ReadDouble(row.InnerThicknessBox.Text, layer.InnerThickness));
            layer.OuterThickness = Math.Max(0.001, ReadDouble(row.OuterThicknessBox.Text, layer.OuterThickness));
            layer.UniformThickness = NearlyEqual(layer.InnerThickness, layer.OuterThickness);
            layer.Thickness = layer.UniformThickness
                ? layer.InnerThickness
                : Math.Max(layer.InnerThickness, layer.OuterThickness);
            layer.InnerWidening = ReadDouble(row.InnerWideningBox.Text, layer.InnerWidening);
            layer.OuterWidening = ReadDouble(row.OuterWideningBox.Text, layer.OuterWidening);
            layer.InnerSlope = ReadDouble(row.InnerSlopeBox.Text, layer.InnerSlope);
            layer.OuterSlope = ReadDouble(row.OuterSlopeBox.Text, layer.OuterSlope);
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

    private static bool NearlyEqual(double first, double second)
        => Math.Abs(first - second) < 1.0e-9;

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
            TextBox innerThicknessBox,
            TextBox outerThicknessBox,
            TextBox innerWideningBox,
            TextBox outerWideningBox,
            TextBox innerSlopeBox,
            TextBox outerSlopeBox)
        {
            Layer = layer;
            Element = element;
            NameBox = nameBox;
            InnerThicknessBox = innerThicknessBox;
            OuterThicknessBox = outerThicknessBox;
            InnerWideningBox = innerWideningBox;
            OuterWideningBox = outerWideningBox;
            InnerSlopeBox = innerSlopeBox;
            OuterSlopeBox = outerSlopeBox;
        }

        public PavementLayerTemplateLayerDto Layer { get; }
        public FrameworkElement Element { get; }
        public TextBox NameBox { get; }
        public TextBox InnerThicknessBox { get; }
        public TextBox OuterThicknessBox { get; }
        public TextBox InnerWideningBox { get; }
        public TextBox OuterWideningBox { get; }
        public TextBox InnerSlopeBox { get; }
        public TextBox OuterSlopeBox { get; }
    }
}
