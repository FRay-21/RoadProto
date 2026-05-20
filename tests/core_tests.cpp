#include "application/cross_section/RoadModelBuildService.h"
#include "application/cross_section/SubgradeTemplateCreateService.h"
#include "application/profile/ProfileGradeGraphCreateService.h"
#include "application/profile/ProfileVerticalCurveCreateService.h"
#include "application/profile/ProfileVerticalCurveEditService.h"
#include "application/terrain/TerrainUpdateSampleService.h"
#include "app/startup/CrossSectionStartupRegistration.h"
#include "core/command/CommandRegistry.h"
#include "core/module/ModuleRegistry.h"
#include "domain/alignment/AlignmentGeometry.h"
#include "domain/alignment/AlignmentElementChainBuilder.h"
#include "domain/alignment/AlignmentGripEditService.h"
#include "domain/alignment/HorizontalAlignmentBuilder.h"
#include "domain/alignment/IcdAlignmentFile.h"
#include "domain/alignment/StationFormatter.h"
#include "domain/cross_section/PavementLayerTemplateModel.h"
#include "domain/cross_section/RoadModel.h"
#include "domain/cross_section/SlopeTemplateModel.h"
#include "domain/cross_section/SubgradeTemplateModel.h"
#include "domain/profile/ProfileDmxFile.h"
#include "domain/profile/ProfileGradeGraphLayout.h"
#include "domain/profile/ProfileVerticalCurveCalculator.h"
#include "domain/profile/ProfileVerticalCurveDisplayPlanner.h"
#include "domain/profile/ProfileVerticalCurveModel.h"
#include "domain/relation/EntityRelationManager.h"
#include "domain/terrain/TerrainPointNormalizer.h"
#include "domain/terrain/TerrainMeshFile.h"
#include "domain/terrain/TerrainSurfaceQuery.h"
#include "domain/terrain/TerrainTextElevationParser.h"
#include "domain/terrain/TerrainTriangleSpatialIndex.h"
#include "domain/terrain/TerrainTinBuilder.h"
#include "app/startup/ProfileStartupRegistration.h"
#include "modules/cross_section/CrossSectionModule.h"
#include "modules/profile/ProfileModule.h"
#include "ui/ribbon/RibbonModel.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

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

std::filesystem::path findRepositoryRootForTests()
{
    auto current = std::filesystem::current_path();
    for (int i = 0; i < 8; ++i) {
        if (std::filesystem::exists(current / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI")) {
            return current;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }

    return std::filesystem::current_path();
}

std::string readTextFileForTests(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {};
    }

    auto text = std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    std::string normalized;
    normalized.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                continue;
            }
            normalized.push_back('\n');
            continue;
        }
        normalized.push_back(text[i]);
    }
    return normalized;
}

void checkBusinessDocExistsForTests(const std::wstring& businessDocPath)
{
    CHECK(std::filesystem::exists(findRepositoryRootForTests() / std::filesystem::path(businessDocPath)));
}

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

void profileModuleRegistersCommandsAndRibbonPanel()
{
    roadproto::core::CommandRegistry commands;
    roadproto::ui::RibbonModel ribbon;

    auto module = roadproto::modules::profile::createProfileModule();
    module.registerCommands(commands);
    module.registerRibbon(ribbon);

    const auto createCommand = commands.find(L"RD_PROFILE_GRADE_GRAPH_CREATE");
    CHECK(createCommand.has_value());
    if (createCommand.has_value()) {
        CHECK(createCommand->moduleCode == L"PROFILE");
        CHECK(createCommand->displayName == L"\u7eb5\u65ad\u9762\u62c9\u5761\u56fe");
        CHECK(createCommand->businessDocPath == L"docs/business/profile/\u7eb5\u65ad\u9762\u62c9\u5761\u56fe_\u521b\u5efa.md");
        CHECK(createCommand->ribbonAttachable);
        CHECK(createCommand->isPrototype);
        CHECK(createCommand->reusable);
    }

    const auto editHandleCommand = commands.find(L"RD_PROFILE_GRADE_GRAPH_EDIT_HANDLE");
    CHECK(editHandleCommand.has_value());
    if (editHandleCommand.has_value()) {
        CHECK(editHandleCommand->moduleCode == L"PROFILE");
        CHECK(editHandleCommand->displayName == L"\u6309 handle \u7f16\u8f91\u7eb5\u65ad\u9762\u62c9\u5761\u56fe");
        CHECK(editHandleCommand->businessDocPath == L"docs/business/profile/\u7eb5\u65ad\u9762\u62c9\u5761\u56fe_\u5c5e\u6027\u7f16\u8f91.md");
        CHECK(!editHandleCommand->ribbonAttachable);
        CHECK(!editHandleCommand->reusable);
    }

    const auto applyDialogFileCommand = commands.find(L"RD_PROFILE_APPLY_DIALOG_FILE");
    CHECK(applyDialogFileCommand.has_value());
    if (applyDialogFileCommand.has_value()) {
        CHECK(applyDialogFileCommand->moduleCode == L"PROFILE");
        CHECK(applyDialogFileCommand->displayName == L"\u5e94\u7528\u7eb5\u65ad\u9762\u62c9\u5761\u56fe\u5bf9\u8bdd\u6846\u7ed3\u679c");
        CHECK(applyDialogFileCommand->businessDocPath == L"docs/business/profile/\u7eb5\u65ad\u9762\u62c9\u5761\u56fe_\u5c5e\u6027\u7f16\u8f91.md");
        CHECK(!applyDialogFileCommand->ribbonAttachable);
        CHECK(!applyDialogFileCommand->reusable);
    }

    const auto verticalCurveCreate = commands.find(L"RD_PROFILE_VERTICAL_CURVE_CREATE");
    CHECK(verticalCurveCreate.has_value());
    if (verticalCurveCreate.has_value()) {
        CHECK(verticalCurveCreate->displayName == L"\u521b\u5efa\u7ad6\u66f2\u7ebf");
        CHECK(verticalCurveCreate->moduleCode == L"PROFILE");
        CHECK(verticalCurveCreate->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u521b\u5efa.md");
        CHECK(verticalCurveCreate->ribbonAttachable);
    }

    const auto verticalCurveEdit = commands.find(L"RD_PROFILE_VERTICAL_CURVE_EDIT_HANDLE");
    CHECK(verticalCurveEdit.has_value());
    if (verticalCurveEdit.has_value()) {
        CHECK(verticalCurveEdit->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u7f16\u8f91.md");
        CHECK(!verticalCurveEdit->ribbonAttachable);
    }

    const auto verticalCurveApply = commands.find(L"RD_PROFILE_VERTICAL_CURVE_APPLY_DIALOG_FILE");
    CHECK(verticalCurveApply.has_value());
    if (verticalCurveApply.has_value()) {
        CHECK(verticalCurveApply->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u7f16\u8f91.md");
    }

    const auto verticalCurveAddPvi = commands.find(L"RD_PROFILE_VERTICAL_CURVE_ADD_PVI");
    CHECK(verticalCurveAddPvi.has_value());
    if (verticalCurveAddPvi.has_value()) {
        CHECK(verticalCurveAddPvi->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u5939\u70b9\u4e0e\u53f3\u952e\u7f16\u8f91.md");
    }

    const auto verticalCurveDeletePvi = commands.find(L"RD_PROFILE_VERTICAL_CURVE_DELETE_PVI");
    CHECK(verticalCurveDeletePvi.has_value());
    if (verticalCurveDeletePvi.has_value()) {
        CHECK(verticalCurveDeletePvi->businessDocPath == L"docs/business/profile/\u7ad6\u66f2\u7ebf_\u5939\u70b9\u4e0e\u53f3\u952e\u7f16\u8f91.md");
    }

    CHECK(ribbon.tab().panels.size() == 1);
    CHECK(ribbon.tab().panels.front().moduleCode == L"PROFILE");
    CHECK(ribbon.tab().panels.front().title == L"\u7eb5\u65ad\u9762\u8bbe\u8ba1");
}

void startupRegistrationIncludesProfileModule()
{
    roadproto::core::ModuleRegistry modules;
    roadproto::app::registerProfileModuleForStartup(modules);

    CHECK(modules.contains(L"PROFILE"));

    const auto module = modules.find(L"PROFILE");
    CHECK(module.has_value());
    if (!module.has_value()) {
        return;
    }

    roadproto::core::CommandRegistry commands;
    roadproto::ui::RibbonModel ribbon;
    module->registerCommands(commands);
    module->registerRibbon(ribbon);

    CHECK(commands.contains(L"RD_PROFILE_GRADE_GRAPH_CREATE"));
    CHECK(ribbon.tab().panels.size() == 1);
    CHECK(ribbon.tab().panels.front().moduleCode == L"PROFILE");
}

void subgradeTemplateDefaultsBuildExpressway()
{
    using namespace roadproto::domain::cross_section;

    const auto data = SubgradeTemplateDefaults::create(RoadGrade::Expressway);

    CHECK(data.properties.roadGrade == RoadGrade::Expressway);
    CHECK(data.properties.name == L"\u9ed8\u8ba4\u8def\u57fa\u6a21\u677f");
    CHECK(std::fabs(data.properties.displayScale - 10.0) < 1.0e-9);
    CHECK(data.components.size() == 10);

    std::vector<SubgradeTemplateComponent> left;
    std::vector<SubgradeTemplateComponent> right;
    for (const auto& component : data.components) {
        if (component.side == SubgradeSide::Left) {
            left.push_back(component);
        } else if (component.side == SubgradeSide::Right) {
            right.push_back(component);
        }
    }

    CHECK(left.size() == 5);
    CHECK(right.size() == 5);
    CHECK(left[0].type == SubgradeComponentType::Median);
    CHECK(std::fabs(left[0].width - 1.5) < 1.0e-9);
    CHECK(left[1].type == SubgradeComponentType::CurbStrip);
    CHECK(std::fabs(left[1].width - 0.75) < 1.0e-9);
    CHECK(left[2].type == SubgradeComponentType::TravelLane);
    CHECK(std::fabs(left[2].width - 7.5) < 1.0e-9);
    CHECK(left[3].type == SubgradeComponentType::HardShoulder);
    CHECK(std::fabs(left[3].width - 3.0) < 1.0e-9);
    CHECK(left[4].type == SubgradeComponentType::EarthShoulder);
    CHECK(std::fabs(left[4].width - 0.75) < 1.0e-9);

    CHECK(right[0].type == SubgradeComponentType::Median);
    CHECK(std::fabs(right[0].width - left[0].width) < 1.0e-9);
    CHECK(right[4].type == SubgradeComponentType::EarthShoulder);
    CHECK(std::fabs(right[4].width - left[4].width) < 1.0e-9);

    CHECK(left[0].color.r == 0);
    CHECK(left[0].color.g == 120);
    CHECK(left[0].color.b == 0);
    CHECK(left[2].color.r == 0);
    CHECK(left[2].color.g == 90);
    CHECK(left[2].color.b == 180);
    CHECK(left[4].color.r == 120);
    CHECK(left[4].color.g == 120);
    CHECK(left[4].color.b == 120);
}

void subgradeTemplateDefaultsBuildUrbanExpressway()
{
    using namespace roadproto::domain::cross_section;

    const auto data = SubgradeTemplateDefaults::create(RoadGrade::UrbanExpressway);

    CHECK(data.properties.roadGrade == RoadGrade::UrbanExpressway);
    CHECK(data.components.size() == 10);

    std::vector<SubgradeTemplateComponent> left;
    for (const auto& component : data.components) {
        if (component.side == SubgradeSide::Left) {
            left.push_back(component);
        }
    }

    CHECK(left.size() == 5);
    CHECK(left[0].type == SubgradeComponentType::Median);
    CHECK(std::fabs(left[0].width - 1.0) < 1.0e-9);
    CHECK(left[1].type == SubgradeComponentType::TravelLane);
    CHECK(std::fabs(left[1].width - 7.5) < 1.0e-9);
    CHECK(left[2].type == SubgradeComponentType::SideMedian);
    CHECK(std::fabs(left[2].width - 1.0) < 1.0e-9);
    CHECK(left[3].type == SubgradeComponentType::BikeLane);
    CHECK(std::fabs(left[3].width - 3.0) < 1.0e-9);
    CHECK(left[4].type == SubgradeComponentType::Sidewalk);
    CHECK(std::fabs(left[4].width - 4.0) < 1.0e-9);
}

std::vector<roadproto::domain::cross_section::SubgradeTemplateComponent> subgradeComponentsForSide(
    const roadproto::domain::cross_section::SubgradeTemplateData& data,
    roadproto::domain::cross_section::SubgradeSide side)
{
    std::vector<roadproto::domain::cross_section::SubgradeTemplateComponent> result;
    for (const auto& component : data.components) {
        if (component.side == side) {
            result.push_back(component);
        }
    }
    return result;
}

void checkSubgradeSideProfile(
    const std::vector<roadproto::domain::cross_section::SubgradeTemplateComponent>& components,
    const std::vector<std::pair<roadproto::domain::cross_section::SubgradeComponentType, double>>& expected)
{
    CHECK(components.size() == expected.size());
    const auto count = std::min(components.size(), expected.size());
    for (std::size_t i = 0; i < count; ++i) {
        CHECK(components[i].type == expected[i].first);
        CHECK(std::fabs(components[i].width - expected[i].second) < 1.0e-9);
    }
}

void subgradeTemplateDefaultsBuildHighwayGradesFromRoadClassProfiles()
{
    using namespace roadproto::domain::cross_section;

    const auto firstClass = SubgradeTemplateDefaults::create(RoadGrade::FirstClass);
    CHECK(firstClass.properties.roadGrade == RoadGrade::FirstClass);
    const auto firstClassLeft = subgradeComponentsForSide(firstClass, SubgradeSide::Left);
    const auto firstClassRight = subgradeComponentsForSide(firstClass, SubgradeSide::Right);
    checkSubgradeSideProfile(
        firstClassLeft,
        {
            {SubgradeComponentType::Median, 1.0},
            {SubgradeComponentType::CurbStrip, 0.5},
            {SubgradeComponentType::TravelLane, 3.75},
            {SubgradeComponentType::TravelLane, 3.75},
            {SubgradeComponentType::HardShoulder, 2.5},
            {SubgradeComponentType::EarthShoulder, 0.75},
        });
    CHECK(firstClassRight.size() == firstClassLeft.size());
    for (std::size_t i = 0; i < std::min(firstClassLeft.size(), firstClassRight.size()); ++i) {
        CHECK(firstClassRight[i].type == firstClassLeft[i].type);
        CHECK(std::fabs(firstClassRight[i].width - firstClassLeft[i].width) < 1.0e-9);
    }

    const auto secondClass = SubgradeTemplateDefaults::create(RoadGrade::SecondClass);
    CHECK(secondClass.properties.roadGrade == RoadGrade::SecondClass);
    checkSubgradeSideProfile(
        subgradeComponentsForSide(secondClass, SubgradeSide::Left),
        {
            {SubgradeComponentType::TravelLane, 3.75},
            {SubgradeComponentType::HardShoulder, 1.5},
            {SubgradeComponentType::EarthShoulder, 0.75},
        });

    const auto thirdClass = SubgradeTemplateDefaults::create(RoadGrade::ThirdClass);
    CHECK(thirdClass.properties.roadGrade == RoadGrade::ThirdClass);
    checkSubgradeSideProfile(
        subgradeComponentsForSide(thirdClass, SubgradeSide::Left),
        {
            {SubgradeComponentType::TravelLane, 3.5},
            {SubgradeComponentType::HardShoulder, 0.75},
            {SubgradeComponentType::EarthShoulder, 0.75},
        });

    const auto fourthClass = SubgradeTemplateDefaults::create(RoadGrade::FourthClass);
    CHECK(fourthClass.properties.roadGrade == RoadGrade::FourthClass);
    checkSubgradeSideProfile(
        subgradeComponentsForSide(fourthClass, SubgradeSide::Left),
        {
            {SubgradeComponentType::TravelLane, 3.0},
            {SubgradeComponentType::HardShoulder, 0.25},
            {SubgradeComponentType::EarthShoulder, 0.5},
        });
}

void subgradeTemplateDefaultsBuildUrbanRoadClassProfiles()
{
    using namespace roadproto::domain::cross_section;

    const auto arterial = SubgradeTemplateDefaults::create(RoadGrade::UrbanArterial);
    CHECK(arterial.properties.roadGrade == RoadGrade::UrbanArterial);
    checkSubgradeSideProfile(
        subgradeComponentsForSide(arterial, SubgradeSide::Left),
        {
            {SubgradeComponentType::Median, 1.5},
            {SubgradeComponentType::CurbStrip, 0.5},
            {SubgradeComponentType::TravelLane, 3.5},
            {SubgradeComponentType::TravelLane, 3.5},
            {SubgradeComponentType::SideMedian, 1.5},
            {SubgradeComponentType::BikeLane, 2.5},
            {SubgradeComponentType::Sidewalk, 3.0},
        });

    const auto subArterial = SubgradeTemplateDefaults::create(RoadGrade::UrbanSubArterial);
    CHECK(subArterial.properties.roadGrade == RoadGrade::UrbanSubArterial);
    checkSubgradeSideProfile(
        subgradeComponentsForSide(subArterial, SubgradeSide::Left),
        {
            {SubgradeComponentType::CurbStrip, 0.25},
            {SubgradeComponentType::TravelLane, 3.5},
            {SubgradeComponentType::TravelLane, 3.5},
            {SubgradeComponentType::BikeLane, 2.5},
            {SubgradeComponentType::Sidewalk, 3.0},
        });

    const auto branch = SubgradeTemplateDefaults::create(RoadGrade::UrbanBranch);
    CHECK(branch.properties.roadGrade == RoadGrade::UrbanBranch);
    checkSubgradeSideProfile(
        subgradeComponentsForSide(branch, SubgradeSide::Left),
        {
            {SubgradeComponentType::TravelLane, 3.25},
            {SubgradeComponentType::CurbStrip, 0.25},
            {SubgradeComponentType::Sidewalk, 2.0},
        });
}

void subgradeTemplateComponentDisplayNamesAreChinese()
{
    using namespace roadproto::domain::cross_section;

    CHECK(std::wstring(subgradeComponentTypeDisplayName(SubgradeComponentType::TravelLane)) == L"行车道");
    CHECK(std::wstring(subgradeComponentTypeDisplayName(SubgradeComponentType::HardShoulder)) == L"硬路肩");
    CHECK(std::wstring(subgradeComponentTypeDisplayName(SubgradeComponentType::EarthShoulder)) == L"土路肩");
}

void subgradeTemplateRulesUseWideningTableAndPavementThicknessGate()
{
    using namespace roadproto::domain::cross_section;

    SubgradeTemplateComponent component;
    component.width = 3.75;
    component.pavementLayerLinked = false;
    component.pavementLayerThickness = 0.28;
    component.wideningTable.push_back({100.0, 0.50});

    CHECK(std::fabs(SubgradeTemplateRules::widthAtStation(component, 100.0) - 4.25) < 1.0e-9);
    CHECK(std::fabs(SubgradeTemplateRules::widthAtStation(component, 110.0) - 3.75) < 1.0e-9);
    CHECK(std::fabs(SubgradeTemplateRules::effectivePavementThickness(component)) < 1.0e-9);

    component.pavementLayerLinked = true;
    CHECK(std::fabs(SubgradeTemplateRules::effectivePavementThickness(component) - 0.28) < 1.0e-9);
}

void subgradeTemplateVariableSlopeUsesOnlySlopeTable()
{
    using namespace roadproto::domain::cross_section;

    SubgradeTemplateComponent component;
    component.fixedSlope = 0.08;
    component.slopeMode = SubgradeSlopeMode::VariableByStation;
    component.variableSlopeTable.push_back({100.0, 0.02});

    CHECK(std::fabs(SubgradeTemplateRules::slopeAtStation(component, 100.0) - 0.02) < 1.0e-9);
    CHECK(std::fabs(SubgradeTemplateRules::slopeAtStation(component, 110.0)) < 1.0e-9);
}

void subgradeTemplateCreateServiceBuildsDefaultTemplate()
{
    using namespace roadproto::application::cross_section;
    using roadproto::domain::cross_section::RoadGrade;

    SubgradeTemplateCreateInput input;
    input.name = L"\u57ce\u5e02\u5feb\u901f\u8def\u6a21\u677f";
    input.displayScale = 50.0;
    input.roadGrade = RoadGrade::UrbanExpressway;

    const SubgradeTemplateCreateService service;
    const auto result = service.create(input);

    CHECK(result.succeeded);
    CHECK(result.templateData.properties.name == L"\u57ce\u5e02\u5feb\u901f\u8def\u6a21\u677f");
    CHECK(std::fabs(result.templateData.properties.displayScale - 50.0) < 1.0e-9);
    CHECK(result.templateData.properties.roadGrade == RoadGrade::UrbanExpressway);
    CHECK(result.templateData.components.size() == 10);
}

void pavementLayerTemplateRulesNormalizeThicknessAndCodes()
{
    using namespace roadproto::domain::cross_section;

    PavementLayerTemplateData data;
    data.properties.name = L"主线行车道路面结构层";
    data.properties.displayScale = 100.0;
    data.properties.previewWidth = 3.75;
    data.layers = {
        PavementLayerTemplateLayer{
            PavementLayerType::UpperSurface,
            L"4cm 改性沥青混凝土",
            true,
            0.04,
            0.0,
            0.0,
            0.0,
            0.0,
            1.0,
            1.0},
        PavementLayerTemplateLayer{
            PavementLayerType::Base,
            L"水泥稳定碎石",
            false,
            0.0,
            0.18,
            0.20,
            0.15,
            0.25,
            0.0,
            0.0},
    };

    std::wstring errorMessage;
    CHECK(PavementLayerTemplateRules::normalize(data, errorMessage));
    CHECK(std::wstring(pavementLayerTypeCode(PavementLayerType::UpperSurface)) == L"UpperSurface");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::UpperSurface)) == L"上面层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::MiddleSurface)) == L"中面层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::LowerSurface)) == L"下面层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::Base)) == L"基层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::Subbase)) == L"底基层");
    CHECK(std::wstring(pavementLayerTypeDisplayName(PavementLayerType::Cushion)) == L"垫层");
    CHECK(pavementLayerTypeFromCode(L"Base") == PavementLayerType::Base);
    CHECK(std::fabs(data.layers[0].innerThickness - 0.04) < 1.0e-9);
    CHECK(std::fabs(data.layers[0].outerThickness - 0.04) < 1.0e-9);
    CHECK(!data.layers[1].uniformThickness);
    CHECK(std::fabs(data.layers[1].innerWidening - 0.15) < 1.0e-9);
}

