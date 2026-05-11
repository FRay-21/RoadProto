using System;
using System.Collections.Generic;
using System.Globalization;
using System.Windows;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class ProfileGradeGraphWindow : Window
{
    private static readonly List<ColorOption> GroundLineColors =
    [
        new("红色 / ACI 1", 1),
        new("黄色 / ACI 2", 2),
        new("绿色 / ACI 3", 3),
        new("青绿色 / ACI 4", 4),
        new("蓝色 / ACI 5", 5),
        new("洋红 / ACI 6", 6),
        new("白色 / ACI 7", 7),
    ];

    private static readonly List<ScaleOption> VerticalScales =
    [
        new("1:1", 1.0),
        new("1:10", 10.0),
        new("1:100", 100.0),
    ];

    private readonly ProfileGradeGraphDialogRequest _request;

    public ProfileGradeGraphWindow(ProfileGradeGraphDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        LoadRequest();
    }

    public ProfileGradeGraphDialogResponse? Response { get; private set; }

    private void LoadRequest()
    {
        var colorOptions = new List<ColorOption>(GroundLineColors);
        if (!colorOptions.Exists(option => option.Index == _request.GroundLineColorIndex))
        {
            colorOptions.Add(new ColorOption($"ACI {_request.GroundLineColorIndex}", _request.GroundLineColorIndex));
        }

        GroundLineColorComboBox.ItemsSource = colorOptions;
        GroundLineColorComboBox.SelectedValue = _request.GroundLineColorIndex;

        VerticalScaleComboBox.ItemsSource = VerticalScales;
        VerticalScaleComboBox.SelectedValue = IsSupportedVerticalScale(_request.VerticalScale)
            ? _request.VerticalScale
            : 10.0;

        GraphNameTextBox.Text = string.IsNullOrWhiteSpace(_request.GraphName) ? "拉坡图" : _request.GraphName;
        GroundLineWidthTextBox.Text = PositiveOrFallback(_request.GroundLineWidth, 1.0).ToString("G", CultureInfo.InvariantCulture);
        GroundLinePrecisionTextBox.Text = PositiveOrFallback(_request.GroundLinePrecision, 10.0).ToString("G", CultureInfo.InvariantCulture);
        GridSpacingTextBox.Text = PositiveOrFallback(_request.GridSpacing, 10.0).ToString("G", CultureInfo.InvariantCulture);
        SampleCountTextBox.Text = _request.SampleCount.ToString(CultureInfo.InvariantCulture);
        RoadCenterlineHandleTextBox.Text = _request.RoadCenterlineHandle;
        TerrainTinHandleTextBox.Text = _request.TerrainTinHandle;
        DmxFilePathTextBox.Text = _request.DmxFilePath;

        SourceTextBlock.Text = _request.SourceType == ProfileGradeGraphSourceType.DmxFile
            ? "来源: DMX 文件"
            : "来源: 关联地形 TIN";
        UpdateGroundLineButton.IsEnabled = _request.SourceType == ProfileGradeGraphSourceType.DmxFile;
    }

    private void OkButton_Click(object sender, RoutedEventArgs e)
    {
        if (!TryCreateAcceptedResponse(updateGroundLineRequested: false, out var response))
        {
            return;
        }

        Response = response;
        DialogResult = true;
    }

    private void UpdateGroundLineButton_Click(object sender, RoutedEventArgs e)
    {
        if (_request.SourceType != ProfileGradeGraphSourceType.DmxFile)
        {
            ErrorTextBlock.Text = "数模来源暂不支持从属性窗口更新地面线。";
            return;
        }

        if (!TryCreateAcceptedResponse(updateGroundLineRequested: true, out var response))
        {
            return;
        }

        Response = response;
        DialogResult = true;
    }

    private void CancelButton_Click(object sender, RoutedEventArgs e)
    {
        Response = new ProfileGradeGraphDialogResponse
        {
            Accepted = false,
            Handle = _request.Handle,
            GraphName = _request.GraphName,
            GroundLineColorIndex = _request.GroundLineColorIndex,
            GroundLineWidth = _request.GroundLineWidth,
            GroundLinePrecision = _request.GroundLinePrecision,
            VerticalScale = _request.VerticalScale,
            GridSpacing = _request.GridSpacing,
            UpdateGroundLineRequested = false,
        };
        DialogResult = false;
    }

    private bool TryCreateAcceptedResponse(
        bool updateGroundLineRequested,
        out ProfileGradeGraphDialogResponse response)
    {
        response = new ProfileGradeGraphDialogResponse();
        ErrorTextBlock.Text = string.Empty;

        if (!TryReadDouble(GroundLineWidthTextBox.Text, out var groundLineWidth)
            || !TryReadDouble(GroundLinePrecisionTextBox.Text, out var groundLinePrecision)
            || !TryReadDouble(GridSpacingTextBox.Text, out var gridSpacing))
        {
            ErrorTextBlock.Text = "数值字段必须是有效数字。";
            return false;
        }

        if (groundLineWidth <= 0.0 || groundLinePrecision <= 0.0 || gridSpacing <= 0.0
            || !IsFinite(groundLineWidth) || !IsFinite(groundLinePrecision) || !IsFinite(gridSpacing))
        {
            ErrorTextBlock.Text = "线宽、精度和网格线间距必须大于 0。";
            return false;
        }

        var colorIndex = SelectedColorIndex();
        if (colorIndex < 1 || colorIndex > 255)
        {
            ErrorTextBlock.Text = "地面线颜色 ACI index 必须在 1 到 255 之间。";
            return false;
        }

        var verticalScale = SelectedVerticalScale();
        if (!IsSupportedVerticalScale(verticalScale))
        {
            ErrorTextBlock.Text = "纵向比例只支持 1:1、1:10 或 1:100。";
            return false;
        }

        response = new ProfileGradeGraphDialogResponse
        {
            Accepted = true,
            Handle = _request.Handle,
            GraphName = string.IsNullOrWhiteSpace(GraphNameTextBox.Text) ? "拉坡图" : GraphNameTextBox.Text.Trim(),
            GroundLineColorIndex = colorIndex,
            GroundLineWidth = groundLineWidth,
            GroundLinePrecision = groundLinePrecision,
            VerticalScale = verticalScale,
            GridSpacing = gridSpacing,
            UpdateGroundLineRequested = updateGroundLineRequested,
        };
        return true;
    }

    private int SelectedColorIndex()
    {
        if (GroundLineColorComboBox.SelectedItem is ColorOption option)
        {
            return option.Index;
        }

        return _request.GroundLineColorIndex;
    }

    private double SelectedVerticalScale()
    {
        if (VerticalScaleComboBox.SelectedItem is ScaleOption option)
        {
            return option.Value;
        }

        return IsSupportedVerticalScale(_request.VerticalScale) ? _request.VerticalScale : 10.0;
    }

    private static bool TryReadDouble(string text, out double value)
    {
        return double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value)
            || double.TryParse(text, NumberStyles.Float, CultureInfo.CurrentCulture, out value);
    }

    private static double PositiveOrFallback(double value, double fallback)
        => IsFinite(value) && value > 0.0 ? value : fallback;

    private static bool IsFinite(double value)
        => !double.IsNaN(value) && !double.IsInfinity(value);

    private static bool IsSupportedVerticalScale(double value)
        => value == 1.0 || value == 10.0 || value == 100.0;

    private sealed class ColorOption
    {
        public ColorOption(string name, int index)
        {
            Name = name;
            Index = index;
        }

        public string Name { get; }
        public int Index { get; }
    }

    private sealed class ScaleOption
    {
        public ScaleOption(string name, double value)
        {
            Name = name;
            Value = value;
        }

        public string Name { get; }
        public double Value { get; }
    }
}
