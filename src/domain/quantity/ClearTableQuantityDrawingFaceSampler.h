#pragma once

#include <optional>
#include <string>
#include <vector>

namespace roadproto::domain::quantity {

struct ClearTableQuantityDrawingPoint {
    double x = 0.0;
    double y = 0.0;
};

struct ClearTableQuantityDrawingFace {
    std::wstring layerName;
    std::wstring sideName;
    int sourceConfigRowIndex = -1;
    double thickness = 0.0;
    std::vector<ClearTableQuantityDrawingPoint> points;
};

struct ClearTableQuantitySectionSample {
    double station = 0.0;
    std::vector<ClearTableQuantityDrawingFace> faces;
};

class ClearTableQuantityDrawingFaceSampler {
public:
    static std::optional<ClearTableQuantitySectionSample> sampleAtStation(
        double station,
        const std::vector<ClearTableQuantityDrawingFace>& faces);
};

} // namespace roadproto::domain::quantity
