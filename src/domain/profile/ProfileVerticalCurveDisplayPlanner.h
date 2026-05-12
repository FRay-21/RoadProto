#pragma once

#include "domain/profile/ProfileVerticalCurveModel.h"

#include <string>
#include <vector>

namespace roadproto::domain::profile {

enum class VerticalCurveDisplaySegmentRole {
    StraightDesignLine,
    CurveDesignLine,
    CurveTangentLine,
};

struct VerticalCurveDisplaySegment {
    VerticalCurveDisplaySegmentRole role = VerticalCurveDisplaySegmentRole::StraightDesignLine;
    double startStation = 0.0;
    double startElevation = 0.0;
    double endStation = 0.0;
    double endElevation = 0.0;
    int colorIndex = 0;
};

struct ProfileVerticalCurveDisplayPlan {
    bool succeeded = false;
    std::wstring errorMessage;
    std::vector<VerticalCurveDisplaySegment> segments;
};

class ProfileVerticalCurveDisplayPlanner final {
public:
    static ProfileVerticalCurveDisplayPlan build(const ProfileVerticalCurveData& data);
};

} // namespace roadproto::domain::profile
