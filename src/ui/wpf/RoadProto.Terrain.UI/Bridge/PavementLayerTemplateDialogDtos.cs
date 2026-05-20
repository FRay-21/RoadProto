using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public enum PavementLayerType
{
    UpperSurface,
    MiddleSurface,
    LowerSurface,
    Base,
    Subbase,
    Cushion,
}

public sealed class PavementLayerTemplateLayerDto
{
    public PavementLayerType Type { get; set; } = PavementLayerType.UpperSurface;
    public string Name { get; set; } = "上面层";
    public bool UniformThickness { get; set; } = true;
    public double Thickness { get; set; } = 0.04;
    public double InnerThickness { get; set; } = 0.04;
    public double OuterThickness { get; set; } = 0.04;
    public double InnerWidening { get; set; }
    public double OuterWidening { get; set; }
    public double InnerSlope { get; set; }
    public double OuterSlope { get; set; }

    public string DisplayName => $"{PavementLayerTemplateLabels.LayerTypeLabel(Type)}  {Name}";

    public PavementLayerTemplateLayerDto Clone()
        => new()
        {
            Type = Type,
            Name = Name,
            UniformThickness = UniformThickness,
            Thickness = Thickness,
            InnerThickness = InnerThickness,
            OuterThickness = OuterThickness,
            InnerWidening = InnerWidening,
            OuterWidening = OuterWidening,
            InnerSlope = InnerSlope,
            OuterSlope = OuterSlope,
        };
}

public class PavementLayerTemplateDto
{
    public string TemplateName { get; set; } = "路面结构层模板";
    public double DisplayScale { get; set; } = 10.0;
    public double PreviewWidth { get; set; } = 3.75;
    public List<PavementLayerTemplateLayerDto> Layers { get; set; } = new();
}

public sealed class PavementLayerTemplateDialogRequest : PavementLayerTemplateDto
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public double InsertionX { get; set; }
    public double InsertionY { get; set; }
    public double InsertionZ { get; set; }
}

public sealed class PavementLayerTemplateDialogResponse : PavementLayerTemplateDto
{
    public bool Accepted { get; set; }
    public string Handle { get; set; } = string.Empty;
    public double InsertionX { get; set; }
    public double InsertionY { get; set; }
    public double InsertionZ { get; set; }
}

public static class PavementLayerTemplateLabels
{
    public static string LayerTypeLabel(PavementLayerType type)
        => type switch
        {
            PavementLayerType.UpperSurface => "上面层",
            PavementLayerType.MiddleSurface => "中面层",
            PavementLayerType.LowerSurface => "下面层",
            PavementLayerType.Base => "基层",
            PavementLayerType.Subbase => "底基层",
            PavementLayerType.Cushion => "垫层",
            _ => type.ToString(),
        };

    public static PavementLayerTemplateDto CloneTemplate(PavementLayerTemplateDto source)
        => new()
        {
            TemplateName = source.TemplateName,
            DisplayScale = source.DisplayScale,
            PreviewWidth = source.PreviewWidth,
            Layers = source.Layers.ConvertAll(layer => layer.Clone()),
        };

    public static List<PavementLayerTemplateLayerDto> DefaultLayers()
        => new()
        {
            MakeDefault(PavementLayerType.UpperSurface, 0.04, 0.0, 0.0),
            MakeDefault(PavementLayerType.MiddleSurface, 0.06, 0.0, 0.0),
            MakeDefault(PavementLayerType.LowerSurface, 0.08, 0.0, 0.0),
            MakeDefault(PavementLayerType.Base, 0.18, 0.15, 0.15),
            MakeDefault(PavementLayerType.Subbase, 0.20, 0.15, 0.15),
            MakeDefault(PavementLayerType.Cushion, 0.15, 0.15, 0.15),
        };

    private static PavementLayerTemplateLayerDto MakeDefault(
        PavementLayerType type,
        double thickness,
        double innerWidening,
        double outerWidening)
        => new()
        {
            Type = type,
            Name = LayerTypeLabel(type),
            UniformThickness = true,
            Thickness = thickness,
            InnerThickness = thickness,
            OuterThickness = thickness,
            InnerWidening = innerWidening,
            OuterWidening = outerWidening,
        };
}
