using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Globalization;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Shapes;
using RoadProto.Terrain.UI.Bridge;

namespace RoadProto.Terrain.UI;

public partial class RoadModelSectionViewerWindow : Window, INotifyPropertyChanged
{
    private RoadModelSectionViewerPreviewDto? _selectedPreview;
    private string _statusText = string.Empty;
    private bool _panning;
    private Point _lastPanPoint;
    private double _previewZoom = 1.0;
    private Vector _previewPan;

    public RoadModelSectionViewerWindow(RoadModelSectionViewerRequest request)
    {
        InitializeComponent();
        Handle = request.Handle;
        RoadCenterlineHandle = request.RoadCenterlineHandle;
        foreach (var preview in request.Previews)
        {
            Previews.Add(preview);
        }

        Response = null;
        DataContext = this;
        SelectedPreview = Previews.FirstOrDefault();
    }

    public event PropertyChangedEventHandler? PropertyChanged;

    public string Handle { get; }
    public string RoadCenterlineHandle { get; }
    public ObservableCollection<RoadModelSectionViewerPreviewDto> Previews { get; } = new();
    public int PreviewCount => Previews.Count;
    public RoadModelSectionViewerResponse? Response { get; private set; }

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
        var transform = CreatePreviewTransform(SelectedPreview, _previewZoom, _previewPan);
        if (SelectedPreview == null || transform == null)
        {
            return;
        }

        Point Map(RoadModelSectionViewerPointDto point)
            => transform.WorldToScreen(new Point(point.Offset, point.Elevation));

        DrawPavementLayerFills(Map);
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

    private void DrawPavementLayerFills(Func<RoadModelSectionViewerPointDto, Point> map)
    {
        if (SelectedPreview == null)
        {
            return;
        }

        var segments = SelectedPreview.Segments;
        for (var i = 0; i + 3 < segments.Count; i++)
        {
            var first = segments[i];
            var second = segments[i + 1];
            var third = segments[i + 2];
            var fourth = segments[i + 3];
            if (!IsPavementLayerFace(first, second, third, fourth))
            {
                continue;
            }

            var polygon = new Polygon
            {
                Fill = new SolidColorBrush(Color.FromArgb(
                    74,
                    ClampColor(first.ColorR),
                    ClampColor(first.ColorG),
                    ClampColor(first.ColorB))),
                StrokeThickness = 0,
            };
            polygon.Points.Add(map(first.Points[0]));
            polygon.Points.Add(map(first.Points[first.Points.Count - 1]));
            polygon.Points.Add(map(second.Points[second.Points.Count - 1]));
            polygon.Points.Add(map(third.Points[third.Points.Count - 1]));
            PreviewCanvas.Children.Add(polygon);
            i += 3;
        }
    }

