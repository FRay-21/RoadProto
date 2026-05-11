#include "application/terrain/TerrainUpdateSampleService.h"
#include "core/command/CommandRegistry.h"
#include "core/module/ModuleRegistry.h"
#include "domain/alignment/AlignmentGeometry.h"
#include "domain/alignment/AlignmentElementChainBuilder.h"
#include "domain/alignment/AlignmentGripEditService.h"
#include "domain/alignment/HorizontalAlignmentBuilder.h"
#include "domain/alignment/IcdAlignmentFile.h"
#include "domain/alignment/StationFormatter.h"
#include "domain/profile/ProfileDmxFile.h"
#include "domain/profile/ProfileGradeGraphLayout.h"
#include "domain/relation/EntityRelationManager.h"
#include "domain/terrain/TerrainPointNormalizer.h"
#include "domain/terrain/TerrainMeshFile.h"
#include "domain/terrain/TerrainSurfaceQuery.h"
#include "domain/terrain/TerrainTextElevationParser.h"
#include "domain/terrain/TerrainTinBuilder.h"
#include "ui/ribbon/RibbonModel.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>

namespace {

int g_failures = 0;

void noopCommand()
{
}

void check(bool condition, const char* expression, const char* file, int line)
{
    if (!condition) {
        ++g_failures;
        std::cerr << file << ":" << line << " CHECK failed: " << expression << "\n";
    }
}

#define CHECK(expression) check((expression), #expression, __FILE__, __LINE__)

roadproto::core::CommandDefinition makeCommand(const std::wstring& name)
{
    return roadproto::core::CommandDefinition{
        name,
        L"Display " + name,
        L"TEST",
        L"Test command",
        &noopCommand,
        true,
        true,
        L"docs/business/test/\u6d4b\u8bd5\u547d\u4ee4\u6587\u6863.md",
        true};
}

void commandRegistryStoresMetadataAndRejectsDuplicates()
{
    roadproto::core::CommandRegistry registry;

    CHECK(registry.registerCommand(makeCommand(L"RD_TEST_ONE")));
    CHECK(!registry.registerCommand(makeCommand(L"RD_TEST_ONE")));
    CHECK(registry.contains(L"RD_TEST_ONE"));

    const auto found = registry.find(L"RD_TEST_ONE");
    CHECK(found.has_value());
    CHECK(found->moduleCode == L"TEST");
    CHECK(found->isPrototype);
    CHECK(found->reusable);
    CHECK(found->businessDocPath == L"docs/business/test/\u6d4b\u8bd5\u547d\u4ee4\u6587\u6863.md");
    CHECK(registry.allCommands().size() == 1);
}

void moduleRegistryRegistersCommandsAndRibbonPanels()
{
    roadproto::core::ModuleRegistry modules;
    roadproto::core::ModuleDefinition definition{
        L"Test Module",
        L"TEST",
        L"Module registry test module",
        []() { return true; },
        []() { return true; },
        [](roadproto::core::CommandRegistry& commands) {
            commands.registerCommand(makeCommand(L"RD_TEST_FROM_MODULE"));
        },
        [](roadproto::ui::RibbonModel& ribbon) {
            ribbon.ensurePanel(L"TEST", L"Test Module");
        },
        L"docs/modules/test.md"};

    CHECK(modules.registerModule(definition));
    CHECK(!modules.registerModule(definition));
    CHECK(modules.initializeModules());

    roadproto::core::CommandRegistry commands;
    modules.registerCommands(commands);
    CHECK(commands.contains(L"RD_TEST_FROM_MODULE"));

    roadproto::ui::RibbonModel ribbon;
    modules.registerRibbon(ribbon);
    CHECK(ribbon.tab().panels.size() == 1);
    CHECK(ribbon.tab().panels.front().moduleCode == L"TEST");
}

void relationManagerMarksDependentsForRebuild()
{
    roadproto::domain::EntityRelationManager manager;
    const auto terrain = roadproto::domain::EntityId::fromString(L"terrain.001");
    const auto profile = roadproto::domain::EntityId::fromString(L"profile.001");
    const auto section = roadproto::domain::EntityId::fromString(L"section.001");

    manager.upsertEntity({terrain, roadproto::domain::DesignEntityType::TerrainModel, L"TERRAIN"});
    manager.upsertEntity({profile, roadproto::domain::DesignEntityType::Profile, L"PROFILE"});
    manager.upsertEntity({section, roadproto::domain::DesignEntityType::CrossSection, L"CROSS_SECTION"});
    CHECK(manager.addDependency(profile, terrain));
    CHECK(manager.addDependency(section, terrain));

    const auto requests = manager.markUpdated(terrain, L"terrain changed");
    CHECK(requests.size() == 2);

    const auto profileAfter = manager.findEntity(profile);
    const auto sectionAfter = manager.findEntity(section);
    CHECK(profileAfter.has_value());
    CHECK(sectionAfter.has_value());
    CHECK(profileAfter->updateStatus == roadproto::domain::UpdateStatus::NeedsRebuild);
    CHECK(sectionAfter->updateStatus == roadproto::domain::UpdateStatus::NeedsRebuild);
    CHECK(profileAfter->needsRecalculate);
    CHECK(sectionAfter->needsGraphicRefresh);
}

void terrainSampleServiceCreatesRelationUpdateScenario()
{
    roadproto::domain::EntityRelationManager manager;
    roadproto::application::TerrainUpdateSampleService service(manager);

    const auto result = service.run();
    CHECK(result.terrainId.value() == L"terrain.sample.tin.001");
    CHECK(result.rebuildRequests.size() == 2);

    const auto entities = manager.allEntities();
    CHECK(entities.size() == 3);
}

void terrainTextElevationParserRecognizesSupportedForms()
{
    roadproto::domain::terrain::TerrainTextElevationParser parser;

    const auto direct = parser.tryParse(L"12.35");
    CHECK(direct.has_value());
    CHECK(std::fabs(*direct - 12.35) < 1e-9);

    const auto signedValue = parser.tryParse(L"EL=-3.20");
    CHECK(signedValue.has_value());
    CHECK(std::fabs(*signedValue + 3.20) < 1e-9);

    const auto chinese = parser.tryParse(L"设计高程12.35");
    CHECK(chinese.has_value());
    CHECK(std::fabs(*chinese - 12.35) < 1e-9);

    CHECK(!parser.tryParse(L"桩号 K0+120").has_value());
}

void terrainPointNormalizerMergesDuplicateAndTextElevationPoints()
{
    using namespace roadproto::domain::terrain;

    TinExtractOptions options;
    options.xyMergeTolerance = 0.001;
    options.zEqualTolerance = 0.01;
    options.textPointSearchDistance = 0.5;

    std::vector<TinPoint> raw{
        TinPoint{0.0, 0.0, 0.0, false, TinPointSourceType::CadPoint, L"P1"},
        TinPoint{0.0004, 0.0003, 12.0, true, TinPointSourceType::TextElevation, L"T1"},
        TinPoint{5.0, 0.0, 8.0, true, TinPointSourceType::CadPoint, L"P2"},
        TinPoint{5.0002, 0.0001, 8.005, true, TinPointSourceType::LineVertex, L"L1"},
        TinPoint{0.0, 5.0, 8.0, true, TinPointSourceType::CadPoint, L"P3"},
    };

    TerrainPointNormalizer normalizer;
    const auto result = normalizer.normalize(raw, options);

    CHECK(result.points.size() == 3);
    CHECK(result.summary.duplicatePointCount == 1);
    CHECK(result.summary.textAssignedElevationPointCount == 1);
    CHECK(result.summary.textElevationPointCount == 1);
    CHECK(result.points.front().hasValidElevation);
    CHECK(std::fabs(result.points.front().z - 12.0) < 1e-9);
}

void terrainTinBuilderCreatesTrianglesAndRejectsCollinearInput()
{
    using namespace roadproto::domain::terrain;

    TerrainTinBuilder builder;
    TinBuildOptions options;
    options.minTriangleArea = 1e-8;

    std::vector<TinPoint> square{
        TinPoint{0.0, 0.0, 10.0, true},
        TinPoint{10.0, 0.0, 20.0, true},
        TinPoint{10.0, 10.0, 30.0, true},
        TinPoint{0.0, 10.0, 20.0, true},
    };

    const auto result = builder.build(square, options);
    CHECK(result.succeeded);
    CHECK(result.triangles.size() == 2);
    CHECK(result.boundaryEdges.size() == 4);
    CHECK(std::fabs(result.minElevation - 10.0) < 1e-9);
    CHECK(std::fabs(result.maxElevation - 30.0) < 1e-9);

    std::vector<TinPoint> collinear{
        TinPoint{0.0, 0.0, 1.0, true},
        TinPoint{1.0, 1.0, 2.0, true},
        TinPoint{2.0, 2.0, 3.0, true},
    };
    const auto failed = builder.build(collinear, options);
    CHECK(!failed.succeeded);
    CHECK(failed.errorMessage == L"点集共线");
}

void terrainSurfaceQueryInterpolatesElevationInsideTriangle()
{
    using namespace roadproto::domain::terrain;

    std::vector<TinPoint> points{
        TinPoint{0.0, 0.0, 10.0, true},
        TinPoint{10.0, 0.0, 20.0, true},
        TinPoint{0.0, 10.0, 30.0, true},
    };
    std::vector<TinTriangle> triangles{
        TinTriangle{0, 1, 2},
    };

    TinTriangle found;
    CHECK(TerrainSurfaceQuery::findTriangle(points, triangles, 2.0, 2.0, found));
    CHECK(found.a == 0 && found.b == 1 && found.c == 2);

    double z = 0.0;
    CHECK(TerrainSurfaceQuery::sampleElevation(points, triangles, 2.0, 2.0, z));
    CHECK(std::fabs(z - 16.0) < 1e-9);
}

void terrainMeshFileRoundTripsTinData()
{
    using namespace roadproto::domain::terrain;

    TinBuildResult mesh;
    mesh.succeeded = true;
    mesh.extractOptions.xyMergeTolerance = 0.002;
    mesh.extractOptions.textPointSearchDistance = 0.75;
    mesh.extractOptions.enableNestedBlockExtraction = false;
    mesh.buildOptions.maxEdgeLength = 120.0;
    mesh.buildOptions.minTriangleArea = 1e-6;
    mesh.buildOptions.removeDegenerateTriangles = false;
    mesh.buildOptions.displayMode = TerrainTinDisplayMode::BoundaryOnly;
    mesh.minElevation = 10.0;
    mesh.maxElevation = 30.0;
    mesh.extractSummary.selectedObjectCount = 2;
    mesh.extractSummary.rawPointCount = 3;
    mesh.extractSummary.validPointCount = 3;
    mesh.extractSummary.triangleCount = 1;
    mesh.extractSummary.status = L"loaded";

    TinPoint pointA;
    pointA.x = 0.0;
    pointA.y = 0.0;
    pointA.z = 10.0;
    pointA.sourceType = TinPointSourceType::CadPoint;
    pointA.sourceObjectHandle = L"10A";

    TinPoint pointB;
    pointB.x = 10.0;
    pointB.y = 0.0;
    pointB.z = 20.0;
    pointB.sourceType = TinPointSourceType::BlockAttribute;
    pointB.sourceObjectHandle = L"10B";
    pointB.blockName = L"\u6807\u9ad8\u5757";
    pointB.attributeTag = L"\u9ad8\u7a0b";

    TinPoint pointC;
    pointC.x = 0.0;
    pointC.y = 10.0;
    pointC.z = 30.0;
    pointC.sourceType = TinPointSourceType::TextElevation;
    pointC.sourceObjectHandle = L"10C";
    pointC.associatedTextHandle = L"20C";
    pointC.associatedGeometryHandle = L"30C";
    pointC.mergedFromText = true;

    mesh.points = {pointA, pointB, pointC};
    mesh.triangles = {TinTriangle{0, 1, 2}};
    mesh.boundaryEdges = {{0, 1}, {1, 2}, {0, 2}};

    const auto path = std::filesystem::temp_directory_path() / L"roadproto_rmesh_roundtrip.rmesh";
    std::filesystem::remove(path);

    TerrainMeshFile meshFile;
    std::wstring error;
    CHECK(meshFile.write(path.wstring(), mesh, error));
    CHECK(error.empty());

    const auto loaded = meshFile.read(path.wstring());
    CHECK(loaded.succeeded);
    CHECK(loaded.mesh.points.size() == 3);
    CHECK(loaded.mesh.triangles.size() == 1);
    CHECK(loaded.mesh.boundaryEdges.size() == 3);
    CHECK(loaded.mesh.buildOptions.displayMode == TerrainTinDisplayMode::BoundaryOnly);
    CHECK(std::fabs(loaded.mesh.buildOptions.maxEdgeLength - 120.0) < 1e-9);
    CHECK(!loaded.mesh.extractOptions.enableNestedBlockExtraction);
    CHECK(loaded.mesh.points[1].sourceType == TinPointSourceType::BlockAttribute);
    CHECK(loaded.mesh.points[1].blockName == L"\u6807\u9ad8\u5757");
    CHECK(loaded.mesh.points[1].attributeTag == L"\u9ad8\u7a0b");
    CHECK(loaded.mesh.points[2].mergedFromText);
    CHECK(loaded.mesh.extractSummary.triangleCount == 1);

    std::filesystem::remove(path);
}

void terrainMeshFileRejectsInvalidFiles()
{
    using namespace roadproto::domain::terrain;

    const auto path = std::filesystem::temp_directory_path() / L"roadproto_rmesh_invalid.rmesh";
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << "not a roadproto mesh";
    }

