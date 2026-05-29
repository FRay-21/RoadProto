#include "domain/quantity/ClearTableQuantityDrawingFaceSampler.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::quantity {
namespace {

bool allPointsFinite(const std::vector<ClearTableQuantityDrawingPoint>& points)
{
    return std::all_of(points.begin(), points.end(), [](const auto& point) {
        return std::isfinite(point.x) && std::isfinite(point.y);
    });
}

} // namespace

std::optional<ClearTableQuantitySectionSample> ClearTableQuantityDrawingFaceSampler::sampleAtStation(
    double station,
    const std::vector<ClearTableQuantityDrawingFace>& faces)
{
    if (!std::isfinite(station)) {
        return std::nullopt;
    }

    ClearTableQuantitySectionSample sample;
    sample.station = station;
    for (const auto& face : faces) {
        if (face.points.size() < 3 || !allPointsFinite(face.points)) {
            continue;
        }
        sample.faces.push_back(face);
    }

    if (sample.faces.empty()) {
        return std::nullopt;
    }
    return sample;
}

} // namespace roadproto::domain::quantity