void pavementLayerTemplateGeometryUsesInnerOuterWithoutSlopeWidening()
{
    using namespace roadproto::domain::cross_section;

    PavementLayerTemplateData data;
    data.properties.previewWidth = 3.75;
    PavementLayerTemplateLayer layer;
    layer.type = PavementLayerType::Base;
    layer.name = L"基层";
    layer.uniformThickness = false;
    layer.innerThickness = 0.18;
    layer.outerThickness = 0.20;
    layer.innerWidening = 0.10;
    layer.outerWidening = 0.30;
    layer.innerSlope = 1.0;
    layer.outerSlope = 1.0;
    data.layers.push_back(layer);
    PavementLayerTemplateLayer secondLayer = layer;
    secondLayer.type = PavementLayerType::Subbase;
    secondLayer.name = L"底基层";
    secondLayer.innerThickness = 0.05;
    secondLayer.outerThickness = 0.06;
    secondLayer.innerWidening = 0.05;
    secondLayer.outerWidening = 0.10;
    data.layers.push_back(secondLayer);

    const auto section = PavementLayerTemplateRules::buildSection(data, 3.75, SubgradeSide::Right, 100.0, 99.925);
    CHECK(section.succeeded);
    CHECK(section.layers.size() == 2);
    CHECK(std::fabs(section.layers[0].topInner.offset - 0.0) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].topOuter.offset - 3.75) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].bottomInner.offset - -0.10) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].bottomOuter.offset - 4.05) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].bottomInner.elevation - 99.82) < 1.0e-9);
    CHECK(std::fabs(section.layers[0].bottomOuter.elevation - 99.725) < 1.0e-9);
    CHECK(std::fabs(section.layers[1].topInner.offset - -0.10) < 1.0e-9);
    CHECK(std::fabs(section.layers[1].bottomInner.offset - -0.15) < 1.0e-9);
}

void pavementLayerTemplateRulesAcceptPositiveFiniteDisplayScale()
{
    using namespace roadproto::domain::cross_section;

    auto data = PavementLayerTemplateDefaults::create();
    data.properties.displayScale = 2.0;

    std::wstring errorMessage;
    CHECK(PavementLayerTemplateRules::normalize(data, errorMessage));
    CHECK(PavementLayerTemplateRules::isSupportedDisplayScale(25.0));
    CHECK(!PavementLayerTemplateRules::isSupportedDisplayScale(0.0));
    CHECK(!PavementLayerTemplateRules::isSupportedDisplayScale(std::numeric_limits<double>::quiet_NaN()));
}

void slopeTemplateDefaultsBuildFillAndCutProfiles()
{
    using namespace roadproto::domain::cross_section;

    const auto fill = SlopeTemplateDefaults::create(SlopeTemplateKind::Fill);
    CHECK(fill.properties.kind == SlopeTemplateKind::Fill);
    CHECK(fill.properties.name == L"边坡模板1");
    CHECK(std::fabs(fill.properties.displayScale - 10.0) < 1.0e-9);
    CHECK(fill.components.size() == 5);
    CHECK(fill.components[0].type == SlopeComponentType::FillSlope);
    CHECK(fill.components[1].type == SlopeComponentType::Berm);
    CHECK(fill.components[2].type == SlopeComponentType::FillSlope);
    CHECK(fill.components[3].type == SlopeComponentType::Berm);
    CHECK(fill.components[4].type == SlopeComponentType::FillSlope);
    CHECK(fill.components[0].constraintMode == SlopeGeometryConstraintMode::SlopeAndHeight);
    CHECK(std::fabs(fill.components[0].slope - (-1.0 / 1.5)) < 1.0e-9);
    CHECK(std::fabs(fill.components[0].height - 4.0) < 1.0e-9);
    CHECK(std::fabs(fill.components[0].groundSearchHeightIncrement - 2.0) < 1.0e-9);
    CHECK(fill.components[1].constraintMode == SlopeGeometryConstraintMode::SlopeAndWidth);
    CHECK(std::fabs(fill.components[1].width - 1.0) < 1.0e-9);
    CHECK(std::fabs(fill.components[1].slope - (-0.03)) < 1.0e-9);
    CHECK(std::fabs(fill.components[1].groundSearchHeightIncrement) < 1.0e-9);

    const auto cut = SlopeTemplateDefaults::create(SlopeTemplateKind::Cut);
    CHECK(cut.properties.kind == SlopeTemplateKind::Cut);
    CHECK(cut.components.size() == 5);
    CHECK(cut.components[0].type == SlopeComponentType::CutSlope);
    CHECK(cut.components[1].type == SlopeComponentType::Berm);
    CHECK(cut.components[4].type == SlopeComponentType::CutSlope);
    CHECK(std::fabs(cut.components[0].slope - (1.0 / 1.5)) < 1.0e-9);
    CHECK(std::fabs(cut.components[0].height - 4.0) < 1.0e-9);
    CHECK(std::fabs(cut.components[0].groundSearchHeightIncrement - 2.0) < 1.0e-9);
    CHECK(std::fabs(cut.components[1].slope - 0.03) < 1.0e-9);
}

void slopeTemplateRulesResolveThreeGeometryModes()
{
    using namespace roadproto::domain::cross_section;

    SlopeTemplateComponent component;
    component.type = SlopeComponentType::FillSlope;
    component.slope = -1.0 / 1.5;
    component.height = 4.0;
    component.width = 6.0;

    component.constraintMode = SlopeGeometryConstraintMode::SlopeAndHeight;
    auto resolved = SlopeTemplateRules::resolveGeometry(component);
    CHECK(resolved.succeeded);
    CHECK(std::fabs(resolved.width - 6.0) < 1.0e-9);
    CHECK(std::fabs(resolved.height - 4.0) < 1.0e-9);
    CHECK(std::fabs(resolved.elevationDelta + 4.0) < 1.0e-9);

    component.constraintMode = SlopeGeometryConstraintMode::SlopeAndWidth;
    component.height = 123.0;
    resolved = SlopeTemplateRules::resolveGeometry(component);
    CHECK(resolved.succeeded);
    CHECK(std::fabs(resolved.height - 4.0) < 1.0e-9);
    CHECK(std::fabs(resolved.elevationDelta + 4.0) < 1.0e-9);

    component.constraintMode = SlopeGeometryConstraintMode::HeightAndWidth;
    component.slope = 0.0;
    component.height = 2.0;
    component.width = 3.0;
    resolved = SlopeTemplateRules::resolveGeometry(component);
    CHECK(resolved.succeeded);
    CHECK(std::fabs(resolved.slope - (-2.0 / 3.0)) < 1.0e-9);
    CHECK(std::fabs(resolved.elevationDelta + 2.0) < 1.0e-9);

    SlopeTemplateComponent cut = component;
    cut.type = SlopeComponentType::CutSlope;
    resolved = SlopeTemplateRules::resolveGeometry(cut);
    CHECK(resolved.succeeded);
    CHECK(std::fabs(resolved.slope - (2.0 / 3.0)) < 1.0e-9);
    CHECK(std::fabs(resolved.elevationDelta - 2.0) < 1.0e-9);
}

void slopeTemplateRulesValidateRepeatLastGroup()
{
    using namespace roadproto::domain::cross_section;

    auto data = SlopeTemplateDefaults::create(SlopeTemplateKind::Fill);
    data.properties.repeatLastGroupUntilGround = true;

    std::wstring errorMessage;
    CHECK(SlopeTemplateRules::normalize(data, errorMessage));
    const auto group = SlopeTemplateRules::lastRepeatGroup(data);
    CHECK(group.has_value());
    if (group.has_value()) {
        CHECK(group->bermIndex == 3);
        CHECK(group->slopeIndex == 4);
    }

    data.components.erase(data.components.begin() + 3);
    errorMessage.clear();
    CHECK(!SlopeTemplateRules::normalize(data, errorMessage));
    CHECK(!errorMessage.empty());
}

void slopeTemplateCodesRoundTripAndDisplayChinese()
{
    using namespace roadproto::domain::cross_section;

    CHECK(std::wstring(slopeTemplateKindCode(SlopeTemplateKind::Fill)) == L"Fill");
    CHECK(std::wstring(slopeTemplateKindCode(SlopeTemplateKind::Cut)) == L"Cut");
    CHECK(slopeTemplateKindFromCode(L"Cut") == SlopeTemplateKind::Cut);
    CHECK(std::wstring(slopeComponentTypeDisplayName(SlopeComponentType::FillSlope)) == L"填方边坡");
    CHECK(std::wstring(slopeComponentTypeDisplayName(SlopeComponentType::CutSlope)) == L"挖方边坡");
    CHECK(std::wstring(slopeComponentTypeDisplayName(SlopeComponentType::Berm)) == L"护坡道");
    CHECK(slopeComponentTypeFromCode(L"Berm") == SlopeComponentType::Berm);
    CHECK(slopeGeometryConstraintModeFromCode(L"HeightAndWidth") == SlopeGeometryConstraintMode::HeightAndWidth);
}

void roadModelTemplateResolverUsesHigherPriorityRows()
{
    using namespace roadproto::domain::cross_section;

    RoadModelTemplateAssignment low;
    low.startStation = 0.0;
    low.endStation = 100.0;
    low.templateHandle = L"LOW";
    low.templateName = L"Low priority";

    RoadModelTemplateAssignment high;
    high.startStation = 30.0;
    high.endStation = 60.0;
    high.templateHandle = L"HIGH";
    high.templateName = L"High priority";

    RoadModelTemplateResolver resolver({high, low});

    const auto* station20 = resolver.resolve(20.0);
    CHECK(station20 != nullptr);
    if (station20 != nullptr) {
        CHECK(station20->templateHandle == L"LOW");
    }

    const auto* station40 = resolver.resolve(40.0);
    CHECK(station40 != nullptr);
    if (station40 != nullptr) {
        CHECK(station40->templateHandle == L"HIGH");
    }

    RoadModelTemplateAssignment earlier;
    earlier.startStation = 0.0;
    earlier.endStation = 100.0;
    earlier.templateHandle = L"EARLIER";

    RoadModelTemplateAssignment later;
    later.startStation = 0.0;
    later.endStation = 100.0;
    later.templateHandle = L"LATER";

    RoadModelTemplateResolver overlappingResolver({earlier, later});
    const auto* overlappingStation = overlappingResolver.resolve(50.0);
    CHECK(overlappingStation != nullptr);
    if (overlappingStation != nullptr) {
        CHECK(overlappingStation->templateHandle == L"EARLIER");
    }

    CHECK(resolver.resolve(120.0) == nullptr);
    CHECK(resolver.resolve(100.0 + 1.0e-10) == nullptr);
}

void roadModelTemplateResolverRejectsInvalidRows()
{
    using namespace roadproto::domain::cross_section;

    std::wstring errorMessage;
    RoadModelTemplateAssignment reversed;
    reversed.startStation = 100.0;
    reversed.endStation = 0.0;
    reversed.templateHandle = L"TEMPLATE";
    CHECK(!RoadModelRules::validateAssignments({reversed}, errorMessage));
    CHECK(!errorMessage.empty());

    errorMessage.clear();
    RoadModelTemplateAssignment missingHandle;
    missingHandle.startStation = 0.0;
    missingHandle.endStation = 100.0;
    CHECK(!RoadModelRules::validateAssignments({missingHandle}, errorMessage));
    CHECK(!errorMessage.empty());
}

void roadModelStationSamplerIncludesIntervalTemplateAndVerticalCurveStations()
{
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData verticalCurve;
    verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 20.0, 100.0},
        {VerticalCurvePointRole::Pvi, 50.0, 105.0},
        {VerticalCurvePointRole::End, 90.0, 101.0},
    };
    verticalCurve.pvis = {
        {50.0, 105.0, 40.0, false},
    };

    RoadModelTemplateAssignment first;
    first.startStation = 0.0;
    first.endStation = 60.0;
    first.templateHandle = L"T1";

    RoadModelTemplateAssignment second;
    second.startStation = 70.0;
    second.endStation = 120.0;
    second.templateHandle = L"T2";

    const auto stations = RoadModelStationSampler::collectStations(
        0.0,
        100.0,
        verticalCurve,
        {first, second},
        25.0);

    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 20.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 44.6666666667) < 1e-6; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 45.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 50.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 51.3333333333) < 1e-6; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 55.3333333333) < 1e-6; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 60.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 70.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 90.0) < 1e-9; }) != stations.end());
    CHECK(std::none_of(stations.begin(), stations.end(), [](double station) { return station < 20.0 || station > 90.0; }));
    CHECK(std::is_sorted(stations.begin(), stations.end()));
}

void roadModelStationSamplerOnlyKeepsTemplateCoveredStations()
{
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData verticalCurve;
    verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 100.0, 101.0},
    };

    RoadModelTemplateAssignment first;
    first.startStation = 20.0;
    first.endStation = 40.0;
    first.templateHandle = L"T1";

    RoadModelTemplateAssignment second;
    second.startStation = 60.0;
    second.endStation = 80.0;
    second.templateHandle = L"T2";

    const auto stations = RoadModelStationSampler::collectStations(
        0.0,
        100.0,
        verticalCurve,
        {first, second},
        10.0);

    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 20.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 30.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 40.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 60.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 70.0) < 1e-9; }) != stations.end());
    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 80.0) < 1e-9; }) != stations.end());
    CHECK(std::none_of(stations.begin(), stations.end(), [](double station) {
        return std::fabs(station - 0.0) < 1e-9 ||
            std::fabs(station - 10.0) < 1e-9 ||
            std::fabs(station - 50.0) < 1e-9 ||
            std::fabs(station - 90.0) < 1e-9 ||
            std::fabs(station - 100.0) < 1e-9;
    }));

    const auto emptyAssignments = RoadModelStationSampler::collectStations(
        0.0,
        100.0,
        verticalCurve,
        {},
        10.0);
    CHECK(emptyAssignments.empty());

    ProfileVerticalCurveData emptyVerticalCurve;
    const auto noVerticalRange = RoadModelStationSampler::collectStations(
        0.0,
        100.0,
        emptyVerticalCurve,
        {first},
        10.0);
    CHECK(noVerticalRange.empty());
}

