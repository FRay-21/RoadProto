using System.Collections.Generic;
using System.Linq;

namespace RoadProto.Terrain.UI.Bridge;

public enum PavementLayerType
{
    UpperSurface,
    MiddleSurface,
    LowerSurface,
    Base,
    Subbase,
    Cushion,
    AsphaltSeal,
    ApproachSlab,
}

public enum PavementLayerTemplateDisplayMode
{
    Color,
    Hatch,
    HatchAndColor,
}

public enum PavementSubgradeMoistureType
{
    Dry,
    Medium,
    Wet,
    OverWet,
}

public enum PavementSurfaceType
{
    Asphalt,
    Concrete,
}

public enum PavementSubgradeSoilGroup
{
    Bedrock,
    CrushedStoneSoil,
    GravelSoil,
    SandSoil,
    SiltySoil,
    LowLiquidLimitClay,
    HighLiquidLimitClay,
    OrganicSoil,
    SoftSoil,
    ExpansiveSoil,
    Loess,
    Other,
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
    public int ColorR { get; set; } = -1;
    public int ColorG { get; set; } = -1;
    public int ColorB { get; set; } = -1;
    public string HatchPattern { get; set; } = "SOLID";
    public double HatchAngle { get; set; }
    public double HatchScale { get; set; } = 1.0;

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
            ColorR = ColorR,
            ColorG = ColorG,
            ColorB = ColorB,
            HatchPattern = PavementLayerTemplateLabels.NormalizeHatchPattern(HatchPattern),
            HatchAngle = PavementLayerTemplateLabels.NormalizeHatchAngle(HatchAngle),
            HatchScale = PavementLayerTemplateLabels.NormalizeHatchScale(HatchScale),
        };
}

public class PavementLayerTemplateDto
{
    public string TemplateName { get; set; } = "路面结构层模板";
    public double DisplayScale { get; set; } = 10.0;
    public double PreviewWidth { get; set; } = 3.75;
    public PavementLayerTemplateDisplayMode DisplayMode { get; set; } = PavementLayerTemplateDisplayMode.Color;
    public bool ShowAllGeneralParameters { get; set; }
    public string StructureCode { get; set; } = string.Empty;
    public List<PavementSubgradeMoistureType> SubgradeMoistureTypes { get; set; } = new();
    public PavementSurfaceType PavementType { get; set; } = PavementSurfaceType.Asphalt;
    public List<PavementSubgradeSoilGroup> SubgradeSoilGroups { get; set; } = new();
    public string DesignDeflection { get; set; } = string.Empty;
    public string CumulativeAxleLoads { get; set; } = string.Empty;
    public List<PavementLayerTemplateLayerDto> Layers { get; set; } = new();
}

