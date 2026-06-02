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
    public double PreviewWidth { get; set; } = 3.0;
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
            PavementLayerType.ApproachSlab => "搭板",
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

    public static readonly Dictionary<string, List<string>> PavementLayerMaterialOptions = new()
    {
        ["上面层"] = new()
        {
            "细粒式沥青混凝土 AC-13C",
            "细粒式沥青混凝土 AC-10C",
            "SBS改性沥青混凝土 AC-13C",
            "SBS改性沥青混凝土 AC-10C",
            "沥青玛蹄脂碎石混合料 SMA-13",
            "沥青玛蹄脂碎石混合料 SMA-10",
            "开级配排水式沥青磨耗层 OGFC-13",
            "开级配排水式沥青磨耗层 OGFC-10",
            "排水沥青混合料 PAC-13",
            "橡胶沥青混凝土 ARAC-13",
            "高黏改性沥青混合料",
            "彩色沥青混凝土",
            "薄层罩面沥青混合料",
            "超薄磨耗层",
            "沥青表面处治",
        },
        ["中面层"] = new()
        {
            "中粒式沥青混凝土 AC-20C",
            "中粒式沥青混凝土 AC-16C",
            "SBS改性沥青混凝土 AC-20C",
            "SBS改性沥青混凝土 AC-16C",
            "高模量沥青混凝土 AC-20C",
            "高模量沥青混凝土 AC-16C",
            "抗车辙沥青混凝土 AC-20C",
            "抗车辙沥青混凝土 AC-16C",
            "沥青玛蹄脂碎石混合料 SMA-16",
            "厂拌热再生沥青混合料 AC-20C",
            "厂拌热再生沥青混合料 AC-16C",
            "橡胶沥青混凝土 ARAC-20",
            "橡胶沥青混凝土 ARAC-16",
        },
        ["下面层"] = new()
        {
            "粗粒式沥青混凝土 AC-25C",
            "中粒式沥青混凝土 AC-20C",
            "SBS改性沥青混凝土 AC-25C",
            "SBS改性沥青混凝土 AC-20C",
            "高模量沥青混凝土 AC-25C",
            "高模量沥青混凝土 AC-20C",
            "沥青稳定碎石 ATB-25",
            "沥青稳定碎石 ATB-30",
            "密级配沥青稳定碎石 ATB-25",
            "密级配沥青稳定碎石 ATB-30",
            "厂拌热再生沥青混合料 AC-25C",
            "厂拌热再生沥青混合料 AC-20C",
            "抗疲劳沥青混合料",
            "沥青贯入碎石",
            "上拌下贯沥青碎石",
        },
        ["基层"] = new()
        {
            "水泥稳定碎石",
            "水泥稳定级配碎石",
            "水泥稳定砂砾",
            "水泥稳定级配砂砾",
            "水泥粉煤灰稳定碎石",
            "水泥粉煤灰稳定砂砾",
            "石灰粉煤灰稳定碎石",
            "石灰粉煤灰稳定砂砾",
            "二灰碎石",
            "二灰砂砾",
            "石灰稳定土",
            "水泥稳定土",
            "级配碎石",
            "级配砾石",
            "天然砂砾",
            "填隙碎石",
            "沥青稳定碎石 ATB-25",
            "沥青稳定碎石 ATB-30",
            "沥青贯入碎石",
            "贫混凝土基层",
            "碾压混凝土基层",
            "水泥混凝土基层",
        },
        ["底基层"] = new()
        {
            "水泥稳定碎石",
            "低剂量水泥稳定碎石",
            "水泥稳定级配碎石",
            "水泥稳定砂砾",
            "低剂量水泥稳定砂砾",
            "水泥稳定级配砂砾",
            "石灰粉煤灰稳定碎石",
            "石灰粉煤灰稳定砂砾",
            "二灰碎石",
            "二灰砂砾",
            "石灰稳定土",
            "水泥稳定土",
            "石灰土",
            "级配碎石",
            "级配砾石",
            "天然砂砾",
            "填隙碎石",
            "未筛分碎石",
            "砂砾土",
            "改良土",
        },
        ["垫层"] = new()
        {
            "碎石垫层",
            "级配碎石垫层",
            "砂砾垫层",
            "级配砂砾垫层",
            "天然砂砾垫层",
            "中粗砂垫层",
            "砂垫层",
            "未筛分碎石垫层",
            "片石垫层",
            "块石垫层",
            "透水性材料垫层",
            "排水垫层",
            "防冻垫层",
            "隔水垫层",
            "低剂量水泥稳定碎石垫层",
            "低剂量水泥稳定砂砾垫层",
        },
        ["沥青封层"] = new()
        {
            "乳化沥青稀浆封层",
            "改性乳化沥青稀浆封层",
            "微表处",
            "同步碎石封层",
            "橡胶沥青同步碎石封层",
            "单层沥青表面处治",
            "双层沥青表面处治",
            "下封层",
            "上封层",
            "透层油",
            "黏层油",
            "改性乳化沥青黏层",
            "乳化沥青透层",
            "热沥青封层",
            "SAMI应力吸收层",
            "橡胶沥青应力吸收层",
            "防水黏结层",
            "桥面防水黏结层",
            "隧道路面防水黏结层",
        },
        ["搭板"] = new()
        {
            "钢筋混凝土搭板",
            "C30钢筋混凝土搭板",
            "C35钢筋混凝土搭板",
            "C40钢筋混凝土搭板",
            "普通水泥混凝土搭板",
            "桥头搭板",
            "涵洞搭板",
            "通道搭板",
            "桥台背搭板",
            "水泥混凝土过渡板",
            "钢筋混凝土过渡板",
            "搭板下水泥稳定碎石基层",
            "搭板下级配碎石垫层",
            "搭板下贫混凝土垫层",
        },
    };

    public static List<string> MaterialOptionsForLayerType(PavementLayerType type)
    {
        var key = type == PavementLayerType.ApproachSlab ? "搭板" : LayerTypeLabel(type);
        return PavementLayerMaterialOptions.TryGetValue(key, out var options)
            ? options.ToList()
            : new List<string>();
    }

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
