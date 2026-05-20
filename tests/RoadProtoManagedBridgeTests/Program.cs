using RoadProto.Terrain.UI.Bridge;
using System.Globalization;
using System.Text;
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

        var content = File.ReadAllText(path, Encoding.UTF8);
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
        }, Encoding.UTF8);

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
        }, Encoding.UTF8);

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

static void SlopeTemplateDialogFileRoundTripsComponents()
{
    var path = NewTempFile();
    try
    {
        File.WriteAllLines(path, new[]
        {
            "handle=SL-1",
            "responsePath=C:/temp/slope-template.response",
            "insertionX=1.5",
            "insertionY=2.25",
            "insertionZ=3.75",
            "templateName=边坡%0A模板",
            "displayScale=10",
            "kind=Cut",
            "stopAtGround=1",
            "repeatLastGroupUntilGround=true",
            "componentCount=1",
            "component.0.type=Berm",
            "component.0.constraintMode=SlopeAndWidth",
            "component.0.slope=0.03",
            "component.0.height=0.5",
            "component.0.width=1.25",
            "component.0.groundSearchHeightIncrement=2.5",
            "component.0.colorR=10",
            "component.0.colorG=20",
            "component.0.colorB=30",
        }, Encoding.UTF8);

        WithCulture("fr-FR", () =>
        {
            var request = SlopeTemplateDialogFile.ReadRequest(path);
            Check(request.Handle == "SL-1", "slope template request should keep handle");
            Check(request.TemplateName == "边坡\n模板", "slope template name should unescape newline");
            Check(request.Kind == SlopeTemplateKind.Cut, "slope template kind should parse");
            Check(request.StopAtGround, "slope template stop-at-ground should parse");
            Check(request.RepeatLastGroupUntilGround, "slope template repeat-last-group should parse");
            Check(request.Components.Count == 1, "slope template component count should parse");
            Check(request.Components[0].Type == SlopeComponentType.Berm, "slope component type should parse");
            Check(request.Components[0].ConstraintMode == SlopeGeometryConstraintMode.SlopeAndWidth, "slope component mode should parse");
            Check(Math.Abs(request.Components[0].GroundSearchHeightIncrement - 2.5) < 1.0e-9, "slope component search increment should parse invariant decimal");
        });

        var response = new SlopeTemplateDialogResponse
        {
            Accepted = true,
            Handle = "SL%1",
            InsertionX = 1.5,
            InsertionY = 2.25,
            InsertionZ = 3.75,
            TemplateName = "边坡\n模板",
            DisplayScale = 10,
            Kind = SlopeTemplateKind.Fill,
            StopAtGround = true,
            RepeatLastGroupUntilGround = false,
        };
        response.Components.Add(new SlopeComponentDto
        {
            Type = SlopeComponentType.FillSlope,
            ConstraintMode = SlopeGeometryConstraintMode.SlopeAndHeight,
            Slope = -0.6666666666666666,
            Height = 4,
            Width = 6,
            GroundSearchHeightIncrement = 2,
            ColorR = 30,
            ColorG = 132,
            ColorB = 88,
        });

        SlopeTemplateDialogFile.WriteResponse(path, response);
        var content = File.ReadAllText(path, Encoding.UTF8);
        Check(content.Contains("accepted=1"), "slope template response should record accepted state");
        Check(content.Contains("handle=SL%251"), "slope template response should escape percent in handle");
        Check(content.Contains("templateName=边坡%0A模板"), "slope template response should escape newline in name");
        Check(content.Contains("componentCount=1"), "slope template response should write component count");
        Check(content.Contains("component.0.groundSearchHeightIncrement=2"), "slope template response should write search increment");
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

        var content = File.ReadAllText(path, Encoding.UTF8);
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

static void RoadModelResponseWritesPickTemplateActionAndRowIndex()
{
    var path = NewTempFile();
    try
    {
        var response = new RoadModelDialogResponse
        {
            Action = RoadModelDialogAction.PickTemplate,
            Accepted = false,
            PickAssignmentIndex = 1,
            Handle = "RM-1",
            RoadCenterlineHandle = "CL-1",
            ProfileVerticalCurveHandle = "VC-1",
            SampleInterval = 10.0,
        };
        response.Assignments.Add(new RoadModelTemplateAssignmentDto
        {
            StartStation = 0,
            EndStation = 100,
            TemplateHandle = "TPL-A",
            TemplateName = "模板A",
        });
        response.Assignments.Add(new RoadModelTemplateAssignmentDto
        {
            StartStation = 100,
            EndStation = 200,
            TemplateHandle = string.Empty,
            TemplateName = string.Empty,
        });

        RoadModelDialogFile.WriteResponse(path, response);
        var content = File.ReadAllText(path, Encoding.UTF8);
        Check(content.Contains("action=pickTemplate"), "road model response should request template picking");
        Check(content.Contains("pickAssignmentIndex=1"), "road model response should keep selected assignment index");
        Check(content.Contains("assignmentCount=2"), "road model response should keep current rows when picking a template");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

static void RoadModelSlopeGroupsRoundTripUsingInvariantCultureAndEscaping()
{
    var path = NewTempFile();
    try
    {
        File.WriteAllLines(path, new[]
        {
            "handle=RM-1",
            "responsePath=C:/temp/road-model.response",
            "roadCenterlineHandle=CL-1",
            "profileVerticalCurveHandle=VC-1",
            "sampleInterval=10",
            "leftSlopeSearchWidth=45.5",
            "rightSlopeSearchWidth=55.25",
            "selectedLeftSlopeGroupIndex=1",
            "selectedRightSlopeGroupIndex=0",
            "assignmentCount=0",
            "leftSlopeGroupCount=1",
            "leftSlopeGroup.0.startStation=0.5",
            "leftSlopeGroup.0.endStation=100.25",
            "leftSlopeGroup.0.templateCount=2",
            "leftSlopeGroup.0.template.0.templateHandle=SL%251",
            "leftSlopeGroup.0.template.0.templateName=填方%0A模板",
            "leftSlopeGroup.0.template.1.templateHandle=SL-2",
            "leftSlopeGroup.0.template.1.templateName=备用",
            "rightSlopeGroupCount=0",
        }, Encoding.UTF8);

        WithCulture("fr-FR", () =>
        {
            var request = RoadModelDialogFile.ReadRequest(path);
            Check(Math.Abs(request.LeftSlopeSearchWidth - 45.5) < 1.0e-9, "left slope search width should parse with invariant culture");
            Check(Math.Abs(request.RightSlopeSearchWidth - 55.25) < 1.0e-9, "right slope search width should parse with invariant culture");
            Check(request.SelectedLeftSlopeGroupIndex == 1, "request should keep selected left slope group");
            Check(request.SelectedRightSlopeGroupIndex == 0, "request should keep selected right slope group");
            Check(request.LeftSlopeGroups.Count == 1, "request should read left slope group count");
            Check(request.LeftSlopeGroups[0].Templates.Count == 2, "request should read slope templates in group");
            Check(request.LeftSlopeGroups[0].Templates[0].TemplateHandle == "SL%1", "slope template handle should unescape percent");
            Check(request.LeftSlopeGroups[0].Templates[0].TemplateName == "填方\n模板", "slope template name should unescape newline");
        });

        var response = new RoadModelDialogResponse
        {
            Action = RoadModelDialogAction.PickLeftSlopeTemplate,
            Accepted = false,
            PickSlopeGroupIndex = 0,
            Handle = "RM-1",
            RoadCenterlineHandle = "CL-1",
            ProfileVerticalCurveHandle = "VC-1",
            SampleInterval = 10,
            LeftSlopeSearchWidth = 45.5,
            RightSlopeSearchWidth = 55.25,
        };
        response.LeftSlopeGroups.Add(new RoadModelSlopeTemplateGroupDto
        {
            StartStation = 0.5,
            EndStation = 100.25,
            Templates = new List<RoadModelSlopeTemplateReferenceDto>
            {
                new() { TemplateHandle = "SL%1", TemplateName = "填方\n模板" },
            },
        });

        RoadModelDialogFile.WriteResponse(path, response);
        var content = File.ReadAllText(path, Encoding.UTF8);
        Check(content.Contains("action=pickLeftSlopeTemplate"), "road model response should request left slope template picking");
        Check(content.Contains("pickSlopeGroupIndex=0"), "road model response should keep selected slope group index");
        Check(content.Contains("leftSlopeSearchWidth=45.5"), "response should write left slope search width using invariant culture");
        Check(content.Contains("leftSlopeGroupCount=1"), "response should write left slope group count");
        Check(content.Contains("leftSlopeGroup.0.templateCount=1"), "response should write template count in slope group");
        Check(content.Contains("leftSlopeGroup.0.template.0.templateHandle=SL%251"), "response should escape slope template handle percent");
        Check(content.Contains("leftSlopeGroup.0.template.0.templateName=填方%0A模板"), "response should escape slope template name newline");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

static void RoadModelRequestReadsSelectedAssignmentIndex()
{
    var path = NewTempFile();
    try
    {
        File.WriteAllLines(path, new[]
        {
            "handle=RM-1",
            "responsePath=C:/temp/road-model.response",
            "roadCenterlineHandle=CL-1",
            "profileVerticalCurveHandle=VC-1",
            "sampleInterval=10",
            "selectedAssignmentIndex=2",
            "assignmentCount=0",
        }, Encoding.UTF8);

        var request = RoadModelDialogFile.ReadRequest(path);
        Check(request.SelectedAssignmentIndex == 2, "road model request should keep selected assignment index after template pick");
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
        }, Encoding.UTF8);
        File.WriteAllLines(emptyPath, new[]
        {
            "handle=RM-1",
            "responsePath=   ",
            "sampleInterval=10",
            "assignmentCount=0",
        }, Encoding.UTF8);

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

static void RoadModelSectionViewerRequestReadsPreviewsUsingInvariantCultureAndEscaping()
{
    var path = NewTempFile();
    try
    {
        File.WriteAllLines(path, new[]
        {
            "handle=RM%251",
            "roadCenterlineHandle=CL%0A1",
            "previewCount=1",
            "preview.0.station=10.5",
            "preview.0.stationLabel=K0+010.5",
            "preview.0.statusMessage=已生成%0A预览",
            "preview.0.hasGroundLine=1",
            "preview.0.segmentCount=2",
            "preview.0.segment.0.kind=Subgrade",
            "preview.0.segment.0.label=路基模板",
            "preview.0.segment.0.colorR=1",
            "preview.0.segment.0.colorG=2",
            "preview.0.segment.0.colorB=3",
            "preview.0.segment.0.pointCount=2",
            "preview.0.segment.0.point.0.offset=0",
            "preview.0.segment.0.point.0.elevation=101.25",
            "preview.0.segment.0.point.1.offset=-3.5",
            "preview.0.segment.0.point.1.elevation=101.18",
            "preview.0.segment.1.kind=Ground",
            "preview.0.segment.1.label=地面线",
            "preview.0.segment.1.colorR=132",
            "preview.0.segment.1.colorG=96",
            "preview.0.segment.1.colorB=56",
            "preview.0.segment.1.pointCount=2",
            "preview.0.segment.1.point.0.offset=-10",
            "preview.0.segment.1.point.0.elevation=98.5",
            "preview.0.segment.1.point.1.offset=10",
            "preview.0.segment.1.point.1.elevation=103.75",
        }, Encoding.UTF8);

        WithCulture("fr-FR", () =>
        {
            var request = RoadModelSectionViewerFile.ReadRequest(path);
            Check(request.Handle == "RM%1", "section viewer request should unescape handle percent");
            Check(request.RoadCenterlineHandle == "CL\n1", "section viewer request should unescape centerline newline");
            Check(request.Previews.Count == 1, "section viewer request should read preview count");
            Check(Math.Abs(request.Previews[0].Station - 10.5) < 1.0e-9, "section viewer station should parse invariant decimal");
            Check(request.Previews[0].StatusMessage == "已生成\n预览", "section viewer status should unescape newline");
            Check(request.Previews[0].HasGroundLine, "section viewer should keep ground line flag");
            Check(request.Previews[0].Segments.Count == 2, "section viewer should read segments");
            Check(request.Previews[0].Segments[0].Kind == RoadModelSectionViewerSegmentKind.Subgrade, "section viewer should parse segment kind");
            Check(request.Previews[0].Segments[0].Points.Count == 2, "section viewer should read segment points");
            Check(Math.Abs(request.Previews[0].Segments[0].Points[1].Offset + 3.5) < 1.0e-9, "section viewer point offset should parse invariant decimal");
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

static void RoadModelSectionViewerWindowContainsStationListPreviewAndLegend()
{
    var xamlPath = Path.Combine(
        FindRepoRoot(),
        "src",
        "ui",
        "wpf",
        "RoadProto.Terrain.UI",
        "RoadModelSectionViewerWindow.xaml");
    var xaml = File.ReadAllText(xamlPath, Encoding.UTF8);
    Check(xaml.Contains("Title=\"查看横断面\""), "section viewer window title should be 查看横断面");
    Check(xaml.Contains("x:Name=\"StationListBox\""), "section viewer should include station selector");
    Check(xaml.Contains("x:Name=\"PreviewCanvas\""), "section viewer should include preview canvas");
    Check(xaml.Contains("路基模板") && xaml.Contains("边坡模板") && xaml.Contains("地面线"), "section viewer should show layer legend");
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
    var xaml = File.ReadAllText(xamlPath, Encoding.UTF8);
    Check(
        xaml.Contains("Text=\"{Binding RoadCenterlineHandle, Mode=OneWay}\""),
        "road model window should bind read-only road centerline handle with Mode=OneWay");
    Check(
        xaml.Contains("Header=\"点选模板\""),
        "road model window should offer per-row template picking");
    Check(
        xaml.Contains("Click=\"OnPickTemplate\""),
        "road model window should wire template picking button");
    Check(
        xaml.Contains("Header=\"边坡模板\""),
        "road model window should include slope template tab");
    Check(
        xaml.Contains("Click=\"OnPickLeftSlopeTemplate\"") && xaml.Contains("Click=\"OnPickRightSlopeTemplate\""),
        "road model window should wire left and right slope template picking buttons");
    Check(
        xaml.Contains("Header=\"管理模板组\""),
        "road model window should expose per-row slope template group management");
    Check(
        xaml.Contains("Click=\"OnManageLeftSlopeGroup\"") && xaml.Contains("Click=\"OnManageRightSlopeGroup\""),
        "road model window should wire left and right slope template group management buttons");
    Check(
        xaml.Contains("当前模板组管理") && xaml.Contains("组内模板"),
        "road model window should show selected group management controls");
    Check(
        xaml.Contains("Click=\"OnDeleteLeftSlopeTemplate\"") && xaml.Contains("Click=\"OnMoveLeftSlopeTemplateUp\""),
        "road model window should allow editing templates inside a group");
}

static PavementLayerTemplateLayerDto MakePavementLayer(
    PavementLayerType type,
    string name,
    bool uniformThickness,
    double thickness,
    double innerThickness,
    double outerThickness)
    => new()
    {
        Type = type,
        Name = name,
        UniformThickness = uniformThickness,
        Thickness = thickness,
        InnerThickness = innerThickness,
        OuterThickness = outerThickness,
        InnerWidening = 0.15,
        OuterWidening = 0.25,
        InnerSlope = -0.02,
        OuterSlope = 0.03,
    };

static void PavementLayerTemplateDialogFileReadsRequestUsingInvariantCultureAndEscaping()
{
    var path = NewTempFile();
    try
    {
        File.WriteAllLines(path, new[]
        {
            "handle=PV%251",
            "responsePath=C:/temp/pavement.response",
            "insertionX=10.5",
            "insertionY=20.25",
            "insertionZ=0.75",
            "templateName=主线%25%0A路面结构层",
            "displayScale=20.5",
            "previewWidth=3.75",
            "layerCount=2",
            "layer.0.type=UpperSurface",
            "layer.0.name=上面层%0AAC-13",
            "layer.0.uniformThickness=1",
            "layer.0.thickness=0.04",
            "layer.0.innerThickness=0.04",
            "layer.0.outerThickness=0.04",
            "layer.0.innerWidening=0.1",
            "layer.0.outerWidening=0.2",
            "layer.0.innerSlope=-0.02",
            "layer.0.outerSlope=0.03",
            "layer.1.type=Base",
            "layer.1.name=基层%25水稳",
            "layer.1.uniformThickness=false",
            "layer.1.thickness=0.18",
            "layer.1.innerThickness=0.16",
            "layer.1.outerThickness=0.2",
            "layer.1.innerWidening=0.3",
            "layer.1.outerWidening=0.4",
            "layer.1.innerSlope=0",
            "layer.1.outerSlope=0.01",
        }, Encoding.UTF8);

        WithCulture("fr-FR", () =>
        {
            var request = PavementLayerTemplateDialogFile.ReadRequest(path);
            Check(request.Handle == "PV%1", "pavement request should unescape percent in handle");
            Check(request.ResponsePath == "C:/temp/pavement.response", "pavement request should keep response path");
            Check(Math.Abs(request.InsertionX - 10.5) < 1.0e-9, "pavement insertion X should parse invariant decimal");
            Check(Math.Abs(request.InsertionY - 20.25) < 1.0e-9, "pavement insertion Y should parse invariant decimal");
            Check(Math.Abs(request.InsertionZ - 0.75) < 1.0e-9, "pavement insertion Z should parse invariant decimal");
            Check(request.TemplateName == "主线%\n路面结构层", "pavement template name should unescape unicode, percent, and newline");
            Check(Math.Abs(request.DisplayScale - 20.5) < 1.0e-9, "pavement display scale should parse invariant decimal");
            Check(Math.Abs(request.PreviewWidth - 3.75) < 1.0e-9, "pavement preview width should parse invariant decimal");
            Check(request.Layers.Count == 2, "pavement layerCount should control layers");
            Check(request.Layers[0].Type == PavementLayerType.UpperSurface, "pavement layer type should parse");
            Check(request.Layers[0].Name == "上面层\nAC-13", "pavement layer name should unescape newline");
            Check(request.Layers[0].UniformThickness, "pavement layer uniform thickness should parse true");
            Check(Math.Abs(request.Layers[0].InnerWidening - 0.1) < 1.0e-9, "pavement inner widening should parse invariant decimal");
            Check(request.Layers[1].Type == PavementLayerType.Base, "second pavement layer type should parse");
            Check(request.Layers[1].Name == "基层%水稳", "pavement layer name should unescape percent");
            Check(!request.Layers[1].UniformThickness, "pavement layer uniform thickness should parse false");
            Check(Math.Abs(request.Layers[1].InnerThickness - 0.16) < 1.0e-9, "pavement inner thickness should parse invariant decimal");
            Check(Math.Abs(request.Layers[1].OuterThickness - 0.2) < 1.0e-9, "pavement outer thickness should parse invariant decimal");
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

static void PavementLayerTemplateDialogFileWritesAcceptedResponseUsingInvariantCultureAndEscaping()
{
    var path = NewTempFile();
    try
    {
        WithCulture("de-DE", () =>
        {
            var response = new PavementLayerTemplateDialogResponse
            {
                Accepted = true,
                Handle = "PV%1",
                InsertionX = 10.5,
                InsertionY = 20.25,
                InsertionZ = 0.75,
                TemplateName = "主线%\n路面结构层",
                DisplayScale = 20.5,
                PreviewWidth = 3.75,
            };
            response.Layers.Add(MakePavementLayer(PavementLayerType.UpperSurface, "上面层\nAC-13", true, 0.04, 0.04, 0.04));
            response.Layers.Add(MakePavementLayer(PavementLayerType.Base, "基层%水稳", false, 0.18, 0.16, 0.2));

            PavementLayerTemplateDialogFile.WriteResponse(path, response);
        });

        var content = File.ReadAllText(path, Encoding.UTF8);
        Check(content.Contains("accepted=1"), "pavement response should write accepted flag");
        Check(content.Contains("handle=PV%251"), "pavement response should escape percent in handle");
        Check(content.Contains("insertionX=10.5"), "pavement response should write insertion X using invariant decimal");
        Check(content.Contains("insertionY=20.25"), "pavement response should write insertion Y using invariant decimal");
        Check(content.Contains("insertionZ=0.75"), "pavement response should write insertion Z using invariant decimal");
        Check(content.Contains("templateName=主线%25%0A路面结构层"), "pavement response should escape percent and newline in template name");
        Check(content.Contains("displayScale=20.5"), "pavement response should write display scale using invariant decimal");
        Check(content.Contains("previewWidth=3.75"), "pavement response should write preview width using invariant decimal");
        Check(content.Contains("layerCount=2"), "pavement response should write layer count");
        Check(content.Contains("layer.0.type=UpperSurface"), "pavement response should write first layer type");
        Check(content.Contains("layer.0.name=上面层%0AAC-13"), "pavement response should escape newline in layer name");
        Check(content.Contains("layer.0.uniformThickness=1"), "pavement response should write uniform thickness bool");
        Check(content.Contains("layer.0.thickness=0.04"), "pavement response should write thickness");
        Check(content.Contains("layer.0.innerThickness=0.04"), "pavement response should write inner thickness");
        Check(content.Contains("layer.0.outerThickness=0.04"), "pavement response should write outer thickness");
        Check(content.Contains("layer.0.innerWidening=0.15"), "pavement response should write inner widening");
        Check(content.Contains("layer.0.outerWidening=0.25"), "pavement response should write outer widening");
        Check(content.Contains("layer.0.innerSlope=-0.02"), "pavement response should write inner slope");
        Check(content.Contains("layer.0.outerSlope=0.03"), "pavement response should write outer slope");
        Check(content.Contains("layer.1.type=Base"), "pavement response should write second layer type");
        Check(content.Contains("layer.1.name=基层%25水稳"), "pavement response should escape percent in layer name");
        Check(content.Contains("layer.1.uniformThickness=0"), "pavement response should write non-uniform thickness bool");
        Check(content.Contains("layer.1.innerThickness=0.16"), "pavement response should write non-uniform inner thickness");
        Check(content.Contains("layer.1.outerThickness=0.2"), "pavement response should write non-uniform outer thickness");
        Check(!content.Contains("displayScale=20,5"), "pavement response should not use current culture decimal separator");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

static void PavementLayerTemplateXmlFileRoundTripsPavementTemplate()
{
    var path = Path.Combine(Path.GetTempPath(), $"RoadProtoManagedBridgeTests_{Guid.NewGuid():N}.rpavement.xml");
    try
    {
        var template = new PavementLayerTemplateDto
        {
            TemplateName = "主线路面结构层",
            DisplayScale = 25,
            PreviewWidth = 4.25,
        };
        template.Layers.Add(MakePavementLayer(PavementLayerType.UpperSurface, "上面层", true, 0.04, 0.04, 0.04));
        template.Layers.Add(MakePavementLayer(PavementLayerType.Base, "基层", false, 0.18, 0.16, 0.2));

        PavementLayerTemplateXmlFile.Write(path, template);
        var content = File.ReadAllText(path, Encoding.UTF8);
        Check(content.Contains("RoadProtoPavementLayerTemplate"), "pavement XML should write expected root element");
        Check(content.Contains("version=\"1\""), "pavement XML should write version");
        Check(content.Contains("uniformThickness=\"true\""), "pavement XML should write true uniform thickness");
        Check(content.Contains("uniformThickness=\"false\""), "pavement XML should write false uniform thickness");

        var roundTrip = PavementLayerTemplateXmlFile.Read(path);
        Check(roundTrip.TemplateName == "主线路面结构层", "pavement XML should round-trip template name");
        Check(Math.Abs(roundTrip.DisplayScale - 25) < 1.0e-9, "pavement XML should round-trip display scale");
        Check(Math.Abs(roundTrip.PreviewWidth - 4.25) < 1.0e-9, "pavement XML should round-trip preview width");
        Check(roundTrip.Layers.Count == 2, "pavement XML should round-trip layer count");
        Check(roundTrip.Layers[0].Type == PavementLayerType.UpperSurface, "pavement XML should round-trip layer type");
        Check(roundTrip.Layers[0].UniformThickness, "pavement XML should round-trip uniform thickness true");
        Check(Math.Abs(roundTrip.Layers[0].Thickness - 0.04) < 1.0e-9, "pavement XML should round-trip uniform thickness value");
        Check(roundTrip.Layers[1].Type == PavementLayerType.Base, "pavement XML should round-trip second layer type");
        Check(!roundTrip.Layers[1].UniformThickness, "pavement XML should round-trip uniform thickness false");
        Check(Math.Abs(roundTrip.Layers[1].InnerThickness - 0.16) < 1.0e-9, "pavement XML should round-trip inner thickness");
        Check(Math.Abs(roundTrip.Layers[1].OuterThickness - 0.2) < 1.0e-9, "pavement XML should round-trip outer thickness");
        Check(Math.Abs(roundTrip.Layers[1].InnerWidening - 0.15) < 1.0e-9, "pavement XML should round-trip inner widening");
        Check(Math.Abs(roundTrip.Layers[1].OuterSlope - 0.03) < 1.0e-9, "pavement XML should round-trip outer slope");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

static void PavementLayerTemplateXmlFileRejectsMalformedXml()
{
    var path = Path.Combine(Path.GetTempPath(), $"RoadProtoManagedBridgeTests_{Guid.NewGuid():N}.rpavement.xml");
    try
    {
        void WriteXml(string propertiesAttributes, string layerAttributes)
        {
            File.WriteAllText(
                path,
                $"""
                <?xml version="1.0" encoding="utf-8"?>
                <RoadProtoPavementLayerTemplate version="1">
                  <Properties {propertiesAttributes} />
                  <Layer {layerAttributes} />
                </RoadProtoPavementLayerTemplate>
                """,
                Encoding.UTF8);
        }

        const string validProperties = "name=\"主线路面结构层\" displayScale=\"25\" previewWidth=\"4.25\"";
        const string validLayer = "type=\"UpperSurface\" name=\"上面层\" uniformThickness=\"true\" thickness=\"0.04\" innerThickness=\"0.04\" outerThickness=\"0.04\" innerWidening=\"0\" outerWidening=\"0\" innerSlope=\"0\" outerSlope=\"0\"";

        WriteXml(validProperties, validLayer.Replace("UpperSurface", "BadType"));
        ExpectThrows<InvalidDataException>(
            () => PavementLayerTemplateXmlFile.Read(path),
            "pavement XML should reject invalid layer type");

        WriteXml(validProperties, validLayer.Replace("uniformThickness=\"true\"", "uniformThickness=\"maybe\""));
        ExpectThrows<InvalidDataException>(
            () => PavementLayerTemplateXmlFile.Read(path),
            "pavement XML should reject invalid bool attributes");

        WriteXml(validProperties.Replace("displayScale=\"25\"", "displayScale=\"abc\""), validLayer);
        ExpectThrows<InvalidDataException>(
            () => PavementLayerTemplateXmlFile.Read(path),
            "pavement XML should reject invalid numeric attributes");

        WriteXml(validProperties, validLayer.Replace(" thickness=\"0.04\"", string.Empty));
        ExpectThrows<InvalidDataException>(
            () => PavementLayerTemplateXmlFile.Read(path),
            "pavement XML should reject missing mandatory layer attributes");

        WriteXml(validProperties, validLayer.Replace("innerWidening=\"0\"", "innerWidening=\"-0.1\""));
        ExpectThrows<InvalidDataException>(
            () => PavementLayerTemplateXmlFile.Read(path),
            "pavement XML should reject negative widening values");

        WriteXml(validProperties.Replace("previewWidth=\"4.25\"", "previewWidth=\"0\""), validLayer);
        ExpectThrows<InvalidDataException>(
            () => PavementLayerTemplateXmlFile.Read(path),
            "pavement XML should reject non-positive preview width");
    }
    finally
    {
        if (File.Exists(path))
        {
            File.Delete(path);
        }
    }
}

static void PavementLayerTemplateApplyUsesUniqueResponsePathContract()
{
    var root = FindRepoRoot();
    var commandPath = Path.Combine(
        root,
        "src",
        "ui",
        "wpf",
        "RoadProto.Terrain.UI",
        "AutoCad",
        "PavementLayerTemplateDialogCommands.cs");
    var windowPath = Path.Combine(
        FindRepoRoot(),
        "src",
        "ui",
        "wpf",
        "RoadProto.Terrain.UI",
        "PavementLayerTemplateWindow.xaml.cs");
    var source = File.ReadAllText(commandPath, Encoding.UTF8);
    var windowSource = File.ReadAllText(windowPath, Encoding.UTF8);
    var applyHandlerStart = source.IndexOf("window.ApplyRequested += (_, response) =>", StringComparison.Ordinal);
    var dialogStart = source.IndexOf("var dialogResult = window.ShowDialog();", StringComparison.Ordinal);
    Check(applyHandlerStart >= 0 && dialogStart > applyHandlerStart, "pavement command should contain ApplyRequested handler before ShowDialog");

    var applyHandler = source.Substring(applyHandlerStart, dialogStart - applyHandlerStart);
    Check(applyHandler.Contains("CreateApplyResponsePath("), "pavement Apply should create a unique response path");
    Check(applyHandler.Contains("PavementLayerTemplateDialogFile.WriteResponse(applyResponsePath, response)"), "pavement Apply should write to the unique response path");
    Check(applyHandler.Contains("SendApplyCommand(document, applyResponsePath)"), "pavement Apply should queue native apply with the unique response path");
    Check(!applyHandler.Contains("request.ResponsePath"), "pavement Apply should not write or queue the original request response path");
    Check(source.Contains("PavementLayerTemplateDialogFile.WriteResponse(request.ResponsePath, response)"), "pavement final OK/Cancel should keep original response path");

    var applyClickStart = windowSource.IndexOf("private void Apply_Click", StringComparison.Ordinal);
    var okClickStart = windowSource.IndexOf("private void Ok_Click", StringComparison.Ordinal);
    Check(applyClickStart >= 0 && okClickStart > applyClickStart, "pavement window should contain Apply_Click before Ok_Click");
    var applyClick = windowSource.Substring(applyClickStart, okClickStart - applyClickStart);
    Check(applyClick.Contains("ApplyRequested?.Invoke(this, response)"), "pavement Apply button should raise apply event");
    Check(!applyClick.Contains("Response ="), "pavement Apply button should not overwrite the final dialog response");
}

static void PavementLayerTemplateWindowContainsRequiredEditorContracts()
{
    var root = FindRepoRoot();
    var xaml = File.ReadAllText(Path.Combine(root, "src", "ui", "wpf", "RoadProto.Terrain.UI", "PavementLayerTemplateWindow.xaml"), Encoding.UTF8);
    var source = File.ReadAllText(Path.Combine(root, "src", "ui", "wpf", "RoadProto.Terrain.UI", "PavementLayerTemplateWindow.xaml.cs"), Encoding.UTF8);
    var combined = xaml + "\n" + source;

    Check(combined.Contains("PreviewCanvas"), "pavement window should contain PreviewCanvas");
    Check(combined.Contains("LayerCountBox"), "pavement window should contain LayerCountBox");
    Check(combined.Contains("SaveXml"), "pavement window should expose SaveXml action");
    Check(combined.Contains("ImportXml"), "pavement window should expose ImportXml action");
    Check(combined.Contains("MouseWheel"), "pavement preview should handle mouse wheel zoom");
    Check(combined.Contains("MouseMiddleButtonDown"), "pavement preview should handle middle-button pan start");
    Check(combined.Contains("MouseMiddleButtonUp"), "pavement preview should handle middle-button pan end");
    Check(!combined.Contains("左侧") && !combined.Contains("右侧"), "pavement editor labels should use 内侧/外侧 wording, not 左侧/右侧");
}

static void PavementLayerTemplateRibbonAndCommandSourceContractsExist()
{
    var root = FindRepoRoot();
    var ribbon = File.ReadAllText(Path.Combine(root, "src", "ui", "wpf", "RoadProto.Terrain.UI", "AutoCad", "RoadProtoRibbonExtension.cs"), Encoding.UTF8);
    var command = File.ReadAllText(Path.Combine(root, "src", "ui", "wpf", "RoadProto.Terrain.UI", "AutoCad", "PavementLayerTemplateDialogCommands.cs"), Encoding.UTF8);
    var combined = ribbon + "\n" + command;

    Check(combined.Contains("CommandClass(typeof(RoadProto.Terrain.UI.AutoCad.PavementLayerTemplateDialogCommands))"), "pavement command class should be registered");
    Check(combined.Contains("RD_SECTION_PAVEMENT_LAYER_TEMPLATE_CREATE"), "ribbon should contain pavement create command");
    Check(combined.Contains("RD_SECTION_PAVEMENT_LAYER_TEMPLATE_SHOW_WPF_DIALOG"), "managed command source should contain pavement show WPF command");
    Check(combined.Contains("DNPAVEMENTLAYERTEMPLATEENTITY"), "double-click source should contain pavement DXF constant");
    Check(combined.Contains("RD_SECTION_PAVEMENT_LAYER_TEMPLATE_EDIT_HANDLE"), "double-click source should queue pavement edit handle command");
}

ResponseWritesPickTerrainAction();
SubgradeRequestReadsPersistedEntityComponents();
SlopeTemplateDialogFileRoundTripsComponents();
RoadModelRequestReadsAssignmentsUsingInvariantCultureAndEscaping();
RoadModelResponseWritesAssignmentsUsingInvariantCultureAndEscaping();
RoadModelResponseWritesPickTemplateActionAndRowIndex();
RoadModelSlopeGroupsRoundTripUsingInvariantCultureAndEscaping();
RoadModelRequestReadsSelectedAssignmentIndex();
RoadModelRequestRejectsMissingOrEmptyResponsePath();
RoadModelSectionViewerRequestReadsPreviewsUsingInvariantCultureAndEscaping();
RoadModelSectionViewerWindowContainsStationListPreviewAndLegend();
RoadModelWindowReadOnlyHandleBindingIsOneWay();
PavementLayerTemplateDialogFileReadsRequestUsingInvariantCultureAndEscaping();
PavementLayerTemplateDialogFileWritesAcceptedResponseUsingInvariantCultureAndEscaping();
PavementLayerTemplateXmlFileRoundTripsPavementTemplate();
PavementLayerTemplateXmlFileRejectsMalformedXml();
PavementLayerTemplateApplyUsesUniqueResponsePathContract();
PavementLayerTemplateWindowContainsRequiredEditorContracts();
PavementLayerTemplateRibbonAndCommandSourceContractsExist();
Console.WriteLine("All RoadProto managed bridge tests passed.");