void roadModelStationSamplerSnapsTemplateBoundaryTolerance()
{
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    constexpr double nearStart = 69.99999995;

    ProfileVerticalCurveData verticalCurve;
    verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, nearStart, 100.0},
        {VerticalCurvePointRole::End, 120.0, 101.0},
    };

    RoadModelTemplateAssignment assignment;
    assignment.startStation = 70.0;
    assignment.endStation = 120.0;
    assignment.templateHandle = L"T1";

    const auto stations = RoadModelStationSampler::collectStations(
        nearStart,
        120.0,
        verticalCurve,
        {assignment},
        10.0);

    CHECK(std::find_if(stations.begin(), stations.end(), [](double station) { return std::fabs(station - 70.0) < 1e-12; }) != stations.end());
    CHECK(std::none_of(stations.begin(), stations.end(), [nearStart](double station) { return std::fabs(station - nearStart) < 1e-12; }));

    RoadModelTemplateResolver resolver({assignment});
    CHECK(std::all_of(stations.begin(), stations.end(), [&resolver](double station) {
        return resolver.resolve(station) != nullptr;
    }));
}

void roadModelBuilderCreatesThreeDimensionalComponentLines()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;
    lane.fixedSlope = -0.02;
    lane.color = {1, 2, 3};

    SubgradeTemplateData templateData;
    templateData.components.push_back(lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments.push_back({0.0, 20.0, L"T1", L"Template 1"});
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 102.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"T1", templateData},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    CHECK(result.sampledStations.size() == 3);
    CHECK(result.data.sampledStations == result.sampledStations);
    CHECK(result.data.componentLines.size() >= 2);
    if (!result.data.componentLines.empty()) {
        const auto& firstLine = result.data.componentLines.front();
        CHECK(firstLine.points.size() == 3);
        if (firstLine.points.size() == 3) {
            CHECK(std::fabs(firstLine.points.front().z - 100.0) < 1e-9);
            CHECK(std::fabs(firstLine.points.back().z - 102.0) < 1e-9);
        }
        CHECK(firstLine.key.templateHandle == L"T1");
        CHECK(firstLine.color.r == 1);
    }
}

void roadModelBuilderCreatesPavementLayerWireLinesForBoundSubgradeComponent()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.75;
    lane.fixedSlope = -0.02;
    lane.pavementLayerLinked = true;
    lane.pavementLayerHandle = L"PV-1";
    lane.pavementLayerName = L"行车道路面结构层";

    SubgradeTemplateData subgrade;
    subgrade.components.push_back(lane);

    PavementLayerTemplateData pavement;
    pavement.properties.name = L"行车道路面结构层";
    PavementLayerTemplateLayer upper;
    upper.type = PavementLayerType::UpperSurface;
    upper.name = L"上面层";
    upper.uniformThickness = true;
    upper.thickness = 0.04;
    pavement.layers.push_back(upper);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments = {RoadModelTemplateAssignment{0.0, 20.0, L"SG-1", L"路基模板"}};
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {RoadModelTemplateSource{L"SG-1", subgrade}};
    input.pavementLayerTemplates = {RoadModelPavementLayerTemplateSource{L"PV-1", pavement}};

    const auto result = RoadModelBuilder::build(input);
    CHECK(result.succeeded);
    CHECK(!result.data.pavementLayerLines.empty());
    CHECK(std::any_of(result.data.wireLines.begin(), result.data.wireLines.end(), [](const auto& line) {
        return line.kind == RoadModelWireLineKind::PavementLayer;
    }));
    CHECK(std::any_of(result.data.sections.begin(), result.data.sections.end(), [](const auto& section) {
        return !section.rightPavementLayerNodes.empty();
    }));
}

void roadModelBuilderKeepsPavementLayerInnerOuterSemanticOnLeftSide()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Left;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.75;
    lane.fixedSlope = -0.02;
    lane.pavementLayerLinked = true;
    lane.pavementLayerHandle = L"PV-LEFT";
    lane.pavementLayerName = L"左侧行车道路面结构层";

    SubgradeTemplateData subgrade;
    subgrade.components.push_back(lane);

    PavementLayerTemplateData pavement;
    pavement.properties.name = L"左侧行车道路面结构层";
    PavementLayerTemplateLayer layer;
    layer.type = PavementLayerType::Base;
    layer.name = L"基层";
    layer.uniformThickness = true;
    layer.thickness = 0.18;
    layer.innerWidening = 0.10;
    layer.outerWidening = 0.30;
    pavement.layers.push_back(layer);

    RoadModelBuildInput input;
    input.config.sampleInterval = 20.0;
    input.config.assignments = {RoadModelTemplateAssignment{0.0, 20.0, L"SG-LEFT", L"左侧路基模板"}};
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {RoadModelTemplateSource{L"SG-LEFT", subgrade}};
    input.pavementLayerTemplates = {RoadModelPavementLayerTemplateSource{L"PV-LEFT", pavement}};

    const auto result = RoadModelBuilder::build(input);
    CHECK(result.succeeded);
    CHECK(!result.data.sections.empty());

    const auto section = std::find_if(
        result.data.sections.begin(),
        result.data.sections.end(),
        [](const auto& candidate) {
            return !candidate.leftPavementLayerNodes.empty();
        });
    CHECK(section != result.data.sections.end());
    if (section != result.data.sections.end()) {
        const auto& nodes = section->leftPavementLayerNodes;
        CHECK(nodes.size() >= 4);
        if (nodes.size() >= 4) {
            const double innerTop = nodes[0].offset;
            const double outerTop = nodes[1].offset;
            const double innerBottom = nodes[2].offset;
            const double outerBottom = nodes[3].offset;
            CHECK(innerBottom <= innerTop + 1.0e-9);
            CHECK(std::fabs(innerBottom) <= std::fabs(outerBottom) + 1.0e-9);
            CHECK(outerBottom > outerTop);
        }
    }
}

void roadModelSectionPreviewBuilderCreatesSubgradePreviewAtStation()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;
    lane.fixedSlope = -0.02;
    lane.color = {1, 2, 3};

    SubgradeTemplateData templateData;
    templateData.components.push_back(lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments.push_back({0.0, 20.0, L"T1", L"Template 1"});
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 102.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"T1", templateData},
    };

    const auto result = RoadModelBuilder::build(input);
    CHECK(result.succeeded);

    RoadModelSectionPreviewRequest request;
    request.data = result.data;
    request.alignmentSamples = input.alignmentSamples;
    request.station = 10.0;

    const auto preview = RoadModelSectionPreviewBuilder::build(request);

    CHECK(preview.succeeded);
    CHECK(std::fabs(preview.station - 10.0) < 1e-9);
    const auto subgrade = std::find_if(
        preview.segments.begin(),
        preview.segments.end(),
        [](const RoadModelSectionPreviewSegment& segment) {
            return segment.kind == RoadModelSectionPreviewSegmentKind::Subgrade &&
                segment.points.size() == 2;
        });
    CHECK(subgrade != preview.segments.end());
    if (subgrade != preview.segments.end()) {
        CHECK(subgrade->color.r == 1);
        CHECK(std::fabs(subgrade->points[0].offset) < 1e-9);
        CHECK(std::fabs(subgrade->points[0].elevation - 101.0) < 1e-9);
        CHECK(std::fabs(subgrade->points[1].offset + 3.5) < 1e-9);
        CHECK(std::fabs(subgrade->points[1].elevation - 100.93) < 1e-7);
    }
}

void roadModelSectionPreviewBuilderAddsGroundLineFromTin()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::terrain;

    RoadModelSectionPreviewRequest request;
    request.station = 10.0;
    request.data.config.slopeConfig.leftSearchWidth = 20.0;
    request.data.config.slopeConfig.rightSearchWidth = 20.0;
    request.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    request.terrainSurface.points = {
        TinPoint{0.0, -10.0, 90.0},
        TinPoint{20.0, -10.0, 90.0},
        TinPoint{0.0, 10.0, 110.0},
        TinPoint{20.0, 10.0, 110.0},
    };
    request.terrainSurface.triangles = {
        TinTriangle{0, 1, 2},
        TinTriangle{1, 3, 2},
    };

    const auto preview = RoadModelSectionPreviewBuilder::build(request);

    CHECK(preview.succeeded);
    CHECK(preview.hasGroundLine);
    const auto ground = std::find_if(
        preview.segments.begin(),
        preview.segments.end(),
        [](const RoadModelSectionPreviewSegment& segment) {
            return segment.kind == RoadModelSectionPreviewSegmentKind::Ground;
        });
    CHECK(ground != preview.segments.end());
    if (ground != preview.segments.end()) {
        CHECK(ground->points.size() >= 2);
        CHECK(ground->points.front().offset < ground->points.back().offset);
    }
}

void roadModelBuilderStoresGroundProfileSnapshotsForSections()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;
    using namespace roadproto::domain::terrain;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 4.0;

    SubgradeTemplateData subgrade;
    subgrade.components.push_back(lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments.push_back({0.0, 20.0, L"SUB", L"Subgrade"});
    input.config.slopeConfig.leftSearchWidth = 20.0;
    input.config.slopeConfig.rightSearchWidth = 20.0;
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"SUB", subgrade},
    };
    input.terrainSurface.points = {
        TinPoint{0.0, -20.0, 90.0},
        TinPoint{20.0, -20.0, 90.0},
        TinPoint{0.0, 20.0, 110.0},
        TinPoint{20.0, 20.0, 110.0},
    };
    input.terrainSurface.triangles = {
        TinTriangle{0, 1, 2},
        TinTriangle{1, 3, 2},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    const auto section = std::find_if(
        result.data.sections.begin(),
        result.data.sections.end(),
        [](const RoadModelSection& candidate) {
            return std::fabs(candidate.station - 10.0) < 1.0e-9;
        });
    CHECK(section != result.data.sections.end());
    if (section != result.data.sections.end()) {
        CHECK(section->leftGroundProfile.size() >= 2);
        CHECK(section->rightGroundProfile.size() >= 2);
        if (!section->rightGroundProfile.empty()) {
            CHECK(section->rightGroundProfile.front().offset >= -1.0e-9);
            CHECK(section->rightGroundProfile.back().offset <= input.config.slopeConfig.rightSearchWidth + 1.0e-9);
        }
    }
}

void roadModelSectionPreviewBuilderUsesStoredGroundSnapshotWithoutTin()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;

    RoadModelSection section;
    section.station = 10.0;
    section.leftGroundProfile = {
        {0.0, 101.0},
        {8.0, 103.0},
    };
    section.rightGroundProfile = {
        {0.0, 99.0},
        {10.0, 96.0},
    };

    RoadModelSectionPreviewRequest request;
    request.station = 10.0;
    request.data.sections.push_back(section);
    request.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };

    const auto preview = RoadModelSectionPreviewBuilder::build(request);

    CHECK(preview.succeeded);
    CHECK(preview.hasGroundLine);
    const auto ground = std::find_if(
        preview.segments.begin(),
        preview.segments.end(),
        [](const RoadModelSectionPreviewSegment& segment) {
            return segment.kind == RoadModelSectionPreviewSegmentKind::Ground;
        });
    CHECK(ground != preview.segments.end());
    if (ground != preview.segments.end()) {
        CHECK(ground->points.size() == 3);
        CHECK(std::fabs(ground->points.front().offset + 10.0) < 1.0e-9);
        CHECK(std::fabs(ground->points.back().offset - 8.0) < 1.0e-9);
    }
}

void roadModelBuilderReportsProgressDuringBuild()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;
    lane.fixedSlope = -0.02;

    SubgradeTemplateData templateData;
    templateData.components.push_back(lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments.push_back({0.0, 20.0, L"T1", L"Template 1"});
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 102.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"T1", templateData},
    };

    std::vector<int> percents;
    std::vector<std::wstring> messages;
    input.progressCallback = [&](int percent, const std::wstring& message) {
        percents.push_back(percent);
        messages.push_back(message);
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    CHECK(!percents.empty());
    CHECK(percents.front() == 0);
    CHECK(percents.back() == 100);
    CHECK(std::is_sorted(percents.begin(), percents.end()));
    CHECK(std::any_of(messages.begin(), messages.end(), [](const auto& message) {
        return message.find(L"采样") != std::wstring::npos;
    }));
    CHECK(std::any_of(messages.begin(), messages.end(), [](const auto& message) {
        return message.find(L"生成") != std::wstring::npos;
    }));
}

void roadModelSlopeTemplateGroupResolverKeepsPriorityOrder()
{
    using namespace roadproto::domain::cross_section;

    RoadModelSlopeTemplateGroup low;
    low.startStation = 0.0;
    low.endStation = 100.0;
    low.templates.push_back({L"LOW_A", L"Low A"});
    low.templates.push_back({L"LOW_B", L"Low B"});

    RoadModelSlopeTemplateGroup high;
    high.startStation = 40.0;
    high.endStation = 60.0;
    high.templates.push_back({L"HIGH", L"High"});

    RoadModelSlopeTemplateGroupResolver resolver({high, low});

    const auto station50 = resolver.resolve(50.0);
    CHECK(station50.size() == 2);
    if (station50.size() == 2) {
        CHECK(station50[0]->templates[0].templateHandle == L"HIGH");
        CHECK(station50[1]->templates[0].templateHandle == L"LOW_A");
    }

    const auto station20 = resolver.resolve(20.0);
    CHECK(station20.size() == 1);
    if (station20.size() == 1) {
        CHECK(station20[0]->templates[0].templateHandle == L"LOW_A");
    }
}

void roadModelBuilderCreatesSlopeLinesFromSubgradeOuterEdge()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 4.0;

    SubgradeTemplateData subgrade;
    subgrade.components.push_back(lane);

    SlopeTemplateData slopeTemplate;
    slopeTemplate.properties.name = L"Slope";
    slopeTemplate.properties.kind = SlopeTemplateKind::Fill;
    SlopeTemplateComponent slope;
    slope.type = SlopeComponentType::FillSlope;
    slope.constraintMode = SlopeGeometryConstraintMode::SlopeAndHeight;
    slope.slope = -1.0;
    slope.height = 2.0;
    slope.color = {7, 8, 9};
    slopeTemplate.components.push_back(slope);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments.push_back({0.0, 20.0, L"SUB", L"Subgrade"});
    RoadModelSlopeTemplateGroup group;
    group.startStation = 0.0;
    group.endStation = 20.0;
    group.templates.push_back({L"SLOPE", L"Slope"});
    input.config.slopeConfig.rightGroups.push_back(group);
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"SUB", subgrade},
    };
    input.slopeTemplates = {
        {L"SLOPE", slopeTemplate},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    const auto outer = std::find_if(
        result.data.slopeLines.begin(),
        result.data.slopeLines.end(),
        [](const RoadModelSlopeComponentLine& line) {
            return line.key.templateHandle == L"SLOPE" &&
                line.key.side == SubgradeSide::Right &&
                line.key.componentType == SlopeComponentType::FillSlope &&
                line.key.componentIndex == 0 &&
                line.key.boundaryIndex == 1;
        });
    CHECK(outer != result.data.slopeLines.end());
    if (outer != result.data.slopeLines.end()) {
        CHECK(outer->color.r == 7);
        CHECK(outer->points.size() == 3);
        if (outer->points.size() == 3) {
            CHECK(std::fabs(outer->points.front().y + 6.0) < 1e-9);
            CHECK(std::fabs(outer->points.front().z - 98.0) < 1e-9);
            CHECK(std::fabs(outer->points.back().x - 20.0) < 1e-9);
        }
    }
}

void roadModelBuilderCreatesMeshWireframeFromSampledSections()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 4.0;
    lane.color = {1, 2, 3};

    SubgradeTemplateData subgrade;
    subgrade.components.push_back(lane);

    SlopeTemplateData slopeTemplate;
    slopeTemplate.properties.name = L"Slope";
    slopeTemplate.properties.kind = SlopeTemplateKind::Fill;
    SlopeTemplateComponent slope;
    slope.type = SlopeComponentType::FillSlope;
    slope.constraintMode = SlopeGeometryConstraintMode::SlopeAndHeight;
    slope.slope = -1.0;
    slope.height = 2.0;
    slope.color = {7, 8, 9};
    slopeTemplate.components.push_back(slope);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments.push_back({0.0, 20.0, L"SUB", L"Subgrade"});
    RoadModelSlopeTemplateGroup group;
    group.startStation = 0.0;
    group.endStation = 20.0;
    group.templates.push_back({L"SLOPE", L"Slope"});
    input.config.slopeConfig.rightGroups.push_back(group);
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"SUB", subgrade},
    };
    input.slopeTemplates = {
        {L"SLOPE", slopeTemplate},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    CHECK(result.data.sections.size() == result.sampledStations.size());
    CHECK(std::all_of(
        result.data.sections.begin(),
        result.data.sections.end(),
        [](const RoadModelSection& section) {
            return !section.rightNodes.empty();
        }));

    const auto sectionRib = std::find_if(
        result.data.wireLines.begin(),
        result.data.wireLines.end(),
        [](const RoadModelWireLine& line) {
            return line.kind == RoadModelWireLineKind::SectionRib &&
                line.side == SubgradeSide::Right &&
                line.points.size() == 2;
        });
    CHECK(sectionRib != result.data.wireLines.end());

    const auto outer = std::find_if(
        result.data.wireLines.begin(),
        result.data.wireLines.end(),
        [](const RoadModelWireLine& line) {
            return line.kind == RoadModelWireLineKind::OuterBoundary &&
                line.side == SubgradeSide::Right &&
                line.color.r == 7 &&
                line.color.g == 8 &&
                line.color.b == 9;
        });
    CHECK(outer != result.data.wireLines.end());

    const auto endCap = std::find_if(
        result.data.wireLines.begin(),
        result.data.wireLines.end(),
        [](const RoadModelWireLine& line) {
            return line.kind == RoadModelWireLineKind::EndCap &&
                line.side == SubgradeSide::Right;
        });
    CHECK(endCap != result.data.wireLines.end());
}

