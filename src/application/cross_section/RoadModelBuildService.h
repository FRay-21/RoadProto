#pragma once

#include "domain/cross_section/RoadModel.h"

namespace roadproto::application::cross_section {

using RoadModelBuildInput = domain::cross_section::RoadModelBuildInput;
using RoadModelBuildResult = domain::cross_section::RoadModelBuildResult;

class RoadModelBuildService final {
public:
    RoadModelBuildResult build(const RoadModelBuildInput& input) const;
};

} // namespace roadproto::application::cross_section
