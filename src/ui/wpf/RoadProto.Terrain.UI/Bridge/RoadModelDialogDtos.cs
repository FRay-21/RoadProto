using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public enum RoadModelDialogAction
{
    None,
    PickTemplate,
    PickLeftSlopeTemplate,
    PickRightSlopeTemplate,
}

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

public sealed class RoadModelSlopeTemplateReferenceDto
{
    public string TemplateHandle { get; set; } = string.Empty;
    public string TemplateName { get; set; } = string.Empty;

    public RoadModelSlopeTemplateReferenceDto Clone()
        => new()
        {
            TemplateHandle = TemplateHandle,
            TemplateName = TemplateName,
        };
}

public sealed class RoadModelSlopeTemplateGroupDto
{
    public double StartStation { get; set; }
    public double EndStation { get; set; }
    public List<RoadModelSlopeTemplateReferenceDto> Templates { get; set; } = new();

    public string TemplateSummary
        => Templates.Count == 0
            ? string.Empty
            : string.Join(" / ", Templates.ConvertAll(item => string.IsNullOrWhiteSpace(item.TemplateName) ? item.TemplateHandle : item.TemplateName));

    public RoadModelSlopeTemplateGroupDto Clone()
        => new()
        {
            StartStation = StartStation,
            EndStation = EndStation,
            Templates = Templates.ConvertAll(item => item.Clone()),
        };
}

public sealed class RoadModelDialogRequest
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public string ProfileVerticalCurveHandle { get; set; } = string.Empty;
    public double SampleInterval { get; set; } = 10.0;
    public double LeftSlopeSearchWidth { get; set; } = 50.0;
    public double RightSlopeSearchWidth { get; set; } = 50.0;
    public int SelectedAssignmentIndex { get; set; } = -1;
    public int SelectedLeftSlopeGroupIndex { get; set; } = -1;
    public int SelectedRightSlopeGroupIndex { get; set; } = -1;
    public List<RoadModelTemplateAssignmentDto> Assignments { get; set; } = new();
    public List<RoadModelSlopeTemplateGroupDto> LeftSlopeGroups { get; set; } = new();
    public List<RoadModelSlopeTemplateGroupDto> RightSlopeGroups { get; set; } = new();
}

public sealed class RoadModelDialogResponse
{
    public RoadModelDialogAction Action { get; set; } = RoadModelDialogAction.None;
    public bool Accepted { get; set; }
    public int PickAssignmentIndex { get; set; } = -1;
    public int PickSlopeGroupIndex { get; set; } = -1;
    public string Handle { get; set; } = string.Empty;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public string ProfileVerticalCurveHandle { get; set; } = string.Empty;
    public double SampleInterval { get; set; } = 10.0;
    public double LeftSlopeSearchWidth { get; set; } = 50.0;
    public double RightSlopeSearchWidth { get; set; } = 50.0;
    public List<RoadModelTemplateAssignmentDto> Assignments { get; set; } = new();
    public List<RoadModelSlopeTemplateGroupDto> LeftSlopeGroups { get; set; } = new();
    public List<RoadModelSlopeTemplateGroupDto> RightSlopeGroups { get; set; } = new();
}
