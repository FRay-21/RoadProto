#pragma once

#include "domain/cross_section/SubgradeTemplateModel.h"

#include <string>
#include <vector>

namespace roadproto::domain::cross_section {

enum class PavementLayerType {
    UpperSurface,
    MiddleSurface,
    LowerSurface,
    Base,
    Subbase,
    Cushion
};

struct PavementLayerTemplateLayer {
    PavementLayerType type = PavementLayerType::UpperSurface;
    std::wstring name;
    bool uniformThickness = true;
    double thickness = 0.04;
    double innerThickness = 0.04;
    double outerThickness = 0.04;
    double innerWidening = 0.0;
    double outerWidening = 0.0;
    double innerSlope = 0.0;
    double outerSlope = 0.0;
};

struct PavementLayerTemplateProperties {
    std::wstring name = L"\u8def\u9762\u7ed3\u6784\u5c42\u6a21\u677f";
    double displayScale = 10.0;
    double previewWidth = 3.75;
};

struct PavementLayerTemplateData {
    PavementLayerTemplateProperties properties;
    std::vector<PavementLayerTemplateLayer> layers;
};

struct PavementLayerSectionPoint {
    double offset = 0.0;
    double elevation = 0.0;
};

struct PavementLayerSectionLayer {
    PavementLayerType type = PavementLayerType::UpperSurface;
    std::wstring name;
    PavementLayerSectionPoint topInner;
    PavementLayerSectionPoint topOuter;
    PavementLayerSectionPoint bottomInner;
    PavementLayerSectionPoint bottomOuter;
    double innerSlope = 0.0;
    double outerSlope = 0.0;
};

struct PavementLayerTemplateSection {
    bool succeeded = false;
    std::wstring errorMessage;
    SubgradeSide side = SubgradeSide::Right;
    std::vector<PavementLayerSectionLayer> layers;
};

class PavementLayerTemplateDefaults {
public:
    static PavementLayerTemplateData create();
};

class PavementLayerTemplateRules {
public:
    static bool isSupportedDisplayScale(double displayScale);
    static bool isSupportedPreviewWidth(double previewWidth);
    static bool normalize(PavementLayerTemplateData& data, std::wstring& errorMessage);
    static PavementLayerTemplateSection buildSection(
        const PavementLayerTemplateData& data,
        double baseWidth,
        SubgradeSide side,
        double topInnerElevation,
        double topOuterElevation);
};

const wchar_t* pavementLayerTypeCode(PavementLayerType type);
const wchar_t* pavementLayerTypeDisplayName(PavementLayerType type);
PavementLayerType pavementLayerTypeFromCode(
    const std::wstring& code,
    PavementLayerType fallback = PavementLayerType::UpperSurface);

} // namespace roadproto::domain::cross_section
