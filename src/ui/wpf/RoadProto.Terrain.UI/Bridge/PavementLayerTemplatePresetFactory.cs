using System;
using System.Collections.Generic;
using System.Linq;

namespace RoadProto.Terrain.UI.Bridge;

public enum PavementLayerTemplateRoadSegmentType
{
    MainlineLane,
    MainlineShoulder,
    Ramp,
    BridgeTransition,
    BridgeDeck,
    InterchangeCrossroad,
    Tunnel,
    TollPlaza,
    PassageConnection,
}

public sealed class PavementLayerTemplatePresetOption<T>
{
    public PavementLayerTemplatePresetOption(string label, T value)
    {
        Label = label;
        Value = value;
    }

    public string Label { get; }
    public T Value { get; }
}

public static class PavementLayerTemplatePresetFactory
{
    private const double DefaultPreviewWidth = 3.0;

    private static readonly IReadOnlyList<PavementLayerTemplatePresetOption<PavementSurfaceType>> PavementTypes =
        new[]
        {
            new PavementLayerTemplatePresetOption<PavementSurfaceType>("沥青路面", PavementSurfaceType.Asphalt),
            new PavementLayerTemplatePresetOption<PavementSurfaceType>("混凝土路面", PavementSurfaceType.Concrete),
        };

    private static readonly IReadOnlyList<PavementLayerTemplatePresetOption<PavementLayerTemplateRoadSegmentType>> AsphaltSegments =
        new[]
        {
            Option("主线行车道", PavementLayerTemplateRoadSegmentType.MainlineLane),
            Option("主线硬路肩", PavementLayerTemplateRoadSegmentType.MainlineShoulder),
            Option("互通匝道", PavementLayerTemplateRoadSegmentType.Ramp),
            Option("桥头过渡段", PavementLayerTemplateRoadSegmentType.BridgeTransition),
            Option("桥面铺装", PavementLayerTemplateRoadSegmentType.BridgeDeck),
            Option("互通被交路", PavementLayerTemplateRoadSegmentType.InterchangeCrossroad),
            Option("隧道", PavementLayerTemplateRoadSegmentType.Tunnel),
        };

    private static readonly IReadOnlyList<PavementLayerTemplatePresetOption<PavementLayerTemplateRoadSegmentType>> ConcreteSegments =
        new[]
        {
            Option("收费站广场", PavementLayerTemplateRoadSegmentType.TollPlaza),
            Option("通道连接线", PavementLayerTemplateRoadSegmentType.PassageConnection),
        };

    public static IReadOnlyList<PavementLayerTemplatePresetOption<PavementSurfaceType>> PavementTypeOptions => PavementTypes;

    public static IReadOnlyList<PavementLayerTemplatePresetOption<PavementLayerTemplateRoadSegmentType>> RoadSegmentOptions(
        PavementSurfaceType pavementType)
        => pavementType == PavementSurfaceType.Concrete ? ConcreteSegments : AsphaltSegments;

    public static string RoadSegmentTypeLabel(PavementLayerTemplateRoadSegmentType type)
        => AsphaltSegments.Concat(ConcreteSegments).First(option => option.Value == type).Label;

    public static PavementLayerTemplateDto Create(
        PavementSurfaceType pavementType,
        PavementLayerTemplateRoadSegmentType roadSegmentType)
    {
        var preset = Presets.FirstOrDefault(item => item.PavementType == pavementType && item.RoadSegmentType == roadSegmentType)
            ?? Presets.First(item => item.PavementType == pavementType);

        return new PavementLayerTemplateDto
        {
            TemplateName = $"{PavementLayerTemplateLabels.PavementTypeLabel(preset.PavementType)}-{RoadSegmentTypeLabel(preset.RoadSegmentType)}",
            DisplayScale = 10.0,
            PreviewWidth = DefaultPreviewWidth,
            DisplayMode = PavementLayerTemplateDisplayMode.HatchAndColor,
            ShowAllGeneralParameters = true,
            StructureCode = preset.StructureCode,
            PavementType = preset.PavementType,
            Layers = preset.Layers.Select(layer => layer.Clone()).ToList(),
        };
    }

    private static PavementLayerTemplatePresetOption<PavementLayerTemplateRoadSegmentType> Option(
        string label,
        PavementLayerTemplateRoadSegmentType value)
        => new(label, value);