    TerrainMeshFile meshFile;
    const auto loaded = meshFile.read(path.wstring());
    CHECK(!loaded.succeeded);
    CHECK(!loaded.errorMessage.empty());

    std::filesystem::remove(path);
}

void stationFormatterFormatsEngineeringStations()
{
    using roadproto::domain::alignment::StationFormatter;

    CHECK(StationFormatter::format(0.0) == L"K0+000");
    CHECK(StationFormatter::format(100.0) == L"K0+100");
    CHECK(StationFormatter::format(1234.56) == L"K1+234.560");
    CHECK(StationFormatter::format(-1.0) == L"K0+000");
}

void clothoidMathUsesRoadDesignFormulas()
{
    using namespace roadproto::domain::alignment;

    const double radius = 300.0;
    const double ls = 60.0;
    CHECK(std::fabs(clothoidA(radius, ls) * clothoidA(radius, ls) - radius * ls) < 1e-9);
    CHECK(std::fabs(clothoidCurvatureAt(30.0, radius, ls) - (30.0 / (radius * ls))) < 1e-12);
    CHECK(std::fabs(clothoidTangentAngleAt(ls, radius, ls) - (ls / (2.0 * radius))) < 1e-12);
    CHECK(std::fabs(clothoidEndX(radius, ls) - 59.940027771368) < 1e-6);
    CHECK(std::fabs(clothoidEndY(radius, ls) - 1.998571883117) < 1e-6);
    CHECK(std::fabs(spiralShiftP(radius, ls) - 0.499821466525) < 1e-6);
    CHECK(std::fabs(spiralTangentOffsetM(radius, ls) - 29.990002777319) < 1e-6);
    CHECK(std::fabs(defaultSpiralTangentLength(1.5707963267948966, radius, ls, ls) - 330.489824243844) < 1e-6);
}

