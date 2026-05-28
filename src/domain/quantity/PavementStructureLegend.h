#pragma once

#include "domain/cross_section/PavementLayerTemplateModel.h"

#include <string>
#include <vector>

namespace roadproto::domain::quantity {

struct PavementStructureLegendTemplateSource {
    std::wstring handle;
    roadproto::domain::cross_section::PavementLayerTemplateData data;
};

struct PavementStructureLegendLayerPlan {
    std::wstring layerName;
    std::wstring thicknessText;
    double displayThicknessCm = 0.0;
    int colorR = 0;
    int colorG = 0;
    int colorB = 0;
    std::wstring hatchPattern = L"SOLID";
    double hatchAngle = 0.0;
    double hatchScale = 1.0;
};

struct PavementStructureLegendColumnPlan {
    std::wstring templateHandle;
    std::wstring structureCode;
    std::wstring subgradeSoilGroupText;
    std::wstring subgradeMoistureText;
    std::wstring designDeflection;
    std::wstring cumulativeAxleLoads;
    double totalThicknessCm = 0.0;
    std::vector<PavementStructureLegendLayerPlan> layers;
};

struct PavementStructureLegendItemPlan {
    std::wstring layerName;
    int colorR = 0;
    int colorG = 0;
    int colorB = 0;
    std::wstring hatchPattern = L"SOLID";
    double hatchAngle = 0.0;
    double hatchScale = 1.0;
};

struct PavementStructureLegendLayoutPlan {
    double structureGraphicWidthCm = 20.0;
    double columnWidth = 36.0;
    double headerColumnWidth = 16.0;
};

struct PavementStructureLegendPlan {
    PavementStructureLegendLayoutPlan layout;
    std::vector<PavementStructureLegendColumnPlan> columns;
    std::vector<PavementStructureLegendItemPlan> legendItems;
};

class PavementStructureLegendPlanner {
public:
    static PavementStructureLegendPlan build(
        const std::vector<PavementStructureLegendTemplateSource>& sources);
};

} // namespace roadproto::domain::quantity
