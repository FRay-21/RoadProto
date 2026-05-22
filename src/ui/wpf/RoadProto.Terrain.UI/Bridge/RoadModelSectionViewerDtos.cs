using System.Collections.Generic;
using System.Globalization;

namespace RoadProto.Terrain.UI.Bridge;

public enum RoadModelSectionViewerSegmentKind
{
    Subgrade,
    Slope,
    Ground,
    PavementLayer,
}

public sealed class RoadModelSectionViewerPointDto
{
    public double Offset { get; set; }
    public double Elevation { get; set; }
}

public sealed class RoadModelSectionViewerSegmentDto
{
    public RoadModelSectionViewerSegmentKind Kind { get; set; } = RoadModelSectionViewerSegmentKind.Subgrade;
    public string Label { get; set; } = string.Empty;
    public int ColorR { get; set; }
    public int ColorG { get; set; }
    public int ColorB { get; set; }
    public List<RoadModelSectionViewerPointDto> Points { get; set; } = new();
}

public sealed class RoadModelSectionViewerPreviewDto
{
    public double Station { get; set; }
    public string StationLabel { get; set; } = string.Empty;
    public string StatusMessage { get; set; } = string.Empty;
    public bool HasGroundLine { get; set; }
    public List<RoadModelSectionViewerSegmentDto> Segments { get; set; } = new();

    public string DisplayStation
        => string.IsNullOrWhiteSpace(StationLabel)
            ? Station.ToString("0.###", CultureInfo.InvariantCulture)
            : StationLabel;
}

public sealed class RoadModelSectionViewerRequest
{
    public string Handle { get; set; } = string.Empty;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public List<RoadModelSectionViewerPreviewDto> Previews { get; set; } = new();
}
