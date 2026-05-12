#pragma once

#include "domain/profile/ProfileVerticalCurveModel.h"

#include <cstddef>
#include <string>
#include <vector>

namespace roadproto::domain::profile {

struct ProfileVerticalCurveBuildResult {
    bool succeeded = false;
    std::wstring errorMessage;
    std::vector<std::wstring> warnings;
    std::vector<VerticalCurveControlPoint> orderedControlPoints;
    std::vector<VerticalCurveElement> elements;
};

struct ProfileVerticalCurveQueryResult {
    bool succeeded = false;
    std::wstring errorMessage;
    double value = 0.0;
};

struct ProfileVerticalCurveSampleResult {
    bool succeeded = false;
    std::wstring errorMessage;
    std::vector<VerticalCurveSamplePoint> points;
};

struct ProfileVerticalCurveEditResult {
    bool succeeded = false;
    bool changed = false;
    std::wstring errorMessage;
};

class ProfileVerticalCurveCalculator final {
public:
    static ProfileVerticalCurveBuildResult rebuild(const ProfileVerticalCurveData& data);
    static ProfileVerticalCurveQueryResult elevationAt(const ProfileVerticalCurveBuildResult& built, double station);
    static ProfileVerticalCurveQueryResult gradeAt(const ProfileVerticalCurveBuildResult& built, double station);
    static ProfileVerticalCurveSampleResult sample(const ProfileVerticalCurveData& data, double interval);
    static ProfileVerticalCurveEditResult moveControlPoint(
        ProfileVerticalCurveData& data,
        std::size_t pointIndex,
        double station,
        double elevation);
    static ProfileVerticalCurveEditResult movePvi(
        ProfileVerticalCurveData& data,
        std::size_t pviIndex,
        double station,
        double elevation);
    static ProfileVerticalCurveEditResult updateRadius(ProfileVerticalCurveData& data, std::size_t pviIndex, double radius);
    static ProfileVerticalCurveEditResult insertPvi(
        ProfileVerticalCurveData& data,
        double station,
        double elevation,
        double radius);
    static ProfileVerticalCurveEditResult removePvi(ProfileVerticalCurveData& data, std::size_t pviIndex);
};

} // namespace roadproto::domain::profile
