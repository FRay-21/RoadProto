#include "domain/cross_section/SubgradeTemplateModel.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::cross_section {
namespace {

constexpr double kStationTolerance = 1.0e-6;

SubgradeTemplateComponent makeComponent(
    SubgradeSide side,
    SubgradeComponentType type,
    double width)
{
    SubgradeTemplateComponent component;
    component.side = side;
    component.type = type;
    component.width = width;
    component.color = SubgradeTemplateDefaults::defaultColorFor(type);
    return component;
}

void appendSymmetricComponents(
    SubgradeTemplateData& data,
    const std::vector<std::pair<SubgradeComponentType, double>>& leftToOutside)
{
    for (const auto& item : leftToOutside) {
        data.components.push_back(makeComponent(SubgradeSide::Left, item.first, item.second));
    }

    for (const auto& item : leftToOutside) {
        data.components.push_back(makeComponent(SubgradeSide::Right, item.first, item.second));
    }
}

bool stationMatches(double lhs, double rhs)
{
    return std::fabs(lhs - rhs) <= kStationTolerance;
}

bool isFiniteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool isFiniteValue(double value)
{
    return std::isfinite(value);
}

} // namespace

SubgradeTemplateRgbColor SubgradeTemplateDefaults::defaultColorFor(SubgradeComponentType type)
{
    switch (type) {
    case SubgradeComponentType::Median:
    case SubgradeComponentType::SideMedian:
        return {0, 120, 0};
    case SubgradeComponentType::TravelLane:
    case SubgradeComponentType::HardShoulder:
    case SubgradeComponentType::BikeLane:
    case SubgradeComponentType::CurbStrip:
        return {0, 90, 180};
    case SubgradeComponentType::EarthShoulder:
    case SubgradeComponentType::Sidewalk:
    default:
        return {120, 120, 120};
    }
}

SubgradeTemplateData SubgradeTemplateDefaults::create(RoadGrade grade)
{
    SubgradeTemplateData data;
    data.properties.roadGrade = grade;
    data.properties.displayScale = 10.0;
    data.properties.name = L"\u9ed8\u8ba4\u8def\u57fa\u6a21\u677f";

    switch (grade) {
    case RoadGrade::Expressway:
        appendSymmetricComponents(
            data,
            {
                {SubgradeComponentType::Median, 1.5},
                {SubgradeComponentType::CurbStrip, 0.75},
                {SubgradeComponentType::TravelLane, 7.5},
                {SubgradeComponentType::HardShoulder, 3.0},
                {SubgradeComponentType::EarthShoulder, 0.75},
            });
        break;
    case RoadGrade::FirstClass:
        appendSymmetricComponents(
            data,
            {
                {SubgradeComponentType::Median, 1.0},
                {SubgradeComponentType::CurbStrip, 0.5},
                {SubgradeComponentType::TravelLane, 3.75},
                {SubgradeComponentType::TravelLane, 3.75},
                {SubgradeComponentType::HardShoulder, 2.5},
                {SubgradeComponentType::EarthShoulder, 0.75},
            });
        break;
    case RoadGrade::SecondClass:
        appendSymmetricComponents(
            data,
            {
                {SubgradeComponentType::TravelLane, 3.75},
                {SubgradeComponentType::HardShoulder, 1.5},
                {SubgradeComponentType::EarthShoulder, 0.75},
            });
        break;
    case RoadGrade::ThirdClass:
        appendSymmetricComponents(
            data,
            {
                {SubgradeComponentType::TravelLane, 3.5},
                {SubgradeComponentType::HardShoulder, 0.75},
                {SubgradeComponentType::EarthShoulder, 0.75},
            });
        break;
    case RoadGrade::FourthClass:
        appendSymmetricComponents(
            data,
            {
                {SubgradeComponentType::TravelLane, 3.0},
                {SubgradeComponentType::HardShoulder, 0.25},
                {SubgradeComponentType::EarthShoulder, 0.5},
            });
        break;
    case RoadGrade::UrbanExpressway:
        appendSymmetricComponents(
            data,
            {
                {SubgradeComponentType::Median, 1.0},
                {SubgradeComponentType::TravelLane, 7.5},
                {SubgradeComponentType::SideMedian, 1.0},
                {SubgradeComponentType::BikeLane, 3.0},
                {SubgradeComponentType::Sidewalk, 4.0},
            });
        break;
    case RoadGrade::UrbanArterial:
        appendSymmetricComponents(
            data,
            {
                {SubgradeComponentType::Median, 1.5},
                {SubgradeComponentType::CurbStrip, 0.5},
                {SubgradeComponentType::TravelLane, 3.5},
                {SubgradeComponentType::TravelLane, 3.5},
                {SubgradeComponentType::SideMedian, 1.5},
                {SubgradeComponentType::BikeLane, 2.5},
                {SubgradeComponentType::Sidewalk, 3.0},
            });
        break;
    case RoadGrade::UrbanSubArterial:
        appendSymmetricComponents(
            data,
            {
                {SubgradeComponentType::CurbStrip, 0.25},
                {SubgradeComponentType::TravelLane, 3.5},
                {SubgradeComponentType::TravelLane, 3.5},
                {SubgradeComponentType::BikeLane, 2.5},
                {SubgradeComponentType::Sidewalk, 3.0},
            });
        break;
    case RoadGrade::UrbanBranch:
        appendSymmetricComponents(
            data,
            {
                {SubgradeComponentType::TravelLane, 3.25},
                {SubgradeComponentType::CurbStrip, 0.25},
                {SubgradeComponentType::Sidewalk, 2.0},
            });
        break;
    default:
        break;
    }

    return data;
}