    private static readonly IReadOnlyList<Preset> Presets = new[]
    {
        new Preset(
            PavementSurfaceType.Asphalt,
            PavementLayerTemplateRoadSegmentType.MainlineLane,
            DefaultPreviewWidth,
            "Ⅰ-1",
            MainlineLaneLayers()),
        new Preset(
            PavementSurfaceType.Asphalt,
            PavementLayerTemplateRoadSegmentType.MainlineShoulder,
            DefaultPreviewWidth,
            "Ⅰ-2",
            MainlineShoulderLayers()),
        new Preset(
            PavementSurfaceType.Asphalt,
            PavementLayerTemplateRoadSegmentType.Ramp,
            DefaultPreviewWidth,
            "Ⅰ-4",
            new[]
            {
                AsphaltUpper(),
                AsphaltMiddle(),
                AsphaltSeal(),
                Layer(PavementLayerType.Base, "32cm水泥稳定碎石", 0.32, 0.15, 1.0, 55, 108, 189, 0.04, "GRAVEL"),
                Layer(PavementLayerType.Subbase, "20cm低剂量水泥稳定碎石", 0.20, 0.15, 1.0, 61, 120, 210, 0.20, "SACNCR"),
            }),
        new Preset(
            PavementSurfaceType.Asphalt,
            PavementLayerTemplateRoadSegmentType.BridgeTransition,
            DefaultPreviewWidth,
            "Ⅱ-1",
            new[]
            {
                AsphaltUpper(),
                AsphaltMiddle(),
                AsphaltSeal(),
                Layer(PavementLayerType.ApproachSlab, "水泥混凝土", 0.35, 0.0, 0.0, 102, 102, 102, 0.04, "TRIANG"),
                Layer(PavementLayerType.Cushion, "水泥砂浆", 0.03, 0.0, 0.0, 181, 53, 53, 0.07, "AR-SAND"),
                Layer(PavementLayerType.Subbase, "级配碎石", 0.25, 0.0, 0.0, 61, 120, 210, 0.50, "HEX"),
            }),
        new Preset(
            PavementSurfaceType.Asphalt,
            PavementLayerTemplateRoadSegmentType.BridgeDeck,
            DefaultPreviewWidth,
            "Ⅱ-2",
            new[]
            {
                AsphaltUpper(),
                AsphaltMiddle(),
                AsphaltSeal(),
            }),
        new Preset(
            PavementSurfaceType.Asphalt,
            PavementLayerTemplateRoadSegmentType.InterchangeCrossroad,
            DefaultPreviewWidth,
            "Ⅱ-3",
            new[]
            {
                AsphaltUpper(),
                AsphaltMiddle(),
                Layer(PavementLayerType.Base, "32cm水泥稳定碎石", 0.32, 0.15, 1.0, 55, 108, 189, 0.04, "GRAVEL"),
                Layer(PavementLayerType.Subbase, "20cm低剂量水泥稳定碎石", 0.20, 0.15, 1.0, 61, 120, 210, 0.20, "SACNCR"),
            }),
        new Preset(
            PavementSurfaceType.Asphalt,
            PavementLayerTemplateRoadSegmentType.Tunnel,
            DefaultPreviewWidth,
            "Ⅱ-4",
            new[]
            {
                AsphaltUpper(),
                AsphaltMiddle(),
                AsphaltSeal(),
                Layer(PavementLayerType.Base, "水泥混凝土", 0.24, 0.0, 0.0, 102, 102, 102, 0.04, "TRIANG"),
                Layer(PavementLayerType.Subbase, "20cm低剂量水泥稳定碎石", 0.20, 0.0, 0.0, 61, 120, 210, 0.20, "SACNCR"),
            }),
        new Preset(
            PavementSurfaceType.Concrete,
            PavementLayerTemplateRoadSegmentType.TollPlaza,
            DefaultPreviewWidth,
            "Ⅲ-1",
            new[]
            {
                Layer(PavementLayerType.UpperSurface, "水泥混凝土", 0.26, 0.0, 0.0, 102, 102, 102, 0.04, "TRIANG"),
                Layer(PavementLayerType.Base, "水泥稳定碎石", 0.20, 0.0, 0.0, 55, 108, 189, 0.04, "GRAVEL"),
                Layer(PavementLayerType.Subbase, "级配碎石", 0.20, 0.0, 0.0, 61, 120, 210, 0.50, "HEX"),
            }),
        new Preset(
            PavementSurfaceType.Concrete,
            PavementLayerTemplateRoadSegmentType.PassageConnection,
            DefaultPreviewWidth,
            "Ⅲ-2",
            new[]
            {
                Layer(PavementLayerType.UpperSurface, "水泥混凝土", 0.22, 0.0, 0.0, 102, 102, 102, 0.04, "TRIANG"),
                Layer(PavementLayerType.Base, "级配碎石", 0.18, 0.0, 0.0, 61, 120, 210, 0.50, "HEX"),
                Layer(PavementLayerType.Subbase, "石灰土", 0.18, 0.0, 0.0, 61, 120, 210, 0.20, "SACNCR"),
            }),
    };