void horizontalAlignmentBuilderCreatesFiveElements()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{200.0, 0.0},
        AlignmentPoint2d{200.0, 200.0},
    };
    input.defaultParameters.radius = 80.0;
    input.defaultParameters.ls1 = 20.0;
    input.defaultParameters.ls2 = 20.0;
    input.properties.stationLabelInterval = 100.0;

    HorizontalAlignmentBuilder builder;
    const auto result = builder.build(input);

    CHECK(result.succeeded);
    if (!result.succeeded) {
        return;
    }
    CHECK(result.alignment.elements.size() == 5);
    CHECK(result.alignment.elements[0].type == AlignmentElementType::Line);
    CHECK(result.alignment.elements[1].type == AlignmentElementType::SpiralIn);
    CHECK(result.alignment.elements[2].type == AlignmentElementType::CircularArc);
    CHECK(result.alignment.elements[3].type == AlignmentElementType::SpiralOut);
    CHECK(result.alignment.elements[4].type == AlignmentElementType::Line);
    CHECK(result.alignment.featurePoints.size() >= 6);
    CHECK(result.alignment.stationLabels.size() >= 2);
    CHECK(result.alignment.totalLength > 100.0);

    const auto pi = std::find_if(
        result.alignment.featurePoints.begin(),
        result.alignment.featurePoints.end(),
        [](const auto& feature) {
            return feature.type == AlignmentFeaturePointType::PI;
        });
    CHECK(pi != result.alignment.featurePoints.end());
    if (pi != result.alignment.featurePoints.end()) {
        CHECK(std::fabs(pi->point.x - 200.0) < 1e-6);
        CHECK(std::fabs(pi->point.y - 0.0) < 1e-6);
    }

    const auto expectedT = defaultSpiralTangentLength(
        1.5707963267948966,
        input.defaultParameters.radius,
        input.defaultParameters.ls1,
        input.defaultParameters.ls2);
    CHECK(std::fabs(result.alignment.curveParameters.front().tangentIn - expectedT) < 1e-6);
    CHECK(std::fabs(result.alignment.elements.front().end.x - (200.0 - expectedT)) < 1e-6);
    CHECK(std::fabs(result.alignment.elements.back().start.x - 200.0) < 1e-3);
}

