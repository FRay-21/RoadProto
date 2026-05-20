#include "domain/cross_section/PavementLayerTemplateModel.h"

#include <cmath>

namespace roadproto::domain::cross_section {
namespace {

constexpr double kTolerance = 1.0e-9;

bool isFinite(double value)
{
    return std::isfinite(value);
}

bool isPositiveFinite(double value)
{
    return std::isfinite(value) && value > 0.0;
}

bool isFiniteNonNegative(double value)
{
    return std::isfinite(value) && value >= 0.0;
}

PavementLayerTemplateLayer makeLayer(
    PavementLayerType type,
    double thickness,
    double innerWidening,
    double outerWidening)
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
    return layer;
}

} // namespace

PavementLayerTemplateData PavementLayerTemplateDefaults::create()
{
    PavementLayerTemplateData data;
    data.properties.name = L"\u8def\u9762\u7ed3\u6784\u5c42\u6a21\u677f";
    data.properties.displayScale = 10.0;
    data.properties.previewWidth = 3.75;
    data.layers = {
        makeLayer(PavementLayerType::UpperSurface, 0.04, 0.0, 0.0),
        makeLayer(PavementLayerType::MiddleSurface, 0.06, 0.0, 0.0),
        makeLayer(PavementLayerType::LowerSurface, 0.08, 0.0, 0.0),
        makeLayer(PavementLayerType::Base, 0.18, 0.15, 0.15),
        makeLayer(PavementLayerType::Subbase, 0.20, 0.15, 0.15),
        makeLayer(PavementLayerType::Cushion, 0.15, 0.15, 0.15),
    };
    return data;
}

bool PavementLayerTemplateRules::isSupportedDisplayScale(double displayScale)
{
    return std::isfinite(displayScale)
        && (std::fabs(displayScale - 1.0) < kTolerance
            || std::fabs(displayScale - 10.0) < kTolerance
            || std::fabs(displayScale - 20.0) < kTolerance
            || std::fabs(displayScale - 50.0) < kTolerance
            || std::fabs(displayScale - 100.0) < kTolerance);
}

bool PavementLayerTemplateRules::isSupportedPreviewWidth(double previewWidth)
{
    return isPositiveFinite(previewWidth);
}

bool PavementLayerTemplateRules::normalize(PavementLayerTemplateData& data, std::wstring& errorMessage)
{
    errorMessage.clear();
    if (data.properties.name.empty()) {
        data.properties.name = L"\u8def\u9762\u7ed3\u6784\u5c42\u6a21\u677f";
    }
    if (!isSupportedDisplayScale(data.properties.displayScale)) {
        errorMessage = L"Pavement layer template display scale must be 1, 10, 20, 50, or 100.";
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

    for (auto& layer : data.layers) {
        if (layer.name.empty()) {
            layer.name = pavementLayerTypeDisplayName(layer.type);
        }
        if (!isFinite(layer.thickness) || !isFinite(layer.innerThickness) || !isFinite(layer.outerThickness) ||
            !isFinite(layer.innerSlope) || !isFinite(layer.outerSlope) ||
            !isFiniteNonNegative(layer.innerWidening) || !isFiniteNonNegative(layer.outerWidening)) {
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
        PavementLayerSectionLayer sectionLayer;
        sectionLayer.type = layer.type;
        sectionLayer.name = layer.name;
        sectionLayer.topInner = {topInnerOffset, currentTopInnerElevation};
        sectionLayer.topOuter = {topOuterOffset, currentTopOuterElevation};
        sectionLayer.bottomInner = {topInnerOffset + layer.innerWidening, currentTopInnerElevation - layer.innerThickness};
        sectionLayer.bottomOuter = {topOuterOffset + layer.outerWidening, currentTopOuterElevation - layer.outerThickness};
        sectionLayer.innerSlope = layer.innerSlope;
        sectionLayer.outerSlope = layer.outerSlope;
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