    private PreviewTransform? CreatePreviewTransform(
        RoadModelSectionViewerPreviewDto? preview,
        double zoom,
        Vector pan)
    {
        if (preview == null || PreviewCanvas.ActualWidth <= 1.0 || PreviewCanvas.ActualHeight <= 1.0)
        {
            return null;
        }

        var points = preview.Segments.SelectMany(segment => segment.Points).ToList();
        if (points.Count == 0)
        {
            return null;
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
        var contentWidth = Math.Max(maxOffset - minOffset, 1.0e-6);
        var contentHeight = Math.Max(maxElevation - minElevation, 1.0e-6);
        var usableWidth = Math.Max(PreviewCanvas.ActualWidth - padding * 2.0, 1.0);
        var usableHeight = Math.Max(PreviewCanvas.ActualHeight - padding * 2.0, 1.0);
        var baseScale = Math.Min(usableWidth / contentWidth, usableHeight / contentHeight);
        return new PreviewTransform(
            PreviewCanvas.ActualWidth,
            PreviewCanvas.ActualHeight,
            minOffset,
            maxElevation,
            contentWidth,
            contentHeight,
            baseScale * Math.Max(0.05, zoom),
            pan);
    }

    private void PreviewCanvas_MouseWheel(object sender, MouseWheelEventArgs e)
    {
        var oldTransform = CreatePreviewTransform(SelectedPreview, _previewZoom, _previewPan);
        var mouse = e.GetPosition(PreviewCanvas);
        var newZoom = Math.Max(0.25, Math.Min(10.0, _previewZoom * (e.Delta > 0 ? 1.12 : 1.0 / 1.12)));
        if (oldTransform != null)
        {
            var worldPoint = oldTransform.ScreenToWorld(mouse);
            var newTransform = CreatePreviewTransform(SelectedPreview, newZoom, _previewPan);
            if (newTransform != null)
            {
                _previewPan += mouse - newTransform.WorldToScreen(worldPoint);
            }
        }

        _previewZoom = newZoom;
        DrawPreview();
        e.Handled = true;
    }

    private void PreviewCanvas_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
    {
        _panning = true;
        _lastPanPoint = e.GetPosition(PreviewCanvas);
        PreviewCanvas.CaptureMouse();
        e.Handled = true;
    }

    private void PreviewCanvas_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
    {
        EndPan();
        e.Handled = true;
    }

    private void PreviewCanvas_MouseMove(object sender, MouseEventArgs e)
    {
        if (!_panning)
        {
            return;
        }

        var point = e.GetPosition(PreviewCanvas);
        _previewPan += point - _lastPanPoint;
        _lastPanPoint = point;
        DrawPreview();
        e.Handled = true;
    }

    private void PreviewCanvas_MouseLeave(object sender, MouseEventArgs e)
        => EndPan();

    private void EndPan()
    {
        if (!_panning)
        {
            return;
        }

        _panning = false;
        PreviewCanvas.ReleaseMouseCapture();
    }

    private void OnDrawSections(object sender, RoutedEventArgs e)
    {
        Response = new RoadModelSectionViewerResponse
        {
            Action = RoadModelSectionViewerAction.DrawSections,
            Accepted = true,
            Handle = Handle,
        };
        DialogResult = true;
        Close();
    }

    private void OnClose(object sender, RoutedEventArgs e)
        => Close();

    private static bool IsPavementLayerFace(
        RoadModelSectionViewerSegmentDto first,
        RoadModelSectionViewerSegmentDto second,
        RoadModelSectionViewerSegmentDto third,
        RoadModelSectionViewerSegmentDto fourth)
        => first.Kind == RoadModelSectionViewerSegmentKind.PavementLayer
            && second.Kind == RoadModelSectionViewerSegmentKind.PavementLayer
            && third.Kind == RoadModelSectionViewerSegmentKind.PavementLayer
            && fourth.Kind == RoadModelSectionViewerSegmentKind.PavementLayer
            && first.Points.Count >= 2
            && second.Points.Count >= 2
            && third.Points.Count >= 2
            && fourth.Points.Count >= 2
            && SamePoint(first.Points[first.Points.Count - 1], second.Points[0])
            && SamePoint(second.Points[second.Points.Count - 1], third.Points[0])
            && SamePoint(third.Points[third.Points.Count - 1], fourth.Points[0])
            && SamePoint(fourth.Points[fourth.Points.Count - 1], first.Points[0]);

    private static bool SamePoint(RoadModelSectionViewerPointDto first, RoadModelSectionViewerPointDto second)
        => Math.Abs(first.Offset - second.Offset) <= 1.0e-6
            && Math.Abs(first.Elevation - second.Elevation) <= 1.0e-6;

    private static byte ClampColor(int value)
        => (byte)Math.Max(0, Math.Min(255, value));

    private static double StrokeThicknessFor(RoadModelSectionViewerSegmentKind kind)
        => kind switch
        {
            RoadModelSectionViewerSegmentKind.Ground => 2.2,
            RoadModelSectionViewerSegmentKind.PavementLayer => 2.1,
            _ => 2.0,
        };

    private void OnPropertyChanged([CallerMemberName] string? propertyName = null)
        => PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));

    private sealed class PreviewTransform
    {
        private readonly double _canvasWidth;
        private readonly double _canvasHeight;
        private readonly double _minX;
        private readonly double _maxY;
        private readonly double _contentWidth;
        private readonly double _contentHeight;
        private readonly double _scale;
        private readonly Vector _pan;

        public PreviewTransform(
            double canvasWidth,
            double canvasHeight,
            double minX,
            double maxY,
            double contentWidth,
            double contentHeight,
            double scale,
            Vector pan)
        {
            _canvasWidth = canvasWidth;
            _canvasHeight = canvasHeight;
            _minX = minX;
            _maxY = maxY;
            _contentWidth = contentWidth;
            _contentHeight = contentHeight;
            _scale = scale;
            _pan = pan;
        }

        public Point WorldToScreen(Point point)
        {
            var originX = (_canvasWidth - _contentWidth * _scale) / 2.0 + _pan.X;
            var originY = (_canvasHeight - _contentHeight * _scale) / 2.0 + _pan.Y;
            return new Point(
                originX + (point.X - _minX) * _scale,
                originY + (_maxY - point.Y) * _scale);
        }

        public Point ScreenToWorld(Point point)
        {
            var originX = (_canvasWidth - _contentWidth * _scale) / 2.0 + _pan.X;
            var originY = (_canvasHeight - _contentHeight * _scale) / 2.0 + _pan.Y;
            return new Point(
                _minX + (point.X - originX) / _scale,
                _maxY - (point.Y - originY) / _scale);
        }
    }
}