bool SubgradeTemplateRules::isSupportedDisplayScale(double displayScale)
{
    return std::isfinite(displayScale)
        && (std::fabs(displayScale - 1.0) < 1.0e-9
            || std::fabs(displayScale - 10.0) < 1.0e-9
            || std::fabs(displayScale - 20.0) < 1.0e-9
            || std::fabs(displayScale - 50.0) < 1.0e-9
            || std::fabs(displayScale - 100.0) < 1.0e-9);
}

double SubgradeTemplateRules::widthAtStation(const SubgradeTemplateComponent& component, double station)
{
    if (!isFiniteValue(component.width) || component.width < 0.0 || !std::isfinite(station)) {
        return 0.0;
    }

    for (const auto& row : component.wideningTable) {
        if (stationMatches(row.station, station) && isFiniteValue(row.value)) {
            return std::max(0.0, component.width + row.value);
        }
    }

    return component.width;
}

double SubgradeTemplateRules::slopeAtStation(const SubgradeTemplateComponent& component, double station)
{
    if (component.slopeMode != SubgradeSlopeMode::VariableByStation) {
        return std::isfinite(component.fixedSlope) ? component.fixedSlope : 0.0;
    }
    if (!std::isfinite(station)) {
        return 0.0;
    }

    for (const auto& row : component.variableSlopeTable) {
        if (stationMatches(row.station, station) && isFiniteValue(row.value)) {
            return row.value;
        }
    }

    return 0.0;
}

double SubgradeTemplateRules::effectivePavementThickness(const SubgradeTemplateComponent& component)
{
    if (!component.pavementLayerLinked || !isFiniteNonNegative(component.pavementLayerThickness)) {
        return 0.0;
    }

    return component.pavementLayerThickness;
}

bool SubgradeTemplateRules::normalize(SubgradeTemplateData& data, std::wstring& errorMessage)
{
    if (data.properties.name.empty()) {
        data.properties.name = L"\u9ed8\u8ba4\u8def\u57fa\u6a21\u677f";
    }

    if (!isSupportedDisplayScale(data.properties.displayScale)) {
        errorMessage = L"Subgrade template display scale must be 1, 10, 20, 50, or 100.";
        return false;
    }

    for (auto& component : data.components) {
        if (!isFiniteNonNegative(component.width)) {
            errorMessage = L"Subgrade template component width must be finite and non-negative.";
            return false;
        }
        if (!isFiniteValue(component.height)) {
            errorMessage = L"Subgrade template component height must be finite.";
            return false;
        }
        if (!isFiniteValue(component.fixedSlope)) {
            errorMessage = L"Subgrade template component slope must be finite.";
            return false;
        }
        if (component.slopeMode == SubgradeSlopeMode::VariableByStation) {
            component.fixedSlope = 0.0;
        }
        if (component.pavementLayerHandle.empty()) {
            component.pavementLayerLinked = false;
        }
        if (!component.pavementLayerLinked) {
            component.pavementLayerThickness = 0.0;
            component.pavementLayerHandle.clear();
            component.pavementLayerName.clear();
        } else if (!isFiniteNonNegative(component.pavementLayerThickness)) {
            errorMessage = L"Subgrade template pavement layer thickness must be finite and non-negative.";
            return false;
        }

        component.color.r = std::clamp(component.color.r, 0, 255);
        component.color.g = std::clamp(component.color.g, 0, 255);
        component.color.b = std::clamp(component.color.b, 0, 255);

        for (const auto& row : component.wideningTable) {
            if (!isFiniteValue(row.station) || !isFiniteValue(row.value)) {
                errorMessage = L"Subgrade template widening table contains non-finite values.";
                return false;
            }
        }
        for (const auto& row : component.variableSlopeTable) {
            if (!isFiniteValue(row.station) || !isFiniteValue(row.value)) {
                errorMessage = L"Subgrade template variable slope table contains non-finite values.";
                return false;
            }
        }
    }

    return true;
}

