using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public enum AlignmentDialogMode
{
    Properties,
    Curve,
    Full,
}

public enum AlignmentDialogAction
{
    None,
    PickTerrain,
}

public sealed class AlignmentDialogRequest
{
    public AlignmentDialogMode Mode { get; set; } = AlignmentDialogMode.Full;
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public bool DeleteOnCancel { get; set; }
    public int CurveIndex { get; set; }
    public string RoadName { get; set; } = "K1";
    public int RoadGradeIndex { get; set; } = 9;
    public bool LinkedTerrainEnabled { get; set; }
    public string LinkedTerrainHandle { get; set; } = string.Empty;
    public double StationLabelInterval { get; set; } = 100.0;
    public double TangentIn { get; set; }
    public double TangentOut { get; set; }
    public double Ls1 { get; set; } = 20.0;
    public double Radius { get; set; } = 80.0;
    public double Ls2 { get; set; } = 20.0;
    public List<AlignmentCurveParameterDto> CurveParameters { get; set; } = new();
}

public sealed class AlignmentDialogResponse
{
    public AlignmentDialogAction Action { get; set; } = AlignmentDialogAction.None;
    public bool Accepted { get; set; }
    public AlignmentDialogMode Mode { get; set; } = AlignmentDialogMode.Full;
    public string Handle { get; set; } = string.Empty;
    public bool DeleteOnCancel { get; set; }
    public int CurveIndex { get; set; }
    public string RoadName { get; set; } = "K1";
    public int RoadGradeIndex { get; set; } = 9;
    public bool LinkedTerrainEnabled { get; set; }
    public string LinkedTerrainHandle { get; set; } = string.Empty;
    public double StationLabelInterval { get; set; } = 100.0;
    public double TangentIn { get; set; }
    public double TangentOut { get; set; }
    public double Ls1 { get; set; } = 20.0;
    public double Radius { get; set; } = 80.0;
    public double Ls2 { get; set; } = 20.0;
    public List<AlignmentCurveParameterDto> CurveParameters { get; set; } = new();
}

public sealed class AlignmentCurveParameterDto
{
    public double TangentIn { get; set; }
    public double TangentOut { get; set; }
    public double Ls1 { get; set; } = 20.0;
    public double Radius { get; set; } = 80.0;
    public double Ls2 { get; set; } = 20.0;
}
