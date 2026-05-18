using RoadProto.Terrain.UI.Bridge;
using System.Globalization;
using System.Threading;

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

static string FindRepoRoot()
{
    foreach (var start in new[] { Directory.GetCurrentDirectory(), AppContext.BaseDirectory })
    {
        var directory = new DirectoryInfo(start);
        while (directory != null)
        {
            if (File.Exists(Path.Combine(directory.FullName, "RoadProto.sln")))
            {
                return directory.FullName;
            }

            directory = directory.Parent;
        }
    }

    throw new InvalidOperationException("Cannot find RoadProto repository root.");
}

static void ExpectThrows<TException>(Action action, string message)
    where TException : Exception
{
    try
    {
        action();
    }
    catch (TException)
    {
        return;
    }

    throw new InvalidOperationException(message);
}

static void WithCulture(string cultureName, Action action)
{
    var originalCulture = Thread.CurrentThread.CurrentCulture;
    var originalUICulture = Thread.CurrentThread.CurrentUICulture;
    try
    {
        var culture = CultureInfo.GetCultureInfo(cultureName);
        Thread.CurrentThread.CurrentCulture = culture;
        Thread.CurrentThread.CurrentUICulture = culture;
        action();
    }
    finally
    {
        Thread.CurrentThread.CurrentCulture = originalCulture;
        Thread.CurrentThread.CurrentUICulture = originalUICulture;
    }
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

static void RoadModelRequestReadsAssignmentsUsingInvariantCultureAndEscaping()
{
    var path = NewTempFile();
    try
    {
        File.WriteAllLines(path, new[]
        {
            "handle=AA11",
            "responsePath=C:/temp/road-model.response",
            "roadCenterlineHandle=CL%251",
            "profileVerticalCurveHandle=VC%0A2",
            "sampleInterval=12.5",
            "assignmentCount=2",
            "assignment.0.startStation=0.5",
            "assignment.0.endStation=100.25",
            "assignment.0.templateHandle=TPL%250",
            "assignment.0.templateName=主线%25%0A模板",
            "assignment.1.startStation=100.25",
            "assignment.1.endStation=180.75",
            "assignment.1.templateHandle=TPL-2",
            "assignment.1.templateName=secondary",
        });

        WithCulture("fr-FR", () =>
        {
            var request = RoadModelDialogFile.ReadRequest(path);
            Check(request.Handle == "AA11", "road model request should keep handle");
            Check(request.ResponsePath == "C:/temp/road-model.response", "road model request should require response path");
            Check(request.RoadCenterlineHandle == "CL%1", "road centerline handle should unescape percent");
            Check(request.ProfileVerticalCurveHandle == "VC\n2", "vertical curve handle should unescape newline");
            Check(Math.Abs(request.SampleInterval - 12.5) < 1.0e-9, "sample interval should parse with invariant culture");
            Check(request.Assignments.Count == 2, "assignmentCount should control assignment rows");
            Check(Math.Abs(request.Assignments[0].StartStation - 0.5) < 1.0e-9, "assignment start station should parse with invariant culture");
            Check(Math.Abs(request.Assignments[0].EndStation - 100.25) < 1.0e-9, "assignment end station should parse with invariant culture");
            Check(request.Assignments[0].TemplateHandle == "TPL%0", "template handle should unescape percent");
            Check(request.Assignments[0].TemplateName == "主线%\n模板", "template name should unescape UTF-8, percent, and newline");
        });
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

static void RoadModelResponseWritesAssignmentsUsingInvariantCultureAndEscaping()
{
    var path = NewTempFile();
    try
    {
        WithCulture("de-DE", () =>
        {
            var response = new RoadModelDialogResponse
            {
                Accepted = true,
                Handle = "RM-1",
                RoadCenterlineHandle = "CL\n1",
                ProfileVerticalCurveHandle = "VC%2",
                SampleInterval = 7.5,
            };
            response.Assignments.Add(new RoadModelTemplateAssignmentDto
            {
                StartStation = 0.25,
                EndStation = 50.75,
                TemplateHandle = "TPL%1",
                TemplateName = "左幅\n模板",
            });
            response.Assignments.Add(new RoadModelTemplateAssignmentDto
            {
                StartStation = 50.75,
                EndStation = 120.5,
                TemplateHandle = "TPL-2",
                TemplateName = "right",
            });

            RoadModelDialogFile.WriteResponse(path, response);
        });

        var content = File.ReadAllText(path);
        Check(content.Contains("accepted=1"), "road model response should record accepted state");
        Check(content.Contains("sampleInterval=7.5"), "road model response should write invariant decimal separator");
        Check(!content.Contains("sampleInterval=7,5"), "road model response should not use current culture decimal separator");
        Check(content.Contains("assignmentCount=2"), "road model response should write assignmentCount");
        Check(content.Contains("assignment.0.startStation=0.25"), "road model response should write assignment start station");
        Check(content.Contains("assignment.1.endStation=120.5"), "road model response should write assignment end station");
        Check(content.Contains("roadCenterlineHandle=CL%0A1"), "road centerline handle should escape newline");
        Check(content.Contains("profileVerticalCurveHandle=VC%252"), "vertical curve handle should escape percent");
        Check(content.Contains("assignment.0.templateName=左幅%0A模板"), "template name should escape newline");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

static void RoadModelRequestRejectsMissingOrEmptyResponsePath()
{
    var missingPath = NewTempFile();
    var emptyPath = NewTempFile();
    try
    {
        File.WriteAllLines(missingPath, new[]
        {
            "handle=RM-1",
            "sampleInterval=10",
            "assignmentCount=0",
        });
        File.WriteAllLines(emptyPath, new[]
        {
            "handle=RM-1",
            "responsePath=   ",
            "sampleInterval=10",
            "assignmentCount=0",
        });

        ExpectThrows<InvalidDataException>(
            () => RoadModelDialogFile.ReadRequest(missingPath),
            "road model request should reject missing responsePath");
        ExpectThrows<InvalidDataException>(
            () => RoadModelDialogFile.ReadRequest(emptyPath),
            "road model request should reject empty responsePath");
    }
    finally
    {
        if (File.Exists(missingPath))
        {
            File.Delete(missingPath);
        }
        if (File.Exists(emptyPath))
        {
            File.Delete(emptyPath);
        }
    }
}

static void RoadModelWindowReadOnlyHandleBindingIsOneWay()
{
    var xamlPath = Path.Combine(
        FindRepoRoot(),
        "src",
        "ui",
        "wpf",
        "RoadProto.Terrain.UI",
        "RoadModelWindow.xaml");
    var xaml = File.ReadAllText(xamlPath);
    Check(
        xaml.Contains("Text=\"{Binding RoadCenterlineHandle, Mode=OneWay}\""),
        "road model window should bind read-only road centerline handle with Mode=OneWay");
}

ResponseWritesPickTerrainAction();
SubgradeRequestReadsPersistedEntityComponents();
RoadModelRequestReadsAssignmentsUsingInvariantCultureAndEscaping();
RoadModelResponseWritesAssignmentsUsingInvariantCultureAndEscaping();
RoadModelRequestRejectsMissingOrEmptyResponsePath();
RoadModelWindowReadOnlyHandleBindingIsOneWay();
Console.WriteLine("All RoadProto managed bridge tests passed.");
