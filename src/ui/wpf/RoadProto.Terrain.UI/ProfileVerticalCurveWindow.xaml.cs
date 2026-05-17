using System;
using System.Collections.Generic;
using System.Globalization;
using System.Windows;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class ProfileVerticalCurveWindow : Window
{
    private readonly ProfileVerticalCurveDialogRequest _request;
    private readonly List<ProfileVerticalCurvePviDto> _pvis;
    private int _currentPviIndex;

    public ProfileVerticalCurveWindow(ProfileVerticalCurveDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        _pvis = ClonePvis(request.Pvis);
        _currentPviIndex = ClampPviIndex(request.CurrentPviIndex, _pvis.Count);
        LoadRequest();
    }

    public ProfileVerticalCurveDialogResponse? Response { get; private set; }

    private void LoadRequest()
    {
        NameTextBox.Text = string.IsNullOrWhiteSpace(_request.Name) ? "竖曲线" : _request.Name;
        StartStationTextBox.Text = ToDisplayText(_request.StartStation);
        StartElevationTextBox.Text = ToDisplayText(_request.StartElevation);
        EndStationTextBox.Text = ToDisplayText(_request.EndStation);
        EndElevationTextBox.Text = ToDisplayText(_request.EndElevation);
        ProfileGraphHandleTextBox.Text = _request.ProfileGraphHandle;
        UpdatePviEditor();
    }

    private void PreviousPviButton_Click(object sender, RoutedEventArgs e)
    {
        if (_pvis.Count == 0 || !TrySaveCurrentPvi())
        {
            return;
        }

        _currentPviIndex = Math.Max(0, _currentPviIndex - 1);
        UpdatePviEditor();
    }

    private void NextPviButton_Click(object sender, RoutedEventArgs e)
    {
        if (_pvis.Count == 0 || !TrySaveCurrentPvi())
        {
            return;
        }

        _currentPviIndex = Math.Min(_pvis.Count - 1, _currentPviIndex + 1);
        UpdatePviEditor();
    }

    private void OkButton_Click(object sender, RoutedEventArgs e)
    {
        if (!TryCreateAcceptedResponse(out var response))
        {
            return;
        }

        Response = response;
        DialogResult = true;
    }

    private void CancelButton_Click(object sender, RoutedEventArgs e)
    {
        Response = CreateRejectedResponse();
        DialogResult = false;
    }

    private void UpdatePviEditor()
    {
        ErrorTextBlock.Text = string.Empty;
        var hasPvi = _pvis.Count > 0;
        PviStationTextBox.IsEnabled = hasPvi;
        PviElevationTextBox.IsEnabled = hasPvi;
        PviRadiusTextBox.IsEnabled = hasPvi;
        PreviousPviButton.IsEnabled = hasPvi && _currentPviIndex > 0;
        NextPviButton.IsEnabled = hasPvi && _currentPviIndex + 1 < _pvis.Count;

        if (!hasPvi)
        {
            PviStatusTextBlock.Text = "无变坡点";
            PviStationTextBox.Text = string.Empty;
            PviElevationTextBox.Text = string.Empty;
            PviRadiusTextBox.Text = string.Empty;
            return;
        }

        _currentPviIndex = ClampPviIndex(_currentPviIndex, _pvis.Count);
        var pvi = _pvis[_currentPviIndex];
        PviStatusTextBlock.Text = $"变坡点 {_currentPviIndex + 1} / {_pvis.Count}";
        PviStationTextBox.Text = ToDisplayText(pvi.Station);
        PviElevationTextBox.Text = ToDisplayText(pvi.Elevation);
        PviRadiusTextBox.Text = ToDisplayText(pvi.Radius);
    }

    private bool TryCreateAcceptedResponse(out ProfileVerticalCurveDialogResponse response)
    {
        response = new ProfileVerticalCurveDialogResponse();
        ErrorTextBlock.Text = string.Empty;

        if (!TrySaveCurrentPvi())
        {
            return false;
        }

        if (!TryReadFiniteDouble(StartStationTextBox.Text, out var startStation)
            || !TryReadFiniteDouble(StartElevationTextBox.Text, out var startElevation)
            || !TryReadFiniteDouble(EndStationTextBox.Text, out var endStation)
            || !TryReadFiniteDouble(EndElevationTextBox.Text, out var endElevation))
        {
            ErrorTextBlock.Text = "数值字段必须是有效数字。";
            return false;
        }

        if (endStation <= startStation)
        {
            ErrorTextBlock.Text = "终点桩号必须大于起点桩号。";
            return false;
        }

        for (var index = 0; index < _pvis.Count; index++)
        {
            var pvi = _pvis[index];
            if (!IsFinite(pvi.Station) || !IsFinite(pvi.Elevation) || !IsFinite(pvi.Radius))
            {
                ErrorTextBlock.Text = $"变坡点 {index + 1} 的数值必须有效。";
                return false;
            }

            if (pvi.Radius < 1.0)
            {
                ErrorTextBlock.Text = $"变坡点 {index + 1} 的半径必须大于等于 1。";
                return false;
            }

            if (pvi.Station <= startStation || pvi.Station >= endStation)
            {
                ErrorTextBlock.Text = $"变坡点 {index + 1} 的桩号必须位于起终点之间。";
                return false;
            }
        }

        var pviStations = new List<double>();
        foreach (var pvi in _pvis)
        {
            pviStations.Add(pvi.Station);
        }
        pviStations.Sort();
        for (var index = 1; index < pviStations.Count; index++)
        {
            if (pviStations[index] <= pviStations[index - 1])
            {
                ErrorTextBlock.Text = "变坡点桩号必须互不相同。";
                return false;
            }
        }

        response = new ProfileVerticalCurveDialogResponse
        {
            Accepted = true,
            Handle = _request.Handle,
            ProfileGraphHandle = _request.ProfileGraphHandle,
            Name = string.IsNullOrWhiteSpace(NameTextBox.Text) ? "竖曲线" : NameTextBox.Text.Trim(),
            StartStation = startStation,
            StartElevation = startElevation,
            EndStation = endStation,
            EndElevation = endElevation,
            CurrentPviIndex = ClampPviIndex(_currentPviIndex, _pvis.Count),
            Pvis = ClonePvis(_pvis),
        };
        return true;
    }

    private bool TrySaveCurrentPvi()
    {
        if (_pvis.Count == 0)
        {
            return true;
        }

        if (!TryReadFiniteDouble(PviStationTextBox.Text, out var station)
            || !TryReadFiniteDouble(PviElevationTextBox.Text, out var elevation)
            || !TryReadFiniteDouble(PviRadiusTextBox.Text, out var radius))
        {
            ErrorTextBlock.Text = "当前变坡点数值必须有效。";
            return false;
        }

        if (radius < 1.0)
        {
            ErrorTextBlock.Text = "当前变坡点半径必须大于等于 1。";
            return false;
        }

        var pvi = _pvis[_currentPviIndex];
        pvi.Station = station;
        pvi.Elevation = elevation;
        pvi.Radius = radius;
        return true;
    }

    private ProfileVerticalCurveDialogResponse CreateRejectedResponse()
        => new()
        {
            Accepted = false,
            Handle = _request.Handle,
            ProfileGraphHandle = _request.ProfileGraphHandle,
            Name = _request.Name,
            StartStation = _request.StartStation,
            StartElevation = _request.StartElevation,
            EndStation = _request.EndStation,
            EndElevation = _request.EndElevation,
            CurrentPviIndex = _request.CurrentPviIndex,
            Pvis = ClonePvis(_request.Pvis),
        };

    private static List<ProfileVerticalCurvePviDto> ClonePvis(IEnumerable<ProfileVerticalCurvePviDto>? pvis)
    {
        var clone = new List<ProfileVerticalCurvePviDto>();
        if (pvis == null)
        {
            return clone;
        }

        foreach (var pvi in pvis)
        {
            clone.Add(new ProfileVerticalCurvePviDto
            {
                Station = pvi.Station,
                Elevation = pvi.Elevation,
                Radius = pvi.Radius,
            });
        }

        return clone;
    }

    private static string ToDisplayText(double value)
        => value.ToString("G", CultureInfo.InvariantCulture);

    private static bool TryReadFiniteDouble(string text, out double value)
    {
        if ((double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value)
                || double.TryParse(text, NumberStyles.Float, CultureInfo.CurrentCulture, out value))
            && IsFinite(value))
        {
            return true;
        }

        value = 0.0;
        return false;
    }

    private static bool IsFinite(double value)
        => !double.IsNaN(value) && !double.IsInfinity(value);

    private static int ClampPviIndex(int index, int count)
    {
        if (count <= 0)
        {
            return 0;
        }

        if (index < 0)
        {
            return 0;
        }

        return index >= count ? count - 1 : index;
    }
}