    private static IReadOnlyList<PavementLayerTemplateLayerDto> MainlineLaneLayers()
        => new[]
        {
            AsphaltUpper(),
            AsphaltMiddle(),
            Layer(PavementLayerType.LowerSurface, "8cm沥青马蹄脂碎石混合料（SUP-25）", 0.08, 0.0, 0.0, 80, 80, 80, 0.005, "AR-HBONE"),
            AsphaltSeal(),
            LayerWithSides(PavementLayerType.Base, "36cm水泥稳定碎石", 0.36, 0.36, 0.10, 0.0, 1.0, 0.0, 55, 108, 189, 0.04, "GRAVEL"),
            LayerWithSides(PavementLayerType.Subbase, "20cm低剂量水泥稳定碎石", 0.20, 0.20, 0.10, 0.0, 1.0, 0.0, 61, 120, 210, 0.20, "SACNCR"),
        };

    private static IReadOnlyList<PavementLayerTemplateLayerDto> MainlineShoulderLayers()
        => new[]
        {
            AsphaltUpper(),
            AsphaltMiddle(),
            Layer(PavementLayerType.LowerSurface, "8cm沥青马蹄脂碎石混合料（SUP-25）", 0.08, 0.0, 0.0, 80, 80, 80, 0.005, "AR-HBONE"),
            AsphaltSeal(),
            LayerWithSides(PavementLayerType.Base, "36cm水泥稳定碎石", 0.36, 0.36, 0.0, 0.10, 0.0, 1.0, 55, 108, 189, 0.04, "GRAVEL"),
            LayerWithSides(PavementLayerType.Subbase, "20cm低剂量水泥稳定碎石", 0.20, 0.20, 0.0, 0.10, 0.0, 1.0, 61, 120, 210, 0.20, "SACNCR"),
        };

    private static PavementLayerTemplateLayerDto AsphaltUpper()
        => Layer(PavementLayerType.UpperSurface, "4cm沥青马蹄脂碎石混合料（SMA-13s）", 0.04, 0.0, 0.0, 102, 102, 102, 0.10, "NET");

    private static PavementLayerTemplateLayerDto AsphaltMiddle()
        => Layer(PavementLayerType.MiddleSurface, "6cm沥青马蹄脂碎石混合料（SUP-20）", 0.06, 0.0, 0.0, 102, 102, 102, 0.003, "AR-HBONE");

    private static PavementLayerTemplateLayerDto AsphaltSeal()
        => Layer(PavementLayerType.AsphaltSeal, "沥青封层", 0.01, 0.0, 0.0, 80, 80, 80, 1.0);

    private static PavementLayerTemplateLayerDto Layer(
        PavementLayerType type,
        string name,
        double thickness,
        double widening,
        double slope,
        int colorR,
        int colorG,
        int colorB,
        double hatchScale,
        string hatchPattern = "SOLID")
        => LayerWithSides(
            type,
            name,
            thickness,
            thickness,
            widening,
            widening,
            slope,
            slope,
            colorR,
            colorG,
            colorB,
            hatchScale,
            hatchPattern);

    private static PavementLayerTemplateLayerDto LayerWithSides(
        PavementLayerType type,
        string name,
        double innerThickness,
        double outerThickness,
        double innerWidening,
        double outerWidening,
        double innerSlope,
        double outerSlope,
        int colorR,
        int colorG,
        int colorB,
        double hatchScale,
        string hatchPattern = "SOLID")
        => new()
        {
            Type = type,
            Name = name,
            UniformThickness = Math.Abs(innerThickness - outerThickness) < 1.0e-9,
            Thickness = Math.Max(innerThickness, outerThickness),
            InnerThickness = innerThickness,
            OuterThickness = outerThickness,
            InnerWidening = innerWidening,
            OuterWidening = outerWidening,
            InnerSlope = innerSlope,
            OuterSlope = outerSlope,
            ColorR = colorR,
            ColorG = colorG,
            ColorB = colorB,
            HatchPattern = hatchPattern,
            HatchAngle = 0.0,
            HatchScale = hatchScale,
        };

    private sealed class Preset
    {
        public Preset(
            PavementSurfaceType pavementType,
            PavementLayerTemplateRoadSegmentType roadSegmentType,
            double previewWidth,
            string structureCode,
            IReadOnlyList<PavementLayerTemplateLayerDto> layers)
        {
            PavementType = pavementType;
            RoadSegmentType = roadSegmentType;
            PreviewWidth = previewWidth;
            StructureCode = structureCode;
            Layers = layers;
        }

        public PavementSurfaceType PavementType { get; }
        public PavementLayerTemplateRoadSegmentType RoadSegmentType { get; }
        public double PreviewWidth { get; }
        public string StructureCode { get; }
        public IReadOnlyList<PavementLayerTemplateLayerDto> Layers { get; }
    }
}