void roadModelBuilderCreatesTransitionWireLinesWhenSectionNodeCountsDiffer()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 4.0;
    lane.color = {1, 2, 3};

    SubgradeTemplateData narrow;
    narrow.components.push_back(lane);

    SubgradeTemplateComponent shoulder = lane;
    shoulder.type = SubgradeComponentType::EarthShoulder;
    shoulder.width = 2.0;
    shoulder.color = {4, 5, 6};

    SubgradeTemplateData wide;
    wide.components.push_back(lane);
    wide.components.push_back(shoulder);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments.push_back({0.0, 10.0, L"NARROW", L"Narrow"});
    input.config.assignments.push_back({10.0, 20.0, L"WIDE", L"Wide"});
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"NARROW", narrow},
        {L"WIDE", wide},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    CHECK(std::any_of(
        result.data.wireLines.begin(),
        result.data.wireLines.end(),
        [](const RoadModelWireLine& line) {
            return line.kind == RoadModelWireLineKind::Transition &&
                line.side == SubgradeSide::Right;
        }));
}

void roadModelBuilderKeepsSlopeTransitionsOutsideSubgrade()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 4.0;
    lane.color = {1, 2, 3};

    SubgradeTemplateData subgrade;
    subgrade.components.push_back(lane);

    SlopeTemplateData slopeTemplate;
    slopeTemplate.properties.name = L"Slope";
    slopeTemplate.properties.kind = SlopeTemplateKind::Fill;
    SlopeTemplateComponent slope;
    slope.type = SlopeComponentType::FillSlope;
    slope.constraintMode = SlopeGeometryConstraintMode::SlopeAndHeight;
    slope.slope = -1.0;
    slope.height = 2.0;
    slope.color = {7, 8, 9};
    slopeTemplate.components.push_back(slope);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments.push_back({0.0, 20.0, L"SUB", L"Subgrade"});
    RoadModelSlopeTemplateGroup group;
    group.startStation = 10.0;
    group.endStation = 20.0;
    group.templates.push_back({L"SLOPE", L"Slope"});
    input.config.slopeConfig.rightGroups.push_back(group);
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"SUB", subgrade},
    };
    input.slopeTemplates = {
        {L"SLOPE", slopeTemplate},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    bool hasSlopeColoredWireInsideSubgrade = false;
    for (const auto& line : result.data.wireLines) {
        if (line.color.r != 7 || line.color.g != 8 || line.color.b != 9) {
            continue;
        }

        for (const auto& point : line.points) {
            if (point.y > -lane.width + 1e-9) {
                hasSlopeColoredWireInsideSubgrade = true;
            }
        }
    }
    CHECK(!hasSlopeColoredWireInsideSubgrade);
}

void roadModelBuilderStopsSlopeAtTinGroundIntersection()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;
    using namespace roadproto::domain::terrain;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 4.0;

    SubgradeTemplateData subgrade;
    subgrade.components.push_back(lane);

    SlopeTemplateData slopeTemplate;
    slopeTemplate.properties.name = L"Slope";
    slopeTemplate.properties.kind = SlopeTemplateKind::Fill;
    slopeTemplate.properties.stopAtGround = true;
    SlopeTemplateComponent slope;
    slope.type = SlopeComponentType::FillSlope;
    slope.constraintMode = SlopeGeometryConstraintMode::SlopeAndHeight;
    slope.slope = -1.0;
    slope.height = 2.0;
    slope.color = {30, 40, 50};
    slopeTemplate.components.push_back(slope);

    RoadModelBuildInput input;
    input.config.sampleInterval = 20.0;
    input.config.assignments.push_back({0.0, 20.0, L"SUB", L"Subgrade"});
    input.config.slopeConfig.rightSearchWidth = 50.0;
    RoadModelSlopeTemplateGroup group;
    group.startStation = 0.0;
    group.endStation = 20.0;
    group.templates.push_back({L"SLOPE", L"Slope"});
    input.config.slopeConfig.rightGroups.push_back(group);
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"SUB", subgrade},
    };
    input.slopeTemplates = {
        {L"SLOPE", slopeTemplate},
    };
    input.terrainSurface.points = {
        TinPoint{0.0, 0.0, 101.0},
        TinPoint{20.0, 0.0, 101.0},
        TinPoint{0.0, -50.0, 81.0},
        TinPoint{20.0, -50.0, 81.0},
    };
    input.terrainSurface.triangles = {
        TinTriangle{0, 1, 2},
        TinTriangle{1, 3, 2},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    const auto outer = std::find_if(
        result.data.slopeLines.begin(),
        result.data.slopeLines.end(),
        [](const RoadModelSlopeComponentLine& line) {
            return line.key.templateHandle == L"SLOPE" &&
                line.key.boundaryIndex == 1;
        });
    CHECK(outer != result.data.slopeLines.end());
    if (outer != result.data.slopeLines.end()) {
        CHECK(outer->points.size() == 2);
        if (outer->points.size() == 2) {
            CHECK(std::fabs(outer->points.front().y + 5.0) < 1e-7);
            CHECK(std::fabs(outer->points.front().z - 99.0) < 1e-7);
            CHECK(std::fabs(outer->points.back().y + 5.0) < 1e-7);
            CHECK(std::fabs(outer->points.back().z - 99.0) < 1e-7);
        }
    }
}

void roadModelBuilderDoesNotConnectAcrossTemplateSwitches()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent t1Lane;
    t1Lane.side = SubgradeSide::Right;
    t1Lane.type = SubgradeComponentType::TravelLane;
    t1Lane.width = 3.5;
    t1Lane.color = {10, 0, 0};

    SubgradeTemplateComponent t2Lane = t1Lane;
    t2Lane.color = {20, 0, 0};

    SubgradeTemplateData t1;
    t1.components.push_back(t1Lane);
    SubgradeTemplateData t2;
    t2.components.push_back(t2Lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments = {
        {0.0, 10.0, L"T1", L"Template 1"},
        {10.0, 20.0, L"T2", L"Template 2"},
    };
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"T1", t1},
        {L"T2", t2},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    CHECK(result.data.componentLines.size() >= 4);

    const auto hasT1TwoPointLine = std::any_of(
        result.data.componentLines.begin(),
        result.data.componentLines.end(),
        [](const RoadModelComponentLine& line) {
            return line.key.templateHandle == L"T1" && line.points.size() == 2;
        });
    const auto hasT2TwoPointLine = std::any_of(
        result.data.componentLines.begin(),
        result.data.componentLines.end(),
        [](const RoadModelComponentLine& line) {
            return line.key.templateHandle == L"T2" && line.points.size() == 2;
        });
    CHECK(hasT1TwoPointLine);
    CHECK(hasT2TwoPointLine);
}

void roadModelBuilderDoesNotConnectAcrossTemplateGaps()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;
    lane.color = {1, 0, 0};

    SubgradeTemplateData templateData;
    templateData.components.push_back(lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments = {
        {0.0, 10.0, L"T1", L"Template 1"},
        {20.0, 30.0, L"T1", L"Template 1"},
    };
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 30.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{30.0, 0.0}, 30.0},
    };
    input.templates = {
        {L"T1", templateData},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    bool hasGapSpanningPair = false;
    for (const auto& line : result.data.componentLines) {
        if (line.key.templateHandle != L"T1") {
            continue;
        }
        for (std::size_t i = 1; i < line.points.size(); ++i) {
            const double previousX = line.points[i - 1].x;
            const double currentX = line.points[i].x;
            if (std::fabs(previousX - 10.0) < 1e-9 && std::fabs(currentX - 20.0) < 1e-9) {
                hasGapSpanningPair = true;
            }
        }
    }
    CHECK(!hasGapSpanningPair);

    bool hasGapSpanningWire = false;
    for (const auto& line : result.data.wireLines) {
        for (std::size_t i = 1; i < line.points.size(); ++i) {
            const double previousX = line.points[i - 1].x;
            const double currentX = line.points[i].x;
            if (std::fabs(previousX - 10.0) < 1e-9 && std::fabs(currentX - 20.0) < 1e-9) {
                hasGapSpanningWire = true;
            }
        }
    }
    CHECK(!hasGapSpanningWire);
}

void roadModelBuilderDoesNotMergeGapWhenBoundaryPointsCoincide()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;
    lane.color = {1, 0, 0};

    SubgradeTemplateData templateData;
    templateData.components.push_back(lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments = {
        {0.0, 10.0, L"T1", L"Template 1"},
        {20.0, 30.0, L"T1", L"Template 1"},
    };
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 30.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{10.0, 0.0}, 10.0},
        {{15.0, 0.0}, 15.0},
        {{10.0, 0.0}, 20.0},
        {{0.0, 0.0}, 30.0},
    };
    input.templates = {
        {L"T1", templateData},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    for (const auto& line : result.data.componentLines) {
        if (line.key.templateHandle == L"T1") {
            CHECK(line.points.size() == 2);
        }
    }
}

void roadModelBuilderSplitsLowerPriorityTemplateAroundOverride()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;

    SubgradeTemplateData lowTemplate;
    lane.color = {10, 0, 0};
    lowTemplate.components.push_back(lane);

    SubgradeTemplateData highTemplate;
    lane.color = {20, 0, 0};
    highTemplate.components.push_back(lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments = {
        {30.0, 60.0, L"HIGH", L"High priority"},
        {0.0, 100.0, L"LOW", L"Low priority"},
    };
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 100.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{100.0, 0.0}, 100.0},
    };
    input.templates = {
        {L"LOW", lowTemplate},
        {L"HIGH", highTemplate},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(result.succeeded);
    bool hasLowBeforeOverride = false;
    bool hasLowAfterOverride = false;
    bool hasHighOverride = false;
    bool hasLowThroughOverride = false;

    for (const auto& line : result.data.componentLines) {
        if (line.points.size() < 2) {
            continue;
        }

        double minX = line.points.front().x;
        double maxX = line.points.front().x;
        for (const auto& point : line.points) {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
        }

        if (line.key.templateHandle == L"LOW") {
            hasLowBeforeOverride = hasLowBeforeOverride ||
                (minX <= 0.0 + 1e-9 && maxX >= 30.0 - 1e-9);
            hasLowAfterOverride = hasLowAfterOverride ||
                (minX <= 60.0 + 1e-9 && maxX >= 100.0 - 1e-9);
            hasLowThroughOverride = hasLowThroughOverride ||
                (minX < 60.0 - 1e-9 && maxX > 30.0 + 1e-9);
        } else if (line.key.templateHandle == L"HIGH") {
            hasHighOverride = hasHighOverride ||
                (minX <= 30.0 + 1e-9 && maxX >= 60.0 - 1e-9);
        }

        for (std::size_t i = 1; i < line.points.size(); ++i) {
            const double previousX = line.points[i - 1].x;
            const double currentX = line.points[i].x;
            if (line.key.templateHandle == L"LOW") {
                hasLowThroughOverride = hasLowThroughOverride ||
                    (previousX < 60.0 - 1e-9 && currentX > 30.0 + 1e-9);
            }
        }
    }

    CHECK(hasLowBeforeOverride);
    CHECK(hasLowAfterOverride);
    CHECK(hasHighOverride);
    CHECK(!hasLowThroughOverride);
}

void roadModelBuilderRejectsInvalidAlignmentSamples()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;

    SubgradeTemplateData templateData;
    templateData.components.push_back(lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments = {
        {0.0, 20.0, L"T1", L"Template 1"},
    };
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.templates = {
        {L"T1", templateData},
    };

    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{10.0, 0.0}, 10.0},
        {{20.0, 0.0}, 10.0},
        {{30.0, 0.0}, 20.0},
    };
    const auto duplicateStation = RoadModelBuilder::build(input);
    CHECK(!duplicateStation.succeeded);
    CHECK(!duplicateStation.errorMessage.empty());

    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{0.0, 0.0}, 10.0},
        {{20.0, 0.0}, 20.0},
    };
    const auto zeroLengthSegment = RoadModelBuilder::build(input);
    CHECK(!zeroLengthSegment.succeeded);
    CHECK(!zeroLengthSegment.errorMessage.empty());

    input.alignmentSamples = {
        {{20.0, 0.0}, 20.0},
        {{0.0, 0.0}, 0.0},
    };
    const auto unorderedStations = RoadModelBuilder::build(input);
    CHECK(!unorderedStations.succeeded);
    CHECK(!unorderedStations.errorMessage.empty());
}

void roadModelBuilderRejectsInvalidTemplateSource()
{
    using namespace roadproto::domain::alignment;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;
    lane.height = std::numeric_limits<double>::quiet_NaN();

    SubgradeTemplateData templateData;
    templateData.components.push_back(lane);

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments = {
        {0.0, 20.0, L"T1", L"Template 1"},
    };
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 20.0, 100.0},
    };
    input.alignmentSamples = {
        {{0.0, 0.0}, 0.0},
        {{20.0, 0.0}, 20.0},
    };
    input.templates = {
        {L"T1", templateData},
    };

    const auto result = RoadModelBuilder::build(input);

    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());
}

void roadModelBuildServiceRejectsMissingHandlesAndDelegatesBuild()
{
    using namespace roadproto::application::cross_section;
    using namespace roadproto::domain::cross_section;
    using namespace roadproto::domain::profile;

    RoadModelBuildInput input;
    input.config.sampleInterval = 10.0;
    input.config.assignments = {{0.0, 10.0, L"T1", L"Template"}};

    RoadModelBuildService service;
    auto result = service.build(input);
    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());
    CHECK(result.data.config.sampleInterval == 10.0);

    input.config.roadCenterlineHandle = L"C1";
    result = service.build(input);
    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());
    CHECK(result.data.config.roadCenterlineHandle == L"C1");
    CHECK(result.data.config.profileVerticalCurveHandle.empty());

    input.config.profileVerticalCurveHandle = L"V1";
    input.alignmentSamples = {{{0.0, 0.0}, 0.0}, {{10.0, 0.0}, 10.0}};
    input.verticalCurve.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 100.0},
        {VerticalCurvePointRole::End, 10.0, 100.0},
    };
    SubgradeTemplateData data;
    SubgradeTemplateComponent lane;
    lane.side = SubgradeSide::Right;
    lane.type = SubgradeComponentType::TravelLane;
    lane.width = 3.5;
    data.components.push_back(lane);
    input.templates = {{L"T1", data}};

    result = service.build(input);
    CHECK(result.succeeded);
    CHECK(result.data.config.roadCenterlineHandle == L"C1");
    CHECK(result.data.config.profileVerticalCurveHandle == L"V1");
}

void crossSectionModuleRegistersSubgradeTemplateCommandsAndRibbonPanel()
{
    roadproto::core::CommandRegistry commands;
    roadproto::ui::RibbonModel ribbon;

    auto module = roadproto::modules::cross_section::createCrossSectionModule();
    module.registerCommands(commands);
    module.registerRibbon(ribbon);

    const auto createCommand = commands.find(L"RD_SECTION_SUBGRADE_TEMPLATE_CREATE");
    CHECK(createCommand.has_value());
    if (createCommand.has_value()) {
        CHECK(createCommand->moduleCode == L"CROSS_SECTION");
        CHECK(createCommand->displayName == L"\u521b\u5efa\u8def\u57fa\u6a21\u677f");
        CHECK(createCommand->businessDocPath == L"docs/business/cross_section/\u8def\u57fa\u6a21\u677f_\u521b\u5efa.md");
        CHECK(createCommand->ribbonAttachable);
        CHECK(createCommand->isPrototype);
        CHECK(createCommand->reusable);
    }

    const auto editCommand = commands.find(L"RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE");
    CHECK(editCommand.has_value());
    if (editCommand.has_value()) {
        CHECK(editCommand->moduleCode == L"CROSS_SECTION");
        CHECK(!editCommand->ribbonAttachable);
        CHECK(!editCommand->reusable);
    }

    const auto applyCommand = commands.find(L"RD_SECTION_SUBGRADE_TEMPLATE_APPLY_DIALOG_FILE");
    CHECK(applyCommand.has_value());
    if (applyCommand.has_value()) {
        CHECK(applyCommand->moduleCode == L"CROSS_SECTION");
        CHECK(!applyCommand->ribbonAttachable);
        CHECK(!applyCommand->reusable);
    }

    const auto slopeCreateCommand = commands.find(L"RD_SECTION_SLOPE_TEMPLATE_CREATE");
    CHECK(slopeCreateCommand.has_value());
    if (slopeCreateCommand.has_value()) {
        CHECK(slopeCreateCommand->moduleCode == L"CROSS_SECTION");
        CHECK(slopeCreateCommand->displayName == L"\u521b\u5efa\u8fb9\u5761\u6a21\u677f");
        CHECK(slopeCreateCommand->businessDocPath == L"docs/business/cross_section/\u8fb9\u5761\u6a21\u677f_\u521b\u5efa.md");
        CHECK(slopeCreateCommand->ribbonAttachable);
        CHECK(slopeCreateCommand->isPrototype);
        CHECK(slopeCreateCommand->reusable);
    }

    const auto slopeEditCommand = commands.find(L"RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE");
    CHECK(slopeEditCommand.has_value());
    if (slopeEditCommand.has_value()) {
        CHECK(slopeEditCommand->moduleCode == L"CROSS_SECTION");
        CHECK(slopeEditCommand->businessDocPath == L"docs/business/cross_section/\u8fb9\u5761\u6a21\u677f_\u7f16\u8f91.md");
        CHECK(!slopeEditCommand->ribbonAttachable);
        CHECK(!slopeEditCommand->reusable);
    }

    const auto slopeApplyCommand = commands.find(L"RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE");
    CHECK(slopeApplyCommand.has_value());
    if (slopeApplyCommand.has_value()) {
        CHECK(slopeApplyCommand->moduleCode == L"CROSS_SECTION");
        CHECK(slopeApplyCommand->businessDocPath == L"docs/business/cross_section/\u8fb9\u5761\u6a21\u677f_WPF\u6865\u63a5\u56de\u5199.md");
        CHECK(!slopeApplyCommand->ribbonAttachable);
        CHECK(!slopeApplyCommand->reusable);
    }

    const auto roadModelCreateCommand = commands.find(L"RD_SECTION_ROAD_MODEL_CREATE");
    CHECK(roadModelCreateCommand.has_value());
    if (roadModelCreateCommand.has_value()) {
        CHECK(roadModelCreateCommand->moduleCode == L"CROSS_SECTION");
        CHECK(roadModelCreateCommand->displayName == L"横断面戴帽");
        CHECK(roadModelCreateCommand->businessDocPath == L"docs/business/cross_section/横断面戴帽_道路模型创建.md");
        CHECK(roadModelCreateCommand->ribbonAttachable);
        CHECK(roadModelCreateCommand->isPrototype);
        CHECK(roadModelCreateCommand->reusable);
    }

    const auto roadModelEditCommand = commands.find(L"RD_SECTION_ROAD_MODEL_EDIT");
    CHECK(roadModelEditCommand.has_value());
    if (roadModelEditCommand.has_value()) {
        CHECK(roadModelEditCommand->moduleCode == L"CROSS_SECTION");
        CHECK(roadModelEditCommand->displayName == L"编辑道路模型");
        CHECK(roadModelEditCommand->businessDocPath == L"docs/business/cross_section/道路模型_编辑.md");
        CHECK(roadModelEditCommand->ribbonAttachable);
        CHECK(roadModelEditCommand->isPrototype);
        CHECK(roadModelEditCommand->reusable);
    }

    const auto roadModelEditHandleCommand = commands.find(L"RD_SECTION_ROAD_MODEL_EDIT_HANDLE");
    CHECK(roadModelEditHandleCommand.has_value());
    if (roadModelEditHandleCommand.has_value()) {
        CHECK(roadModelEditHandleCommand->moduleCode == L"CROSS_SECTION");
        CHECK(roadModelEditHandleCommand->businessDocPath == L"docs/business/cross_section/道路模型_编辑.md");
        CHECK(!roadModelEditHandleCommand->ribbonAttachable);
        CHECK(roadModelEditHandleCommand->isPrototype);
        CHECK(!roadModelEditHandleCommand->reusable);
    }

    const auto roadModelApplyDialogFileCommand = commands.find(L"RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE");
    CHECK(roadModelApplyDialogFileCommand.has_value());
    if (roadModelApplyDialogFileCommand.has_value()) {
        CHECK(roadModelApplyDialogFileCommand->moduleCode == L"CROSS_SECTION");
        CHECK(roadModelApplyDialogFileCommand->businessDocPath == L"docs/business/cross_section/道路模型_WPF桥接回写.md");
        CHECK(!roadModelApplyDialogFileCommand->ribbonAttachable);
        CHECK(roadModelApplyDialogFileCommand->isPrototype);
        CHECK(!roadModelApplyDialogFileCommand->reusable);
    }

    const auto sectionViewerCommand = commands.find(L"RD_SECTION_ROAD_MODEL_VIEW_SECTION");
    CHECK(sectionViewerCommand.has_value());
    if (sectionViewerCommand.has_value()) {
        CHECK(sectionViewerCommand->moduleCode == L"CROSS_SECTION");
        CHECK(sectionViewerCommand->displayName == L"查看横断面");
        CHECK(sectionViewerCommand->businessDocPath == L"docs/business/cross_section/查看横断面.md");
        CHECK(sectionViewerCommand->ribbonAttachable);
        CHECK(sectionViewerCommand->isPrototype);
        CHECK(sectionViewerCommand->reusable);
    }

    checkBusinessDocExistsForTests(L"docs/business/cross_section/横断面戴帽_道路模型创建.md");
    checkBusinessDocExistsForTests(L"docs/business/cross_section/横断面戴帽_边坡模板.md");
    checkBusinessDocExistsForTests(L"docs/business/cross_section/道路模型_编辑.md");
    checkBusinessDocExistsForTests(L"docs/business/cross_section/道路模型_WPF桥接回写.md");
    checkBusinessDocExistsForTests(L"docs/business/cross_section/边坡模板_创建.md");
    checkBusinessDocExistsForTests(L"docs/business/cross_section/边坡模板_编辑.md");
    checkBusinessDocExistsForTests(L"docs/business/cross_section/边坡模板_WPF桥接回写.md");
    checkBusinessDocExistsForTests(L"docs/business/cross_section/查看横断面.md");

    CHECK(ribbon.tab().panels.size() == 1);
    CHECK(ribbon.tab().panels.front().moduleCode == L"CROSS_SECTION");
    CHECK(ribbon.tab().panels.front().title == L"\u6a2a\u65ad\u9762\u8bbe\u8ba1");
}

