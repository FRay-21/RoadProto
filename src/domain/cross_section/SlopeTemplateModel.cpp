#include "domain/cross_section/SlopeTemplateModel.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::cross_section {
namespace {

constexpr double kGeometryTolerance = 1.0e-9;

bool isFinite(double value)
{
    return std::isfinite(value);
}

bool isFiniteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

bool isSlopeComponent(SlopeComponentType type)
{
    return type == SlopeComponentType::FillSlope || type == SlopeComponentType::CutSlope;
}

double signForHeightAndWidth(const SlopeTemplateComponent& component)
{
    if (component.type == SlopeComponentType::FillSlope) {
        return -1.0;
    }
    if (component.type == SlopeComponentType::CutSlope) {
        return 1.0;
    }
    if (std::fabs(component.slope) > kGeometryTolerance) {
        return component.slope > 0.0 ? 1.0 : -1.0;
    }
    return -1.0;
}

SlopeTemplateComponent makeSlopeComponent(SlopeTemplateKind kind)
{
    SlopeTemplateComponent component;
    component.type = kind == SlopeTemplateKind::Fill
        ? SlopeComponentType::FillSlope
        : SlopeComponentType::CutSlope;
    component.constraintMode = SlopeGeometryConstraintMode::SlopeAndHeight;
    component.slope = kind == SlopeTemplateKind::Fill ? -1.0 / 1.5 : 1.0 / 1.5;
    component.height = 4.0;
    component.width = 6.0;
    component.groundSearchHeightIncrement = 2.0;
    component.color = SlopeTemplateDefaults::defaultColorFor(component.type);
    return component;
}

SlopeTemplateComponent makeBermComponent(SlopeTemplateKind kind)
{
    SlopeTemplateComponent component;
    component.type = SlopeComponentType::Berm;
    component.constraintMode = SlopeGeometryConstraintMode::SlopeAndWidth;
    component.slope = kind == SlopeTemplateKind::Fill ? -0.03 : 0.03;
    component.height = 0.03;
    component.width = 1.0;
    component.groundSearchHeightIncrement = 0.0;
    component.color = SlopeTemplateDefaults::defaultColorFor(component.type);
    return component;
}

} // namespace

SlopeTemplateRgbColor SlopeTemplateDefaults::defaultColorFor(SlopeComponentType type)
{
    switch (type) {
    case SlopeComponentType::FillSlope:
        return {30, 132, 88};
    case SlopeComponentType::CutSlope:
        return {190, 90, 45};
    case SlopeComponentType::Berm:
    default:
        return {90, 110, 130};
    }
}

SlopeTemplateData SlopeTemplateDefaults::create(SlopeTemplateKind kind)
{
    SlopeTemplateData data;
    data.properties.kind = kind;
    data.properties.name = L"\u8fb9\u5761\u6a21\u677f1";
    data.properties.displayScale = 10.0;

    data.components.push_back(makeSlopeComponent(kind));
    data.components.push_back(makeBermComponent(kind));
    data.components.push_back(makeSlopeComponent(kind));
    data.components.push_back(makeBermComponent(kind));
    data.components.push_back(makeSlopeComponent(kind));

    return data;
}

bool SlopeTemplateRules::isSupportedDisplayScale(double displayScale)
{
    return std::isfinite(displayScale)
        && (std::fabs(displayScale - 1.0) < kGeometryTolerance
            || std::fabs(displayScale - 10.0) < kGeometryTolerance
            || std::fabs(displayScale - 20.0) < kGeometryTolerance
            || std::fabs(displayScale - 50.0) < kGeometryTolerance
            || std::fabs(displayScale - 100.0) < kGeometryTolerance);
}

SlopeResolvedGeometry SlopeTemplateRules::resolveGeometry(const SlopeTemplateComponent& component)
{
    SlopeResolvedGeometry result;
    switch (component.constraintMode) {
    case SlopeGeometryConstraintMode::SlopeAndHeight:
        if (!isFinite(component.slope) || std::fabs(component.slope) <= kGeometryTolerance ||
            !isFiniteNonNegative(component.height)) {
            result.errorMessage = L"Slope template component requires finite non-zero slope and non-negative height.";
            return result;
        }
        result.slope = component.slope;
        result.height = component.height;
        result.width = component.height / std::fabs(component.slope);
        result.elevationDelta = component.slope > 0.0 ? component.height : -component.height;
        break;
    case SlopeGeometryConstraintMode::SlopeAndWidth:
        if (!isFinite(component.slope) || std::fabs(component.slope) <= kGeometryTolerance ||
            !isFiniteNonNegative(component.width)) {
            result.errorMessage = L"Slope template component requires finite non-zero slope and non-negative width.";
            return result;
        }
        result.slope = component.slope;
        result.width = component.width;
        result.elevationDelta = component.width * component.slope;
        result.height = std::fabs(result.elevationDelta);
        break;
    case SlopeGeometryConstraintMode::HeightAndWidth:
        if (!isFiniteNonNegative(component.height) ||
            !isFinite(component.width) || component.width <= kGeometryTolerance) {
            result.errorMessage = L"Slope template component requires non-negative height and positive width.";
            return result;
        }
        result.width = component.width;
        result.height = component.height;
        result.slope = signForHeightAndWidth(component) * component.height / component.width;
        result.elevationDelta = signForHeightAndWidth(component) * component.height;
        break;
    default:
        result.errorMessage = L"Slope template component has unsupported geometry mode.";
        return result;
    }

    result.succeeded = true;
    return result;
}

