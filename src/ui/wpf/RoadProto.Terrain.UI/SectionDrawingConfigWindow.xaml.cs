using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Controls;
using Microsoft.Win32;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class SectionDrawingConfigWindow : Window, INotifyPropertyChanged
{
    private readonly SectionDrawingConfigRequest _request;
    private SectionDrawingConfigRowDto? _selectedRow;
    private string _configPath;

    public SectionDrawingConfigWindow(SectionDrawingConfigRequest request)
    {
        InitializeComponent();
        _request = request;
        _configPath = request.ConfigPath;
        ComponentOptions = new ObservableCollection<SectionDrawingConfigComponentOptionDto>(request.ComponentOptions);
        PavementRows = new ObservableCollection<SectionDrawingConfigRowDto>(request.PavementRows);
        foreach (var row in PavementRows)
        {
            RefreshComponentDisplayText(row);
        }
        DataContext = this;
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public SectionDrawingConfigResponse? Response { get; private set; }

    public string ConfigPath
    {
        get => _configPath;
        set
        {
            _configPath = value;
            OnPropertyChanged();
        }
    }

    public ObservableCollection<SectionDrawingConfigComponentOptionDto> ComponentOptions { get; }

    public ObservableCollection<SectionDrawingConfigRowDto> PavementRows { get; }

    public SectionDrawingConfigRowDto? SelectedRow
    {
        get => _selectedRow;
        set
        {
            _selectedRow = value;
            OnPropertyChanged();
        }
    }

    private void OnImportClick(object sender, RoutedEventArgs e)
    {
        var dialog = new OpenFileDialog
        {
            Filter = "CSV 配置 (*.csv)|*.csv|所有文件 (*.*)|*.*",
        };
        if (dialog.ShowDialog(this) != true)
        {
            return;
        }

        try
        {
            var importedRows = SectionDrawingConfigDialogFile.ImportCsv(dialog.FileName, ComponentOptions.ToList());
            foreach (var row in importedRows)
            {
                RefreshComponentDisplayText(row);
            }

            PavementRows.Clear();
            foreach (var row in importedRows)
            {
                PavementRows.Add(row);
            }
            ConfigPath = dialog.FileName;
            ErrorTextBlock.Text = string.Empty;
        }
        catch (System.Exception error)
        {
            ErrorTextBlock.Text = error.Message;
        }
    }

    private void OnExportClick(object sender, RoutedEventArgs e)
    {
        var path = ConfigPath;
        if (string.IsNullOrWhiteSpace(path))
        {
            var dialog = new SaveFileDialog
            {
                Filter = "CSV 配置 (*.csv)|*.csv|所有文件 (*.*)|*.*",
                DefaultExt = ".csv",
                AddExtension = true,
                FileName = "section_drawing_config.csv",
            };
            if (dialog.ShowDialog(this) != true)
            {
                return;
            }
            path = dialog.FileName;
        }

        try
        {
            SectionDrawingConfigDialogFile.ExportCsv(path, PavementRows.ToList(), ComponentOptions.ToList());
            ConfigPath = path;
            ErrorTextBlock.Text = string.Empty;
        }
        catch (System.Exception error)
        {
            ErrorTextBlock.Text = error.Message;
        }
    }

    private void OnAddRowClick(object sender, RoutedEventArgs e)
    {
        var row = new SectionDrawingConfigRowDto();
        RefreshComponentDisplayText(row);
        PavementRows.Add(row);
        SelectedRow = row;
    }

    private void OnRemoveRowClick(object sender, RoutedEventArgs e)
    {
        if (SelectedRow != null)
        {
            PavementRows.Remove(SelectedRow);
        }
    }

    private void OnMoveUpClick(object sender, RoutedEventArgs e)
        => MoveSelectedRow(-1);

    private void OnMoveDownClick(object sender, RoutedEventArgs e)
        => MoveSelectedRow(1);

    private void OnComponentTypesClick(object sender, RoutedEventArgs e)
    {
        var row = RowFromSender(sender);
        if (row == null)
        {
            return;
        }
        SelectedRow = row;

        var dialog = new Window
        {
            Title = "选择路基类型",
            Owner = this,
            Width = 340,
            Height = 440,
            WindowStartupLocation = WindowStartupLocation.CenterOwner,
        };
        var list = new ListBox
        {
            SelectionMode = SelectionMode.Multiple,
            DisplayMemberPath = nameof(SectionDrawingConfigComponentOptionDto.DisplayName),
            Margin = new Thickness(10),
        };
        foreach (var option in ComponentOptions)
        {
            list.Items.Add(option);
        }
        foreach (var item in list.Items.OfType<SectionDrawingConfigComponentOptionDto>())
        {
            if (row.ComponentCodes.Contains(item.Code))
            {
                list.SelectedItems.Add(item);
            }
        }

        var ok = new Button
        {
            Content = "确定",
            Width = 76,
            Margin = new Thickness(0, 8, 10, 10),
            HorizontalAlignment = HorizontalAlignment.Right,
        };
        var panel = new DockPanel();
        DockPanel.SetDock(ok, Dock.Bottom);
        panel.Children.Add(ok);
        panel.Children.Add(list);
        dialog.Content = panel;
        ok.Click += (_, _) => dialog.DialogResult = true;
        if (dialog.ShowDialog() == true)
        {
            row.ComponentCodes = list.SelectedItems
                .OfType<SectionDrawingConfigComponentOptionDto>()
                .Select(item => item.Code)
                .ToList();
            RefreshComponentDisplayText(row);
            RefreshRows();
        }
    }

    private void OnPickTemplateClick(object sender, RoutedEventArgs e)
    {
        var row = RowFromSender(sender);
        if (row == null)
        {
            return;
        }
        SelectedRow = row;
        Response = CreateResponse(SectionDrawingConfigAction.PickTemplate);
        Response.PickRowIndex = PavementRows.IndexOf(row);
        DialogResult = true;
    }

    private void OnDrawClick(object sender, RoutedEventArgs e)
    {
        Response = CreateResponse(SectionDrawingConfigAction.Draw);
        DialogResult = true;
    }

    private void OnCancelClick(object sender, RoutedEventArgs e)
    {
        Response = CreateCancelledResponse();
        DialogResult = false;
    }

    private void MoveSelectedRow(int offset)
    {
        if (SelectedRow == null)
        {
            return;
        }
        var oldIndex = PavementRows.IndexOf(SelectedRow);
        var newIndex = oldIndex + offset;
        if (oldIndex < 0 || newIndex < 0 || newIndex >= PavementRows.Count)
        {
            return;
        }
        PavementRows.Move(oldIndex, newIndex);
    }

    private SectionDrawingConfigResponse CreateResponse(SectionDrawingConfigAction action)
        => new()
        {
            Action = action,
            Accepted = true,
            DrawingHandle = _request.DrawingHandle,
            RoadModelHandle = _request.RoadModelHandle,
            ResponsePath = _request.ResponsePath,
            ConfigPath = ConfigPath,
            ComponentOptions = ComponentOptions.ToList(),
            PavementRows = PavementRows.ToList(),
        };

    private SectionDrawingConfigResponse CreateCancelledResponse()
        => new()
        {
            Action = SectionDrawingConfigAction.None,
            Accepted = false,
            DrawingHandle = _request.DrawingHandle,
            RoadModelHandle = _request.RoadModelHandle,
            ResponsePath = _request.ResponsePath,
            ConfigPath = ConfigPath,
            ComponentOptions = ComponentOptions.ToList(),
            PavementRows = PavementRows.ToList(),
        };

    private void RefreshComponentDisplayText(SectionDrawingConfigRowDto row)
    {
        row.ComponentDisplayText = string.Join(
            ";",
            row.ComponentCodes
                .Select(code => ComponentOptions.FirstOrDefault(option => option.Code == code)?.DisplayName ?? code)
                .Where(text => !string.IsNullOrWhiteSpace(text)));
        if (string.IsNullOrWhiteSpace(row.ComponentDisplayText))
        {
            row.ComponentDisplayText = "未选择";
        }
    }

    private SectionDrawingConfigRowDto? RowFromSender(object sender)
        => (sender as FrameworkElement)?.DataContext as SectionDrawingConfigRowDto ?? SelectedRow;

    private void RefreshRows()
    {
        var selected = SelectedRow;
        var rows = PavementRows.ToList();
        PavementRows.Clear();
        foreach (var row in rows)
        {
            PavementRows.Add(row);
        }
        SelectedRow = selected;
    }

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
}