void startupRegistrationIncludesCrossSectionModule()
{
    roadproto::core::ModuleRegistry modules;
    roadproto::app::registerCrossSectionModuleForStartup(modules);

    CHECK(modules.contains(L"CROSS_SECTION"));

    const auto module = modules.find(L"CROSS_SECTION");
    CHECK(module.has_value());
    if (!module.has_value()) {
        return;
    }

    roadproto::core::CommandRegistry commands;
    roadproto::ui::RibbonModel ribbon;
    module->registerCommands(commands);
    module->registerRibbon(ribbon);

    CHECK(commands.contains(L"RD_SECTION_SUBGRADE_TEMPLATE_CREATE"));
    CHECK(commands.contains(L"RD_SECTION_SLOPE_TEMPLATE_CREATE"));
    CHECK(commands.contains(L"RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE"));
    CHECK(commands.contains(L"RD_SECTION_SLOPE_TEMPLATE_APPLY_DIALOG_FILE"));
    CHECK(commands.contains(L"RD_SECTION_ROAD_MODEL_CREATE"));
    CHECK(commands.contains(L"RD_SECTION_ROAD_MODEL_EDIT"));
    CHECK(commands.contains(L"RD_SECTION_ROAD_MODEL_EDIT_HANDLE"));
    CHECK(commands.contains(L"RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE"));
    CHECK(commands.contains(L"RD_SECTION_ROAD_MODEL_VIEW_SECTION"));
    CHECK(ribbon.tab().panels.size() == 1);
    CHECK(ribbon.tab().panels.front().moduleCode == L"CROSS_SECTION");
}

void managedRibbonExtensionRegistersSubgradeTemplateEntryPoints()
{
    const auto sourcePath = findRepositoryRootForTests()
        / "src"
        / "ui"
        / "wpf"
        / "RoadProto.Terrain.UI"
        / "AutoCad"
        / "RoadProtoRibbonExtension.cs";
    CHECK(std::filesystem::exists(sourcePath));

    const auto source = readTextFileForTests(sourcePath);
    CHECK(!source.empty());
    CHECK(source.find("CrossSectionPanelId") != std::string::npos);
    CHECK(source.find("RD_SECTION_SUBGRADE_TEMPLATE_CREATE") != std::string::npos);
    CHECK(source.find("RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE") != std::string::npos);
    CHECK(source.find("RD_SECTION_SUBGRADE_TEMPLATE_EDIT_HANDLE {handle}\\n") != std::string::npos);
    CHECK(source.find("DNSUBGRADETEMPLATEENTITY") != std::string::npos);
    CHECK(source.find("SubgradeTemplateDialogCommands") != std::string::npos);
    CHECK(source.find("RD_SECTION_SLOPE_TEMPLATE_CREATE") != std::string::npos);
    CHECK(source.find("RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE") != std::string::npos);
    CHECK(source.find("RD_SECTION_SLOPE_TEMPLATE_EDIT_HANDLE {handle}\\n") != std::string::npos);
    CHECK(source.find("DNSLOPETEMPLATEENTITY") != std::string::npos);
    CHECK(source.find("SlopeTemplateDialogCommands") != std::string::npos);
    CHECK(source.find("SlopeTemplateButtonId") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_CREATE") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_EDIT") != std::string::npos);
    CHECK(source.find("RoadModelCreateButtonId") != std::string::npos);
    CHECK(source.find("RoadModelEditButtonId") != std::string::npos);
}

void managedRibbonExtensionRegistersRoadModelEntryPoints()
{
    const auto sourcePath = findRepositoryRootForTests()
        / "src"
        / "ui"
        / "wpf"
        / "RoadProto.Terrain.UI"
        / "AutoCad"
        / "RoadProtoRibbonExtension.cs";
    CHECK(std::filesystem::exists(sourcePath));

    const auto source = readTextFileForTests(sourcePath);
    CHECK(!source.empty());
    CHECK(source.find("RD_SECTION_ROAD_MODEL_CREATE") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_EDIT") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_EDIT_HANDLE") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_EDIT_HANDLE {handle}\\n") != std::string::npos);
    CHECK(source.find("RD_SECTION_ROAD_MODEL_VIEW_SECTION") != std::string::npos);
    CHECK(source.find("RoadModelSectionViewerCommands") != std::string::npos);
    CHECK(source.find("DNROADMODELENTITY") != std::string::npos);
    CHECK(source.find("RoadModelDialogCommands") != std::string::npos);
    CHECK(source.find("RoadModelCreateButtonId") != std::string::npos);
    CHECK(source.find("RoadModelEditButtonId") != std::string::npos);
    CHECK(source.find("RoadModelSectionViewerButtonId") != std::string::npos);
}

void roadModelSectionViewerNativeBridgeSourceContainsRequiredFields()
{
    const auto root = findRepositoryRootForTests();
    const auto bridgeHeaderPath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "RoadModelSectionViewerBridge.h";
    const auto bridgeSourcePath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "RoadModelSectionViewerBridge.cpp";
    const auto commandHeaderPath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "ObjectArxRoadModelCommand.h";
    const auto commandSourcePath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "ObjectArxRoadModelCommand.cpp";

    CHECK(std::filesystem::exists(bridgeHeaderPath));
    CHECK(std::filesystem::exists(bridgeSourcePath));
    CHECK(std::filesystem::exists(commandHeaderPath));
    CHECK(std::filesystem::exists(commandSourcePath));

    const auto header = readTextFileForTests(bridgeHeaderPath);
    const auto bridge = readTextFileForTests(bridgeSourcePath);
    const auto commandHeader = readTextFileForTests(commandHeaderPath);
    const auto command = readTextFileForTests(commandSourcePath);

    CHECK(header.find("RoadModelSectionViewerRequest") != std::string::npos);
    CHECK(header.find("RoadModelSectionViewerPreview") != std::string::npos);
    CHECK(header.find("queueRoadModelSectionViewerWpfDialog") != std::string::npos);
    CHECK(bridge.find("RoadProtoRoadModelSectionViewer_") != std::string::npos);
    CHECK(bridge.find("RD_SECTION_ROAD_MODEL_VIEW_SECTION_SHOW_WPF_DIALOG") != std::string::npos);
    CHECK(bridge.find("previewCount") != std::string::npos);
    CHECK(bridge.find("segmentCount") != std::string::npos);
    CHECK(bridge.find("pointCount") != std::string::npos);
    CHECK(bridge.find("stationLabel") != std::string::npos);
    CHECK(commandHeader.find("roadModelViewSectionCommandProcedure") != std::string::npos);
    CHECK(command.find("RoadModelSectionViewerBridge.h") != std::string::npos);
    CHECK(command.find("runRoadModelViewSectionCommand") != std::string::npos);
    CHECK(command.find("selectTypedEntity<DnRoadModelEntity>") != std::string::npos);
    CHECK(command.find("needsTerrainSurfaceForSectionPreview") != std::string::npos);
    CHECK(command.find("hasUsableGroundSnapshot") != std::string::npos);
    CHECK(command.find("RoadModelSectionPreviewRequest previewRequest") != std::string::npos);
    CHECK(command.find("RoadModelSectionPreviewBuilder::build") != std::string::npos);
    CHECK(command.find("queueRoadModelSectionViewerWpfDialog") != std::string::npos);
}

void roadModelWpfBridgeSourceContainsRequiredFields()
{
    const auto root = findRepositoryRootForTests();
    const auto uiRoot = root / "src" / "ui" / "wpf" / "RoadProto.Terrain.UI";
    const auto dtoPath = uiRoot / "Bridge" / "RoadModelDialogDtos.cs";
    const auto filePath = uiRoot / "Bridge" / "RoadModelDialogFile.cs";
    const auto xamlPath = uiRoot / "RoadModelWindow.xaml";
    const auto codePath = uiRoot / "RoadModelWindow.xaml.cs";

    CHECK(std::filesystem::exists(dtoPath));
    CHECK(std::filesystem::exists(filePath));
    CHECK(std::filesystem::exists(xamlPath));
    CHECK(std::filesystem::exists(codePath));

    const auto dto = readTextFileForTests(dtoPath);
    const auto file = readTextFileForTests(filePath);
    const auto xaml = readTextFileForTests(xamlPath);
    const auto code = readTextFileForTests(codePath);

    CHECK(dto.find("RoadCenterlineHandle") != std::string::npos);
    CHECK(dto.find("ProfileVerticalCurveHandle") != std::string::npos);
    CHECK(dto.find("SampleInterval") != std::string::npos);
    CHECK(dto.find("RoadModelTemplateAssignmentDto") != std::string::npos);
    CHECK(dto.find("RoadModelDialogAction") != std::string::npos);
    CHECK(dto.find("PickTemplate") != std::string::npos);
    CHECK(dto.find("PickLeftSlopeTemplate") != std::string::npos);
    CHECK(dto.find("PickRightSlopeTemplate") != std::string::npos);
    CHECK(dto.find("PickAssignmentIndex") != std::string::npos);
    CHECK(dto.find("SelectedAssignmentIndex") != std::string::npos);
    CHECK(dto.find("RoadModelSlopeTemplateGroupDto") != std::string::npos);
    CHECK(dto.find("LeftSlopeSearchWidth") != std::string::npos);
    CHECK(dto.find("RightSlopeSearchWidth") != std::string::npos);
    CHECK(dto.find("LeftSlopeGroups") != std::string::npos);
    CHECK(dto.find("RightSlopeGroups") != std::string::npos);

    CHECK(file.find("action") != std::string::npos);
    CHECK(file.find("pickTemplate") != std::string::npos);
    CHECK(file.find("pickLeftSlopeTemplate") != std::string::npos);
    CHECK(file.find("pickRightSlopeTemplate") != std::string::npos);
    CHECK(file.find("pickAssignmentIndex") != std::string::npos);
    CHECK(file.find("pickSlopeGroupIndex") != std::string::npos);
    CHECK(file.find("selectedAssignmentIndex") != std::string::npos);
    CHECK(file.find("assignmentCount") != std::string::npos);
    CHECK(file.find("assignment.{i}.startStation") != std::string::npos);
    CHECK(file.find("leftSlopeGroup") != std::string::npos);
    CHECK(file.find("rightSlopeGroup") != std::string::npos);
    CHECK(file.find("RD_SECTION_ROAD_MODEL_APPLY_DIALOG_FILE") == std::string::npos);

    CHECK(xaml.find("横断面戴帽") != std::string::npos);
    CHECK(xaml.find("路基模板") != std::string::npos);
    CHECK(xaml.find("边坡模板") != std::string::npos);
    CHECK(xaml.find("左侧边坡模板") != std::string::npos);
    CHECK(xaml.find("右侧边坡模板") != std::string::npos);
    CHECK(xaml.find("DataGrid") != std::string::npos);
    CHECK(xaml.find("Header=\"点选模板\"") != std::string::npos);
    CHECK(xaml.find("生成模型") != std::string::npos);
    CHECK(xaml.find("Header=\"管理模板组\"") != std::string::npos);
    CHECK(xaml.find("OnManageLeftSlopeGroup") != std::string::npos);
    CHECK(xaml.find("OnManageRightSlopeGroup") != std::string::npos);
    CHECK(xaml.find("当前模板组管理") != std::string::npos);
    CHECK(xaml.find("组内模板") != std::string::npos);
    CHECK(xaml.find("OnDeleteLeftSlopeTemplate") != std::string::npos);
    CHECK(xaml.find("OnMoveLeftSlopeTemplateUp") != std::string::npos);

    CHECK(code.find("MoveAssignment") != std::string::npos);
    CHECK(code.find("BuildResponse") != std::string::npos);
    CHECK(code.find("OnPickTemplate") != std::string::npos);
    CHECK(code.find("OnPickLeftSlopeTemplate") != std::string::npos);
    CHECK(code.find("OnPickRightSlopeTemplate") != std::string::npos);
    CHECK(code.find("RoadModelDialogAction.PickTemplate") != std::string::npos);
    CHECK(code.find("RoadModelDialogAction.PickLeftSlopeTemplate") != std::string::npos);
    CHECK(code.find("RoadModelDialogAction.PickRightSlopeTemplate") != std::string::npos);
    CHECK(code.find("PickAssignmentIndex") != std::string::npos);
    CHECK(code.find("PickSlopeGroupIndex") != std::string::npos);
    CHECK(code.find("SelectedLeftSlopeTemplate") != std::string::npos);
    CHECK(code.find("SelectedRightSlopeTemplate") != std::string::npos);
    CHECK(code.find("OnManageLeftSlopeGroup") != std::string::npos);
    CHECK(code.find("OnManageRightSlopeGroup") != std::string::npos);
    CHECK(code.find("DeleteSlopeTemplate") != std::string::npos);
    CHECK(code.find("MoveSlopeTemplate") != std::string::npos);
}

