using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Runtime.CompilerServices;
using System.Windows.Input;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI.ViewModels;

public sealed class TerrainTinCreateViewModel : INotifyPropertyChanged
{
    private readonly ITerrainTinBridge? bridge_;
    private TerrainTinExtractSummaryDto summary_ = new();
    private TerrainTinOptionsDto options_ = new();
    private string resultText_ = "尚未生成";
    private string errorText_ = string.Empty;

    public TerrainTinCreateViewModel()
        : this(null)
    {
    }

    public TerrainTinCreateViewModel(ITerrainTinBridge? bridge)
    {
        bridge_ = bridge;
        GenerateCommand = new RelayCommand(GenerateTin, CanGenerateTin);
        ReextractCommand = new RelayCommand(Reextract);
        ToggleDisplayCommand = new RelayCommand(ToggleDisplayMode);
        CloseCommand = new RelayCommand(() => RequestClose?.Invoke());
        RefreshStatistics();
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public event Action? RequestClose;

    public ObservableCollection<string> DisplayModes { get; } = new()
    {
        "ColoredTriangles",
        "BoundaryOnly"
    };

    public ObservableCollection<StatisticRow> Statistics { get; } = new();

    public ICommand GenerateCommand { get; }

    public ICommand ReextractCommand { get; }

    public ICommand ToggleDisplayCommand { get; }

    public ICommand CloseCommand { get; }

    public TerrainTinExtractSummaryDto Summary
    {
        get => summary_;
        set
        {
            summary_ = value;
            RefreshStatistics();
            OnPropertyChanged();
            OnPropertyChanged(nameof(CanGenerate));
            if (GenerateCommand is RelayCommand relay) {
                relay.RaiseCanExecuteChanged();
            }
        }
    }

    public TerrainTinOptionsDto Options
    {
        get => options_;
        set
        {
            options_ = value;
            OnPropertyChanged();
        }
    }

    public bool CanGenerate => Summary.ValidPointCount >= 3;

    public string ResultText
    {
        get => resultText_;
        set
        {
            resultText_ = value;
            OnPropertyChanged();
        }
    }

    public string ErrorText
    {
        get => errorText_;
        set
        {
            errorText_ = value;
            OnPropertyChanged();
        }
    }

    private bool CanGenerateTin() => CanGenerate;

    private void GenerateTin()
    {
        ErrorText = string.Empty;
        if (!ValidateOptions()) {
            return;
        }

        if (bridge_ is null) {
            ResultText = "Bridge 未连接，当前仅展示 UI。";
            return;
        }

        var result = bridge_.BuildTin(Options);
        ResultText = result.Succeeded
            ? $"已生成 {result.TriangleCount} 个三角形，高程 {result.MinElevation:0.###} - {result.MaxElevation:0.###}"
            : "生成失败";
        ErrorText = result.Succeeded ? string.Empty : result.Message;
    }

    private void Reextract()
    {
        ErrorText = string.Empty;
        Summary = bridge_?.Reextract(Options) ?? Summary;
    }

    private void ToggleDisplayMode()
    {
        Options.DisplayMode = Options.DisplayMode == "ColoredTriangles" ? "BoundaryOnly" : "ColoredTriangles";
        OnPropertyChanged(nameof(Options));
        if (bridge_ is not null) {
            var result = bridge_.ToggleDisplayMode(Options.DisplayMode);
            ErrorText = result.Succeeded ? string.Empty : result.Message;
        }
    }

    private bool ValidateOptions()
    {
        if (Options.XyMergeTolerance <= 0.0) {
            ErrorText = "XY 合并容差必须大于 0。";
            return false;
        }
        if (Options.MinTriangleArea <= 0.0) {
            ErrorText = "最小三角形面积必须大于 0。";
            return false;
        }
        if (Options.TextPointSearchDistance < 0.0) {
            ErrorText = "文字-点关联距离不能为负数。";
            return false;
        }
        return true;
    }

    private void RefreshStatistics()
    {
        Statistics.Clear();
        Statistics.Add(new("选择对象", Summary.SelectedObjectCount.ToString()));
        Statistics.Add(new("原始点", Summary.RawPointCount.ToString()));
        Statistics.Add(new("有效点", Summary.ValidPointCount.ToString()));
        Statistics.Add(new("重复点", Summary.DuplicatePointCount.ToString()));
        Statistics.Add(new("Z 冲突点", Summary.ZConflictPointCount.ToString()));
        Statistics.Add(new("无效对象", Summary.InvalidObjectCount.ToString()));
        Statistics.Add(new("文字高程点", Summary.TextElevationPointCount.ToString()));
        Statistics.Add(new("文字-点合并", Summary.TextPointMergeCount.ToString()));
        Statistics.Add(new("文字赋高程", Summary.TextAssignedElevationPointCount.ToString()));
        Statistics.Add(new("块数量", Summary.BlockCount.ToString()));
        Statistics.Add(new("识别高程块", Summary.RecognizedElevationBlockCount.ToString()));
        Statistics.Add(new("块属性高程", Summary.BlockAttributeElevationCount.ToString()));
        Statistics.Add(new("块解析失败", Summary.BlockParseFailedCount.ToString()));
        Statistics.Add(new("三角形", Summary.TriangleCount.ToString()));
        Statistics.Add(new("当前状态", Summary.Status));
    }

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
    {
        PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
    }
}

public sealed class StatisticRow
{
    public StatisticRow(string name, string value)
    {
        Name = name;
        Value = value;
    }

    public string Name { get; }
    public string Value { get; }
}
