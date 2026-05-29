#pragma once

#include "domain/quantity/PavementQuantityTable.h"

#include <optional>
#include <string>
#include <vector>

namespace roadproto::domain::quantity {

struct PavementQuantityDrawingPoint {
    double x = 0.0;
    double y = 0.0;
};

struct PavementQuantityDrawingFace {
    std::wstring layerName;
    std::wstring componentName;
    std::vector<PavementQuantityDrawingPoint> points;
};

class PavementQuantityDrawingFaceSampler {
public:
    static std::optional<PavementQuantitySectionSample> sampleAtStation(
        double station,
        const std::vector<PavementQuantityDrawingFace>& faces);
};

} // namespace roadproto::domain::quantity