void roadModelNativeDialogBridgeSourceContainsRequiredFields()
{
    const auto root = findRepositoryRootForTests();
    const auto bridgeHeaderPath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "RoadModelDialogBridge.h";
    const auto bridgeSourcePath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "RoadModelDialogBridge.cpp";
    const auto commandSourcePath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "ObjectArxRoadModelCommand.cpp";

    CHECK(std::filesystem::exists(bridgeHeaderPath));
    CHECK(std::filesystem::exists(bridgeSourcePath));
    CHECK(std::filesystem::exists(commandSourcePath));

    const auto header = readTextFileForTests(bridgeHeaderPath);
    const auto bridge = readTextFileForTests(bridgeSourcePath);
    const auto command = readTextFileForTests(commandSourcePath);

    CHECK(header.find("RoadModelDialogRequest") != std::string::npos);
    CHECK(header.find("RoadModelDialogResponse") != std::string::npos);
    CHECK(header.find("RoadModelDialogAction") != std::string::npos);
    CHECK(header.find("PickTemplate") != std::string::npos);
    CHECK(header.find("PickLeftSlopeTemplate") != std::string::npos);
    CHECK(header.find("PickRightSlopeTemplate") != std::string::npos);
    CHECK(header.find("roadCenterlineHandle") != std::string::npos);
    CHECK(header.find("profileVerticalCurveHandle") != std::string::npos);
    CHECK(header.find("sampleInterval") != std::string::npos);
    CHECK(header.find("selectedAssignmentIndex") != std::string::npos);
    CHECK(header.find("pickAssignmentIndex") != std::string::npos);
    CHECK(header.find("pickSlopeGroupIndex") != std::string::npos);
    CHECK(header.find("leftSlopeSearchWidth") != std::string::npos);
    CHECK(header.find("rightSlopeSearchWidth") != std::string::npos);
    CHECK(header.find("leftSlopeGroups") != std::string::npos);
    CHECK(header.find("rightSlopeGroups") != std::string::npos);
    CHECK(header.find("assignments") != std::string::npos);
    CHECK(header.find("queueRoadModelWpfDialog") != std::string::npos);
    CHECK(header.find("readRoadModelDialogResponse") != std::string::npos);

    CHECK(bridge.find("RoadProtoRoadModelDialog_") != std::string::npos);
    CHECK(bridge.find("RD_SECTION_ROAD_MODEL_SHOW_WPF_DIALOG") != std::string::npos);
    CHECK(bridge.find("selectedAssignmentIndex") != std::string::npos);
    CHECK(bridge.find("pickAssignmentIndex") != std::string::npos);
    CHECK(bridge.find("pickTemplate") != std::string::npos);
    CHECK(bridge.find("pickLeftSlopeTemplate") != std::string::npos);
    CHECK(bridge.find("pickRightSlopeTemplate") != std::string::npos);
    CHECK(bridge.find("pickSlopeGroupIndex") != std::string::npos);
    CHECK(bridge.find("assignmentCount") != std::string::npos);
    CHECK(bridge.find("leftSlopeGroup") != std::string::npos);
    CHECK(bridge.find("rightSlopeGroup") != std::string::npos);
    CHECK(bridge.find("prefix + L\"Count\"") != std::string::npos);
    CHECK(bridge.find("assignment.\" + std::to_wstring(i)") != std::string::npos);
    CHECK(bridge.find(".startStation") != std::string::npos);
    CHECK(bridge.find(".endStation") != std::string::npos);
    CHECK(bridge.find(".templateHandle") != std::string::npos);
    CHECK(bridge.find(".templateName") != std::string::npos);
    CHECK(bridge.find("kMaxDialogAssignments") != std::string::npos);
    CHECK(bridge.find("std::isfinite") != std::string::npos);

    CHECK(command.find("RoadModelDialogBridge.h") != std::string::npos);
    CHECK(command.find("queueRoadModelWpfDialog") != std::string::npos);
    CHECK(command.find("readRoadModelDialogResponse") != std::string::npos);
    CHECK(command.find("runRoadModelCreateCommand") != std::string::npos);
    CHECK(command.find("runRoadModelEditCommand") != std::string::npos);
    CHECK(command.find("runRoadModelEditHandleCommand") != std::string::npos);
    CHECK(command.find("runRoadModelApplyDialogFileCommand") != std::string::npos);
    CHECK(command.find("handlePickTemplateAction") != std::string::npos);
    CHECK(command.find("handlePickSlopeTemplateAction") != std::string::npos);
    CHECK(command.find("selectTypedEntity<DnSubgradeTemplateEntity>") != std::string::npos);
    CHECK(command.find("selectTypedEntity<DnSlopeTemplateEntity>") != std::string::npos);
}

void roadModelCommandSourceContainsCompleteObjectArxFlow()
{
    const auto sourcePath = findRepositoryRootForTests()
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "ObjectArxRoadModelCommand.cpp";
    CHECK(std::filesystem::exists(sourcePath));

    const auto source = readTextFileForTests(sourcePath);
    CHECK(!source.empty());

    CHECK(source.find("RoadModelBuildService") != std::string::npos);
    CHECK(source.find("DnRoadCenterlineEntity") != std::string::npos);
    CHECK(source.find("DnProfileVerticalCurveEntity") != std::string::npos);
    CHECK(source.find("DnSubgradeTemplateEntity") != std::string::npos);
    CHECK(source.find("DnSlopeTemplateEntity") != std::string::npos);
    CHECK(source.find("DnTerrainTinEntity") != std::string::npos);
    CHECK(source.find("DnRoadModelEntity") != std::string::npos);
    CHECK(source.find("selectTypedEntity") != std::string::npos);
    CHECK(source.find("findUniqueVerticalCurveForCenterline") != std::string::npos);
    CHECK(source.find("profileGraphBelongsToCenterline") != std::string::npos
        || source.find("verticalCurveBelongsToCenterline") != std::string::npos);
    CHECK(source.find("RoadModelBuildInput") != std::string::npos);
    CHECK(source.find("StatusProgressMeter") != std::string::npos);
    CHECK(source.find("acedSetStatusBarProgressMeter") != std::string::npos);
    CHECK(source.find("progressCallback") != std::string::npos);
    CHECK(source.find("readSubgradeTemplate") != std::string::npos);
    CHECK(source.find("readSlopeTemplate") != std::string::npos);
    CHECK(source.find("readTerrainSurface") != std::string::npos);
    CHECK(source.find("appendEntityToModelSpace") != std::string::npos);
    CHECK(source.find("setRoadModelData") != std::string::npos);
    CHECK(source.find("recordGraphicsModified") != std::string::npos);
    CHECK(source.find("acedUpdateDisplay") != std::string::npos);

    const auto createCommand = source.find("void runRoadModelCreateCommand()");
    const auto editCommand = source.find("void runRoadModelEditCommand()");
    const auto editHandleCommand = source.find("void runRoadModelEditHandleCommand()");
    const auto applyCommand = source.find("void runRoadModelApplyDialogFileCommand()");
    CHECK(createCommand != std::string::npos);
    CHECK(editCommand != std::string::npos);
    CHECK(editHandleCommand != std::string::npos);
    CHECK(applyCommand != std::string::npos);

    CHECK(source.find("selectTypedEntity<DnRoadCenterlineEntity>", createCommand) != std::string::npos);
    CHECK(source.find("readRoadCenterline", createCommand) != std::string::npos);
    CHECK(source.find("findUniqueVerticalCurveForCenterline", createCommand) != std::string::npos);
    CHECK(source.find("selectTypedEntity<DnProfileVerticalCurveEntity>", createCommand) != std::string::npos);
    CHECK(source.find("queueRoadModelWpfDialog", createCommand) != std::string::npos);
    const auto createRelationValidation = source.find("profileGraphBelongsToCenterline(verticalCurve.profileGraphHandle, centerlineHandle)", createCommand);
    CHECK(createRelationValidation != std::string::npos);
    CHECK(createRelationValidation < source.find("queueRoadModelWpfDialog", createCommand));
    CHECK(source.find("return;", createRelationValidation) < source.find("queueRoadModelWpfDialog", createCommand));

    CHECK(source.find("selectTypedEntity<DnRoadModelEntity>", editCommand) != std::string::npos);
    CHECK(source.find("roadModelData().config", editCommand) != std::string::npos
        || source.find("roadModelData().config", source.find("queueDialogForRoadModelEdit")) != std::string::npos);
    CHECK(source.find("resolveObjectIdFromHandle", editHandleCommand) != std::string::npos);
    CHECK(source.find("isKindOf(DnRoadModelEntity::desc())", editHandleCommand) != std::string::npos
        || source.find("isKindOf(DnRoadModelEntity::desc())", source.find("queueDialogForRoadModelEdit")) != std::string::npos);

    CHECK(source.find("readRoadModelDialogResponse", applyCommand) != std::string::npos);
    CHECK(source.find("RoadModelDialogAction::PickTemplate", applyCommand) != std::string::npos);
    CHECK(source.find("RoadModelDialogAction::PickLeftSlopeTemplate", applyCommand) != std::string::npos);
    CHECK(source.find("RoadModelDialogAction::PickRightSlopeTemplate", applyCommand) != std::string::npos);
    CHECK(source.find("handlePickTemplateAction", applyCommand) != std::string::npos);
    CHECK(source.find("handlePickSlopeTemplateAction", applyCommand) != std::string::npos);
    CHECK(source.find("resolveObjectIdFromHandle(response.roadCenterlineHandle", applyCommand) != std::string::npos);
    CHECK(source.find("resolveObjectIdFromHandle(response.profileVerticalCurveHandle", applyCommand) != std::string::npos);
    const auto applyRelationValidation = source.find("profileGraphBelongsToCenterline(verticalCurve.profileGraphHandle, centerlineHandle)", applyCommand);
    const auto applyBuild = source.find("service.build(input)", applyCommand);
    CHECK(applyRelationValidation != std::string::npos);
    CHECK(applyBuild != std::string::npos);
    CHECK(applyRelationValidation < applyBuild);
    CHECK(source.find("return;", applyRelationValidation) < applyBuild);
    CHECK(source.find("readSubgradeTemplate", applyCommand) != std::string::npos);
    CHECK(source.find("readSlopeTemplate", applyCommand) != std::string::npos);
    CHECK(source.find("terrainSurface", applyCommand) != std::string::npos);
    CHECK(source.find("RoadModelBuildInput input", applyCommand) != std::string::npos);
    CHECK(source.find("service.build(input)", applyCommand) != std::string::npos);
    CHECK(source.find("new DnRoadModelEntity", applyCommand) != std::string::npos);
    CHECK(source.find("appendEntityToModelSpace", applyCommand) != std::string::npos);
    CHECK(source.find("AcDb::kForWrite", applyCommand) != std::string::npos);
    CHECK(source.find("recordGraphicsModified(true)", applyCommand) != std::string::npos);
    CHECK(source.find("acedUpdateDisplay()", applyCommand) != std::string::npos);
}

void subgradeTemplateEntitySourceContainsMoveGrip()
{
    const auto root = findRepositoryRootForTests();
    const auto headerPath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "DnSubgradeTemplateEntity.h";
    const auto sourcePath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "DnSubgradeTemplateEntity.cpp";
    CHECK(std::filesystem::exists(headerPath));
    CHECK(std::filesystem::exists(sourcePath));

    const auto header = readTextFileForTests(headerPath);
    const auto source = readTextFileForTests(sourcePath);
    CHECK(header.find("subGetGripPoints") != std::string::npos);
    CHECK(header.find("subMoveGripPointsAt") != std::string::npos);
    CHECK(source.find("gripPoints.append(insertionPoint_)") != std::string::npos);
    CHECK(source.find("insertionPoint_ += offset") != std::string::npos);
    CHECK(source.find("recordGraphicsModified(true)") != std::string::npos);
}

void roadModelEntitySourceContainsRequiredObjectArxContracts()
{
    const auto root = findRepositoryRootForTests();
    const auto headerPath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "DnRoadModelEntity.h";
    const auto sourcePath = root
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "DnRoadModelEntity.cpp";
    const auto entryPath = root
        / "src"
        / "app"
        / "arx_entry"
        / "RoadProtoArxEntry.cpp";
    const auto projectPath = root / "src" / "app" / "RoadProtoArx.vcxproj";

    CHECK(std::filesystem::exists(headerPath));
    CHECK(std::filesystem::exists(sourcePath));
    CHECK(std::filesystem::exists(entryPath));
    CHECK(std::filesystem::exists(projectPath));

    const auto header = readTextFileForTests(headerPath);
    const auto source = readTextFileForTests(sourcePath);
    const auto entry = readTextFileForTests(entryPath);
    const auto project = readTextFileForTests(projectPath);

    CHECK(header.find("class DnRoadModelEntity : public AcDbEntity") != std::string::npos);
    CHECK(header.find("ACRX_DECLARE_MEMBERS(DnRoadModelEntity)") != std::string::npos);
    CHECK(header.find("RoadModelData data_") != std::string::npos);
    CHECK(header.find("setRoadModelData") != std::string::npos);
    CHECK(header.find("roadModelData") != std::string::npos);
    CHECK(header.find("dwgInFields") != std::string::npos);
    CHECK(header.find("dwgOutFields") != std::string::npos);
    CHECK(header.find("subWorldDraw") != std::string::npos);
    CHECK(header.find("subGetGeomExtents") != std::string::npos);
    CHECK(header.find("subTransformBy") != std::string::npos);
    CHECK(header.find("initializeRoadModelEntityClass") != std::string::npos);
    CHECK(header.find("uninitializeRoadModelEntityClass") != std::string::npos);

    CHECK(source.find("ACRX_DXF_DEFINE_MEMBERS") != std::string::npos);
    CHECK(source.find("DNROADMODELENTITY") != std::string::npos);
    CHECK(source.find("RoadModelData") != std::string::npos);
    CHECK(source.find("componentLines") != std::string::npos);
    CHECK(source.find("sections") != std::string::npos);
    CHECK(source.find("wireLines") != std::string::npos);
    CHECK(source.find("RoadModelGroundProfilePoint") != std::string::npos);
    CHECK(source.find("RoadModelWireLineKind") != std::string::npos);
    CHECK(source.find("constexpr Adesk::Int16 kEntityVersion = 5") != std::string::npos);
    CHECK(source.find("readRoadModelSection") != std::string::npos);
    CHECK(source.find("writeRoadModelSection") != std::string::npos);
    CHECK(source.find("readRoadModelGroundProfile") != std::string::npos);
    CHECK(source.find("writeRoadModelGroundProfile") != std::string::npos);
    CHECK(source.find("readRoadModelWireLine") != std::string::npos);
    CHECK(source.find("writeRoadModelWireLine") != std::string::npos);
    CHECK(source.find("drawRoadModelWireLines") != std::string::npos);
    CHECK(source.find("if (!data_.wireLines.empty())") != std::string::npos);
    CHECK(source.find("subWorldDraw") != std::string::npos);
    CHECK(source.find("subGetGeomExtents") != std::string::npos);
    CHECK(source.find("subTransformBy") != std::string::npos);
    CHECK(source.find("initializeRoadModelEntityClass") != std::string::npos);
    CHECK(source.find("uninitializeRoadModelEntityClass") != std::string::npos);
    CHECK(source.find("readWideString") != std::string::npos);
    CHECK(source.find("acutDelString") != std::string::npos);
    CHECK(source.find("eInvalidInput") != std::string::npos);
    CHECK(source.find("eMakeMeProxy") != std::string::npos);
    CHECK(source.find("recordGraphicsModified") != std::string::npos);
    CHECK(source.find("setTrueColor") != std::string::npos);
    CHECK(source.find("isFiniteRoadModelPoint") != std::string::npos);
    CHECK(source.find("validateRoadModelDataForPersistence") != std::string::npos);
    CHECK(source.find("isValidSubgradeSideValue") != std::string::npos);
    CHECK(source.find("isValidSubgradeComponentTypeValue") != std::string::npos);
    CHECK(source.find("std::isfinite(data.config.sampleInterval)") != std::string::npos);
    CHECK(source.find("std::isfinite(row.startStation)") != std::string::npos);
    CHECK(source.find("std::isfinite(row.endStation)") != std::string::npos);
    CHECK(source.find("version < 0") != std::string::npos);
    CHECK(source.find("!isValidSubgradeSideValue(side)") != std::string::npos);
    CHECK(source.find("!isValidSubgradeComponentTypeValue(type)") != std::string::npos);
    CHECK(source.find("if (!isFiniteRoadModelPoint(points[i - 1])") != std::string::npos);
    CHECK(source.find("if (!isFiniteRoadModelPoint(point))") != std::string::npos);
    CHECK(source.find("if (!isFiniteRoadModelPoint(point))") != source.rfind("if (!isFiniteRoadModelPoint(point))"));
    CHECK(source.find("transformedPointIsFinite") != std::string::npos);
    CHECK(source.find("validateAllRoadModelPointsFinite") != std::string::npos);

    const auto finalFilerStatus = source.find("const auto finalStatus = filer->filerStatus();");
    const auto roadModelDataAssignment = source.find("data_ = std::move(readData);");
    CHECK(finalFilerStatus != std::string::npos);
    CHECK(roadModelDataAssignment != std::string::npos);
    CHECK(finalFilerStatus < roadModelDataAssignment);
    CHECK(source.find("if (finalStatus != Acad::eOk)", finalFilerStatus) != std::string::npos);
    CHECK(source.find("return finalStatus;", roadModelDataAssignment) != std::string::npos);

    const auto transformFunction = source.find("Acad::ErrorStatus DnRoadModelEntity::subTransformBy");
    const auto transformExistingValidation = source.find("if (!validateAllRoadModelPointsFinite(data_))", transformFunction);
    const auto transformCopy = source.find("auto transformedData = data_;", transformFunction);
    const auto transformCommit = source.find("data_ = std::move(transformedData);", transformFunction);
    CHECK(transformFunction != std::string::npos);
    CHECK(transformExistingValidation != std::string::npos);
    CHECK(transformCopy != std::string::npos);
    CHECK(transformCommit != std::string::npos);
    CHECK(transformExistingValidation < transformCopy);
    CHECK(transformCopy < transformCommit);
    CHECK(source.find("continue;", transformFunction) == std::string::npos
        || source.find("continue;", transformFunction) > transformCommit);

    CHECK(entry.find("DnRoadModelEntity.h") != std::string::npos);
    CHECK(entry.find("initializeRoadModelEntityClass") != std::string::npos);
    CHECK(entry.find("uninitializeRoadModelEntityClass") != std::string::npos);
    CHECK(entry.find("uninitializeCustomEntityClasses") != std::string::npos);
    CHECK(entry.find("if (!app::initialize(g_editor))") != std::string::npos);
    CHECK(entry.find("uninitializeCustomEntityClasses();\n            return AcRx::kRetError;") != std::string::npos);
    CHECK(entry.find("if (!commandsRegistered)") != std::string::npos);
    CHECK(project.find("DnRoadModelEntity.cpp") != std::string::npos);
}

void subgradeTemplateWindowSourceKeepsControlsReadable()
{
    const auto root = findRepositoryRootForTests();
    const auto xamlPath = root
        / "src"
        / "ui"
        / "wpf"
        / "RoadProto.Terrain.UI"
        / "SubgradeTemplateWindow.xaml";
    const auto codePath = xamlPath;
    auto codeSourcePath = codePath;
    codeSourcePath += ".cs";

    const auto xaml = readTextFileForTests(xamlPath);
    const auto source = readTextFileForTests(codeSourcePath);

    CHECK(!xaml.empty());
    CHECK(!source.empty());
    CHECK(xaml.find("ClipToBounds=\"True\"") != std::string::npos);
    CHECK(xaml.find("坡度变化数据表") != std::string::npos);
    CHECK(source.find("CreateModeDefaultRoadGrade") != std::string::npos);
    CHECK(source.find("private bool IsCreateMode") != std::string::npos);
    CHECK(source.find("IsCreateMode ? CreateModeDefaultRoadGrade : _request.RoadGrade") != std::string::npos);
    CHECK(source.find("!IsCreateMode && RequestHasPersistedComponents") != std::string::npos);
    CHECK(source.find("BuildDefaults(initialRoadGrade)") != std::string::npos);
    CHECK(source.find("RequestHasPersistedComponents") != std::string::npos);
    CHECK(source.find("MoveSelectionToward") != std::string::npos);
    CHECK(source.find("PreviewComponentHitTarget") != std::string::npos);
    CHECK(source.find("IsHitTestVisible = false") != std::string::npos);
    CHECK(source.find("UpdateSlopeModeInputState") != std::string::npos);
    CHECK(source.find("WidthText") != std::string::npos);
    CHECK(source.find("SlopeText") != std::string::npos);
    CHECK(source.find("SubgradeRoadGrade.FirstClass") != std::string::npos);
    CHECK(source.find("SubgradeRoadGrade.UrbanArterial") != std::string::npos);
    CHECK(source.find("SubgradeRoadGrade.UrbanBranch") != std::string::npos);
}