std::optional<SlopeRepeatGroup> SlopeTemplateRules::lastRepeatGroup(const SlopeTemplateData& data)
{
    if (data.components.size() < 2) {
        return std::nullopt;
    }

    const std::size_t bermIndex = data.components.size() - 2;
    const std::size_t slopeIndex = data.components.size() - 1;
    if (data.components[bermIndex].type == SlopeComponentType::Berm &&
        isSlopeComponent(data.components[slopeIndex].type)) {
        return SlopeRepeatGroup{bermIndex, slopeIndex};
    }

    return std::nullopt;
}

bool SlopeTemplateRules::normalize(SlopeTemplateData& data, std::wstring& errorMessage)
{
    errorMessage.clear();
    if (data.properties.name.empty()) {
        data.properties.name = L"\u8fb9\u5761\u6a21\u677f1";
    }
    if (!isSupportedDisplayScale(data.properties.displayScale)) {
        errorMessage = L"Slope template display scale must be 1, 10, 20, 50, or 100.";
        return false;
    }
    if (data.components.empty()) {
        errorMessage = L"Slope template requires at least one component.";
        return false;
    }

    if (data.properties.repeatLastGroupUntilGround && !lastRepeatGroup(data).has_value()) {
        errorMessage = L"Slope template repeat-until-ground requires a final berm plus slope group.";
        return false;
    }

    for (auto& component : data.components) {
        if (!isFinite(component.slope) || !isFinite(component.height) || !isFinite(component.width) ||
            !isFiniteNonNegative(component.groundSearchHeightIncrement)) {
            errorMessage = L"Slope template component contains invalid numeric values.";
            return false;
        }
        const auto resolved = resolveGeometry(component);
        if (!resolved.succeeded) {
            errorMessage = resolved.errorMessage;
            return false;
        }
        component.slope = resolved.slope;
        component.width = resolved.width;
        component.height = resolved.height;
        component.color.r = std::clamp(component.color.r, 0, 255);
        component.color.g = std::clamp(component.color.g, 0, 255);
        component.color.b = std::clamp(component.color.b, 0, 255);
    }

    return true;
}

const wchar_t* slopeTemplateKindCode(SlopeTemplateKind kind)
{
    return kind == SlopeTemplateKind::Cut ? L"Cut" : L"Fill";
}

const wchar_t* slopeComponentTypeCode(SlopeComponentType type)
{
    switch (type) {
    case SlopeComponentType::FillSlope:
        return L"FillSlope";
    case SlopeComponentType::CutSlope:
        return L"CutSlope";
    case SlopeComponentType::Berm:
    default:
        return L"Berm";
    }
}

const wchar_t* slopeComponentTypeDisplayName(SlopeComponentType type)
{
    switch (type) {
    case SlopeComponentType::FillSlope:
        return L"\u586b\u65b9\u8fb9\u5761";
    case SlopeComponentType::CutSlope:
        return L"\u6316\u65b9\u8fb9\u5761";
    case SlopeComponentType::Berm:
    default:
        return L"\u62a4\u5761\u9053";
    }
}

const wchar_t* slopeGeometryConstraintModeCode(SlopeGeometryConstraintMode mode)
{
    switch (mode) {
    case SlopeGeometryConstraintMode::SlopeAndHeight:
        return L"SlopeAndHeight";
    case SlopeGeometryConstraintMode::SlopeAndWidth:
        return L"SlopeAndWidth";
    case SlopeGeometryConstraintMode::HeightAndWidth:
    default:
        return L"HeightAndWidth";
    }
}

SlopeTemplateKind slopeTemplateKindFromCode(const std::wstring& code, SlopeTemplateKind fallback)
{
    if (code == L"Fill") {
        return SlopeTemplateKind::Fill;
    }
    if (code == L"Cut") {
        return SlopeTemplateKind::Cut;
    }
    return fallback;
}

SlopeComponentType slopeComponentTypeFromCode(
    const std::wstring& code,
    SlopeComponentType fallback)
{
    if (code == L"FillSlope") {
        return SlopeComponentType::FillSlope;
    }
    if (code == L"CutSlope") {
        return SlopeComponentType::CutSlope;
    }
    if (code == L"Berm") {
        return SlopeComponentType::Berm;
    }
    return fallback;
}

SlopeGeometryConstraintMode slopeGeometryConstraintModeFromCode(
    const std::wstring& code,
    SlopeGeometryConstraintMode fallback)
{
    if (code == L"SlopeAndHeight") {
        return SlopeGeometryConstraintMode::SlopeAndHeight;
    }
    if (code == L"SlopeAndWidth") {
        return SlopeGeometryConstraintMode::SlopeAndWidth;
    }
    if (code == L"HeightAndWidth") {
        return SlopeGeometryConstraintMode::HeightAndWidth;
    }
    return fallback;
}

} // namespace roadproto::domain::cross_section