void horizontalAlignmentBuilderKeepsFiveElementShapeWithStaleTangents()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{200.0, 0.0},
        AlignmentPoint2d{200.0, 200.0},
    };
    input.defaultParameters.radius = 80.0;
    input.defaultParameters.ls1 = 20.0;
    input.defaultParameters.ls2 = 20.0;
    input.curveParameters = {HorizontalCurveParameters{5.0, 500.0, 20.0, 80.0, 20.0}};
    input.properties.stationLabelInterval = 100.0;

    HorizontalAlignmentBuilder builder;
    const auto result = builder.build(input);

    CHECK(result.succeeded);
    if (!result.succeeded) {
        return;
    }

    const auto expectedT = defaultSpiralTangentLength(
        1.5707963267948966,
        input.defaultParameters.radius,
        input.defaultParameters.ls1,
        input.defaultParameters.ls2);
    CHECK(std::fabs(result.alignment.curveParameters.front().tangentIn - expectedT) < 1e-6);
    CHECK(std::fabs(result.alignment.curveParameters.front().tangentOut - expectedT) < 1e-6);
    CHECK(result.alignment.elements[1].end.x <= result.alignment.elements[2].start.x + 1e-6);
    CHECK(result.alignment.elements[2].end.y <= result.alignment.elements[3].end.y + 1e-6);
}

void horizontalAlignmentBuilderCreatesContinuousMultiPiChain()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{300.0, 0.0},
        AlignmentPoint2d{300.0, 300.0},
        AlignmentPoint2d{600.0, 300.0},
    };
    input.defaultParameters.radius = 60.0;
    input.defaultParameters.ls1 = 15.0;
    input.defaultParameters.ls2 = 15.0;
    input.properties.stationLabelInterval = 100.0;

    HorizontalAlignmentBuilder builder;
    const auto result = builder.build(input);

    CHECK(result.succeeded);
    if (!result.succeeded) {
        return;
    }
    CHECK(result.alignment.curveParameters.size() == 2);
    CHECK(result.alignment.elements.size() == 9);
    CHECK(result.alignment.elements[4].type == AlignmentElementType::Line);
    CHECK(result.alignment.featurePoints.size() >= 14);
    CHECK(result.alignment.stationLabels.size() >= 4);
    CHECK(result.alignment.totalLength > 600.0);
}

void horizontalAlignmentBuilderUsesAsymmetricTransitionTangents()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{300.0, 0.0},
        AlignmentPoint2d{300.0, 300.0},
    };
    input.defaultParameters.radius = 80.0;
    input.defaultParameters.ls1 = 20.0;
    input.defaultParameters.ls2 = 60.0;

    HorizontalAlignmentBuilder builder;
    const auto result = builder.build(input);

    CHECK(result.succeeded);
    if (!result.succeeded) {
        return;
    }

    const auto& parameters = result.alignment.curveParameters.front();
    CHECK(parameters.tangentOut > parameters.tangentIn);
    CHECK(std::fabs(parameters.tangentIn - 91.860405938723) < 1e-6);
    CHECK(std::fabs(parameters.tangentOut - 110.068140125348) < 1e-6);
}

void horizontalAlignmentBuilderRejectsImpossibleCurve()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{10.0, 0.0},
        AlignmentPoint2d{10.0, 10.0},
    };
    input.defaultParameters.radius = -5.0;

    HorizontalAlignmentBuilder builder;
    const auto result = builder.build(input);

    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());
}

