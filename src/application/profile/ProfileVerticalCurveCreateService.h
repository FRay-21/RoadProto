#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"
#include "domain/profile/ProfileVerticalCurveModel.h"

#include <string>
#include <vector>

namespace roadproto::application::profile {

struct ProfileVerticalCurveCreateInput {
    std::wstring profileGraphHandle;
    std::vector<domain::profile::ProfileGroundSample> groundSamples;
};

struct ProfileVerticalCurveCreateResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::profile::ProfileVerticalCurveData data;
};

class ProfileVerticalCurveCreateService {
public:
    ProfileVerticalCurveCreateResult buildDefaultFromGraph(const ProfileVerticalCurveCreateInput& input) const;
};

} // namespace roadproto::application::profile
