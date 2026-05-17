using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Linq;
using System.Windows;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class StationValueTableWindow : Window
{
    private readonly ObservableCollection<SubgradeStationValueDto> _rows;

    public StationValueTableWindow(
        string title,
        string valueHeader,
        IEnumerable<SubgradeStationValueDto> rows)
    {
        InitializeComponent();
        Title = title;
        ValueColumn.Header = valueHeader;
        _rows = new ObservableCollection<SubgradeStationValueDto>(rows.Select(row => row.Clone()));
        RowsGrid.ItemsSource = _rows;
    }

    public List<SubgradeStationValueDto> Rows
        => _rows.Select(row => row.Clone()).ToList();

    private void Add_Click(object sender, RoutedEventArgs e)
    {
        _rows.Add(new SubgradeStationValueDto());
        RowsGrid.SelectedIndex = _rows.Count - 1;
        RowsGrid.ScrollIntoView(RowsGrid.SelectedItem);
    }

    private void Delete_Click(object sender, RoutedEventArgs e)
    {
        foreach (var row in RowsGrid.SelectedItems.OfType<SubgradeStationValueDto>().ToList())
        {
            _rows.Remove(row);
        }
    }

    private void Ok_Click(object sender, RoutedEventArgs e)
    {
        DialogResult = true;
    }
}