void alignmentElementChainBuilderSamplesOvalPartialSpiral()
{
    using namespace roadproto::domain::alignment;

    AlignmentElementChainInput input;
    input.properties.roadName = L"OVAL";
    input.properties.stationLabelInterval = 25.0;
    input.combinationType = AlignmentCurveCombinationType::OvalCurve;
    input.startPoint = AlignmentPoint2d{0.0, 0.0};
    input.startHeading = 0.0;
    input.elements = {
        AlignmentChainElementInput::circularArc(40.0, 120.0, CurveTurnDirection::Left),
        AlignmentChainElementInput::partialSpiral(60.0, 120.0, 80.0, CurveTurnDirection::Left),
        AlignmentChainElementInput::circularArc(50.0, 80.0, CurveTurnDirection::Left),
    };

    AlignmentElementChainBuilder builder;
    const auto result = builder.build(input);

    CHECK(result.succeeded);
    if (!result.succeeded) {
        return;
    }

    CHECK(result.alignment.combinationType == AlignmentCurveCombinationType::OvalCurve);
    CHECK(result.alignment.elements.size() == 3);
    CHECK(result.alignment.elements[0].type == AlignmentElementType::CircularArc);
    CHECK(result.alignment.elements[1].type == AlignmentElementType::PartialSpiral);
    CHECK(result.alignment.elements[2].type == AlignmentElementType::CircularArc);

    const auto& transition = result.alignment.elements[1];
    CHECK(std::fabs(transition.startCurvature - (1.0 / 120.0)) < 1e-12);
    CHECK(std::fabs(transition.endCurvature - (1.0 / 80.0)) < 1e-12);
    CHECK(std::fabs(transition.length - 60.0) < 1e-12);
    CHECK(transition.samples.size() > 8);
    CHECK(std::fabs(result.alignment.totalLength - 150.0) < 1e-9);
    CHECK(!result.alignment.stationLabels.empty());
}

void alignmentElementChainBuilderRejectsInvalidPartialSpiral()
{
    using namespace roadproto::domain::alignment;

    AlignmentElementChainInput input;
    input.startPoint = AlignmentPoint2d{0.0, 0.0};
    input.startHeading = 0.0;
    input.elements = {
        AlignmentChainElementInput::circularArc(40.0, 120.0, CurveTurnDirection::Left),
        AlignmentChainElementInput::partialSpiral(60.0, 120.0, 120.0, CurveTurnDirection::Left),
        AlignmentChainElementInput::circularArc(50.0, 80.0, CurveTurnDirection::Left),
    };

    AlignmentElementChainBuilder builder;
    const auto result = builder.build(input);

    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());
}

void alignmentElementChainBuilderSamplesSCurveCurvatureTransition()
{
    using namespace roadproto::domain::alignment;

    AlignmentElementChainInput input;
    input.properties.roadName = L"S";
    input.combinationType = AlignmentCurveCombinationType::SCurve;
    input.startPoint = AlignmentPoint2d{0.0, 0.0};
    input.startHeading = 0.0;
    input.elements = {
        AlignmentChainElementInput::circularArc(30.0, 100.0, CurveTurnDirection::Left),
        AlignmentChainElementInput::curvatureTransition(45.0, 1.0 / 100.0, -1.0 / 120.0),
        AlignmentChainElementInput::circularArc(35.0, 120.0, CurveTurnDirection::Right),
    };

    AlignmentElementChainBuilder builder;
    const auto result = builder.build(input);

    CHECK(result.succeeded);
    if (!result.succeeded) {
        return;
    }

    CHECK(result.alignment.combinationType == AlignmentCurveCombinationType::SCurve);
    CHECK(result.alignment.elements.size() == 3);
    const auto& transition = result.alignment.elements[1];
    CHECK(transition.type == AlignmentElementType::PartialSpiral);
    CHECK(transition.startCurvature > 0.0);
    CHECK(transition.endCurvature < 0.0);
    CHECK(result.alignment.elements[2].startCurvature < 0.0);
    CHECK(std::fabs(result.alignment.totalLength - 110.0) < 1e-9);
}

void icdAlignmentFileRoundTripsFiveElementAlignment()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{200.0, 0.0},
        AlignmentPoint2d{200.0, 200.0},
    };
    input.defaultParameters.radius = 80.0;
    input.defaultParameters.ls1 = 20.0;
    input.defaultParameters.ls2 = 20.0;
    input.properties.roadName = L"ICD";
    input.properties.stationLabelInterval = 50.0;

    HorizontalAlignmentBuilder alignmentBuilder;
    const auto built = alignmentBuilder.build(input);
    CHECK(built.succeeded);
    if (!built.succeeded) {
        return;
    }

    IcdAlignmentFile file;
    const auto document = file.documentFromAlignment(built.alignment);
    CHECK(document.units.size() == 5);
    CHECK(document.units[0].type == IcdUnitType::Line);
    CHECK(document.units[1].type == IcdUnitType::SpiralIn);
    CHECK(document.units[2].type == IcdUnitType::CircularArc);
    CHECK(document.units[3].type == IcdUnitType::SpiralOut);
    CHECK(document.units[4].type == IcdUnitType::Line);

    const auto path = std::filesystem::temp_directory_path() / L"roadproto_alignment_roundtrip.icd";
    std::wstring errorMessage;
    CHECK(file.write(path.wstring(), document, errorMessage));

    const auto loaded = file.read(path.wstring());
    CHECK(loaded.succeeded);
    if (!loaded.succeeded) {
        return;
    }

    const auto imported = file.alignmentFromDocument(loaded.document, input.properties);
    CHECK(imported.succeeded);
    if (!imported.succeeded) {
        return;
    }

    CHECK(imported.alignment.elements.size() == 5);
    CHECK(imported.alignment.elements[1].type == AlignmentElementType::PartialSpiral);
    CHECK(imported.alignment.elements[2].type == AlignmentElementType::CircularArc);
    CHECK(std::fabs(imported.alignment.totalLength - built.alignment.totalLength) < 1e-3);

    const auto exportedAgain = file.documentFromAlignment(imported.alignment);
    CHECK(exportedAgain.units[1].type == IcdUnitType::SpiralIn);
    CHECK(exportedAgain.units[3].type == IcdUnitType::SpiralOut);

    std::filesystem::remove(path);
}