void subgradeTemplateBridgeWritesEnumCodesAsText()
{
    const auto sourcePath = findRepositoryRootForTests()
        / "src"
        / "cad_adapter"
        / "objectarx"
        / "cross_section"
        / "SubgradeTemplateDialogBridge.cpp";
    CHECK(std::filesystem::exists(sourcePath));

    const auto source = readTextFileForTests(sourcePath);
    CHECK(!source.empty());
    CHECK(source.find("void writeKeyValue(std::ostream& stream, const std::wstring& key, const wchar_t* value)") != std::string::npos);
    CHECK(source.find("std::wstring(value == nullptr ? L\"\" : value)") != std::string::npos);
}

void managedRibbonExtensionRegistersVerticalCurveContextMenu()
{
    const auto sourcePath = findRepositoryRootForTests()
        / "src"
        / "ui"
        / "wpf"
        / "RoadProto.Terrain.UI"
        / "AutoCad"
        / "RoadProtoRibbonExtension.cs";
    CHECK(std::filesystem::exists(sourcePath));

    const auto source = readTextFileForTests(sourcePath);
    CHECK(!source.empty());
    CHECK(source.find("AddObjectContextMenuExtension") != std::string::npos);
    CHECK(source.find("RemoveObjectContextMenuExtension") != std::string::npos);
    CHECK(source.find("RD_PROFILE_VERTICAL_CURVE_CONTEXT_ADD_PVI") != std::string::npos);
    CHECK(source.find("RD_PROFILE_VERTICAL_CURVE_CONTEXT_DELETE_PVI") != std::string::npos);
    CHECK(source.find("新增竖曲线变坡点") != std::string::npos);
    CHECK(source.find("删除竖曲线变坡点") != std::string::npos);
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
        L"37123.456_2 36.12000000\n");

    CHECK(parsed.succeeded);
    CHECK(parsed.samples.size() == 4);
    CHECK(parsed.invalidLineCount == 0);
    CHECK(!parsed.samples[0].breakChainIndex.has_value());
    CHECK(std::fabs(parsed.samples[1].station - 2.7) < 1e-9);
    CHECK(std::fabs(parsed.samples[2].station - 2.7) < 1e-9);
    CHECK(std::fabs(parsed.samples[1].elevation - 19.954) < 1e-9);
    CHECK(std::fabs(parsed.samples[2].elevation - 19.934) < 1e-9);
    CHECK(parsed.samples[3].rawStationText == L"37123.456_2");
    CHECK(parsed.samples[3].breakChainIndex.has_value());
    CHECK(*parsed.samples[3].breakChainIndex == 2);
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

void profileDmxFileRejectsInvalidRowsEvenWithEnoughSamples()
{
    using namespace roadproto::domain::profile;

    const auto parsed = ProfileDmxFile::parseText(
        L"0.00000000 21.25100000\n"
        L"bad line\n"
        L"10.00000000 22.25100000\n");

    CHECK(!parsed.succeeded);
    CHECK(parsed.samples.size() == 2);
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

    CHECK(!parsed.succeeded);
    CHECK(parsed.samples.size() == 2);
    CHECK(parsed.invalidLineCount == 2);
    CHECK(!parsed.errorMessage.empty());
    CHECK(std::fabs(parsed.samples[0].station - 0.0) < 1e-9);
    CHECK(std::fabs(parsed.samples[1].station - 20.0) < 1e-9);
}

void profileDmxFileIgnoresBlankLines()
{
    using namespace roadproto::domain::profile;

    const auto parsed = ProfileDmxFile::parseText(
        L"\n"
        L"   \n"
        L"0.00000000 21.25100000\n"
        L"\t \n"
        L"10.00000000 22.25100000\n"
        L"\n");

    CHECK(parsed.succeeded);
    CHECK(parsed.samples.size() == 2);
    CHECK(parsed.invalidLineCount == 0);
    CHECK(std::fabs(parsed.samples[1].station - 10.0) < 1e-9);
}

void profileDmxFileSkipsStationHeader()
{
    using namespace roadproto::domain::profile;

    const auto parsed = ProfileDmxFile::parseText(
        L"ZH H\n"
        L"0.00000000 21.25100000\n"
        L"10.00000000 22.25100000\n");

    CHECK(parsed.succeeded);
    CHECK(parsed.samples.size() == 2);
    CHECK(parsed.invalidLineCount == 0);
    CHECK(std::fabs(parsed.samples[0].elevation - 21.251) < 1e-9);
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

    const auto uniqueSuffix = std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / (L"roadproto_profile_read_test_" + uniqueSuffix + L".dmx");
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
    CHECK(layout.mappedPoints.size() == 2);
    CHECK(std::fabs(layout.mappedPoints[0].station - 100.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[0].elevation - 23.5) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[0].x - 0.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[0].y - 35.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[1].x - 20.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[1].y - 160.0) < 1e-9);
    CHECK(std::fabs(ProfileGradeGraphLayout::mapX(layout, 115.0) - 15.0) < 1e-9);
    CHECK(std::fabs(ProfileGradeGraphLayout::mapY(graph, layout, 23.5) - 35.0) < 1e-9);
}

void profileGradeGraphLayoutMappedPointsPreserveInputOrderAndDuplicateStations()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    graph.properties.gridSpacing = 10.0;
    graph.properties.verticalScale = 10.0;
    graph.groundSamples = {
        ProfileGroundSample{100.0, 20.0},
        ProfileGroundSample{110.0, 21.0},
        ProfileGroundSample{110.0, 22.0},
    };

    const auto layout = ProfileGradeGraphLayout::calculate(graph);
    CHECK(layout.succeeded);
    CHECK(layout.mappedPoints.size() == 3);
    CHECK(std::fabs(layout.mappedPoints[0].station - 100.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[1].station - 110.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[2].station - 110.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[0].x - 0.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[1].x - 10.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[2].x - 10.0) < 1e-9);
    CHECK(std::fabs(layout.mappedPoints[2].y - 20.0) < 1e-9);
}

void profileGradeGraphLayoutRejectsZeroStationSpan()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    graph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{100.0, 36.0},
    };

    const auto layout = ProfileGradeGraphLayout::calculate(graph);
    CHECK(!layout.succeeded);
    CHECK(!layout.errorMessage.empty());
}

void profileGradeGraphLayoutRejectsUnsupportedVerticalScale()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    graph.properties.verticalScale = 2.0;
    graph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const auto layout = ProfileGradeGraphLayout::calculate(graph);
    CHECK(!layout.succeeded);
    CHECK(!layout.errorMessage.empty());
}

void profileGradeGraphLayoutRejectsNonPositiveGridSpacing()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData zeroGridGraph;
    zeroGridGraph.properties.gridSpacing = 0.0;
    zeroGridGraph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const auto zeroGridLayout = ProfileGradeGraphLayout::calculate(zeroGridGraph);
    CHECK(!zeroGridLayout.succeeded);
    CHECK(!zeroGridLayout.errorMessage.empty());

    ProfileGradeGraphData negativeGridGraph;
    negativeGridGraph.properties.gridSpacing = -1.0;
    negativeGridGraph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const auto negativeGridLayout = ProfileGradeGraphLayout::calculate(negativeGridGraph);
    CHECK(!negativeGridLayout.succeeded);
    CHECK(!negativeGridLayout.errorMessage.empty());
}

void profileGradeGraphLayoutRejectsNonPositiveVerticalScale()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData zeroScaleGraph;
    zeroScaleGraph.properties.verticalScale = 0.0;
    zeroScaleGraph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const auto zeroScaleLayout = ProfileGradeGraphLayout::calculate(zeroScaleGraph);
    CHECK(!zeroScaleLayout.succeeded);
    CHECK(!zeroScaleLayout.errorMessage.empty());

    ProfileGradeGraphData negativeScaleGraph;
    negativeScaleGraph.properties.verticalScale = -1.0;
    negativeScaleGraph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const auto negativeScaleLayout = ProfileGradeGraphLayout::calculate(negativeScaleGraph);
    CHECK(!negativeScaleLayout.succeeded);
    CHECK(!negativeScaleLayout.errorMessage.empty());
}

void profileGradeGraphLayoutRejectsNonFiniteProperties()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData nanGridGraph;
    nanGridGraph.properties.gridSpacing = std::numeric_limits<double>::quiet_NaN();
    nanGridGraph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const auto nanGridLayout = ProfileGradeGraphLayout::calculate(nanGridGraph);
    CHECK(!nanGridLayout.succeeded);
    CHECK(!nanGridLayout.errorMessage.empty());

    ProfileGradeGraphData infScaleGraph;
    infScaleGraph.properties.verticalScale = std::numeric_limits<double>::infinity();
    infScaleGraph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const auto infScaleLayout = ProfileGradeGraphLayout::calculate(infScaleGraph);
    CHECK(!infScaleLayout.succeeded);
    CHECK(!infScaleLayout.errorMessage.empty());
}

void profileGradeGraphLayoutUsesGridIntervalForFlatProfileHeight()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    graph.properties.gridSpacing = 10.0;
    graph.properties.verticalScale = 10.0;
    graph.groundSamples = {
        ProfileGroundSample{100.0, 20.0},
        ProfileGroundSample{120.0, 20.0},
    };

    const auto layout = ProfileGradeGraphLayout::calculate(graph);
    CHECK(layout.succeeded);
    CHECK(std::fabs(layout.graphHeight - 100.0) < 1e-9);
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

void profileGradeGraphLayoutRejectsNonFiniteSamples()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData nanStationGraph;
    nanStationGraph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{std::numeric_limits<double>::quiet_NaN(), 36.0},
    };

    const auto nanStationLayout = ProfileGradeGraphLayout::calculate(nanStationGraph);
    CHECK(!nanStationLayout.succeeded);
    CHECK(!nanStationLayout.errorMessage.empty());

    ProfileGradeGraphData nanElevationGraph;
    nanElevationGraph.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, std::numeric_limits<double>::quiet_NaN()},
    };

    const auto nanElevationLayout = ProfileGradeGraphLayout::calculate(nanElevationGraph);
    CHECK(!nanElevationLayout.succeeded);
    CHECK(!nanElevationLayout.errorMessage.empty());
}

void profileGradeGraphDataDefaultsToDmxFileSource()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    CHECK(graph.sourceType == ProfileGroundSourceType::DmxFile);
}

void profileGradeGraphPropertiesDefaultGroundLinePrecision()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphProperties properties;
    CHECK(std::fabs(properties.groundLinePrecision - 10.0) < 1e-9);
}

void profileGradeGraphLayoutMapYUsesDefaultVerticalScale()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphLayoutResult layout;
    layout.baseElevation = 20.0;

    CHECK(std::fabs(ProfileGradeGraphLayout::mapY(layout, 23.5) - 35.0) < 1e-9);
}

void profileGradeGraphLayoutMapYRejectsUnsupportedGraphVerticalScale()
{
    using namespace roadproto::domain::profile;

    ProfileGradeGraphData graph;
    graph.properties.verticalScale = -1.0;

    ProfileGradeGraphLayoutResult layout;
    layout.baseElevation = 20.0;

    CHECK(!std::isfinite(ProfileGradeGraphLayout::mapY(graph, layout, 23.5)));
}

void profileGradeGraphCreateServiceBuildsDefaultGraphData()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileGradeGraphCreateInput input;
    input.sourceType = ProfileGroundSourceType::DmxFile;
    input.roadName = L"K1";
    input.roadCenterlineHandle = L"ABC";
    input.terrainTinHandle = L"TIN42";
    input.dmxFilePath = L"C:\\temp\\k1.dmx";
    input.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const ProfileGradeGraphCreateService service;
    const auto result = service.build(input);

    CHECK(result.succeeded);
    CHECK(result.errorMessage.empty());
    CHECK(result.graph.sourceType == ProfileGroundSourceType::DmxFile);
    CHECK(result.graph.roadCenterlineHandle == L"ABC");
    CHECK(result.graph.terrainTinHandle == L"TIN42");
    CHECK(result.graph.dmxFilePath == L"C:\\temp\\k1.dmx");
    CHECK(result.graph.groundSamples.size() == 2);
    CHECK(std::fabs(result.graph.groundSamples[0].station - 100.0) < 1e-9);
    CHECK(std::fabs(result.graph.groundSamples[1].elevation - 36.0) < 1e-9);
    CHECK(result.graph.properties.graphName == L"K1\u62c9\u5761\u56fe");
    CHECK(result.graph.properties.groundLineColorIndex == 4);
    CHECK(std::fabs(result.graph.properties.groundLineWidth - 1.0) < 1e-9);
    CHECK(std::fabs(result.graph.properties.groundLinePrecision - 10.0) < 1e-9);
    CHECK(std::fabs(result.graph.properties.verticalScale - 10.0) < 1e-9);
    CHECK(std::fabs(result.graph.properties.gridSpacing - 10.0) < 1e-9);
}

void profileGradeGraphCreateServiceUsesDefaultRoadName()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileGradeGraphCreateInput input;
    input.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 36.0},
    };

    const ProfileGradeGraphCreateService service;
    const auto result = service.build(input);

    CHECK(result.succeeded);
    CHECK(result.graph.properties.graphName == L"\u9053\u8def\u62c9\u5761\u56fe");
}

void profileGradeGraphCreateServiceRejectsTooFewSamples()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileGradeGraphCreateInput input;
    input.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
    };

    const ProfileGradeGraphCreateService service;
    const auto result = service.build(input);

    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());
}

void profileGradeGraphCreateServiceRejectsInvalidLayoutSamples()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileGradeGraphCreateInput sameStationInput;
    sameStationInput.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{100.0, 36.0},
    };

    const ProfileGradeGraphCreateService service;
    const auto sameStationResult = service.build(sameStationInput);
    CHECK(!sameStationResult.succeeded);
    CHECK(!sameStationResult.errorMessage.empty());

    ProfileGradeGraphCreateInput nonFiniteInput;
    nonFiniteInput.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{std::numeric_limits<double>::quiet_NaN(), 36.0},
    };

    const auto nonFiniteResult = service.build(nonFiniteInput);
    CHECK(!nonFiniteResult.succeeded);
    CHECK(!nonFiniteResult.errorMessage.empty());
}

void profileVerticalCurveModelDefaultsToDesignLine()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    CHECK(data.version == 1);
    CHECK(data.properties.name == L"\u7ad6\u66f2\u7ebf");
    CHECK(data.properties.designLineColorIndex == 4);
    CHECK(data.properties.tangentLineColorIndex == 7);
    CHECK(data.properties.keyPointColorIndex == 2);
    CHECK(std::fabs(data.properties.designLineWidth - 0.35) < 1e-9);
    CHECK(data.properties.sampleInterval == 5.0);
    CHECK(data.properties.showLabels);
    CHECK(data.properties.showTangentLines);
    CHECK(data.controlPoints.empty());
    CHECK(data.pvis.empty());

    VerticalCurveControlPoint controlPoint;
    CHECK(controlPoint.role == VerticalCurvePointRole::Pvi);
    CHECK(std::fabs(controlPoint.station) < 1e-9);
    CHECK(std::fabs(controlPoint.elevation) < 1e-9);

    VerticalCurvePvi pvi;
    CHECK(std::fabs(pvi.station) < 1e-9);
    CHECK(std::fabs(pvi.elevation) < 1e-9);
    CHECK(std::fabs(pvi.radius - 1000.0) < 1e-9);
    CHECK(!pvi.radiusLocked);
}

void profileVerticalCurveCalculatorInterpolatesStraightLine()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 100.0, 20.0},
        {VerticalCurvePointRole::End, 200.0, 30.0},
    };

    const auto built = ProfileVerticalCurveCalculator::rebuild(data);
    CHECK(built.succeeded);
    CHECK(built.elements.empty());
    const auto elevation = ProfileVerticalCurveCalculator::elevationAt(built, 150.0);
    CHECK(elevation.succeeded);
    CHECK(std::fabs(elevation.value - 25.0) < 1e-9);
    const auto grade = ProfileVerticalCurveCalculator::gradeAt(built, 150.0);
    CHECK(grade.succeeded);
    CHECK(std::fabs(grade.value - 0.1) < 1e-9);
}

void profileVerticalCurveCalculatorBuildsSingleCrestCurve()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::Pvi, 100.0, 10.0},
        {VerticalCurvePointRole::End, 200.0, 10.0},
    };
    data.pvis = {
        VerticalCurvePvi{100.0, 10.0, 1000.0},
    };

    const auto built = ProfileVerticalCurveCalculator::rebuild(data);
    CHECK(built.succeeded);
    CHECK(built.elements.size() == 1);
    CHECK(built.elements[0].type == VerticalCurveType::Crest);
    CHECK(std::fabs(built.elements[0].i1 - 0.1) < 1e-9);
    CHECK(std::fabs(built.elements[0].i2 - 0.0) < 1e-9);
    CHECK(std::fabs(built.elements[0].gradeDifference + 0.1) < 1e-9);
    CHECK(std::fabs(built.elements[0].length - 100.0) < 1e-9);
    CHECK(std::fabs(built.elements[0].tangentLength - 50.0) < 1e-9);
    CHECK(std::fabs(built.elements[0].bvcStation - 50.0) < 1e-9);
    CHECK(std::fabs(built.elements[0].evcStation - 150.0) < 1e-9);
    CHECK(built.elements[0].highLowPoint.has_value());
    CHECK(built.elements[0].highLowPoint->isHighPoint);
}

