#include "domain/quantity/PavementQuantityDrawingFaceSampler.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace roadproto::domain::quantity {
namespace {

struct Totals {
    double projectedWidth = 0.0;
    double sectionArea = 0.0;
};

using LayerKey = std::pair<std::wstring, std::wstring>;

std::wstring normalizeLayerName(const std::wstring& value)
{
    return value.empty() ? L"路面结构层" : value;
}

std::wstring normalizeComponentName(const std::wstring& value)
{
    return value.empty() ? L"未分部件" : value;
}

bool allPointsFinite(const std::vector<PavementQuantityDrawingPoint>& points)
{
    return std::all_of(points.begin(), points.end(), [](const auto& point) {
        return std::isfinite(point.x) && std::isfinite(point.y);
    });
}

double polygonArea(const std::vector<PavementQuantityDrawingPoint>& points)
{
    if (points.size() < 3 || !allPointsFinite(points)) {
        return 0.0;
    }

    double sum = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const auto& first = points[i];
        const auto& second = points[(i + 1) % points.size()];
        sum += first.x * second.y - second.x * first.y;
    }
    return std::fabs(sum) * 0.5;
}

double projectedWidth(const std::vector<PavementQuantityDrawingPoint>& points)
{
    if (points.empty() || !allPointsFinite(points)) {
        return 0.0;
    }

    auto minMax = std::minmax_element(points.begin(), points.end(), [](const auto& first, const auto& second) {
        return first.x < second.x;
    });
    return minMax.second->x - minMax.first->x;
}

} // namespace

std::optional<PavementQuantitySectionSample> PavementQuantityDrawingFaceSampler::sampleAtStation(
    double station,
    const std::vector<PavementQuantityDrawingFace>& faces)
{
    if (!std::isfinite(station)) {
        return std::nullopt;
    }

    std::map<LayerKey, Totals> totalsByLayer;
    for (const auto& face : faces) {
        if (face.points.size() < 3) {
            continue;
        }

        const auto width = projectedWidth(face.points);
        const auto area = polygonArea(face.points);
        if (area <= 0.0 || width <= 0.0) {
            continue;
        }

        auto& totals = totalsByLayer[LayerKey{
            normalizeComponentName(face.componentName),
            normalizeLayerName(face.layerName)}];
        totals.projectedWidth += width;
        totals.sectionArea += area;
    }

    if (totalsByLayer.empty()) {
        return std::nullopt;
    }

    PavementQuantitySectionSample sample;
    sample.station = station;
    for (const auto& item : totalsByLayer) {
        sample.layers.push_back(
            PavementQuantityLayerSample{
                item.first.second,
                item.second.projectedWidth,
                item.second.sectionArea,
                item.first.first});
    }
    return sample;
}

} // namespace roadproto::domain::quantity
