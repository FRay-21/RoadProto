#include "domain/cross_section/PavementLayerTemplateModel.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::cross_section {
namespace {

bool isFinite(double value)
{
    return std::isfinite(value);
}

bool isPositiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

PavementLayerTemplateDisplayColor defaultLayerColorForIndex(std::size_t index)
{
    switch (index % 6) {
    case 0:
        return {65, 174, 221};
    case 1:
        return {79, 203, 137};
    case 2:
        return {250, 197, 83};
    case 3:
        return {236, 132, 80};
    case 4:
        return {177, 138, 230};
    default:
        return {142, 164, 180};
    }
}

bool isMissingLayerColor(const PavementLayerTemplateDisplayColor& color)
{
    return color.r < 0 || color.g < 0 || color.b < 0;
}

int normalizeColorChannel(int value)
{
    return std::clamp(value, 0, 255);
}

PavementLayerTemplateLayer makeLayer(
    PavementLayerType type,
    double thickness,
    double innerWidening,
    double outerWidening,
    std::size_t colorIndex)
{
    PavementLayerTemplateLayer layer;
    layer.type = type;
    layer.name = pavementLayerTypeDisplayName(type);
    layer.uniformThickness = true;
    layer.thickness = thickness;
    layer.innerThickness = thickness;
    layer.outerThickness = thickness;
    layer.innerWidening = innerWidening;
    layer.outerWidening = outerWidening;
    layer.color = defaultLayerColorForIndex(colorIndex);
    layer.hatchPattern = L"SOLID";
    return layer;
}

double sideInsetFromSlope(double thickness, double slope)
{
    if (!isPositiveFinite(thickness) || !isFinite(slope)) {
        return 0.0;
    }
    return -thickness * slope;
}

double gradeBetween(
    double innerOffset,
    double innerElevation,
    double outerOffset,
    double outerElevation)
{
    const double width = outerOffset - innerOffset;
    if (std::fabs(width) <= 1.0e-9) {
        return 0.0;
    }
    return (outerElevation - innerElevation) / width;
}

void clampSideInsets(double topWidth, double& innerInset, double& outerInset)
{
    const double totalInset = innerInset + outerInset;
    if (topWidth <= 0.0 || totalInset <= topWidth) {
        return;
    }

    const double scale = topWidth / totalInset;
    innerInset *= scale;
    outerInset *= scale;
}

} // namespace

PavementLayerTemplateData PavementLayerTemplateDefaults::create()
{
    PavementLayerTemplateData data;
    data.properties.name = L"\u8def\u9762\u7ed3\u6784\u5c42\u6a21\u677f";
    data.properties.displayScale = 10.0;
    data.properties.previewWidth = 3.75;
    data.layers = {
        makeLayer(PavementLayerType::UpperSurface, 0.04, 0.0, 0.0, 0),
        makeLayer(PavementLayerType::MiddleSurface, 0.06, 0.0, 0.0, 1),
        makeLayer(PavementLayerType::LowerSurface, 0.08, 0.0, 0.0, 2),
        makeLayer(PavementLayerType::Base, 0.18, 0.15, 0.15, 3),
        makeLayer(PavementLayerType::Subbase, 0.20, 0.15, 0.15, 4),
        makeLayer(PavementLayerType::Cushion, 0.15, 0.15, 0.15, 5),
    };
    return data;
}

bool PavementLayerTemplateRules::isSupportedDisplayScale(double displayScale)
{
    return isPositiveFinite(displayScale);
}

bool PavementLayerTemplateRules::isSupportedPreviewWidth(double previewWidth)
{
    return isPositiveFinite(previewWidth);
}

bool PavementLayerTemplateRules::isSupportedHatchPattern(const std::wstring& hatchPattern)
{
    return hatchPattern == L"SOLID"
        || hatchPattern == L"ANSI31"
        || hatchPattern == L"ANSI32"
        || hatchPattern == L"ANSI33"
        || hatchPattern == L"ANSI34"
        || hatchPattern == L"ANSI35"
        || hatchPattern == L"ANSI36"
        || hatchPattern == L"ANSI37"
        || hatchPattern == L"ANSI38"
        || hatchPattern == L"CROSS"
        || hatchPattern == L"DOTS"
        || hatchPattern == L"GRAVEL"
        || hatchPattern == L"EARTH";
}

