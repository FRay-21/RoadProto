using RoadProto.Terrain.UI.Bridge;

static void Check(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}

static string NewTempFile()
{
    return Path.Combine(Path.GetTempPath(), $"RoadProtoManagedBridgeTests_{Guid.NewGuid():N}.response");
}

static void ResponseWritesPickTerrainAction()
{
    var path = NewTempFile();
    try
    {
        AlignmentDialogFile.WriteResponse(path, new AlignmentDialogResponse
        {
            Accepted = true,
            Action = AlignmentDialogAction.PickTerrain,
            Mode = AlignmentDialogMode.Full,
            Handle = "1A2",
            DeleteOnCancel = true,
            RoadName = "K1",
            RoadGradeIndex = 9,
            StationLabelInterval = 100,
            Radius = 80,
            Ls1 = 20,
            Ls2 = 20,
        });

        var content = File.ReadAllText(path);
        Check(content.Contains("action=pickTerrain"), "response file should request terrain picking");
        Check(content.Contains("handle=1A2"), "response should keep target centerline handle");
        Check(content.Contains("deleteOnCancel=1"), "response should keep create-cancel cleanup flag");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

static void SubgradeRequestReadsPersistedEntityComponents()
{
    var path = NewTempFile();
    try
    {
        File.WriteAllLines(path, new[]
        {
            "handle=2B",
            "responsePath=C:/temp/subgrade.response",
            "insertionX=10.5",
            "insertionY=20.25",
            "insertionZ=0",
            "templateName=edited-template",
            "displayScale=20",
            "roadGrade=FirstClass",
            "roadCenterlineHandle=1A",
            "componentCount=2",
            "component.0.side=Left",
            "component.0.type=TravelLane",
            "component.0.width=3.75",
            "component.0.height=0.1",
            "component.0.fixedSlope=0.02",
            "component.0.slopeMode=VariableByStation",
            "component.0.colorR=10",
            "component.0.colorG=20",
            "component.0.colorB=30",
            "component.0.wideningCount=1",
            "component.0.widening.0.station=100",
            "component.0.widening.0.value=0.5",
            "component.0.slopeTableCount=1",
            "component.0.slopeTable.0.station=100",
            "component.0.slopeTable.0.value=-0.025",
            "component.0.pavementLayerLinked=1",
            "component.0.pavementLayerHandle=44",
            "component.0.pavementLayerThickness=0.28",
            "component.1.side=Right",
            "component.1.type=HardShoulder",
            "component.1.width=2.5",
            "component.1.height=0",
            "component.1.fixedSlope=0.015",
            "component.1.slopeMode=Fixed",
            "component.1.colorR=0",
            "component.1.colorG=90",
            "component.1.colorB=180",
            "component.1.wideningCount=0",
            "component.1.slopeTableCount=0",
            "component.1.pavementLayerLinked=0",
            "component.1.pavementLayerHandle=",
            "component.1.pavementLayerThickness=0",
        });

        var request = SubgradeTemplateDialogFile.ReadRequest(path);
        Check(request.Handle == "2B", "edit request should keep entity handle");
        Check(request.RoadGrade == SubgradeRoadGrade.FirstClass, "edit request should keep road grade");
        Check(request.Components.Count == 2, "edit request should keep persisted components");
        Check(request.Components[0].Side == SubgradeSide.Left, "first component side should round-trip");
        Check(request.Components[0].Type == SubgradeComponentType.TravelLane, "first component type should round-trip");
        Check(Math.Abs(request.Components[0].Width - 3.75) < 1.0e-9, "first component width should round-trip");
        Check(request.Components[0].SlopeMode == SubgradeSlopeMode.VariableByStation, "slope mode should round-trip");
        Check(request.Components[0].VariableSlopeTable.Count == 1, "variable slope table should round-trip");
        Check(Math.Abs(request.Components[0].VariableSlopeTable[0].Value + 0.025) < 1.0e-9, "slope value should round-trip");
        Check(request.Components[0].PavementLayerLinked, "pavement link should round-trip");
        Check(request.Components[1].Side == SubgradeSide.Right, "second component side should round-trip");
        Check(request.Components[1].Type == SubgradeComponentType.HardShoulder, "second component type should round-trip");
        Check(Math.Abs(request.Components[1].Width - 2.5) < 1.0e-9, "second component width should round-trip");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

ResponseWritesPickTerrainAction();
SubgradeRequestReadsPersistedEntityComponents();
Console.WriteLine("All RoadProto managed bridge tests passed.");
