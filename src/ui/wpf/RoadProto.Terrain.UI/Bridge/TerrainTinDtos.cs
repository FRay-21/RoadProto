namespace RoadProto.Terrain.UI.Bridge;

public sealed class TerrainTinExtractSummaryDto
{
    public int SelectedObjectCount { get; set; }
    public int RawPointCount { get; set; }
    public int ValidPointCount { get; set; }
    public int DuplicatePointCount { get; set; }
    public int ZConflictPointCount { get; set; }
    public int InvalidObjectCount { get; set; }
    public int TextElevationPointCount { get; set; }
    public int TextPointMergeCount { get; set; }
    public int TextAssignedElevationPointCount { get; set; }
    public int BlockCount { get; set; }
    public int RecognizedElevationBlockCount { get; set; }
    public int BlockAttributeElevationCount { get; set; }
    public int BlockParseFailedCount { get; set; }
    public int TriangleCount { get; set; }
    public string Status { get; set; } = "等待提取";
}

public sealed class TerrainTinOptionsDto
{
    public double XyMergeTolerance { get; set; } = 0.001;
    public double ZEqualTolerance { get; set; } = 0.01;
    public double TextPointSearchDistance { get; set; } = 0.5;
    public double MaxEdgeLength { get; set; }
    public double MinTriangleArea { get; set; } = 1e-8;
    public bool RemoveDegenerateTriangles { get; set; } = true;
    public bool EnableBlockExtraction { get; set; } = true;
    public bool EnableNestedBlockExtraction { get; set; } = true;
    public bool UseTextElevationForNearbyFlatPoint { get; set; } = true;
    public string DisplayMode { get; set; } = "ColoredTriangles";
}

public sealed class TerrainTinBuildResultDto
{
    public bool Succeeded { get; set; }
    public int TriangleCount { get; set; }
    public double MinElevation { get; set; }
    public double MaxElevation { get; set; }
    public string Message { get; set; } = string.Empty;
}
