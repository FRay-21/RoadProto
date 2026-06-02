#include "domain/cross_section/PavementLayerTemplateModel.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

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
    layer.hatchAngle = 0.0;
    layer.hatchScale = 1.0;
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

bool isSupportedMoistureType(PavementSubgradeMoistureType type)
{
    switch (type) {
    case PavementSubgradeMoistureType::Dry:
    case PavementSubgradeMoistureType::Medium:
    case PavementSubgradeMoistureType::Wet:
    case PavementSubgradeMoistureType::OverWet:
        return true;
    default:
        return false;
    }
}

bool isSupportedSurfaceType(PavementSurfaceType type)
{
    switch (type) {
    case PavementSurfaceType::Asphalt:
    case PavementSurfaceType::Concrete:
        return true;
    default:
        return false;
    }
}

bool isSupportedSoilGroup(PavementSubgradeSoilGroup group)
{
    switch (group) {
    case PavementSubgradeSoilGroup::Bedrock:
    case PavementSubgradeSoilGroup::CrushedStoneSoil:
    case PavementSubgradeSoilGroup::GravelSoil:
    case PavementSubgradeSoilGroup::SandSoil:
    case PavementSubgradeSoilGroup::SiltySoil:
    case PavementSubgradeSoilGroup::LowLiquidLimitClay:
    case PavementSubgradeSoilGroup::HighLiquidLimitClay:
    case PavementSubgradeSoilGroup::OrganicSoil:
    case PavementSubgradeSoilGroup::SoftSoil:
    case PavementSubgradeSoilGroup::ExpansiveSoil:
    case PavementSubgradeSoilGroup::Loess:
    case PavementSubgradeSoilGroup::Other:
        return true;
    default:
        return false;
    }
}

template <typename T, typename Predicate>
void normalizeUniqueEnumList(std::vector<T>& values, Predicate isSupported)
{
    std::vector<T> normalized;
    normalized.reserve(values.size());
    for (const auto value : values) {
        if (!isSupported(value)) {
            continue;
        }
        if (std::find(normalized.begin(), normalized.end(), value) == normalized.end()) {
            normalized.push_back(value);
        }
    }
    values = std::move(normalized);
}

} // namespace