void profileVerticalCurveCalculatorKeepsOriginalPviIndexAfterSorting()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::End, 300.0, 0.0},
    };
    data.pvis = {
        VerticalCurvePvi{200.0, 30.0, 100.0},
        VerticalCurvePvi{100.0, 20.0, 100.0},
    };

    const auto built = ProfileVerticalCurveCalculator::rebuild(data);
    CHECK(built.succeeded);
    CHECK(built.elements.size() == 2);

    const auto station100 = std::find_if(built.elements.begin(), built.elements.end(), [](const auto& element) {
        return std::fabs(element.pviStation - 100.0) < 1e-9;
    });
    const auto station200 = std::find_if(built.elements.begin(), built.elements.end(), [](const auto& element) {
        return std::fabs(element.pviStation - 200.0) < 1e-9;
    });

    CHECK(station100 != built.elements.end());
    CHECK(station200 != built.elements.end());
    if (station100 != built.elements.end()) {
        CHECK(station100->pviIndex == 1);
    }
    if (station200 != built.elements.end()) {
        CHECK(station200->pviIndex == 0);
    }
}

void profileVerticalCurveCalculatorRejectsCurveBeyondAdjacentTangents()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::Pvi, 100.0, 10.0},
        {VerticalCurvePointRole::End, 200.0, 10.0},
    };
    data.pvis = {
        VerticalCurvePvi{100.0, 10.0, 5000.0},
    };

    const auto built = ProfileVerticalCurveCalculator::rebuild(data);
    CHECK(!built.succeeded);
    CHECK(!built.errorMessage.empty());
}

void profileVerticalCurveCalculatorUpdateRadiusRollsBackInvalidCurve()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::Pvi, 100.0, 10.0},
        {VerticalCurvePointRole::End, 200.0, 10.0},
    };
    data.pvis = {
        VerticalCurvePvi{100.0, 10.0, 1000.0},
    };

    const auto edit = ProfileVerticalCurveCalculator::updateRadius(data, 0, 5000.0);
    CHECK(!edit.succeeded);
    CHECK(!edit.changed);
    CHECK(std::fabs(data.pvis[0].radius - 1000.0) < 1e-9);
}

void profileVerticalCurveCalculatorSamplesBeyondGradeGraphRange()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, -20.0, 10.0},
        {VerticalCurvePointRole::End, 120.0, 24.0},
    };
    data.properties.sampleInterval = 25.0;

    const auto samples = ProfileVerticalCurveCalculator::sample(data, data.properties.sampleInterval);
    CHECK(samples.succeeded);
    CHECK(samples.points.size() >= 2);
    CHECK(std::fabs(samples.points.front().station + 20.0) < 1e-9);
    CHECK(std::fabs(samples.points.back().station - 120.0) < 1e-9);
}

void profileVerticalCurveCalculatorRejectsNonAdvancingSampleInterval()
{
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 1.0e16, 10.0},
        {VerticalCurvePointRole::End, 1.0e16 + 1024.0, 20.0},
    };

    const auto samples = ProfileVerticalCurveCalculator::sample(data, 0.1);
    CHECK(!samples.succeeded);
    CHECK(!samples.errorMessage.empty());
}

void profileVerticalCurveCreateServiceBuildsDefaultLineFromGraphSamples()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveCreateInput input;
    input.profileGraphHandle = L"ABCD";
    input.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
        ProfileGroundSample{120.0, 24.5},
        ProfileGroundSample{150.0, 26.0},
    };

    const ProfileVerticalCurveCreateService service;
    const auto result = service.buildDefaultFromGraph(input);

    CHECK(result.succeeded);
    CHECK(result.errorMessage.empty());
    CHECK(result.data.profileGraphHandle == L"ABCD");
    CHECK(result.data.controlPoints.size() == 2);
    CHECK(result.data.controlPoints[0].role == VerticalCurvePointRole::Start);
    CHECK(std::fabs(result.data.controlPoints[0].station - 100.0) < 1e-9);
    CHECK(std::fabs(result.data.controlPoints[0].elevation - 23.5) < 1e-9);
    CHECK(result.data.controlPoints[1].role == VerticalCurvePointRole::End);
    CHECK(std::fabs(result.data.controlPoints[1].station - 150.0) < 1e-9);
    CHECK(std::fabs(result.data.controlPoints[1].elevation - 26.0) < 1e-9);
    CHECK(result.data.pvis.empty());
}

void profileVerticalCurveCreateServiceRejectsTooFewGroundSamples()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveCreateInput input;
    input.profileGraphHandle = L"ABCD";
    input.groundSamples = {
        ProfileGroundSample{100.0, 23.5},
    };

    const ProfileVerticalCurveCreateService service;
    const auto result = service.buildDefaultFromGraph(input);

    CHECK(!result.succeeded);
    CHECK(!result.errorMessage.empty());
}

void profileVerticalCurveEditServiceAddsAndDeletesPvi()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::End, 200.0, 10.0},
    };

    const ProfileVerticalCurveEditService service;
    const auto added = service.addPvi(data, 100.0, 12.0, 800.0);
    CHECK(added.succeeded);
    CHECK(added.changed);
    CHECK(data.pvis.size() == 1);
    CHECK(data.controlPoints.size() == 3);

    const auto deleted = service.deletePvi(data, 0);
    CHECK(deleted.succeeded);
    CHECK(deleted.changed);
    CHECK(data.pvis.empty());
    CHECK(data.controlPoints.size() == 2);
}

void profileVerticalCurveEditServiceAppliesDialogEdit()
{
    using namespace roadproto::application::profile;
    using namespace roadproto::domain::profile;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::Pvi, 100.0, 10.0},
        {VerticalCurvePointRole::End, 200.0, 10.0},
    };
    data.pvis = {VerticalCurvePvi{100.0, 10.0, 1000.0}};

    ProfileVerticalCurveDialogEdit edit;
    edit.name = L"VC-1";
    edit.startStation = 0.0;
    edit.startElevation = 1.0;
    edit.endStation = 210.0;
    edit.endElevation = 11.0;
    edit.pvis = {VerticalCurvePvi{105.0, 12.0, 900.0}};

    const ProfileVerticalCurveEditService service;
    const auto result = service.applyDialogEdit(data, edit);
    CHECK(result.succeeded);
    CHECK(result.changed);
    CHECK(data.properties.name == L"VC-1");
    CHECK(std::fabs(data.controlPoints.front().elevation - 1.0) < 1e-9);
    CHECK(std::fabs(data.controlPoints.back().station - 210.0) < 1e-9);
    CHECK(std::fabs(data.pvis[0].radius - 900.0) < 1e-9);
}

void profileVerticalCurveDisplayPlannerColorsStraightAndCurveSegments()
{
    using roadproto::domain::profile::ProfileVerticalCurveData;
    using roadproto::domain::profile::ProfileVerticalCurveDisplayPlanner;
    using roadproto::domain::profile::VerticalCurveDisplaySegmentRole;
    using roadproto::domain::profile::VerticalCurvePointRole;
    using roadproto::domain::profile::VerticalCurvePvi;

    ProfileVerticalCurveData data;
    data.controlPoints = {
        {VerticalCurvePointRole::Start, 0.0, 0.0},
        {VerticalCurvePointRole::Pvi, 100.0, 10.0},
        {VerticalCurvePointRole::End, 200.0, 10.0},
    };
    data.pvis = {VerticalCurvePvi{100.0, 10.0, 1000.0}};
    data.properties.sampleInterval = 25.0;

    const auto plan = ProfileVerticalCurveDisplayPlanner::build(data);
    CHECK(plan.succeeded);

    int straightCount = 0;
    int curveCount = 0;
    int tangentCount = 0;
    bool hasBvcBoundary = false;
    bool hasEvcBoundary = false;
    for (const auto& segment : plan.segments) {
        if (segment.role == VerticalCurveDisplaySegmentRole::StraightDesignLine) {
            ++straightCount;
            CHECK(segment.colorIndex == 4);
            CHECK(segment.endStation <= 50.0 || segment.startStation >= 150.0);
        } else if (segment.role == VerticalCurveDisplaySegmentRole::CurveDesignLine) {
            ++curveCount;
            CHECK(segment.colorIndex == 2);
            CHECK(segment.startStation >= 50.0);
            CHECK(segment.endStation <= 150.0);
            hasBvcBoundary = hasBvcBoundary || std::fabs(segment.startStation - 50.0) < 1.0e-9;
            hasEvcBoundary = hasEvcBoundary || std::fabs(segment.endStation - 150.0) < 1.0e-9;
        } else if (segment.role == VerticalCurveDisplaySegmentRole::CurveTangentLine) {
            ++tangentCount;
            CHECK(segment.colorIndex == 7);
        }
    }

    CHECK(straightCount > 0);
    CHECK(curveCount > 0);
    CHECK(tangentCount == 2);
    CHECK(hasBvcBoundary);
    CHECK(hasEvcBoundary);
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

void terrainTriangleSpatialIndexFiltersCrossSectionCandidates()
{
    using namespace roadproto::domain::terrain;

    constexpr int gridSize = 20;
    std::vector<TinPoint> points;
    points.reserve((gridSize + 1) * (gridSize + 1));
    for (int y = 0; y <= gridSize; ++y) {
        for (int x = 0; x <= gridSize; ++x) {
            points.push_back(TinPoint{static_cast<double>(x), static_cast<double>(y), 0.0});
        }
    }

    const auto pointIndex = [gridSize](int x, int y) {
        return static_cast<std::size_t>(y * (gridSize + 1) + x);
    };

    std::vector<TinTriangle> triangles;
    triangles.reserve(gridSize * gridSize * 2);
    for (int y = 0; y < gridSize; ++y) {
        for (int x = 0; x < gridSize; ++x) {
            triangles.push_back(TinTriangle{
                pointIndex(x, y),
                pointIndex(x + 1, y),
                pointIndex(x, y + 1)});
            triangles.push_back(TinTriangle{
                pointIndex(x + 1, y),
                pointIndex(x + 1, y + 1),
                pointIndex(x, y + 1)});
        }
    }

    TerrainTriangleSpatialIndex index(points, triangles);
    CHECK(index.enabled());
    CHECK(index.triangleReferenceCount() > 0);

    const auto candidates = index.querySegment(0.0, 10.0, 20.0, 10.0);
    CHECK(!candidates.empty());
    CHECK(candidates.size() < triangles.size() / 3);

    const auto narrowCandidates = index.querySegment(0.0, 10.0, 1.0, 10.0);
    CHECK(!narrowCandidates.empty());
    CHECK(narrowCandidates.size() < candidates.size());

    const auto diagonalCandidates = index.querySegment(0.0, 0.0, 20.0, 20.0);
    CHECK(!diagonalCandidates.empty());
    CHECK(diagonalCandidates.size() < triangles.size() / 2);
}

} // namespace

int main()
{
    commandRegistryStoresMetadataAndRejectsDuplicates();
    moduleRegistryRegistersCommandsAndRibbonPanels();
    profileModuleRegistersCommandsAndRibbonPanel();
    startupRegistrationIncludesProfileModule();
    subgradeTemplateDefaultsBuildExpressway();
    subgradeTemplateDefaultsBuildUrbanExpressway();
    subgradeTemplateDefaultsBuildHighwayGradesFromRoadClassProfiles();
    subgradeTemplateDefaultsBuildUrbanRoadClassProfiles();
    subgradeTemplateComponentDisplayNamesAreChinese();
    subgradeTemplateRulesUseWideningTableAndPavementThicknessGate();
    subgradeTemplateVariableSlopeUsesOnlySlopeTable();
    subgradeTemplateCreateServiceBuildsDefaultTemplate();
    pavementLayerTemplateRulesNormalizeThicknessAndCodes();
    pavementLayerTemplateGeometryUsesInnerOuterWithoutSlopeWidening();
    pavementLayerTemplateRulesAcceptPositiveFiniteDisplayScale();
    slopeTemplateDefaultsBuildFillAndCutProfiles();
    slopeTemplateRulesResolveThreeGeometryModes();
    slopeTemplateRulesValidateRepeatLastGroup();
    slopeTemplateCodesRoundTripAndDisplayChinese();
    roadModelTemplateResolverUsesHigherPriorityRows();
    roadModelTemplateResolverRejectsInvalidRows();
    roadModelStationSamplerIncludesIntervalTemplateAndVerticalCurveStations();
    roadModelStationSamplerOnlyKeepsTemplateCoveredStations();
    roadModelStationSamplerSnapsTemplateBoundaryTolerance();
    roadModelBuilderCreatesThreeDimensionalComponentLines();
    roadModelBuilderCreatesPavementLayerWireLinesForBoundSubgradeComponent();
    roadModelBuilderKeepsPavementLayerInnerOuterSemanticOnLeftSide();
    roadModelSectionPreviewBuilderCreatesSubgradePreviewAtStation();
    roadModelSectionPreviewBuilderAddsGroundLineFromTin();
    roadModelBuilderStoresGroundProfileSnapshotsForSections();
    roadModelSectionPreviewBuilderUsesStoredGroundSnapshotWithoutTin();
    roadModelBuilderReportsProgressDuringBuild();
    roadModelSlopeTemplateGroupResolverKeepsPriorityOrder();
    roadModelBuilderCreatesSlopeLinesFromSubgradeOuterEdge();
    roadModelBuilderCreatesMeshWireframeFromSampledSections();
    roadModelBuilderCreatesTransitionWireLinesWhenSectionNodeCountsDiffer();
    roadModelBuilderKeepsSlopeTransitionsOutsideSubgrade();
    roadModelBuilderStopsSlopeAtTinGroundIntersection();
    roadModelBuilderDoesNotConnectAcrossTemplateSwitches();
    roadModelBuilderDoesNotConnectAcrossTemplateGaps();
    roadModelBuilderDoesNotMergeGapWhenBoundaryPointsCoincide();
    roadModelBuilderSplitsLowerPriorityTemplateAroundOverride();
    roadModelBuilderRejectsInvalidAlignmentSamples();
    roadModelBuilderRejectsInvalidTemplateSource();
    roadModelBuildServiceRejectsMissingHandlesAndDelegatesBuild();
    crossSectionModuleRegistersSubgradeTemplateCommandsAndRibbonPanel();
    startupRegistrationIncludesCrossSectionModule();
    managedRibbonExtensionRegistersSubgradeTemplateEntryPoints();
    managedRibbonExtensionRegistersRoadModelEntryPoints();
    roadModelWpfBridgeSourceContainsRequiredFields();
    roadModelNativeDialogBridgeSourceContainsRequiredFields();
    roadModelSectionViewerNativeBridgeSourceContainsRequiredFields();
    roadModelCommandSourceContainsCompleteObjectArxFlow();
    roadModelEntitySourceContainsRequiredObjectArxContracts();
    subgradeTemplateEntitySourceContainsMoveGrip();
    subgradeTemplateWindowSourceKeepsControlsReadable();
    subgradeTemplateBridgeWritesEnumCodesAsText();
    managedRibbonExtensionRegistersVerticalCurveContextMenu();
    relationManagerMarksDependentsForRebuild();
    terrainSampleServiceCreatesRelationUpdateScenario();
    terrainTextElevationParserRecognizesSupportedForms();
    terrainPointNormalizerMergesDuplicateAndTextElevationPoints();
    terrainTinBuilderCreatesTrianglesAndRejectsCollinearInput();
    terrainSurfaceQueryInterpolatesElevationInsideTriangle();
    terrainTriangleSpatialIndexFiltersCrossSectionCandidates();
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
    profileDmxFileRejectsInvalidRowsEvenWithEnoughSamples();
    profileDmxFileRejectsNonFiniteRows();
    profileDmxFileIgnoresBlankLines();
    profileDmxFileSkipsStationHeader();
    profileDmxFileParsesTextWithLeadingBom();
    profileDmxFileParsesTextWithLeadingUtf8BomBytes();
    profileDmxFileReadsTempFile();
    profileGradeGraphLayoutMapsStationAndElevation();
    profileGradeGraphLayoutMappedPointsPreserveInputOrderAndDuplicateStations();
    profileGradeGraphLayoutRejectsZeroStationSpan();
    profileGradeGraphLayoutRejectsUnsupportedVerticalScale();
    profileGradeGraphLayoutRejectsNonPositiveGridSpacing();
    profileGradeGraphLayoutRejectsNonPositiveVerticalScale();
    profileGradeGraphLayoutRejectsNonFiniteProperties();
    profileGradeGraphLayoutUsesGridIntervalForFlatProfileHeight();
    profileGradeGraphLayoutRejectsNonFiniteGeometry();
    profileGradeGraphLayoutRejectsNonFiniteSamples();
    profileGradeGraphDataDefaultsToDmxFileSource();
    profileGradeGraphPropertiesDefaultGroundLinePrecision();
    profileGradeGraphLayoutMapYUsesDefaultVerticalScale();
    profileGradeGraphLayoutMapYRejectsUnsupportedGraphVerticalScale();
    profileGradeGraphCreateServiceBuildsDefaultGraphData();
    profileGradeGraphCreateServiceUsesDefaultRoadName();
    profileGradeGraphCreateServiceRejectsTooFewSamples();
    profileGradeGraphCreateServiceRejectsInvalidLayoutSamples();
    profileVerticalCurveModelDefaultsToDesignLine();
    profileVerticalCurveCalculatorInterpolatesStraightLine();
    profileVerticalCurveCalculatorBuildsSingleCrestCurve();
    profileVerticalCurveCalculatorKeepsOriginalPviIndexAfterSorting();
    profileVerticalCurveCalculatorRejectsCurveBeyondAdjacentTangents();
    profileVerticalCurveCalculatorUpdateRadiusRollsBackInvalidCurve();
    profileVerticalCurveCalculatorSamplesBeyondGradeGraphRange();
    profileVerticalCurveCalculatorRejectsNonAdvancingSampleInterval();
    profileVerticalCurveCreateServiceBuildsDefaultLineFromGraphSamples();
    profileVerticalCurveCreateServiceRejectsTooFewGroundSamples();
    profileVerticalCurveEditServiceAddsAndDeletesPvi();
    profileVerticalCurveEditServiceAppliesDialogEdit();
    profileVerticalCurveDisplayPlannerColorsStraightAndCurveSegments();
    alignmentCommandMetadataUsesExpectedNames();

    if (g_failures != 0) {
        std::cerr << g_failures << " RoadProto core test(s) failed.\n";
        return 1;
    }

    std::cout << "All RoadProto core tests passed.\n";
    return 0;
}