void icdAlignmentFileImportsType5PartialSpiral()
{
    using namespace roadproto::domain::alignment;

    const auto path = std::filesystem::temp_directory_path() / L"roadproto_alignment_type5.icd";
    {
        std::ofstream stream(path);
        stream << "100.000000_2 // start station with chain\n";
        stream << "0.000000,0.000000,0.000000\n";
        stream << "2,120.000000,30.000000,-1\n";
        stream << "5,100.000000,120.000000,80.000000,-1\n";
        stream << "2,80.000000,40.000000,-1\n";
        stream << "0,0,0\n";
    }

    IcdAlignmentFile file;
    const auto loaded = file.read(path.wstring());
    CHECK(loaded.succeeded);
    if (!loaded.succeeded) {
        std::filesystem::remove(path);
        return;
    }

    RoadCenterlineProperties properties;
    properties.roadName = L"TYPE5";
    properties.stationLabelInterval = 25.0;
    const auto imported = file.alignmentFromDocument(loaded.document, properties);
    CHECK(imported.succeeded);
    if (!imported.succeeded) {
        std::filesystem::remove(path);
        return;
    }

    CHECK(imported.alignment.combinationType == AlignmentCurveCombinationType::OvalCurve);
    CHECK(imported.alignment.elements.size() == 3);
    const auto& transition = imported.alignment.elements[1];
    CHECK(transition.type == AlignmentElementType::PartialSpiral);
    CHECK(std::fabs(transition.length - (10000.0 / 80.0 - 10000.0 / 120.0)) < 1e-6);
    CHECK(std::fabs(transition.startCurvature - (1.0 / 120.0)) < 1e-12);
    CHECK(std::fabs(transition.endCurvature - (1.0 / 80.0)) < 1e-12);

    std::filesystem::remove(path);
}

void icdAlignmentFileMapsEngineeringCoordinatesToCadCoordinates()
{
    using namespace roadproto::domain::alignment;

    const auto path = std::filesystem::temp_directory_path() / L"roadproto_alignment_coordinate_mapping.icd";
    {
        std::ofstream stream(path);
        stream << "0.000000\n";
        stream << "100.000000,200.000000,0.000000\n";
        stream << "1,10.000000\n";
        stream << "0,0,0\n";
    }

    IcdAlignmentFile file;
    const auto loaded = file.read(path.wstring());
    CHECK(loaded.succeeded);
    if (!loaded.succeeded) {
        std::filesystem::remove(path);
        return;
    }

    RoadCenterlineProperties properties;
    properties.roadName = L"COORD";
    const auto imported = file.alignmentFromDocument(loaded.document, properties);
    CHECK(imported.succeeded);
    if (!imported.succeeded) {
        std::filesystem::remove(path);
        return;
    }

    const auto& line = imported.alignment.elements.front();
    CHECK(std::fabs(line.start.x - 200.0) < 1e-9);
    CHECK(std::fabs(line.start.y - 100.0) < 1e-9);
    CHECK(std::fabs(line.startHeading - (3.14159265358979323846 / 2.0)) < 1e-9);
    CHECK(std::fabs(line.end.x - 200.0) < 1e-6);
    CHECK(std::fabs(line.end.y - 110.0) < 1e-6);

    const auto exported = file.documentFromAlignment(imported.alignment);
    CHECK(std::fabs(exported.startPoint.x - 100.0) < 1e-9);
    CHECK(std::fabs(exported.startPoint.y - 200.0) < 1e-9);
    CHECK(std::fabs(exported.startHeading) < 1e-9);

    std::filesystem::remove(path);
}

void alignmentGripEditServiceRebuildsDraggedPiPreview()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{200.0, 0.0},
        AlignmentPoint2d{200.0, 200.0},
    };
    input.defaultParameters.radius = 80.0;
    input.defaultParameters.ls1 = 20.0;
    input.defaultParameters.ls2 = 20.0;

    HorizontalAlignmentBuilder builder;
    const auto built = builder.build(input);
    CHECK(built.succeeded);
    if (!built.succeeded) {
        return;
    }

    auto alignment = built.alignment;
    AlignmentGripEditService service;
    const auto editResult = service.applyGripOffsets(alignment, {1}, 30.0, 40.0);

    CHECK(editResult.succeeded);
    CHECK(editResult.changed);
    CHECK(std::fabs(alignment.controlPoints[1].x - 230.0) < 1e-9);
    CHECK(std::fabs(alignment.controlPoints[1].y - 40.0) < 1e-9);
    CHECK(alignment.elements.size() == 5);
    CHECK(alignment.featurePoints.size() >= built.alignment.featurePoints.size());

    const auto pi = std::find_if(
        alignment.featurePoints.begin(),
        alignment.featurePoints.end(),
        [](const auto& feature) {
            return feature.type == AlignmentFeaturePointType::PI;
        });
    CHECK(pi != alignment.featurePoints.end());
    if (pi != alignment.featurePoints.end()) {
        CHECK(std::fabs(pi->point.x - 230.0) < 1e-6);
        CHECK(std::fabs(pi->point.y - 40.0) < 1e-6);
    }
}

