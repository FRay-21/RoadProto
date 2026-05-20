using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public enum SlopeTemplateKind
{
    Fill,
    Cut,
}

public enum SlopeComponentType
{
    FillSlope,
    CutSlope,
    Berm,
}

public enum SlopeGeometryConstraintMode
{
    SlopeAndHeight,
    SlopeAndWidth,
    HeightAndWidth,
}

public sealed class SlopeComponentDto
{
    public SlopeComponentType Type { get; set; } = SlopeComponentType.FillSlope;
    public SlopeGeometryConstraintMode ConstraintMode { get; set; } = SlopeGeometryConstraintMode.SlopeAndHeight;
    public double Slope { get; set; } = -1.0 / 1.5;
    public double Height { get; set; } = 4.0;
    public double Width { get; set; } = 6.0;
    public double GroundSearchHeightIncrement { get; set; }
    public int ColorR { get; set; } = 120;
    public int ColorG { get; set; } = 120;
    public int ColorB { get; set; } = 120;

    public string DisplayName => $"{SlopeTemplateLabels.ComponentTypeLabel(Type)}  {Slope:0.###} / {Height:0.##} / {Width:0.##}";

    public SlopeComponentDto Clone()
        => new()
        {
            Type = Type,
            ConstraintMode = ConstraintMode,
            Slope = Slope,
            Height = Height,
            Width = Width,
            GroundSearchHeightIncrement = GroundSearchHeightIncrement,
            ColorR = ColorR,
            ColorG = ColorG,
            ColorB = ColorB,
        };
}

public sealed class SlopeTemplateDialogRequest
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public double InsertionX { get; set; }
    public double InsertionY { get; set; }
    public double InsertionZ { get; set; }
    public string TemplateName { get; set; } = "边坡模板1";
    public double DisplayScale { get; set; } = 10.0;
    public SlopeTemplateKind Kind { get; set; } = SlopeTemplateKind.Fill;
    public bool StopAtGround { get; set; }
    public bool RepeatLastGroupUntilGround { get; set; }
    public List<SlopeComponentDto> Components { get; set; } = new();
}

public sealed class SlopeTemplateDialogResponse
{
    public bool Accepted { get; set; }
    public string Handle { get; set; } = string.Empty;
    public double InsertionX { get; set; }
    public double InsertionY { get; set; }
    public double InsertionZ { get; set; }
    public string TemplateName { get; set; } = "边坡模板1";
    public double DisplayScale { get; set; } = 10.0;
    public SlopeTemplateKind Kind { get; set; } = SlopeTemplateKind.Fill;
    public bool StopAtGround { get; set; }
    public bool RepeatLastGroupUntilGround { get; set; }
    public List<SlopeComponentDto> Components { get; set; } = new();
}

public static class SlopeTemplateLabels
{
    public static string KindLabel(SlopeTemplateKind kind)
        => kind == SlopeTemplateKind.Cut ? "挖方边坡" : "填方边坡";

    public static string ComponentTypeLabel(SlopeComponentType type)
        => type switch
        {
            SlopeComponentType.FillSlope => "填方边坡",
            SlopeComponentType.CutSlope => "挖方边坡",
            SlopeComponentType.Berm => "护坡道",
            _ => type.ToString(),
        };

    public static string ConstraintModeLabel(SlopeGeometryConstraintMode mode)
        => mode switch
        {
            SlopeGeometryConstraintMode.SlopeAndHeight => "坡率 + 坡高",
            SlopeGeometryConstraintMode.SlopeAndWidth => "坡率 + 宽度",
            SlopeGeometryConstraintMode.HeightAndWidth => "坡高 + 宽度",
            _ => mode.ToString(),
        };
}
