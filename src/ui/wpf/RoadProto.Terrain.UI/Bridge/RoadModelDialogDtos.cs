using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public sealed class RoadModelTemplateAssignmentDto
{
    public double StartStation { get; set; }
    public double EndStation { get; set; }
    public string TemplateHandle { get; set; } = string.Empty;
    public string TemplateName { get; set; } = string.Empty;

    public RoadModelTemplateAssignmentDto Clone()
        => new()
        {
            StartStation = StartStation,
            EndStation = EndStation,
            TemplateHandle = TemplateHandle,
            TemplateName = TemplateName,
        };
}

public sealed class RoadModelDialogRequest
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public string ProfileVerticalCurveHandle { get; set; } = string.Empty;
    public double SampleInterval { get; set; } = 10.0;
    public List<RoadModelTemplateAssignmentDto> Assignments { get; set; } = new();
}

public sealed class RoadModelDialogResponse
{
    public bool Accepted { get; set; }
    public string Handle { get; set; } = string.Empty;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public string ProfileVerticalCurveHandle { get; set; } = string.Empty;
    public double SampleInterval { get; set; } = 10.0;
    public List<RoadModelTemplateAssignmentDto> Assignments { get; set; } = new();
}
