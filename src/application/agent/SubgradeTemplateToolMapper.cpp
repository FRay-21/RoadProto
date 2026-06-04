#include "application/agent/SubgradeTemplateToolMapper.h"

#include <algorithm>

namespace roadproto::application::agent {
namespace {

using namespace roadproto::domain::cross_section;

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

SubgradeTemplateComponent mapComponent(const AgentToolSubgradeComponent& input)
{
    SubgradeTemplateComponent component;
    component.side = subgradeSideFromCode(input.side, SubgradeSide::Right);
    component.type = subgradeComponentTypeFromCode(input.type, SubgradeComponentType::TravelLane);
    component.width = input.hasWidth ? input.width : 0.0;
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
    return component;
}

} // namespace

SubgradeTemplateToolDataResult buildSubgradeTemplateToolData(
    const AgentSubgradeTemplateCreateArguments& arguments,
    std::wstring& errorMessage)
{
    SubgradeTemplateToolDataResult result;
    errorMessage.clear();

    const auto grade = roadGradeFromCode(arguments.roadGrade, RoadGrade::Expressway);
    if (arguments.componentSource == L"DefaultByRoadGrade") {
        result.data = SubgradeTemplateDefaults::create(grade);
    } else if (arguments.componentSource == L"ExplicitComponents") {
        result.data.properties.roadGrade = grade;
        result.data.components.clear();
        for (const auto& component : arguments.components) {
            result.data.components.push_back(mapComponent(component));
        }
    } else {
        errorMessage = L"Unsupported subgrade template component source.";
        result.errorMessage = errorMessage;
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
