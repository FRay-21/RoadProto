using System;
using System.Collections.Generic;
using System.Globalization;
using System.Windows;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class AlignmentCenterlineWindow : Window
{
    private static readonly string[] RoadGrades =
    [
        "高速公路",
        "一级道路",
        "二级道路",
        "三级道路",
        "四级道路",
        "城市快速路",
        "城市主干道",
        "城市次干道",
        "城市支路",
        "其他道路",
        "等外公路",
    ];

    private readonly AlignmentDialogRequest _request;
    private readonly List<AlignmentCurveParameterDto> _curveParameters;
    private double _stationInterval;
    private int _currentCurveIndex;

    public AlignmentCenterlineWindow(AlignmentDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        _curveParameters = CloneCurveParameters(request);
        _currentCurveIndex = Clamp(request.CurveIndex, 0, _curveParameters.Count - 1);
        _stationInterval = request.StationLabelInterval > 0 ? request.StationLabelInterval : 100.0;
        LoadRequest();
    }

    public AlignmentDialogResponse? Response { get; private set; }

    private void LoadRequest()
    {
        RoadGradeComboBox.ItemsSource = RoadGrades;
        RoadNameTextBox.Text = string.IsNullOrWhiteSpace(_request.RoadName) ? "K1" : _request.RoadName;
        RoadGradeComboBox.SelectedIndex = Clamp(_request.RoadGradeIndex, 0, RoadGrades.Length - 1);
        LinkedTerrainCheckBox.IsChecked = _request.LinkedTerrainEnabled;
        LinkedTerrainHandleTextBox.Text = _request.LinkedTerrainHandle;
        StationIntervalTextBox.Text = _stationInterval.ToString("G", CultureInfo.InvariantCulture);

        LoadCurveParameter(_currentCurveIndex);
        UpdateCurveNavigation();

        if (_request.Mode == AlignmentDialogMode.Curve)
        {
            PropertiesTab.Visibility = System.Windows.Visibility.Collapsed;
            EditorTabs.SelectedItem = CurveTab;
            SubtitleText.Text = "编辑平曲线参数";
        }
        else if (_request.Mode == AlignmentDialogMode.Properties)
        {
            CurveTab.Visibility = System.Windows.Visibility.Collapsed;
            EditorTabs.SelectedItem = PropertiesTab;
            SubtitleText.Text = "编辑道路属性";
        }
    }

    private void LoadCurveParameter(int index)
    {
        var curve = _curveParameters[index];
        TangentInTextBox.Text = curve.TangentIn.ToString("G", CultureInfo.InvariantCulture);
        TangentOutTextBox.Text = curve.TangentOut.ToString("G", CultureInfo.InvariantCulture);
        Ls1TextBox.Text = curve.Ls1.ToString("G", CultureInfo.InvariantCulture);
        RadiusTextBox.Text = curve.Radius.ToString("G", CultureInfo.InvariantCulture);
        Ls2TextBox.Text = curve.Ls2.ToString("G", CultureInfo.InvariantCulture);
    }

    private void UpdateCurveNavigation()
    {
        CurveTitleTextBlock.Text = $"第{_currentCurveIndex + 1}个平曲线";
        PreviousCurveButton.IsEnabled = _currentCurveIndex > 0;
        NextCurveButton.IsEnabled = _currentCurveIndex + 1 < _curveParameters.Count;
    }

    private void PreviousCurveButton_Click(object sender, RoutedEventArgs e)
    {
        NavigateCurve(-1);
    }

    private void NextCurveButton_Click(object sender, RoutedEventArgs e)
    {
        NavigateCurve(1);
    }

    private void NavigateCurve(int offset)
    {
        ErrorTextBlock.Text = string.Empty;
        if (!TrySaveCurrentCurveParameter())
        {
            return;
        }

        _currentCurveIndex = Clamp(_currentCurveIndex + offset, 0, _curveParameters.Count - 1);
        LoadCurveParameter(_currentCurveIndex);
        UpdateCurveNavigation();
    }

    private void StationSettingsButton_Click(object sender, RoutedEventArgs e)
    {
        var settingsWindow = new StationLabelSettingsWindow(_stationInterval)
        {
            Owner = this,
        };

        if (settingsWindow.ShowDialog() == true)
        {
            _stationInterval = settingsWindow.Interval;
            StationIntervalTextBox.Text = _stationInterval.ToString("G", CultureInfo.InvariantCulture);
        }
    }

    private void SelectTerrainButton_Click(object sender, RoutedEventArgs e)
    {
        ErrorTextBlock.Text = string.Empty;
        if (!TrySaveCurrentCurveParameter())
        {
            return;
        }

        Response = CreateResponse(false);
        Response.Action = AlignmentDialogAction.PickTerrain;
        DialogResult = true;
    }

    private void OkButton_Click(object sender, RoutedEventArgs e)
    {
        ErrorTextBlock.Text = string.Empty;
        if (!TrySaveCurrentCurveParameter())
        {
            return;
        }

        if (_stationInterval <= 0)
        {
            ErrorTextBlock.Text = "桩号标注间距必须大于 0。";
            return;
        }

        Response = CreateResponse(true);
        DialogResult = true;
    }

    private void CancelButton_Click(object sender, RoutedEventArgs e)
    {
        Response = CreateResponse(false);
        DialogResult = false;
    }

    private bool TrySaveCurrentCurveParameter()
    {
        if (!TryReadDouble(TangentInTextBox.Text, out var tangentIn)
            || !TryReadDouble(TangentOutTextBox.Text, out var tangentOut)
            || !TryReadDouble(Ls1TextBox.Text, out var ls1)
            || !TryReadDouble(RadiusTextBox.Text, out var radius)
            || !TryReadDouble(Ls2TextBox.Text, out var ls2))
        {
            ErrorTextBlock.Text = "平曲线参数必须是有效数字。";
            return false;
        }

        if (radius <= 0 || ls1 < 0 || ls2 < 0 || tangentIn < 0 || tangentOut < 0)
        {
            ErrorTextBlock.Text = "半径必须大于 0，切线长和缓和曲线长不能为负。";
            return false;
        }

        _curveParameters[_currentCurveIndex] = new AlignmentCurveParameterDto
        {
            TangentIn = tangentIn,
            TangentOut = tangentOut,
            Ls1 = ls1,
            Radius = radius,
            Ls2 = ls2,
        };
        return true;
    }

    private AlignmentDialogResponse CreateResponse(bool accepted)
    {
        var selected = _curveParameters[_currentCurveIndex];
        return new AlignmentDialogResponse
        {
            Accepted = accepted,
            Mode = _request.Mode,
            Handle = _request.Handle,
            DeleteOnCancel = _request.DeleteOnCancel,
            CurveIndex = _currentCurveIndex,
            RoadName = string.IsNullOrWhiteSpace(RoadNameTextBox.Text) ? "K1" : RoadNameTextBox.Text.Trim(),
            RoadGradeIndex = RoadGradeComboBox.SelectedIndex < 0 ? 9 : RoadGradeComboBox.SelectedIndex,
            LinkedTerrainEnabled = LinkedTerrainCheckBox.IsChecked == true,
            LinkedTerrainHandle = LinkedTerrainHandleTextBox.Text.Trim(),
            StationLabelInterval = _stationInterval,
            TangentIn = selected.TangentIn,
            TangentOut = selected.TangentOut,
            Ls1 = selected.Ls1,
            Radius = selected.Radius,
            Ls2 = selected.Ls2,
            CurveParameters = CloneCurveParameters(_curveParameters),
        };
    }

    private static List<AlignmentCurveParameterDto> CloneCurveParameters(AlignmentDialogRequest request)
    {
        if (request.CurveParameters.Count > 0)
        {
            return CloneCurveParameters(request.CurveParameters);
        }

        return
        [
            new AlignmentCurveParameterDto
            {
                TangentIn = request.TangentIn,
                TangentOut = request.TangentOut,
                Ls1 = request.Ls1,
                Radius = request.Radius,
                Ls2 = request.Ls2,
            },
        ];
    }

    private static List<AlignmentCurveParameterDto> CloneCurveParameters(
        IReadOnlyList<AlignmentCurveParameterDto> parameters)
    {
        var result = new List<AlignmentCurveParameterDto>(parameters.Count);
        foreach (var parameter in parameters)
        {
            result.Add(new AlignmentCurveParameterDto
            {
                TangentIn = parameter.TangentIn,
                TangentOut = parameter.TangentOut,
                Ls1 = parameter.Ls1,
                Radius = parameter.Radius,
                Ls2 = parameter.Ls2,
            });
        }

        return result.Count > 0 ? result : [new AlignmentCurveParameterDto()];
    }

    private static bool TryReadDouble(string text, out double value)
    {
        return double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value)
            || double.TryParse(text, NumberStyles.Float, CultureInfo.CurrentCulture, out value);
    }

    private static int Clamp(int value, int min, int max)
    {
        if (max < min)
        {
            return min;
        }

        if (value < min)
        {
            return min;
        }

        return value > max ? max : value;
    }
}
