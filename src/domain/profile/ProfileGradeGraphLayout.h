#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

#include <string>

namespace roadproto::domain::profile {

struct ProfileGradeGraphLayoutResult {
    bool succeeded = false;
    std::wstring errorMessage;
    double minStation = 0.0;
    double maxStation = 0.0;
    double minElevation = 0.0;
    double maxElevation = 0.0;
    double baseElevation = 0.0;
    double graphWidth = 0.0;
    double graphHeight = 0.0;
};

class ProfileGradeGraphLayout final {
public:
    static ProfileGradeGraphLayoutResult calculate(const ProfileGradeGraphData& graph);
    static double mapX(const ProfileGradeGraphLayoutResult& layout, double station);
    static double mapY(const ProfileGradeGraphLayoutResult& layout, double elevation, double verticalScale = 10.0);
    static double mapY(const ProfileGradeGraphData& graph, const ProfileGradeGraphLayoutResult& layout, double elevation);
};

} // namespace roadproto::domain::profile
