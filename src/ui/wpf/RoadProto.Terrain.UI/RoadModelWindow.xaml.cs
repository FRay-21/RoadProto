using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Data;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class RoadModelWindow : Window, INotifyPropertyChanged
{
    private readonly RoadModelDialogRequest _request;
    private string _sampleIntervalText = "10";
    private string _leftSlopeSearchWidthText = "50";
    private string _rightSlopeSearchWidthText = "50";
    private RoadModelTemplateAssignmentDto? _selectedAssignment;
    private RoadModelSlopeTemplateGroupDto? _selectedLeftSlopeGroup;
    private RoadModelSlopeTemplateGroupDto? _selectedRightSlopeGroup;
    private RoadModelSlopeTemplateReferenceDto? _selectedLeftSlopeTemplate;
    private RoadModelSlopeTemplateReferenceDto? _selectedRightSlopeTemplate;

    public RoadModelWindow(RoadModelDialogRequest request)
    {
        InitializeComponent();
        _request = request;
        RoadCenterlineHandle = request.RoadCenterlineHandle;
        ProfileVerticalCurveHandle = request.ProfileVerticalCurveHandle;
        SampleIntervalText = PositiveOrFallback(request.SampleInterval, 10.0).ToString("G", CultureInfo.InvariantCulture);
        LeftSlopeSearchWidthText = PositiveOrFallback(request.LeftSlopeSearchWidth, 50.0).ToString("G", CultureInfo.InvariantCulture);
        RightSlopeSearchWidthText = PositiveOrFallback(request.RightSlopeSearchWidth, 50.0).ToString("G", CultureInfo.InvariantCulture);

        foreach (var assignment in request.Assignments)
        {
            Assignments.Add(assignment.Clone());
        }
        foreach (var group in request.LeftSlopeGroups)
        {
            LeftSlopeGroups.Add(group.Clone());
        }
        foreach (var group in request.RightSlopeGroups)
        {
            RightSlopeGroups.Add(group.Clone());
        }
        if (request.SelectedAssignmentIndex >= 0 && request.SelectedAssignmentIndex < Assignments.Count)
        {
            SelectedAssignment = Assignments[request.SelectedAssignmentIndex];
        }
        if (request.SelectedLeftSlopeGroupIndex >= 0 && request.SelectedLeftSlopeGroupIndex < LeftSlopeGroups.Count)
        {
            SelectedLeftSlopeGroup = LeftSlopeGroups[request.SelectedLeftSlopeGroupIndex];
        }
        if (request.SelectedRightSlopeGroupIndex >= 0 && request.SelectedRightSlopeGroupIndex < RightSlopeGroups.Count)
        {
            SelectedRightSlopeGroup = RightSlopeGroups[request.SelectedRightSlopeGroupIndex];
        }

        DataContext = this;
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public RoadModelDialogResponse? Response { get; private set; }

    public ObservableCollection<RoadModelTemplateAssignmentDto> Assignments { get; } = new();
    public ObservableCollection<RoadModelSlopeTemplateGroupDto> LeftSlopeGroups { get; } = new();
    public ObservableCollection<RoadModelSlopeTemplateGroupDto> RightSlopeGroups { get; } = new();

    public string RoadCenterlineHandle { get; }
    public string ProfileVerticalCurveHandle { get; }

    public string SampleIntervalText
    {
        get => _sampleIntervalText;
        set
        {
            if (_sampleIntervalText == value) return;
            _sampleIntervalText = value;
            OnPropertyChanged();
        }
    }

    public string LeftSlopeSearchWidthText
    {
        get => _leftSlopeSearchWidthText;
        set
        {
            if (_leftSlopeSearchWidthText == value) return;
            _leftSlopeSearchWidthText = value;
            OnPropertyChanged();
        }
    }

    public string RightSlopeSearchWidthText
    {
        get => _rightSlopeSearchWidthText;
        set
        {
            if (_rightSlopeSearchWidthText == value) return;
            _rightSlopeSearchWidthText = value;
            OnPropertyChanged();
        }
    }

    public RoadModelTemplateAssignmentDto? SelectedAssignment
    {
        get => _selectedAssignment;
        set
        {
            if (_selectedAssignment == value) return;
            _selectedAssignment = value;
            OnPropertyChanged();
        }
    }

    public RoadModelSlopeTemplateGroupDto? SelectedLeftSlopeGroup
    {
        get => _selectedLeftSlopeGroup;
        set
        {
            if (_selectedLeftSlopeGroup == value) return;
            _selectedLeftSlopeGroup = value;
            OnPropertyChanged();
            SelectedLeftSlopeTemplate = value?.Templates.FirstOrDefault();
        }
    }

    public RoadModelSlopeTemplateGroupDto? SelectedRightSlopeGroup
    {
        get => _selectedRightSlopeGroup;
        set
        {
            if (_selectedRightSlopeGroup == value) return;
            _selectedRightSlopeGroup = value;
            OnPropertyChanged();
            SelectedRightSlopeTemplate = value?.Templates.FirstOrDefault();
        }
    }

    public RoadModelSlopeTemplateReferenceDto? SelectedLeftSlopeTemplate
    {
        get => _selectedLeftSlopeTemplate;
        set
        {
            if (_selectedLeftSlopeTemplate == value) return;
            _selectedLeftSlopeTemplate = value;
            OnPropertyChanged();
        }
    }

    public RoadModelSlopeTemplateReferenceDto? SelectedRightSlopeTemplate
    {
        get => _selectedRightSlopeTemplate;
        set
        {
            if (_selectedRightSlopeTemplate == value) return;
            _selectedRightSlopeTemplate = value;
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
        if (SelectedAssignment == null) return;
        var index = Assignments.IndexOf(SelectedAssignment);
        Assignments.Remove(SelectedAssignment);
        SelectedAssignment = Assignments.Count > 0 ? Assignments[System.Math.Min(index, Assignments.Count - 1)] : null;
    }

    private void OnMoveAssignmentUp(object sender, RoutedEventArgs e)
        => MoveAssignment(-1);

    private void OnMoveAssignmentDown(object sender, RoutedEventArgs e)
        => MoveAssignment(1);

    private void MoveAssignment(int delta)
    {
        if (SelectedAssignment == null) return;
        var oldIndex = Assignments.IndexOf(SelectedAssignment);
        var newIndex = oldIndex + delta;
        if (oldIndex < 0 || newIndex < 0 || newIndex >= Assignments.Count) return;
        Assignments.Move(oldIndex, newIndex);
    }

    private void OnAddLeftSlopeGroup(object sender, RoutedEventArgs e)
        => AddSlopeGroup(LeftSlopeGroups, group => SelectedLeftSlopeGroup = group);

    private void OnAddRightSlopeGroup(object sender, RoutedEventArgs e)
        => AddSlopeGroup(RightSlopeGroups, group => SelectedRightSlopeGroup = group);

    private static void AddSlopeGroup(
        ObservableCollection<RoadModelSlopeTemplateGroupDto> groups,
        System.Action<RoadModelSlopeTemplateGroupDto> select)
    {
        var group = new RoadModelSlopeTemplateGroupDto();
        groups.Add(group);
        select(group);
    }

    private void OnDeleteLeftSlopeGroup(object sender, RoutedEventArgs e)
        => DeleteSlopeGroup(LeftSlopeGroups, SelectedLeftSlopeGroup, group => SelectedLeftSlopeGroup = group);

    private void OnDeleteRightSlopeGroup(object sender, RoutedEventArgs e)
        => DeleteSlopeGroup(RightSlopeGroups, SelectedRightSlopeGroup, group => SelectedRightSlopeGroup = group);

    private static void DeleteSlopeGroup(
        ObservableCollection<RoadModelSlopeTemplateGroupDto> groups,
        RoadModelSlopeTemplateGroupDto? selected,
        System.Action<RoadModelSlopeTemplateGroupDto?> select)
    {
        if (selected == null) return;
        var index = groups.IndexOf(selected);
        groups.Remove(selected);
        select(groups.Count > 0 ? groups[System.Math.Min(index, groups.Count - 1)] : null);
    }

    private void OnMoveLeftSlopeGroupUp(object sender, RoutedEventArgs e)
        => MoveSlopeGroup(LeftSlopeGroups, SelectedLeftSlopeGroup, -1);

    private void OnMoveLeftSlopeGroupDown(object sender, RoutedEventArgs e)
        => MoveSlopeGroup(LeftSlopeGroups, SelectedLeftSlopeGroup, 1);

    private void OnMoveRightSlopeGroupUp(object sender, RoutedEventArgs e)
        => MoveSlopeGroup(RightSlopeGroups, SelectedRightSlopeGroup, -1);

    private void OnMoveRightSlopeGroupDown(object sender, RoutedEventArgs e)
        => MoveSlopeGroup(RightSlopeGroups, SelectedRightSlopeGroup, 1);

    private static void MoveSlopeGroup(
        ObservableCollection<RoadModelSlopeTemplateGroupDto> groups,
        RoadModelSlopeTemplateGroupDto? selected,
        int delta)
    {
        if (selected == null) return;
        var oldIndex = groups.IndexOf(selected);
        var newIndex = oldIndex + delta;
        if (oldIndex < 0 || newIndex < 0 || newIndex >= groups.Count) return;
        groups.Move(oldIndex, newIndex);
    }

    private void OnManageLeftSlopeGroup(object sender, RoutedEventArgs e)
        => ManageSlopeGroup(sender, "左侧", group => SelectedLeftSlopeGroup = group);

    private void OnManageRightSlopeGroup(object sender, RoutedEventArgs e)
        => ManageSlopeGroup(sender, "右侧", group => SelectedRightSlopeGroup = group);

    private void ManageSlopeGroup(
        object sender,
        string sideLabel,
        System.Action<RoadModelSlopeTemplateGroupDto> select)
    {
        ErrorTextBlock.Text = string.Empty;
        if (sender is not FrameworkElement { DataContext: RoadModelSlopeTemplateGroupDto group })
        {
            return;
        }

        select(group);
        ErrorTextBlock.Text = $"已选中{sideLabel}模板组，可在下方管理组内模板。";
    }

    private void OnPickTemplate(object sender, RoutedEventArgs e)
    {
        ErrorTextBlock.Text = string.Empty;
        if (sender is FrameworkElement { DataContext: RoadModelTemplateAssignmentDto row })
        {
            SelectedAssignment = row;
        }
        if (SelectedAssignment == null)
        {
            ErrorTextBlock.Text = "请先选择一行路基模板范围。";
            return;
        }
        var rowIndex = Assignments.IndexOf(SelectedAssignment);
        if (rowIndex < 0)
        {
            ErrorTextBlock.Text = "请先选择一行路基模板范围。";
            return;
        }
        Response = BuildResponse(false, ReadSampleIntervalOrFallback(), RoadModelDialogAction.PickTemplate, rowIndex);
        DialogResult = true;
    }

    private void OnPickLeftSlopeTemplate(object sender, RoutedEventArgs e)
        => PickSlopeTemplate(LeftSlopeGroups, SelectedLeftSlopeGroup, RoadModelDialogAction.PickLeftSlopeTemplate);

    private void OnPickRightSlopeTemplate(object sender, RoutedEventArgs e)
        => PickSlopeTemplate(RightSlopeGroups, SelectedRightSlopeGroup, RoadModelDialogAction.PickRightSlopeTemplate);

    private void PickSlopeTemplate(
        ObservableCollection<RoadModelSlopeTemplateGroupDto> groups,
        RoadModelSlopeTemplateGroupDto? selected,
        RoadModelDialogAction action)
    {
        ErrorTextBlock.Text = string.Empty;
        if (selected == null)
        {
            ErrorTextBlock.Text = "请先选择一行边坡模板组。";
            return;
        }
        var rowIndex = groups.IndexOf(selected);
        if (rowIndex < 0)
        {
            ErrorTextBlock.Text = "请先选择一行边坡模板组。";
            return;
        }
        Response = BuildResponse(false, ReadSampleIntervalOrFallback(), action, pickSlopeGroupIndex: rowIndex);
        DialogResult = true;
    }

    private void OnClearLeftSlopeTemplates(object sender, RoutedEventArgs e)
        => ClearSlopeTemplates(SelectedLeftSlopeGroup);

    private void OnClearRightSlopeTemplates(object sender, RoutedEventArgs e)
        => ClearSlopeTemplates(SelectedRightSlopeGroup);

    private void OnDeleteLeftSlopeTemplate(object sender, RoutedEventArgs e)
        => DeleteSlopeTemplate(SelectedLeftSlopeGroup, SelectedLeftSlopeTemplate, template => SelectedLeftSlopeTemplate = template);

    private void OnDeleteRightSlopeTemplate(object sender, RoutedEventArgs e)
        => DeleteSlopeTemplate(SelectedRightSlopeGroup, SelectedRightSlopeTemplate, template => SelectedRightSlopeTemplate = template);

    private void OnMoveLeftSlopeTemplateUp(object sender, RoutedEventArgs e)
        => MoveSlopeTemplate(SelectedLeftSlopeGroup, SelectedLeftSlopeTemplate, -1, template => SelectedLeftSlopeTemplate = template);

    private void OnMoveLeftSlopeTemplateDown(object sender, RoutedEventArgs e)
        => MoveSlopeTemplate(SelectedLeftSlopeGroup, SelectedLeftSlopeTemplate, 1, template => SelectedLeftSlopeTemplate = template);

    private void OnMoveRightSlopeTemplateUp(object sender, RoutedEventArgs e)
        => MoveSlopeTemplate(SelectedRightSlopeGroup, SelectedRightSlopeTemplate, -1, template => SelectedRightSlopeTemplate = template);

    private void OnMoveRightSlopeTemplateDown(object sender, RoutedEventArgs e)
        => MoveSlopeTemplate(SelectedRightSlopeGroup, SelectedRightSlopeTemplate, 1, template => SelectedRightSlopeTemplate = template);

    private void DeleteSlopeTemplate(
        RoadModelSlopeTemplateGroupDto? group,
        RoadModelSlopeTemplateReferenceDto? selected,
        System.Action<RoadModelSlopeTemplateReferenceDto?> select)
    {
        if (group == null || selected == null) return;
        var index = group.Templates.IndexOf(selected);
        if (index < 0) return;

        group.Templates.RemoveAt(index);
        select(group.Templates.Count > 0 ? group.Templates[System.Math.Min(index, group.Templates.Count - 1)] : null);
        RefreshSlopeTemplateViews(group);
    }

    private void MoveSlopeTemplate(
        RoadModelSlopeTemplateGroupDto? group,
        RoadModelSlopeTemplateReferenceDto? selected,
        int delta,
        System.Action<RoadModelSlopeTemplateReferenceDto?> select)
    {
        if (group == null || selected == null) return;
        var oldIndex = group.Templates.IndexOf(selected);
        var newIndex = oldIndex + delta;
        if (oldIndex < 0 || newIndex < 0 || newIndex >= group.Templates.Count) return;

        group.Templates.RemoveAt(oldIndex);
        group.Templates.Insert(newIndex, selected);
        select(selected);
        RefreshSlopeTemplateViews(group);
    }

    private void ClearSlopeTemplates(RoadModelSlopeTemplateGroupDto? group)
    {
        if (group == null) return;
        group.Templates.Clear();
        if (ReferenceEquals(group, SelectedLeftSlopeGroup))
        {
            SelectedLeftSlopeTemplate = null;
        }
        if (ReferenceEquals(group, SelectedRightSlopeGroup))
        {
            SelectedRightSlopeTemplate = null;
        }
        RefreshSlopeTemplateViews(group);
    }

    private void RefreshSlopeTemplateViews(RoadModelSlopeTemplateGroupDto group)
    {
        CollectionViewSource.GetDefaultView(group.Templates).Refresh();
        CollectionViewSource.GetDefaultView(LeftSlopeGroups).Refresh();
        CollectionViewSource.GetDefaultView(RightSlopeGroups).Refresh();
        OnPropertyChanged(nameof(SelectedLeftSlopeGroup));
        OnPropertyChanged(nameof(SelectedRightSlopeGroup));
    }

    private void OnGenerateModel(object sender, RoutedEventArgs e)
    {
        ErrorTextBlock.Text = string.Empty;
        if (!TryReadSampleInterval(out var sampleInterval) || sampleInterval <= 0.0 || !IsFinite(sampleInterval))
        {
            ErrorTextBlock.Text = "采样间隔必须是大于 0 的有效数字。";
            return;
        }
        if (!TryReadPositive(LeftSlopeSearchWidthText, out var leftSearchWidth) ||
            !TryReadPositive(RightSlopeSearchWidthText, out var rightSearchWidth))
        {
            ErrorTextBlock.Text = "边坡搜索宽度必须是大于 0 的有效数字。";
            return;
        }

        Response = BuildResponse(true, sampleInterval, leftSearchWidth: leftSearchWidth, rightSearchWidth: rightSearchWidth);
        DialogResult = true;
    }

    private RoadModelDialogResponse BuildResponse(
        bool accepted,
        double sampleInterval,
        RoadModelDialogAction action = RoadModelDialogAction.None,
        int pickAssignmentIndex = -1,
        int pickSlopeGroupIndex = -1,
        double? leftSearchWidth = null,
        double? rightSearchWidth = null)
        => new()
        {
            Action = action,
            Accepted = accepted,
            PickAssignmentIndex = pickAssignmentIndex,
            PickSlopeGroupIndex = pickSlopeGroupIndex,
            Handle = _request.Handle,
            RoadCenterlineHandle = RoadCenterlineHandle,
            ProfileVerticalCurveHandle = ProfileVerticalCurveHandle,
            SampleInterval = sampleInterval,
            LeftSlopeSearchWidth = leftSearchWidth ?? ReadPositiveOrFallback(LeftSlopeSearchWidthText, PositiveOrFallback(_request.LeftSlopeSearchWidth, 50.0)),
            RightSlopeSearchWidth = rightSearchWidth ?? ReadPositiveOrFallback(RightSlopeSearchWidthText, PositiveOrFallback(_request.RightSlopeSearchWidth, 50.0)),
            Assignments = new(Assignments),
            LeftSlopeGroups = LeftSlopeGroups.Select(group => group.Clone()).ToList(),
            RightSlopeGroups = RightSlopeGroups.Select(group => group.Clone()).ToList(),
        };

    private void OnCancel(object sender, RoutedEventArgs e)
    {
        Response = BuildResponse(false, PositiveOrFallback(_request.SampleInterval, 10.0));
        DialogResult = false;
    }

    private double ReadSampleIntervalOrFallback()
        => TryReadSampleInterval(out var value) && value > 0.0 && IsFinite(value)
            ? value
            : PositiveOrFallback(_request.SampleInterval, 10.0);

    private bool TryReadSampleInterval(out double sampleInterval)
        => double.TryParse(SampleIntervalText, NumberStyles.Float, CultureInfo.InvariantCulture, out sampleInterval)
            || double.TryParse(SampleIntervalText, NumberStyles.Float, CultureInfo.CurrentCulture, out sampleInterval);

    private static double ReadPositiveOrFallback(string text, double fallback)
        => TryReadPositive(text, out var value) ? value : fallback;

    private static bool TryReadPositive(string text, out double value)
    {
        var ok = double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value)
            || double.TryParse(text, NumberStyles.Float, CultureInfo.CurrentCulture, out value);
        return ok && IsFinite(value) && value > 0.0;
    }

    private static double PositiveOrFallback(double value, double fallback)
        => IsFinite(value) && value > 0.0 ? value : fallback;

    private static bool IsFinite(double value)
        => !double.IsNaN(value) && !double.IsInfinity(value);

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
}
