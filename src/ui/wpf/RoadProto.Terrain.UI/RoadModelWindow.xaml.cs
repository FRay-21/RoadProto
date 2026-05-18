using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.Runtime.CompilerServices;
using System.Windows;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class RoadModelWindow : Window, INotifyPropertyChanged
{
    private readonly RoadModelDialogRequest _request;
    private string _sampleIntervalText = "10";
    private RoadModelTemplateAssignmentDto? _selectedAssignment;

    public RoadModelWindow(RoadModelDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        RoadCenterlineHandle = request.RoadCenterlineHandle;
        ProfileVerticalCurveHandle = request.ProfileVerticalCurveHandle;
        SampleIntervalText = PositiveOrFallback(request.SampleInterval, 10.0).ToString("G", CultureInfo.InvariantCulture);

        foreach (var assignment in request.Assignments)
        {
            Assignments.Add(assignment.Clone());
        }

        DataContext = this;
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public RoadModelDialogResponse? Response { get; private set; }

    public ObservableCollection<RoadModelTemplateAssignmentDto> Assignments { get; } = new();

    public string RoadCenterlineHandle { get; }

    public string ProfileVerticalCurveHandle { get; }

    public string SampleIntervalText
    {
        get => _sampleIntervalText;
        set
        {
            if (_sampleIntervalText == value)
            {
                return;
            }

            _sampleIntervalText = value;
            OnPropertyChanged();
        }
    }

    public RoadModelTemplateAssignmentDto? SelectedAssignment
    {
        get => _selectedAssignment;
        set
        {
            if (_selectedAssignment == value)
            {
                return;
            }

            _selectedAssignment = value;
            OnPropertyChanged();
        }
    }

    private void OnAddAssignment(object sender, RoutedEventArgs e)
    {
        var assignment = new RoadModelTemplateAssignmentDto();
        Assignments.Add(assignment);
        SelectedAssignment = assignment;
    }

    private void OnDeleteAssignment(object sender, RoutedEventArgs e)
    {
        if (SelectedAssignment == null)
        {
            return;
        }

        var index = Assignments.IndexOf(SelectedAssignment);
        Assignments.Remove(SelectedAssignment);
        if (Assignments.Count > 0)
        {
            SelectedAssignment = Assignments[System.Math.Min(index, Assignments.Count - 1)];
        }
    }

    private void OnMoveAssignmentUp(object sender, RoutedEventArgs e)
        => MoveAssignment(-1);

    private void OnMoveAssignmentDown(object sender, RoutedEventArgs e)
        => MoveAssignment(1);

    private void MoveAssignment(int delta)
    {
        if (SelectedAssignment == null)
        {
            return;
        }

        var oldIndex = Assignments.IndexOf(SelectedAssignment);
        var newIndex = oldIndex + delta;
        if (oldIndex < 0 || newIndex < 0 || newIndex >= Assignments.Count)
        {
            return;
        }

        Assignments.Move(oldIndex, newIndex);
    }

    private void OnPickTemplate(object sender, RoutedEventArgs e)
    {
        MessageBox.Show(
            this,
            "第一版请手动填写模板 handle",
            "路基模板",
            MessageBoxButton.OK,
            MessageBoxImage.Information);
    }

    private void OnGenerateModel(object sender, RoutedEventArgs e)
    {
        ErrorTextBlock.Text = string.Empty;
        if (!TryReadSampleInterval(out var sampleInterval) || sampleInterval <= 0.0 || !IsFinite(sampleInterval))
        {
            ErrorTextBlock.Text = "采样间隔必须是大于 0 的有效数字。";
            return;
        }

        Response = BuildResponse(accepted: true, sampleInterval);
        DialogResult = true;
    }

    private RoadModelDialogResponse BuildResponse(bool accepted, double sampleInterval)
        => new()
        {
            Accepted = accepted,
            Handle = _request.Handle,
            RoadCenterlineHandle = RoadCenterlineHandle,
            ProfileVerticalCurveHandle = ProfileVerticalCurveHandle,
            SampleInterval = sampleInterval,
            Assignments = new(Assignments),
        };

    private void OnCancel(object sender, RoutedEventArgs e)
    {
        Response = BuildResponse(false, PositiveOrFallback(_request.SampleInterval, 10.0));
        DialogResult = false;
    }

    private bool TryReadSampleInterval(out double sampleInterval)
    {
        return double.TryParse(SampleIntervalText, NumberStyles.Float, CultureInfo.InvariantCulture, out sampleInterval)
            || double.TryParse(SampleIntervalText, NumberStyles.Float, CultureInfo.CurrentCulture, out sampleInterval);
    }

    private static double PositiveOrFallback(double value, double fallback)
        => IsFinite(value) && value > 0.0 ? value : fallback;

    private static bool IsFinite(double value)
        => !double.IsNaN(value) && !double.IsInfinity(value);

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
}
