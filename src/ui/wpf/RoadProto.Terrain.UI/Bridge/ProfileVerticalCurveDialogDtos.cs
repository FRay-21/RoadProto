using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public sealed class ProfileVerticalCurvePviDto
{
    public double Station { get; set; }
    public double Elevation { get; set; }
    public double Radius { get; set; } = 1000.0;
}

public sealed class ProfileVerticalCurveDialogRequest
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public string ProfileGraphHandle { get; set; } = string.Empty;
    public string Name { get; set; } = "竖曲线";
    public double StartStation { get; set; }
    public double StartElevation { get; set; }
    public double EndStation { get; set; }
    public double EndElevation { get; set; }
    public int CurrentPviIndex { get; set; }
    public List<ProfileVerticalCurvePviDto> Pvis { get; set; } = [];
}

public sealed class ProfileVerticalCurveDialogResponse
{
    public bool Accepted { get; set; }
    public string Handle { get; set; } = string.Empty;
    public string ProfileGraphHandle { get; set; } = string.Empty;
    public string Name { get; set; } = "竖曲线";
    public double StartStation { get; set; }
    public double StartElevation { get; set; }
    public double EndStation { get; set; }
    public double EndElevation { get; set; }
    public int CurrentPviIndex { get; set; }
    public List<ProfileVerticalCurvePviDto> Pvis { get; set; } = [];
}
