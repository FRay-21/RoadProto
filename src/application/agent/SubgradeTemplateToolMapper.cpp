#include "application/agent/SubgradeTemplateToolMapper.h"

#include <algorithm>

namespace roadproto::application::agent {
namespace {

using namespace roadproto::domain::cross_section;

void fail(SubgradeTemplateToolDataResult& result, std::wstring& errorMessage, const std::wstring& message)
{
    errorMessage = message;
    result.errorMessage = message;
}

bool isRoadGradeCode(const std::wstring& code)
{
    return code == L"Expressway" ||
        code == L"FirstClass" ||
        code == L"SecondClass" ||
        code == L"ThirdClass" ||
        code == L"FourthClass" ||
        code == L"UrbanExpressway" ||
        code == L"UrbanArterial" ||
        code == L"UrbanSubArterial" ||
        code == L"UrbanBranch";
}

bool isSubgradeSideCode(const std::wstring& code)
{
    return code == L"Left" || code == L"Right";
}

bool isSubgradeComponentTypeCode(const std::wstring& code)
{
    return code == L"Median" ||
        code == L"TravelLane" ||
        code == L"HardShoulder" ||
        code == L"EarthShoulder" ||
        code == L"SideMedian" ||
        code == L"Sidewalk" ||
        code == L"BikeLane" ||
        code == L"CurbStrip";
}

bool isSubgradeSlopeModeCode(const std::wstring& code)
{
    return code == L"Fixed" || code == L"VariableByStation";
}

SubgradeTemplateRgbColor mapColorOrDefault(
    const AgentToolColor& color,
    SubgradeComponentType type)
{
    if (color.r >= 0 && color.g >= 0 && color.b >= 0) {
        return {
            std::clamp(color.r, 0, 255),
            std::clamp(color.g, 0, 255),
            std::clamp(color.b, 0, 255)};
    }
    return SubgradeTemplateDefaults::defaultColorFor(type);
}

bool mapComponent(
    const AgentToolSubgradeComponent& input,
    SubgradeTemplateComponent& component,
    std::wstring& errorMessage)
{
    if (input.side.empty()) {
        errorMessage = L"Explicit subgrade component side is required.";
        return false;
    }
    if (!isSubgradeSideCode(input.side)) {
        errorMessage = L"Invalid explicit subgrade component side.";
        return false;
    }
    if (input.type.empty()) {
        errorMessage = L"Explicit subgrade component type is required.";
        return false;
    }
    if (!isSubgradeComponentTypeCode(input.type)) {
        errorMessage = L"Invalid explicit subgrade component type.";
        return false;
    }
    if (!input.hasWidth) {
        errorMessage = L"Explicit subgrade component width is required.";
        return false;
    }
    if (!input.slopeMode.empty() && !isSubgradeSlopeModeCode(input.slopeMode)) {
        errorMessage = L"Invalid explicit subgrade component slopeMode.";
        return false;
    }
    if (input.pavementLayer.linked && input.pavementLayer.handle.empty()) {
        errorMessage = L"Explicit subgrade component pavementLayer.handle is required when pavementLayer.linked is true.";
        return false;
    }

    component.side = subgradeSideFromCode(input.side, SubgradeSide::Right);
    component.type = subgradeComponentTypeFromCode(input.type, SubgradeComponentType::TravelLane);
    component.width = input.width;
    component.height = input.height;
    component.slopeMode = subgradeSlopeModeFromCode(input.slopeMode, SubgradeSlopeMode::Fixed);
    component.fixedSlope = input.fixedSlope;
    component.color = mapColorOrDefault(input.color, component.type);

    for (const auto& row : input.wideningTable) {
        component.wideningTable.push_back({row.station, row.value});
    }
    for (const auto& row : input.variableSlopeTable) {
        component.variableSlopeTable.push_back({row.station, row.value});
    }

    component.pavementLayerLinked = input.pavementLayer.linked;
    component.pavementLayerHandle = input.pavementLayer.handle;
    component.pavementLayerName = input.pavementLayer.name;
    component.pavementLayerThickness = input.pavementLayer.thickness;
    return true;
}

} // namespace

SubgradeTemplateToolDataResult buildSubgradeTemplateToolData(
    const AgentSubgradeTemplateCreateArguments& arguments,
    std::wstring& errorMessage)
{
    SubgradeTemplateToolDataResult result;
    errorMessage.clear();

    if (!isRoadGradeCode(arguments.roadGrade)) {
        fail(result, errorMessage, L"Invalid agent subgrade template roadGrade.");
        return result;
    }

    const auto grade = roadGradeFromCode(arguments.roadGrade, RoadGrade::Expressway);
    if (arguments.componentSource == L"DefaultByRoadGrade") {
        result.data = SubgradeTemplateDefaults::create(grade);
    } else if (arguments.componentSource == L"ExplicitComponents") {
        result.data.properties.roadGrade = grade;
        result.data.components.clear();
        for (const auto& component : arguments.components) {
            SubgradeTemplateComponent mapped;
            if (!mapComponent(component, mapped, errorMessage)) {
                result.errorMessage = errorMessage;
                return result;
            }
            result.data.components.push_back(mapped);
        }
    } else {
        fail(result, errorMessage, L"Unsupported subgrade template component source.");
        return result;
    }

    result.data.properties.name = arguments.templateName;
    result.data.properties.displayScale = arguments.displayScale;
    result.data.properties.roadGrade = grade;
    result.data.roadCenterlineHandle = arguments.roadCenterlineHandle;

    if (!SubgradeTemplateRules::normalize(result.data, errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    result.succeeded = true;
    result.errorMessage.clear();
    errorMessage.clear();
    return result;
}

} // namespace roadproto::application::agent
