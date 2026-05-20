using System.Collections.Generic;

namespace RoadProto.Terrain.UI.Bridge;

public enum SubgradeRoadGrade
{
    Expressway,
    FirstClass,
    SecondClass,
    ThirdClass,
    FourthClass,
    UrbanExpressway,
    UrbanArterial,
    UrbanSubArterial,
    UrbanBranch,
}

public enum SubgradeSide
{
    Left,
    Right,
}

public enum SubgradeComponentType
{
    Median,
    TravelLane,
    HardShoulder,
    EarthShoulder,
    SideMedian,
    Sidewalk,
    BikeLane,
    CurbStrip,
}

public enum SubgradeSlopeMode
{
    Fixed,
    VariableByStation,
}

public enum SubgradeTemplateDialogAction
{
    None,
    PickPavementLayerTemplate,
}

public sealed class SubgradeStationValueDto
{
    public double Station { get; set; }
    public double Value { get; set; }

    public SubgradeStationValueDto Clone()
        => new() { Station = Station, Value = Value };
}

public sealed class SubgradeComponentDto
{
    public SubgradeSide Side { get; set; }
    public SubgradeComponentType Type { get; set; } = SubgradeComponentType.TravelLane;
    public double Width { get; set; }
    public double Height { get; set; }
    public double FixedSlope { get; set; }
    public SubgradeSlopeMode SlopeMode { get; set; } = SubgradeSlopeMode.Fixed;
    public int ColorR { get; set; } = 120;
    public int ColorG { get; set; } = 120;
    public int ColorB { get; set; } = 120;
    public List<SubgradeStationValueDto> WideningTable { get; set; } = new();
    public List<SubgradeStationValueDto> VariableSlopeTable { get; set; } = new();
    public bool PavementLayerLinked { get; set; }
    public string PavementLayerHandle { get; set; } = string.Empty;
    public string PavementLayerName { get; set; } = string.Empty;
    public double PavementLayerThickness { get; set; }

    public string DisplayName => $"{SideLabel} {TypeLabel} {Width:0.##}";
    public string SideLabel => Side == SubgradeSide.Left ? "左侧" : "右侧";
    public string TypeLabel => SubgradeTemplateLabels.ComponentTypeLabel(Type);

    public SubgradeComponentDto Clone()
        => new()
        {
            Side = Side,
            Type = Type,
            Width = Width,
            Height = Height,
            FixedSlope = FixedSlope,
            SlopeMode = SlopeMode,
            ColorR = ColorR,
            ColorG = ColorG,
            ColorB = ColorB,
            WideningTable = WideningTable.ConvertAll(row => row.Clone()),
            VariableSlopeTable = VariableSlopeTable.ConvertAll(row => row.Clone()),
            PavementLayerLinked = PavementLayerLinked,
            PavementLayerHandle = PavementLayerHandle,
            PavementLayerName = PavementLayerName,
            PavementLayerThickness = PavementLayerThickness,
        };
}

public sealed class SubgradeTemplateDialogRequest
{
    public SubgradeTemplateDialogAction Action { get; set; } = SubgradeTemplateDialogAction.None;
    public int PickComponentIndex { get; set; } = -1;
    public string Handle { get; set; } = string.Empty;
    public string ResponsePath { get; set; } = string.Empty;
    public double InsertionX { get; set; }
    public double InsertionY { get; set; }
    public double InsertionZ { get; set; }
    public string TemplateName { get; set; } = "默认路基模板";
    public double DisplayScale { get; set; } = 10.0;
    public SubgradeRoadGrade RoadGrade { get; set; } = SubgradeRoadGrade.Expressway;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public List<SubgradeComponentDto> Components { get; set; } = new();
}

public sealed class SubgradeTemplateDialogResponse
{
    public SubgradeTemplateDialogAction Action { get; set; } = SubgradeTemplateDialogAction.None;
    public int PickComponentIndex { get; set; } = -1;
    public bool Accepted { get; set; }
    public string Handle { get; set; } = string.Empty;
    public double InsertionX { get; set; }
    public double InsertionY { get; set; }
    public double InsertionZ { get; set; }
    public string TemplateName { get; set; } = "默认路基模板";
    public double DisplayScale { get; set; } = 10.0;
    public SubgradeRoadGrade RoadGrade { get; set; } = SubgradeRoadGrade.Expressway;
    public string RoadCenterlineHandle { get; set; } = string.Empty;
    public List<SubgradeComponentDto> Components { get; set; } = new();
}

public static class SubgradeTemplateLabels
{
    public static string RoadGradeLabel(SubgradeRoadGrade grade)
        => grade switch
        {
            SubgradeRoadGrade.Expressway => "高速公路",
            SubgradeRoadGrade.FirstClass => "一级道路",
            SubgradeRoadGrade.SecondClass => "二级道路",
            SubgradeRoadGrade.ThirdClass => "三级道路",
            SubgradeRoadGrade.FourthClass => "四级道路",
            SubgradeRoadGrade.UrbanExpressway => "城市快速路",
            SubgradeRoadGrade.UrbanArterial => "城市主干道",
            SubgradeRoadGrade.UrbanSubArterial => "城市次干道",
            SubgradeRoadGrade.UrbanBranch => "城市支路",
            _ => grade.ToString(),
        };

    public static string ComponentTypeLabel(SubgradeComponentType type)
        => type switch
        {
            SubgradeComponentType.Median => "中分带",
            SubgradeComponentType.TravelLane => "行车道",
            SubgradeComponentType.HardShoulder => "硬路肩",
            SubgradeComponentType.EarthShoulder => "土路肩",
            SubgradeComponentType.SideMedian => "侧分带",
            SubgradeComponentType.Sidewalk => "人行道",
            SubgradeComponentType.BikeLane => "慢车道",
            SubgradeComponentType.CurbStrip => "路缘带",
            _ => type.ToString(),
        };

    public static string SlopeModeLabel(SubgradeSlopeMode mode)
        => mode == SubgradeSlopeMode.VariableByStation ? "变化值" : "固定值";
}
