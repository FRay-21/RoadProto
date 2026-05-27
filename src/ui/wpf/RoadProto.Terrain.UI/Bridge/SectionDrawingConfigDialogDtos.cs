using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public enum SectionDrawingConfigAction
{
    None,
    Draw,
    PickTemplate,
}

public sealed class SectionDrawingConfigComponentOptionDto
{
    public string Code { get; set; } = string.Empty;
    public string DisplayName { get; set; } = string.Empty;
}

public sealed class SectionDrawingConfigRowDto
{
    public double StartStation { get; set; }
    public double EndStation { get; set; }
    public List<string> ComponentCodes { get; set; } = new();
    public string ComponentDisplayText { get; set; } = string.Empty;
    public string TemplateHandle { get; set; } = string.Empty;
    public string TemplateName { get; set; } = string.Empty;
}

public sealed class SectionDrawingConfigRequest
{
    public string DrawingHandle { get; set; } = string.Empty;
    public string RoadModelHandle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public string ConfigPath { get; set; } = string.Empty;
    public List<SectionDrawingConfigComponentOptionDto> ComponentOptions { get; set; } = new();
    public List<SectionDrawingConfigRowDto> PavementRows { get; set; } = new();
}

public sealed class SectionDrawingConfigResponse
{
    public SectionDrawingConfigAction Action { get; set; } = SectionDrawingConfigAction.None;
    public bool Accepted { get; set; }
    public string DrawingHandle { get; set; } = string.Empty;
    public string RoadModelHandle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public int PickRowIndex { get; set; } = -1;
    public string ConfigPath { get; set; } = string.Empty;
    public List<SectionDrawingConfigComponentOptionDto> ComponentOptions { get; set; } = new();
    public List<SectionDrawingConfigRowDto> PavementRows { get; set; } = new();
}