void alignmentGripEditServiceMovesSharedControlAndPiGripOnce()
{
    using namespace roadproto::domain::alignment;

    HorizontalAlignmentInput input;
    input.controlPoints = {
        AlignmentPoint2d{0.0, 0.0},
        AlignmentPoint2d{200.0, 0.0},
        AlignmentPoint2d{200.0, 200.0},
    };
    input.defaultParameters.radius = 80.0;
    input.defaultParameters.ls1 = 20.0;
    input.defaultParameters.ls2 = 20.0;

    HorizontalAlignmentBuilder builder;
    const auto built = builder.build(input);
    CHECK(built.succeeded);
    if (!built.succeeded) {
        return;
    }

    std::size_t piFeatureIndex = built.alignment.featurePoints.size();
    for (std::size_t i = 0; i < built.alignment.featurePoints.size(); ++i) {
        if (built.alignment.featurePoints[i].type == AlignmentFeaturePointType::PI) {
            piFeatureIndex = i;
            break;
        }
    }
    CHECK(piFeatureIndex < built.alignment.featurePoints.size());
    if (piFeatureIndex >= built.alignment.featurePoints.size()) {
        return;
    }

    auto alignment = built.alignment;
    const auto controlGripIndex = std::size_t{1};
    const auto piFeatureGripIndex = alignment.controlPoints.size() + piFeatureIndex;
    AlignmentGripEditService service;
    const auto editResult = service.applyGripOffsets(
        alignment,
        {controlGripIndex, piFeatureGripIndex},
        30.0,
        40.0);

    CHECK(editResult.succeeded);
    CHECK(editResult.changed);
    CHECK(std::fabs(alignment.controlPoints[1].x - 230.0) < 1e-9);
    CHECK(std::fabs(alignment.controlPoints[1].y - 40.0) < 1e-9);
}

void profileDmxFileParsesStationsAndKeepsDuplicates()
{
    using namespace roadproto::domain::profile;

    const auto parsed = ProfileDmxFile::parseText(
        L"  // comment\n"
        L"0.00000000 21.25100000\n"
        L"2.70000000 19.95400000\n"
        L"2.70000000 19.93400000\n"
        L"37123.456_2 36.12000000\n"
        L"bad line\n");

    CHECK(parsed.succeeded);
    CHECK(parsed.samples.size() == 4);
    CHECK(parsed.invalidLineCount == 1);
    CHECK(std::fabs(parsed.samples[1].station - 2.7) < 1e-9);
    CHECK(std::fabs(parsed.samples[2].station - 2.7) < 1e-9);
    CHECK(std::fabs(parsed.samples[1].elevation - 19.954) < 1e-9);
    CHECK(std::fabs(parsed.samples[2].elevation - 19.934) < 1e-9);
    CHECK(parsed.samples[3].rawStationText == L"37123.456_2");
    CHECK(parsed.samples[3].breakChainIndex == 2);
}

void profileDmxFileRejectsTooFewValidSamples()
{
    using namespace roadproto::domain::profile;

    const auto parsed = ProfileDmxFile::parseText(
        L"0.00000000 21.25100000\n"
        L"bad line\n");

    CHECK(!parsed.succeeded);
    CHECK(parsed.samples.size() == 1);
    CHECK(parsed.invalidLineCount == 1);
    CHECK(!parsed.errorMessage.empty());
}

void profileDmxFileRejectsNonFiniteRows()
{
    using namespace roadproto::domain::profile;

    const auto parsed = ProfileDmxFile::parseText(
        L"0.00000000 21.25100000\n"
        L"nan 22.00000000\n"
        L"10.00000000 inf\n"
        L"20.00000000 23.25100000\n");

    CHECK(parsed.succeeded);
    CHECK(parsed.samples.size() == 2);
    CHECK(parsed.invalidLineCount == 2);
    CHECK(std::fabs(parsed.samples[0].station - 0.0) < 1e-9);
    CHECK(std::fabs(parsed.samples[1].station - 20.0) < 1e-9);
}

void profileDmxFileParsesTextWithLeadingBom()
{
    using namespace roadproto::domain::profile;

    const auto parsed = ProfileDmxFile::parseText(
        L"\ufeff0.00000000 21.25100000\n"
        L"10.00000000 22.25100000\n");

    CHECK(parsed.succeeded);
    CHECK(parsed.samples.size() == 2);
    CHECK(parsed.invalidLineCount == 0);
    CHECK(std::fabs(parsed.samples.front().station - 0.0) < 1e-9);
}

void profileDmxFileParsesTextWithLeadingUtf8BomBytes()
{
    using namespace roadproto::domain::profile;

    std::wstring content;
    content.push_back(static_cast<wchar_t>(0x00EF));
    content.push_back(static_cast<wchar_t>(0x00BB));
    content.push_back(static_cast<wchar_t>(0x00BF));
    content += L"0.00000000 21.25100000\n10.00000000 22.25100000\n";

    const auto parsed = ProfileDmxFile::parseText(content);

    CHECK(parsed.succeeded);
    CHECK(parsed.samples.size() == 2);
    CHECK(parsed.invalidLineCount == 0);
    CHECK(std::fabs(parsed.samples.front().station - 0.0) < 1e-9);
}