PavementLayerTemplateDisplayColor PavementLayerTemplateRules::displayColorForLayerIndex(std::size_t index)
{
    return defaultLayerColorForIndex(index);
}

const wchar_t* PavementLayerTemplateRules::displayModeCode(PavementLayerTemplateDisplayMode mode)
{
    switch (mode) {
    case PavementLayerTemplateDisplayMode::Color:
        return L"Color";
    case PavementLayerTemplateDisplayMode::Hatch:
        return L"Hatch";
    case PavementLayerTemplateDisplayMode::HatchAndColor:
        return L"HatchAndColor";
    default:
        return L"Color";
    }
}

PavementLayerTemplateDisplayMode PavementLayerTemplateRules::displayModeFromCode(
    const std::wstring& code,
    PavementLayerTemplateDisplayMode fallback)
{
    if (code == L"Color") {
        return PavementLayerTemplateDisplayMode::Color;
    }
    if (code == L"Hatch") {
        return PavementLayerTemplateDisplayMode::Hatch;
    }
    if (code == L"HatchAndColor") {
        return PavementLayerTemplateDisplayMode::HatchAndColor;
    }
    return fallback;
}

bool PavementLayerTemplateRules::normalize(PavementLayerTemplateData& data, std::wstring& errorMessage)
{
    errorMessage.clear();
    if (data.properties.name.empty()) {
        data.properties.name = L"\u8def\u9762\u7ed3\u6784\u5c42\u6a21\u677f";
    }
    if (!isSupportedDisplayScale(data.properties.displayScale)) {
        errorMessage = L"Pavement layer template display scale must be positive and finite.";
        return false;
    }
    if (!isSupportedPreviewWidth(data.properties.previewWidth)) {
        errorMessage = L"Pavement layer template preview width must be positive and finite.";
        return false;
    }
    if (data.layers.empty()) {
        errorMessage = L"Pavement layer template requires at least one layer.";
        return false;
    }

    for (std::size_t layerIndex = 0; layerIndex < data.layers.size(); ++layerIndex) {
        auto& layer = data.layers[layerIndex];
        if (layer.name.empty()) {
            layer.name = pavementLayerTypeDisplayName(layer.type);
        }
        if (!isFinite(layer.thickness) || !isFinite(layer.innerThickness) || !isFinite(layer.outerThickness) ||
            !isFinite(layer.innerSlope) || !isFinite(layer.outerSlope) ||
            !isFinite(layer.innerWidening) || !isFinite(layer.outerWidening)) {
            errorMessage = L"Pavement layer template layer contains invalid numeric values.";
            return false;
        }

        if (layer.uniformThickness) {
            if (!isPositiveFinite(layer.thickness)) {
                errorMessage = L"Pavement layer template uniform thickness must be positive and finite.";
                return false;
            }
            layer.innerThickness = layer.thickness;
            layer.outerThickness = layer.thickness;
        } else if (!isPositiveFinite(layer.innerThickness) || !isPositiveFinite(layer.outerThickness)) {
            errorMessage = L"Pavement layer template inner and outer thickness must be positive and finite.";
            return false;
        }

        if (isMissingLayerColor(layer.color)) {
            layer.color = displayColorForLayerIndex(layerIndex);
        } else {
            layer.color.r = normalizeColorChannel(layer.color.r);
            layer.color.g = normalizeColorChannel(layer.color.g);
            layer.color.b = normalizeColorChannel(layer.color.b);
        }
        if (!isSupportedHatchPattern(layer.hatchPattern)) {
            layer.hatchPattern = L"SOLID";
        }
    }

    return true;
}

