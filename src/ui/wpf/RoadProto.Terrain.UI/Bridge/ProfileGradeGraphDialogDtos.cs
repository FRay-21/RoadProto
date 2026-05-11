namespace RoadProto.Terrain.UI.Bridge;

public enum ProfileGradeGraphSourceType
{
    TerrainTin,
    DmxFile,
}

public sealed class ProfileGradeGraphDialogRequest
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public string TerrainTinHandle { get; set; } = string.Empty;
    public string GraphName { get; set; } = "拉坡图";
    public ProfileGradeGraphSourceType SourceType { get; set; } = ProfileGradeGraphSourceType.DmxFile;
    public string DmxFilePath { get; set; } = string.Empty;
    public int GroundLineColorIndex { get; set; } = 4;
    public double GroundLineWidth { get; set; } = 1.0;
    public double GroundLinePrecision { get; set; } = 10.0;
    public double VerticalScale { get; set; } = 10.0;
    public double GridSpacing { get; set; } = 10.0;
    public int SampleCount { get; set; }
}

public sealed class ProfileGradeGraphDialogResponse
{
    public bool Accepted { get; set; }
    public string Handle { get; set; } = string.Empty;
    public string GraphName { get; set; } = "拉坡图";
    public int GroundLineColorIndex { get; set; } = 4;
    public double GroundLineWidth { get; set; } = 1.0;
    public double GroundLinePrecision { get; set; } = 10.0;
    public double VerticalScale { get; set; } = 10.0;
    public double GridSpacing { get; set; } = 10.0;
    public bool UpdateGroundLineRequested { get; set; }
}
