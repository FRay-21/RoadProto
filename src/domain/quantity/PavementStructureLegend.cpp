#include "domain/quantity/PavementStructureLegend.h"

#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace roadproto::domain::quantity {
namespace {

using roadproto::domain::cross_section::PavementLayerTemplateLayer;
using roadproto::domain::cross_section::pavementLayerTypeDisplayName;
using roadproto::domain::cross_section::pavementSubgradeMoistureTypeDisplayName;
using roadproto::domain::cross_section::pavementSubgradeSoilGroupDisplayName;

constexpr double kMetersToCentimeters = 100.0;
constexpr double kNumberTolerance = 1.0e-9;

std::wstring formatCentimeters(double centimeters)
{
    if (!std::isfinite(centimeters)) {
        return L"";
    }

    const auto roundedInteger = std::round(centimeters);
    std::wostringstream stream;
    stream.imbue(std::locale::classic());
    if (std::fabs(centimeters - roundedInteger) <= kNumberTolerance) {
        stream << static_cast<long long>(roundedInteger);
    } else {
        stream << std::fixed << std::setprecision(1) << centimeters;
    }
    return stream.str();
}

std::wstring layerDisplayName(const PavementLayerTemplateLayer& layer)
{
    return layer.name.empty() ? pavementLayerTypeDisplayName(layer.type) : layer.name;
}

std::wstring joinSoilGroups(
    const std::vector<roadproto::domain::cross_section::PavementSubgradeSoilGroup>& groups)
{
    std::wstring result;
    for (const auto group : groups) {
        if (!result.empty()) {
            result += L"、";
        }
        result += pavementSubgradeSoilGroupDisplayName(group);
    }
    return result;
}

std::wstring joinMoistureTypes(
    const std::vector<roadproto::domain::cross_section::PavementSubgradeMoistureType>& types)
{
    std::wstring result;
    for (const auto type : types) {
        if (!result.empty()) {
            result += L"、";
        }
        result += pavementSubgradeMoistureTypeDisplayName(type);
    }
    return result;
}

double uniformThicknessCm(const PavementLayerTemplateLayer& layer)
{
    return layer.thickness * kMetersToCentimeters;
}

double innerThicknessCm(const PavementLayerTemplateLayer& layer)
{
    return layer.innerThickness * kMetersToCentimeters;
}

double outerThicknessCm(const PavementLayerTemplateLayer& layer)
{
    return layer.outerThickness * kMetersToCentimeters;
}

PavementStructureLegendLayerPlan makeLayerPlan(const PavementLayerTemplateLayer& layer)
{
    PavementStructureLegendLayerPlan plan;
    plan.layerName = layerDisplayName(layer);
    plan.colorR = layer.color.r;
    plan.colorG = layer.color.g;
    plan.colorB = layer.color.b;
    plan.hatchPattern = layer.hatchPattern;
    plan.hatchAngle = layer.hatchAngle;
    plan.hatchScale = layer.hatchScale;

    if (layer.uniformThickness) {
        plan.displayThicknessCm = uniformThicknessCm(layer);
        plan.thicknessText = formatCentimeters(plan.displayThicknessCm);
    } else {
        const auto inner = innerThicknessCm(layer);
        const auto outer = outerThicknessCm(layer);
        plan.displayThicknessCm = (inner + outer) * 0.5;
        plan.thicknessText = formatCentimeters(inner) + L"/" + formatCentimeters(outer);
    }
    return plan;
}

PavementStructureLegendColumnPlan makeColumnPlan(const PavementStructureLegendTemplateSource& source)
{
    PavementStructureLegendColumnPlan column;
    column.templateHandle = source.handle;
    column.structureCode = source.data.properties.structureCode;
    column.subgradeSoilGroupText = joinSoilGroups(source.data.properties.subgradeSoilGroups);
    column.subgradeMoistureText = joinMoistureTypes(source.data.properties.subgradeMoistureTypes);
    column.designDeflection = source.data.properties.designDeflection;
    column.cumulativeAxleLoads = source.data.properties.cumulativeAxleLoads;

    column.layers.reserve(source.data.layers.size());
    for (const auto& layer : source.data.layers) {
        auto layerPlan = makeLayerPlan(layer);
        column.totalThicknessCm += layerPlan.displayThicknessCm;
        column.layers.push_back(std::move(layerPlan));
    }
    return column;
}

PavementStructureLegendItemPlan makeLegendItemPlan(const PavementStructureLegendLayerPlan& layer)
{
    PavementStructureLegendItemPlan item;
    item.layerName = layer.layerName;
    item.colorR = layer.colorR;
    item.colorG = layer.colorG;
    item.colorB = layer.colorB;
    item.hatchPattern = layer.hatchPattern;
    item.hatchAngle = layer.hatchAngle;
    item.hatchScale = layer.hatchScale;
    return item;
}

} // namespace

PavementStructureLegendPlan PavementStructureLegendPlanner::build(
    const std::vector<PavementStructureLegendTemplateSource>& sources)
{
    PavementStructureLegendPlan plan;
    std::set<std::wstring> seenHandles;
    for (const auto& source : sources) {
        if (source.handle.empty() || seenHandles.find(source.handle) != seenHandles.end()) {
            continue;
        }
        seenHandles.insert(source.handle);

        auto column = makeColumnPlan(source);
        for (const auto& layer : column.layers) {
            plan.legendItems.push_back(makeLegendItemPlan(layer));
        }
        plan.columns.push_back(std::move(column));
    }
    return plan;
}

} // namespace roadproto::domain::quantity
