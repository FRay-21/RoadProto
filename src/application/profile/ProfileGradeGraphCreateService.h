#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

#include <string>
#include <vector>

namespace roadproto::application::profile {

struct ProfileGradeGraphCreateInput {
    domain::profile::ProfileGroundSourceType sourceType = domain::profile::ProfileGroundSourceType::DmxFile;
    std::wstring roadName;
    std::wstring roadCenterlineHandle;
    std::wstring terrainTinHandle;
    std::wstring dmxFilePath;
    std::vector<domain::profile::ProfileGroundSample> groundSamples;
};

struct ProfileGradeGraphCreateResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::profile::ProfileGradeGraphData graph;
};

class ProfileGradeGraphCreateService {
public:
    ProfileGradeGraphCreateResult build(const ProfileGradeGraphCreateInput& input) const;
};

} // namespace roadproto::application::profile
