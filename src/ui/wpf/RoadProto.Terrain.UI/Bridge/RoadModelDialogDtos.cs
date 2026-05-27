using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public enum RoadModelDialogAction
{
    None,
    PickTemplate,
    PickLeftSlopeTemplate,
    PickRightSlopeTemplate,
}

public enum RoadModelStructureType
{
    Bridge,
    Tunnel,
}

public enum RoadModelStructureSideRange
{
    Left,
    Right,
    Both,
}

public sealed class RoadModelStructureTypeOption
{
    public RoadModelStructureType Value { get; set; }
    public string Label { get; set; } = string.Empty;
}

public sealed class RoadModelStructureSideRangeOption
{
    public RoadModelStructureSideRange Value { get; set; }
    public string Label { get; set; } = string.Empty;
}

public static class RoadModelStructureOptions
{
    public static RoadModelStructureTypeOption[] Types { get; } =
    {
        new() { Value = RoadModelStructureType.Bridge, Label = "桥梁" },
        new() { Value = RoadModelStructureType.Tunnel, Label = "隧道" },
    };

    public static RoadModelStructureSideRangeOption[] SideRanges { get; } =
    {
        new() { Value = RoadModelStructureSideRange.Left, Label = "左侧" },
        new() { Value = RoadModelStructureSideRange.Right, Label = "右侧" },
        new() { Value = RoadModelStructureSideRange.Both, Label = "两侧" },
    };
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

public sealed class RoadModelStructureRangeDto
{
    public double StartStation { get; set; }
    public double EndStation { get; set; }
    public RoadModelStructureType Type { get; set; } = RoadModelStructureType.Bridge;
    public RoadModelStructureSideRange SideRange { get; set; } = RoadModelStructureSideRange.Both;

    public RoadModelStructureRangeDto Clone()
        => new()
        {
            StartStation = StartStation,
            EndStation = EndStation,
            Type = Type,
            SideRange = SideRange,
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
    public List<RoadModelStructureRangeDto> Structures { get; set; } = new();
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
    public List<RoadModelStructureRangeDto> Structures { get; set; } = new();
    public List<RoadModelSlopeTemplateGroupDto> LeftSlopeGroups { get; set; } = new();
    public List<RoadModelSlopeTemplateGroupDto> RightSlopeGroups { get; set; } = new();
}
