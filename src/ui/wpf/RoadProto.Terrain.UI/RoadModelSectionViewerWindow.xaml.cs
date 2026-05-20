using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Media;
using System.Windows.Shapes;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class RoadModelSectionViewerWindow : Window, INotifyPropertyChanged
{
    private RoadModelSectionViewerPreviewDto? _selectedPreview;
    private string _statusText = string.Empty;

    public RoadModelSectionViewerWindow(RoadModelSectionViewerRequest request)
    {
        InitializeComponent();
        Handle = request.Handle;
        RoadCenterlineHandle = request.RoadCenterlineHandle;
        foreach (var preview in request.Previews)
        {
            Previews.Add(preview);
        }

        DataContext = this;
        SelectedPreview = Previews.FirstOrDefault();
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public string Handle { get; }
    public string RoadCenterlineHandle { get; }
    public ObservableCollection<RoadModelSectionViewerPreviewDto> Previews { get; } = new();
    public int PreviewCount => Previews.Count;

    public RoadModelSectionViewerPreviewDto? SelectedPreview
    {
        get => _selectedPreview;
        set
        {
            if (_selectedPreview == value) return;
            _selectedPreview = value;
            OnPropertyChanged();
            UpdateStatusText();
            DrawPreview();
        }
    }

    public string StatusText
    {
        get => _statusText;
        private set
        {
            if (_statusText == value) return;
            _statusText = value;
            OnPropertyChanged();
        }
    }

    private void UpdateStatusText()
    {
        if (SelectedPreview == null)
        {
            StatusText = "没有可显示的横断面。";
            return;
        }

        StatusText = string.Format(
            CultureInfo.InvariantCulture,
            "当前桩号 {0}，线段 {1} 条。{2}",
            SelectedPreview.DisplayStation,
            SelectedPreview.Segments.Count,
            SelectedPreview.StatusMessage);
    }

    private void OnPreviewCanvasSizeChanged(object sender, SizeChangedEventArgs e)
        => DrawPreview();

    private void DrawPreview()
    {
        if (PreviewCanvas == null)
        {
            return;
        }

        PreviewCanvas.Children.Clear();
        if (SelectedPreview == null || PreviewCanvas.ActualWidth <= 0 || PreviewCanvas.ActualHeight <= 0)
        {
            return;
        }

        var points = SelectedPreview.Segments.SelectMany(segment => segment.Points).ToList();
        if (points.Count == 0)
        {
            return;
        }

        var minOffset = points.Min(point => point.Offset);
        var maxOffset = points.Max(point => point.Offset);
        var minElevation = points.Min(point => point.Elevation);
        var maxElevation = points.Max(point => point.Elevation);
        if (maxOffset - minOffset < 1.0e-6)
        {
            minOffset -= 1.0;
            maxOffset += 1.0;
        }
        if (maxElevation - minElevation < 1.0e-6)
        {
            minElevation -= 1.0;
            maxElevation += 1.0;
        }

        const double padding = 42.0;
        var width = System.Math.Max(PreviewCanvas.ActualWidth - padding * 2.0, 1.0);
        var height = System.Math.Max(PreviewCanvas.ActualHeight - padding * 2.0, 1.0);

        Point Map(RoadModelSectionViewerPointDto point)
        {
            var x = padding + (point.Offset - minOffset) / (maxOffset - minOffset) * width;
            var y = padding + (maxElevation - point.Elevation) / (maxElevation - minElevation) * height;
            return new Point(x, y);
        }

        foreach (var segment in SelectedPreview.Segments)
        {
            if (segment.Points.Count < 2)
            {
                continue;
            }

            var polyline = new Polyline
            {
                Stroke = new SolidColorBrush(Color.FromRgb(
                    ClampColor(segment.ColorR),
                    ClampColor(segment.ColorG),
                    ClampColor(segment.ColorB))),
                StrokeThickness = StrokeThicknessFor(segment.Kind),
            };
            foreach (var point in segment.Points)
            {
                polyline.Points.Add(Map(point));
            }

            PreviewCanvas.Children.Add(polyline);
        }
    }

    private static byte ClampColor(int value)
        => (byte)System.Math.Max(0, System.Math.Min(255, value));

    private static double StrokeThicknessFor(RoadModelSectionViewerSegmentKind kind)
        => kind switch
        {
            RoadModelSectionViewerSegmentKind.Ground => 2.2,
            RoadModelSectionViewerSegmentKind.PavementLayer => 2.1,
            _ => 2.0,
        };

    private void OnClose(object sender, RoutedEventArgs e)
        => Close();

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
}
