#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

#include <cstddef>
#include <string>
#include <vector>

namespace roadproto::domain::profile {

struct ProfileDmxParseResult {
    bool succeeded = false;
    std::wstring errorMessage;
    std::vector<ProfileGroundSample> samples;
    std::size_t invalidLineCount = 0;
};

class ProfileDmxFile final {
public:
    static ProfileDmxParseResult parseText(const std::wstring& content);
    static ProfileDmxParseResult read(const std::wstring& path);
};

} // namespace roadproto::domain::profile
