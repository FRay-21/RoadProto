#pragma once

#include "domain/cross_section/RoadModel.h"
#include "domain/quantity/PavementQuantityTable.h"

#include <optional>

namespace roadproto::domain::quantity {

class RoadModelPavementQuantitySampler {
public:
    static std::optional<PavementQuantitySectionSample> sampleAtStation(
        const roadproto::domain::cross_section::RoadModelData& data,
        double station);
};

} // namespace roadproto::domain::quantity
