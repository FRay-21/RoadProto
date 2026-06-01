#pragma once

#include <string>
#include <vector>

namespace roadproto::domain::quantity {

enum class PavementQuantitySegmentType {
    Normal,
    Bridge,
    Tunnel,
};

enum class PavementQuantityAggregationMode {
    ByLayerType,
    ByComponentAndLayer,
};

enum class PavementQuantityCalculationMethod {
    AverageEndArea,
    PlanAreaByThickness,
};

struct PavementQuantityStructureRange {
    double startStation = 0.0;
    double endStation = 0.0;
    PavementQuantitySegmentType type = PavementQuantitySegmentType::Bridge;
};

struct PavementQuantityLayerSample {
    std::wstring layerName;
    double projectedWidth = 0.0;
    double sectionArea = 0.0;
    std::wstring componentName;
};

struct PavementQuantitySectionSample {
    double station = 0.0;
    std::vector<PavementQuantityLayerSample> layers;
};

struct PavementQuantityLayerTotals {
    double projectedArea = 0.0;
    double volume = 0.0;
};

struct PavementQuantitySegmentRow {
    int sequence = 0;
    double startStation = 0.0;
    double endStation = 0.0;
    PavementQuantitySegmentType type = PavementQuantitySegmentType::Normal;
    std::vector<PavementQuantityLayerTotals> totals;
};

struct PavementQuantityTable {
    std::vector<std::wstring> layerNames;
    std::vector<PavementQuantitySegmentRow> rows;
};

const wchar_t* pavementQuantitySegmentTypeDisplayName(PavementQuantitySegmentType type);

class PavementQuantityTableCalculator {
public:
    static PavementQuantityTable build(
        const std::vector<PavementQuantitySectionSample>& samples,
        const std::vector<PavementQuantityStructureRange>& structures,
        std::wstring& errorMessage);
    static PavementQuantityTable build(
        const std::vector<PavementQuantitySectionSample>& samples,
        const std::vector<PavementQuantityStructureRange>& structures,
        PavementQuantityAggregationMode aggregationMode,
        std::wstring& errorMessage);
    static PavementQuantityTable build(
        const std::vector<PavementQuantitySectionSample>& samples,
        const std::vector<PavementQuantityStructureRange>& structures,
        PavementQuantityAggregationMode aggregationMode,
        PavementQuantityCalculationMethod calculationMethod,
        std::wstring& errorMessage);
};

class PavementQuantityTableXlsWriter {
public:
    static bool write(
        const std::wstring& path,
        const PavementQuantityTable& table,
        std::wstring& errorMessage);
};

} // namespace roadproto::domain::quantity