const wchar_t* roadGradeCode(RoadGrade grade)
{
    switch (grade) {
    case RoadGrade::Expressway:
        return L"Expressway";
    case RoadGrade::FirstClass:
        return L"FirstClass";
    case RoadGrade::SecondClass:
        return L"SecondClass";
    case RoadGrade::ThirdClass:
        return L"ThirdClass";
    case RoadGrade::FourthClass:
        return L"FourthClass";
    case RoadGrade::UrbanExpressway:
        return L"UrbanExpressway";
    case RoadGrade::UrbanArterial:
        return L"UrbanArterial";
    case RoadGrade::UrbanSubArterial:
        return L"UrbanSubArterial";
    case RoadGrade::UrbanBranch:
        return L"UrbanBranch";
    default:
        return L"Expressway";
    }
}

const wchar_t* subgradeSideCode(SubgradeSide side)
{
    return side == SubgradeSide::Left ? L"Left" : L"Right";
}

const wchar_t* subgradeComponentTypeCode(SubgradeComponentType type)
{
    switch (type) {
    case SubgradeComponentType::Median:
        return L"Median";
    case SubgradeComponentType::TravelLane:
        return L"TravelLane";
    case SubgradeComponentType::HardShoulder:
        return L"HardShoulder";
    case SubgradeComponentType::EarthShoulder:
        return L"EarthShoulder";
    case SubgradeComponentType::SideMedian:
        return L"SideMedian";
    case SubgradeComponentType::Sidewalk:
        return L"Sidewalk";
    case SubgradeComponentType::BikeLane:
        return L"BikeLane";
    case SubgradeComponentType::CurbStrip:
        return L"CurbStrip";
    default:
        return L"TravelLane";
    }
}

const wchar_t* subgradeComponentTypeDisplayName(SubgradeComponentType type)
{
    switch (type) {
    case SubgradeComponentType::Median:
        return L"中分带";
    case SubgradeComponentType::TravelLane:
        return L"行车道";
    case SubgradeComponentType::HardShoulder:
        return L"硬路肩";
    case SubgradeComponentType::EarthShoulder:
        return L"土路肩";
    case SubgradeComponentType::SideMedian:
        return L"侧分带";
    case SubgradeComponentType::Sidewalk:
        return L"人行道";
    case SubgradeComponentType::BikeLane:
        return L"慢车道";
    case SubgradeComponentType::CurbStrip:
        return L"路缘带";
    default:
        return L"行车道";
    }
}

const wchar_t* subgradeSlopeModeCode(SubgradeSlopeMode mode)
{
    return mode == SubgradeSlopeMode::VariableByStation ? L"VariableByStation" : L"Fixed";
}

RoadGrade roadGradeFromCode(const std::wstring& code, RoadGrade fallback)
{
    if (code == L"Expressway") {
        return RoadGrade::Expressway;
    }
    if (code == L"FirstClass") {
        return RoadGrade::FirstClass;
    }
    if (code == L"SecondClass") {
        return RoadGrade::SecondClass;
    }
    if (code == L"ThirdClass") {
        return RoadGrade::ThirdClass;
    }
    if (code == L"FourthClass") {
        return RoadGrade::FourthClass;
    }
    if (code == L"UrbanExpressway") {
        return RoadGrade::UrbanExpressway;
    }
    if (code == L"UrbanArterial") {
        return RoadGrade::UrbanArterial;
    }
    if (code == L"UrbanSubArterial") {
        return RoadGrade::UrbanSubArterial;
    }
    if (code == L"UrbanBranch") {
        return RoadGrade::UrbanBranch;
    }
    return fallback;
}

SubgradeSide subgradeSideFromCode(const std::wstring& code, SubgradeSide fallback)
{
    if (code == L"Left") {
        return SubgradeSide::Left;
    }
    if (code == L"Right") {
        return SubgradeSide::Right;
    }
    return fallback;
}

SubgradeComponentType subgradeComponentTypeFromCode(
    const std::wstring& code,
    SubgradeComponentType fallback)
{
    if (code == L"Median") {
        return SubgradeComponentType::Median;
    }
    if (code == L"TravelLane") {
        return SubgradeComponentType::TravelLane;
    }
    if (code == L"HardShoulder") {
        return SubgradeComponentType::HardShoulder;
    }
    if (code == L"EarthShoulder") {
        return SubgradeComponentType::EarthShoulder;
    }
    if (code == L"SideMedian") {
        return SubgradeComponentType::SideMedian;
    }
    if (code == L"Sidewalk") {
        return SubgradeComponentType::Sidewalk;
    }
    if (code == L"BikeLane") {
        return SubgradeComponentType::BikeLane;
    }
    if (code == L"CurbStrip") {
        return SubgradeComponentType::CurbStrip;
    }
    return fallback;
}

SubgradeSlopeMode subgradeSlopeModeFromCode(
    const std::wstring& code,
    SubgradeSlopeMode fallback)
{
    if (code == L"Fixed") {
        return SubgradeSlopeMode::Fixed;
    }
    if (code == L"VariableByStation") {
        return SubgradeSlopeMode::VariableByStation;
    }
    return fallback;
}

} // namespace roadproto::domain::cross_section