PavementLayerTemplateData PavementLayerTemplateDefaults::create()
{
    PavementLayerTemplateData data;
    data.properties.name = L"\u8def\u9762\u7ed3\u6784\u5c42\u6a21\u677f";
    data.properties.displayScale = 10.0;
    data.properties.previewWidth = 3.0;
    data.layers = {
        makeLayer(PavementLayerType::UpperSurface, 0.04, 0.0, 0.0, 0),
        makeLayer(PavementLayerType::MiddleSurface, 0.06, 0.0, 0.0, 1),
        makeLayer(PavementLayerType::LowerSurface, 0.08, 0.0, 0.0, 2),
        makeLayer(PavementLayerType::AsphaltSeal, 0.01, 0.0, 0.0, 3),
        makeLayer(PavementLayerType::Base, 0.18, 0.15, 0.15, 4),
        makeLayer(PavementLayerType::Subbase, 0.20, 0.15, 0.15, 5),
        makeLayer(PavementLayerType::Cushion, 0.15, 0.15, 0.15, 6),
        makeLayer(PavementLayerType::ApproachSlab, 0.35, 0.0, 0.0, 7),
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
    static constexpr std::array<const wchar_t*, 57> kSupportedPatterns = {
        L"SOLID",
        L"ANSI31", L"ANSI32", L"ANSI33", L"ANSI34", L"ANSI35", L"ANSI36", L"ANSI37", L"ANSI38",
        L"AR-B816", L"AR-B816C", L"AR-B88", L"AR-BRELM", L"AR-BRSTD", L"AR-CONC",
        L"AR-HBONE", L"AR-PARQ1", L"AR-RROOF", L"AR-RSHKE", L"AR-SAND",
        L"BOX", L"BRASS", L"BRICK", L"BRSTONE", L"CLAY", L"CORK", L"CROSS", L"DASH",
        L"DOLMIT", L"DOTS", L"EARTH", L"ESCHER", L"FLEX", L"GOST_GLASS", L"GOST_GROUND",
        L"GOST_WOOD", L"GRASS", L"GRATE", L"GRAVEL", L"HEX", L"HONEY", L"HOUND",
        L"INSUL", L"LINE", L"MUDST", L"NET", L"NET3", L"PLAST", L"PLASTI",
        L"SACNCR", L"SQUARE", L"STARS", L"STEEL", L"SWAMP", L"TRANS", L"TRIANG",
        L"ZIGZAG"};
    return std::find(kSupportedPatterns.begin(), kSupportedPatterns.end(), hatchPattern) != kSupportedPatterns.end();
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

    if (!isSupportedSurfaceType(data.properties.pavementType)) {
        data.properties.pavementType = PavementSurfaceType::Asphalt;
    }
    normalizeUniqueEnumList(data.properties.subgradeMoistureTypes, isSupportedMoistureType);
    normalizeUniqueEnumList(data.properties.subgradeSoilGroups, isSupportedSoilGroup);

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
        if (!isFinite(layer.hatchAngle)) {
            layer.hatchAngle = 0.0;
        }
        if (!isPositiveFinite(layer.hatchScale)) {
            layer.hatchScale = 1.0;
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
    case PavementLayerType::AsphaltSeal:
        return L"AsphaltSeal";
    case PavementLayerType::ApproachSlab:
        return L"ApproachSlab";
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
    case PavementLayerType::AsphaltSeal:
        return L"\u6ca5\u9752\u5c01\u5c42";
    case PavementLayerType::ApproachSlab:
        return L"\u642d\u677f";
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
    if (code == L"AsphaltSeal") {
        return PavementLayerType::AsphaltSeal;
    }
    if (code == L"ApproachSlab") {
        return PavementLayerType::ApproachSlab;
    }
    return fallback;
}

const wchar_t* pavementSubgradeMoistureTypeCode(PavementSubgradeMoistureType type)
{
    switch (type) {
    case PavementSubgradeMoistureType::Dry:
        return L"Dry";
    case PavementSubgradeMoistureType::Medium:
        return L"Medium";
    case PavementSubgradeMoistureType::Wet:
        return L"Wet";
    case PavementSubgradeMoistureType::OverWet:
        return L"OverWet";
    default:
        return L"Dry";
    }
}

const wchar_t* pavementSubgradeMoistureTypeDisplayName(PavementSubgradeMoistureType type)
{
    switch (type) {
    case PavementSubgradeMoistureType::Dry:
        return L"\u5e72\u71e5";
    case PavementSubgradeMoistureType::Medium:
        return L"\u4e2d\u6e7f";
    case PavementSubgradeMoistureType::Wet:
        return L"\u6f6e\u6e7f";
    case PavementSubgradeMoistureType::OverWet:
        return L"\u8fc7\u6e7f";
    default:
        return L"\u5e72\u71e5";
    }
}

PavementSubgradeMoistureType pavementSubgradeMoistureTypeFromCode(
    const std::wstring& code,
    PavementSubgradeMoistureType fallback)
{
    if (code == L"Dry") {
        return PavementSubgradeMoistureType::Dry;
    }
    if (code == L"Medium") {
        return PavementSubgradeMoistureType::Medium;
    }
    if (code == L"Wet") {
        return PavementSubgradeMoistureType::Wet;
    }
    if (code == L"OverWet") {
        return PavementSubgradeMoistureType::OverWet;
    }
    return fallback;
}

const wchar_t* pavementSurfaceTypeCode(PavementSurfaceType type)
{
    switch (type) {
    case PavementSurfaceType::Asphalt:
        return L"Asphalt";
    case PavementSurfaceType::Concrete:
        return L"Concrete";
    default:
        return L"Asphalt";
    }
}

const wchar_t* pavementSurfaceTypeDisplayName(PavementSurfaceType type)
{
    switch (type) {
    case PavementSurfaceType::Asphalt:
        return L"\u6ca5\u9752\u8def\u9762";
    case PavementSurfaceType::Concrete:
        return L"\u6df7\u51dd\u571f\u8def\u9762";
    default:
        return L"\u6ca5\u9752\u8def\u9762";
    }
}

PavementSurfaceType pavementSurfaceTypeFromCode(
    const std::wstring& code,
    PavementSurfaceType fallback)
{
    if (code == L"Asphalt") {
        return PavementSurfaceType::Asphalt;
    }
    if (code == L"Concrete") {
        return PavementSurfaceType::Concrete;
    }
    return fallback;
}

const wchar_t* pavementSubgradeSoilGroupCode(PavementSubgradeSoilGroup group)
{
    switch (group) {
    case PavementSubgradeSoilGroup::Bedrock:
        return L"Bedrock";
    case PavementSubgradeSoilGroup::CrushedStoneSoil:
        return L"CrushedStoneSoil";
    case PavementSubgradeSoilGroup::GravelSoil:
        return L"GravelSoil";
    case PavementSubgradeSoilGroup::SandSoil:
        return L"SandSoil";
    case PavementSubgradeSoilGroup::SiltySoil:
        return L"SiltySoil";
    case PavementSubgradeSoilGroup::LowLiquidLimitClay:
        return L"LowLiquidLimitClay";
    case PavementSubgradeSoilGroup::HighLiquidLimitClay:
        return L"HighLiquidLimitClay";
    case PavementSubgradeSoilGroup::OrganicSoil:
        return L"OrganicSoil";
    case PavementSubgradeSoilGroup::SoftSoil:
        return L"SoftSoil";
    case PavementSubgradeSoilGroup::ExpansiveSoil:
        return L"ExpansiveSoil";
    case PavementSubgradeSoilGroup::Loess:
        return L"Loess";
    case PavementSubgradeSoilGroup::Other:
        return L"Other";
    default:
        return L"Other";
    }
}

const wchar_t* pavementSubgradeSoilGroupDisplayName(PavementSubgradeSoilGroup group)
{
    switch (group) {
    case PavementSubgradeSoilGroup::Bedrock:
        return L"\u57fa\u5ca9";
    case PavementSubgradeSoilGroup::CrushedStoneSoil:
        return L"\u788e\u77f3\u571f";
    case PavementSubgradeSoilGroup::GravelSoil:
        return L"\u783e\u7c7b\u571f";
    case PavementSubgradeSoilGroup::SandSoil:
        return L"\u7802\u7c7b\u571f";
    case PavementSubgradeSoilGroup::SiltySoil:
        return L"\u7c89\u8d28\u571f";
    case PavementSubgradeSoilGroup::LowLiquidLimitClay:
        return L"\u4f4e\u6db2\u9650\u9ecf\u571f";
    case PavementSubgradeSoilGroup::HighLiquidLimitClay:
        return L"\u9ad8\u6db2\u9650\u9ecf\u571f";
    case PavementSubgradeSoilGroup::OrganicSoil:
        return L"\u6709\u673a\u8d28\u571f";
    case PavementSubgradeSoilGroup::SoftSoil:
        return L"\u8f6f\u571f";
    case PavementSubgradeSoilGroup::ExpansiveSoil:
        return L"\u81a8\u80c0\u571f";
    case PavementSubgradeSoilGroup::Loess:
        return L"\u9ec4\u571f";
    case PavementSubgradeSoilGroup::Other:
        return L"\u5176\u4ed6";
    default:
        return L"\u5176\u4ed6";
    }
}

PavementSubgradeSoilGroup pavementSubgradeSoilGroupFromCode(
    const std::wstring& code,
    PavementSubgradeSoilGroup fallback)
{
    if (code == L"Bedrock") {
        return PavementSubgradeSoilGroup::Bedrock;
    }
    if (code == L"CrushedStoneSoil") {
        return PavementSubgradeSoilGroup::CrushedStoneSoil;
    }
    if (code == L"GravelSoil") {
        return PavementSubgradeSoilGroup::GravelSoil;
    }
    if (code == L"SandSoil") {
        return PavementSubgradeSoilGroup::SandSoil;
    }
    if (code == L"SiltySoil") {
        return PavementSubgradeSoilGroup::SiltySoil;
    }
    if (code == L"LowLiquidLimitClay") {
        return PavementSubgradeSoilGroup::LowLiquidLimitClay;
    }
    if (code == L"HighLiquidLimitClay") {
        return PavementSubgradeSoilGroup::HighLiquidLimitClay;
    }
    if (code == L"OrganicSoil") {
        return PavementSubgradeSoilGroup::OrganicSoil;
    }
    if (code == L"SoftSoil") {
        return PavementSubgradeSoilGroup::SoftSoil;
    }
    if (code == L"ExpansiveSoil") {
        return PavementSubgradeSoilGroup::ExpansiveSoil;
    }
    if (code == L"Loess") {
        return PavementSubgradeSoilGroup::Loess;
    }
    if (code == L"Other") {
        return PavementSubgradeSoilGroup::Other;
    }
    return fallback;
}

} // namespace roadproto::domain::cross_section