PavementLayerTemplateSection PavementLayerTemplateRules::buildSection(
    const PavementLayerTemplateData& data,
    double baseWidth,
    SubgradeSide side,
    double topInnerElevation,
    double topOuterElevation)
{
    PavementLayerTemplateSection section;
    section.side = side;
    if (!isPositiveFinite(baseWidth) || !isFinite(topInnerElevation) || !isFinite(topOuterElevation)) {
        section.errorMessage = L"Pavement layer section requires finite elevations and positive base width.";
        return section;
    }

    auto normalized = data;
    if (!normalize(normalized, section.errorMessage)) {
        return section;
    }

    double topInnerOffset = 0.0;
    double topOuterOffset = baseWidth;
    double currentTopInnerElevation = topInnerElevation;
    double currentTopOuterElevation = topOuterElevation;

    for (const auto& layer : normalized.layers) {
        const double inheritedTopWidth = topOuterOffset - topInnerOffset;
        if (inheritedTopWidth <= 0.0) {
            section.errorMessage = L"Pavement layer template inherited top width must remain positive.";
            return section;
        }
        const double inheritedTopGrade = gradeBetween(
            topInnerOffset,
            currentTopInnerElevation,
            topOuterOffset,
            currentTopOuterElevation);
        const double layerTopInnerOffset = topInnerOffset - layer.innerWidening;
        const double layerTopOuterOffset = topOuterOffset + layer.outerWidening;
        const double layerTopInnerElevation = currentTopInnerElevation - layer.innerWidening * inheritedTopGrade;
        const double layerTopOuterElevation = currentTopOuterElevation + layer.outerWidening * inheritedTopGrade;
        const double layerTopWidth = layerTopOuterOffset - layerTopInnerOffset;
        if (layerTopWidth <= 0.0) {
            section.errorMessage = L"Pavement layer template layer top width must remain positive.";
            return section;
        }
        double innerInset = sideInsetFromSlope(layer.innerThickness, layer.innerSlope);
        double outerInset = sideInsetFromSlope(layer.outerThickness, layer.outerSlope);
        clampSideInsets(layerTopWidth, innerInset, outerInset);

        PavementLayerSectionLayer sectionLayer;
        sectionLayer.type = layer.type;
        sectionLayer.name = layer.name;
        sectionLayer.topInner = {layerTopInnerOffset, layerTopInnerElevation};
        sectionLayer.topOuter = {layerTopOuterOffset, layerTopOuterElevation};
        sectionLayer.bottomInner = {layerTopInnerOffset + innerInset, layerTopInnerElevation - layer.innerThickness};
        sectionLayer.bottomOuter = {layerTopOuterOffset - outerInset, layerTopOuterElevation - layer.outerThickness};
        sectionLayer.innerSlope = layer.innerSlope;
        sectionLayer.outerSlope = layer.outerSlope;
        sectionLayer.color = layer.color;
        section.layers.push_back(sectionLayer);

        topInnerOffset = sectionLayer.bottomInner.offset;
        topOuterOffset = sectionLayer.bottomOuter.offset;
        currentTopInnerElevation = sectionLayer.bottomInner.elevation;
        currentTopOuterElevation = sectionLayer.bottomOuter.elevation;
    }

    section.succeeded = true;
    return section;
}

const wchar_t* pavementLayerTypeCode(PavementLayerType type)
{
    switch (type) {
    case PavementLayerType::UpperSurface:
        return L"UpperSurface";
    case PavementLayerType::MiddleSurface:
        return L"MiddleSurface";
    case PavementLayerType::LowerSurface:
        return L"LowerSurface";
    case PavementLayerType::Base:
        return L"Base";
    case PavementLayerType::Subbase:
        return L"Subbase";
    case PavementLayerType::Cushion:
        return L"Cushion";
    default:
        return L"UpperSurface";
    }
}

const wchar_t* pavementLayerTypeDisplayName(PavementLayerType type)
{
    switch (type) {
    case PavementLayerType::UpperSurface:
        return L"\u4e0a\u9762\u5c42";
    case PavementLayerType::MiddleSurface:
        return L"\u4e2d\u9762\u5c42";
    case PavementLayerType::LowerSurface:
        return L"\u4e0b\u9762\u5c42";
    case PavementLayerType::Base:
        return L"\u57fa\u5c42";
    case PavementLayerType::Subbase:
        return L"\u5e95\u57fa\u5c42";
    case PavementLayerType::Cushion:
        return L"\u57ab\u5c42";
    default:
        return L"\u4e0a\u9762\u5c42";
    }
}

PavementLayerType pavementLayerTypeFromCode(
    const std::wstring& code,
    PavementLayerType fallback)
{
    if (code == L"UpperSurface") {
        return PavementLayerType::UpperSurface;
    }
    if (code == L"MiddleSurface") {
        return PavementLayerType::MiddleSurface;
    }
    if (code == L"LowerSurface") {
        return PavementLayerType::LowerSurface;
    }
    if (code == L"Base") {
        return PavementLayerType::Base;
    }
    if (code == L"Subbase") {
        return PavementLayerType::Subbase;
    }
    if (code == L"Cushion") {
        return PavementLayerType::Cushion;
    }
    return fallback;
}

} // namespace roadproto::domain::cross_section
