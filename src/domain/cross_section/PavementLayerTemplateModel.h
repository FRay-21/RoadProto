#pragma once

#include "domain/cross_section/SubgradeTemplateModel.h"

#include <cstddef>
#include <string>
#include <vector>

namespace roadproto::domain::cross_section {

enum class PavementLayerType {
    UpperSurface,
    MiddleSurface,
    LowerSurface,
    Base,
    Subbase,
    Cushion,
    AsphaltSeal,
    ApproachSlab
};

enum class PavementLayerTemplateDisplayMode {
    Color,
    Hatch,
    HatchAndColor
};

enum class PavementSubgradeMoistureType {
    Dry,
    Medium,
    Wet,
    OverWet
};

enum class PavementSurfaceType {
    Asphalt,
    Concrete
};

enum class PavementSubgradeSoilGroup {
    Bedrock,
    CrushedStoneSoil,
    GravelSoil,
    SandSoil,
    SiltySoil,
    LowLiquidLimitClay,
    HighLiquidLimitClay,
    OrganicSoil,
    SoftSoil,
    ExpansiveSoil,
    Loess,
    Other
};

struct PavementLayerTemplateDisplayColor {
    int r = -1;
    int g = -1;
    int b = -1;
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
    PavementLayerTemplateDisplayColor color;
    std::wstring hatchPattern = L"SOLID";
    double hatchAngle = 0.0;
    double hatchScale = 1.0;
};

struct PavementLayerTemplateProperties {
    std::wstring name = L"\u8def\u9762\u7ed3\u6784\u5c42\u6a21\u677f";
    double displayScale = 10.0;
    double previewWidth = 3.75;
    PavementLayerTemplateDisplayMode displayMode = PavementLayerTemplateDisplayMode::Color;
    bool showAllGeneralParameters = false;
    std::wstring structureCode;
    std::vector<PavementSubgradeMoistureType> subgradeMoistureTypes;
    PavementSurfaceType pavementType = PavementSurfaceType::Asphalt;
    std::vector<PavementSubgradeSoilGroup> subgradeSoilGroups;
    std::wstring designDeflection;
    std::wstring cumulativeAxleLoads;
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
    PavementLayerTemplateDisplayColor color;
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
    static bool isSupportedHatchPattern(const std::wstring& hatchPattern);
    static PavementLayerTemplateDisplayColor displayColorForLayerIndex(std::size_t index);
    static const wchar_t* displayModeCode(PavementLayerTemplateDisplayMode mode);
    static PavementLayerTemplateDisplayMode displayModeFromCode(
        const std::wstring& code,
        PavementLayerTemplateDisplayMode fallback = PavementLayerTemplateDisplayMode::Color);
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

const wchar_t* pavementSubgradeMoistureTypeCode(PavementSubgradeMoistureType type);
const wchar_t* pavementSubgradeMoistureTypeDisplayName(PavementSubgradeMoistureType type);
PavementSubgradeMoistureType pavementSubgradeMoistureTypeFromCode(
    const std::wstring& code,
    PavementSubgradeMoistureType fallback = PavementSubgradeMoistureType::Dry);

const wchar_t* pavementSurfaceTypeCode(PavementSurfaceType type);
const wchar_t* pavementSurfaceTypeDisplayName(PavementSurfaceType type);
PavementSurfaceType pavementSurfaceTypeFromCode(
    const std::wstring& code,
    PavementSurfaceType fallback = PavementSurfaceType::Asphalt);

const wchar_t* pavementSubgradeSoilGroupCode(PavementSubgradeSoilGroup group);
const wchar_t* pavementSubgradeSoilGroupDisplayName(PavementSubgradeSoilGroup group);
PavementSubgradeSoilGroup pavementSubgradeSoilGroupFromCode(
    const std::wstring& code,
    PavementSubgradeSoilGroup fallback = PavementSubgradeSoilGroup::Other);

} // namespace roadproto::domain::cross_section