void profileDmxFileReadsTempFile()
{
    using namespace roadproto::domain::profile;

    const auto path = std::filesystem::temp_directory_path() / L"roadproto_profile_read_test.dmx";
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        stream << "\xEF\xBB\xBF";
        stream << "// comment\n";
        stream << "0.00000000 21.25100000\n";
        stream << "10.00000000 22.25100000\n";
    }

    const auto parsed = ProfileDmxFile::read(path.wstring());
    CHECK(parsed.succeeded);
    CHECK(parsed.samples.size() == 2);
    CHECK(parsed.invalidLineCount == 0);
    CHECK(std::fabs(parsed.samples[1].station - 10.0) < 1e-9);

    std::filesystem::remove(path);
}

void profileGradeGraphLayoutMapsStationAndElevation()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    graph.properties.gridSpacing = 10.0;
    graph.properties.verticalScale = 10.0;
    graph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const auto layout = ProfileGradeGraphLayout::calculate(graph);
    CHECK(layout.succeeded);
    CHECK(std::fabs(layout.minStation - 100.0) < 1e-9);
    CHECK(std::fabs(layout.maxStation - 120.0) < 1e-9);
    CHECK(std::fabs(layout.baseElevation - 20.0) < 1e-9);
    CHECK(std::fabs(ProfileGradeGraphLayout::mapX(layout, 115.0) - 15.0) < 1e-9);
    CHECK(std::fabs(ProfileGradeGraphLayout::mapY(graph, layout, 23.5) - 35.0) < 1e-9);
}

void profileGradeGraphLayoutRejectsNonFiniteGeometry()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    graph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{std::numeric_limits<double>::infinity(), 36.0},
    };

    const auto layout = ProfileGradeGraphLayout::calculate(graph);
    CHECK(!layout.succeeded);
    CHECK(!layout.errorMessage.empty());
}

void profileGradeGraphDataDefaultsToDmxFileSource()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    CHECK(graph.sourceType == ProfileGroundSourceType::DmxFile);
}

void profileGradeGraphLayoutMapYUsesDefaultVerticalScale()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphLayoutResult layout;
    layout.baseElevation = 20.0;

    CHECK(std::fabs(ProfileGradeGraphLayout::mapY(layout, 23.5) - 35.0) < 1e-9);
}

void alignmentCommandMetadataUsesExpectedNames()
{
    roadproto::core::CommandRegistry commands;
    commands.registerCommand(roadproto::core::CommandDefinition{
        L"RD_ALIGN_CENTERLINE_CREATE",
        L"平面布线",
        L"ALIGNMENT",
        L"Creates a road centerline alignment entity.",
        &noopCommand,
        true,
        true,
        L"docs/business/alignment/平面布线_道路中线创建.md",
        true});

    const auto found = commands.find(L"RD_ALIGN_CENTERLINE_CREATE");
    CHECK(found.has_value());
    CHECK(found->moduleCode == L"ALIGNMENT");
    CHECK(found->displayName == L"平面布线");
    CHECK(found->ribbonAttachable);
}

} // namespace

int main()
{
    commandRegistryStoresMetadataAndRejectsDuplicates();
    moduleRegistryRegistersCommandsAndRibbonPanels();
    relationManagerMarksDependentsForRebuild();
    terrainSampleServiceCreatesRelationUpdateScenario();
    terrainTextElevationParserRecognizesSupportedForms();
    terrainPointNormalizerMergesDuplicateAndTextElevationPoints();
    terrainTinBuilderCreatesTrianglesAndRejectsCollinearInput();
    terrainSurfaceQueryInterpolatesElevationInsideTriangle();
    terrainMeshFileRoundTripsTinData();
    terrainMeshFileRejectsInvalidFiles();
    stationFormatterFormatsEngineeringStations();
    clothoidMathUsesRoadDesignFormulas();
    horizontalAlignmentBuilderCreatesFiveElements();
    horizontalAlignmentBuilderKeepsFiveElementShapeWithStaleTangents();
    horizontalAlignmentBuilderCreatesContinuousMultiPiChain();
    horizontalAlignmentBuilderUsesAsymmetricTransitionTangents();
    horizontalAlignmentBuilderRejectsImpossibleCurve();
    alignmentElementChainBuilderSamplesOvalPartialSpiral();
    alignmentElementChainBuilderRejectsInvalidPartialSpiral();
    alignmentElementChainBuilderSamplesSCurveCurvatureTransition();
    icdAlignmentFileRoundTripsFiveElementAlignment();
    icdAlignmentFileImportsType5PartialSpiral();
    icdAlignmentFileMapsEngineeringCoordinatesToCadCoordinates();
    alignmentGripEditServiceRebuildsDraggedPiPreview();
    alignmentGripEditServiceMovesSharedControlAndPiGripOnce();
    profileDmxFileParsesStationsAndKeepsDuplicates();
    profileDmxFileRejectsTooFewValidSamples();
    profileDmxFileRejectsNonFiniteRows();
    profileDmxFileParsesTextWithLeadingBom();
    profileDmxFileParsesTextWithLeadingUtf8BomBytes();
    profileDmxFileReadsTempFile();
    profileGradeGraphLayoutMapsStationAndElevation();
    profileGradeGraphLayoutRejectsNonFiniteGeometry();
    profileGradeGraphDataDefaultsToDmxFileSource();
    profileGradeGraphLayoutMapYUsesDefaultVerticalScale();
    alignmentCommandMetadataUsesExpectedNames();

    if (g_failures != 0) {
        std::cerr << g_failures << " RoadProto core test(s) failed.\n";
        return 1;
    }

    std::cout << "All RoadProto core tests passed.\n";
    return 0;
}