public sealed class PavementLayerTemplateDialogRequest : PavementLayerTemplateDto
{
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public bool ShowCreateWizard { get; set; }
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
            PavementLayerType.AsphaltSeal => "沥青封层",
            PavementLayerType.ApproachSlab => "搭板层",
            _ => type.ToString(),
        };

    public static PavementLayerTemplateDto CloneTemplate(PavementLayerTemplateDto source)
        => new()
        {
            TemplateName = source.TemplateName,
            DisplayScale = source.DisplayScale,
            PreviewWidth = source.PreviewWidth,
            DisplayMode = source.DisplayMode,
            ShowAllGeneralParameters = source.ShowAllGeneralParameters,
            StructureCode = source.StructureCode,
            SubgradeMoistureTypes = source.SubgradeMoistureTypes.ToList(),
            PavementType = source.PavementType,
            SubgradeSoilGroups = source.SubgradeSoilGroups.ToList(),
            DesignDeflection = source.DesignDeflection,
            CumulativeAxleLoads = source.CumulativeAxleLoads,
            Layers = source.Layers.ConvertAll(layer => layer.Clone()),
        };

    public static string MoistureTypeLabel(PavementSubgradeMoistureType type)
        => type switch
        {
            PavementSubgradeMoistureType.Dry => "干燥",
            PavementSubgradeMoistureType.Medium => "中湿",
            PavementSubgradeMoistureType.Wet => "潮湿",
            PavementSubgradeMoistureType.OverWet => "过湿",
            _ => type.ToString(),
        };

    public static string PavementTypeLabel(PavementSurfaceType type)
        => type switch
        {
            PavementSurfaceType.Concrete => "混凝土路面",
            _ => "沥青路面",
        };

    public static string SoilGroupLabel(PavementSubgradeSoilGroup group)
        => group switch
        {
            PavementSubgradeSoilGroup.Bedrock => "基岩",
            PavementSubgradeSoilGroup.CrushedStoneSoil => "碎石土",
            PavementSubgradeSoilGroup.GravelSoil => "砾类土",
            PavementSubgradeSoilGroup.SandSoil => "砂类土",
            PavementSubgradeSoilGroup.SiltySoil => "粉质土",
            PavementSubgradeSoilGroup.LowLiquidLimitClay => "低液限黏土",
            PavementSubgradeSoilGroup.HighLiquidLimitClay => "高液限黏土",
            PavementSubgradeSoilGroup.OrganicSoil => "有机质土",
            PavementSubgradeSoilGroup.SoftSoil => "软土",
            PavementSubgradeSoilGroup.ExpansiveSoil => "膨胀土",
            PavementSubgradeSoilGroup.Loess => "黄土",
            _ => "其他",
        };

    public static List<PavementLayerTemplateLayerDto> DefaultLayers()
        => new()
        {
            MakeDefault(PavementLayerType.UpperSurface, 0.04, 0.0, 0.0),
            MakeDefault(PavementLayerType.MiddleSurface, 0.06, 0.0, 0.0),
            MakeDefault(PavementLayerType.LowerSurface, 0.08, 0.0, 0.0),
            MakeDefault(PavementLayerType.AsphaltSeal, 0.01, 0.0, 0.0),
            MakeDefault(PavementLayerType.Base, 0.18, 0.15, 0.15),
            MakeDefault(PavementLayerType.Subbase, 0.20, 0.15, 0.15),
            MakeDefault(PavementLayerType.Cushion, 0.15, 0.15, 0.15),
            MakeDefault(PavementLayerType.ApproachSlab, 0.35, 0.0, 0.0),
        };

    private static PavementLayerTemplateLayerDto MakeDefault(
        PavementLayerType type,
        double thickness,
        double innerWidening,
        double outerWidening)
    {
        var color = DefaultColorForLayerIndex((int)type);
        return new()
        {
            Type = type,
            Name = LayerTypeLabel(type),
            UniformThickness = true,
            Thickness = thickness,
            InnerThickness = thickness,
            OuterThickness = thickness,
            InnerWidening = innerWidening,
            OuterWidening = outerWidening,
            ColorR = color.R,
            ColorG = color.G,
            ColorB = color.B,
            HatchPattern = "SOLID",
            HatchAngle = 0.0,
            HatchScale = 1.0,
        };
    }

    public static IReadOnlyList<string> HatchPatternOptions { get; } = new[]
    {
        "SOLID",
        "ANSI31",
        "ANSI32",
        "ANSI33",
        "ANSI34",
        "ANSI35",
        "ANSI36",
        "ANSI37",
        "ANSI38",
        "AR-B816",
        "AR-B816C",
        "AR-B88",
        "AR-BRELM",
        "AR-BRSTD",
        "AR-CONC",
        "AR-HBONE",
        "AR-PARQ1",
        "AR-RROOF",
        "AR-RSHKE",
        "AR-SAND",
        "BOX",
        "BRASS",
        "BRICK",
        "BRSTONE",
        "CLAY",
        "CORK",
        "CROSS",
        "DASH",
        "DOLMIT",
        "DOTS",
        "EARTH",
        "ESCHER",
        "FLEX",
        "GOST_GLASS",
        "GOST_GROUND",
        "GOST_WOOD",
        "GRASS",
        "GRATE",
        "GRAVEL",
        "HEX",
        "HONEY",
        "HOUND",
        "INSUL",
        "LINE",
        "MUDST",
        "NET",
        "NET3",
        "PLAST",
        "PLASTI",
        "SACNCR",
        "SQUARE",
        "STARS",
        "STEEL",
        "SWAMP",
        "TRANS",
        "TRIANG",
        "ZIGZAG",
    };

    public static string NormalizeHatchPattern(string? hatchPattern)
        => HatchPatternOptions.Contains(hatchPattern ?? string.Empty)
            ? hatchPattern!
            : "SOLID";

    public static double NormalizeHatchAngle(double hatchAngle)
        => double.IsNaN(hatchAngle) || double.IsInfinity(hatchAngle) ? 0.0 : hatchAngle;

    public static double NormalizeHatchScale(double hatchScale)
        => double.IsNaN(hatchScale) || double.IsInfinity(hatchScale) || hatchScale <= 0.0 ? 1.0 : hatchScale;

    public static (int R, int G, int B) DefaultColorForLayerIndex(int index)
        => (((index % 6) + 6) % 6) switch
        {
            0 => (65, 174, 221),
            1 => (79, 203, 137),
            2 => (250, 197, 83),
            3 => (236, 132, 80),
            4 => (177, 138, 230),
            _ => (142, 164, 180),
        };
}
