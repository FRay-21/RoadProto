#pragma once

#include "domain/profile/ProfileVerticalCurveCalculator.h"
#include "domain/profile/ProfileVerticalCurveModel.h"

#include <cstddef>
#include <string>
#include <vector>

namespace roadproto::application::profile {

struct ProfileVerticalCurveDialogEdit {
    std::wstring name;
    double startStation = 0.0;
    double startElevation = 0.0;
    double endStation = 0.0;
    double endElevation = 0.0;
    std::vector<domain::profile::VerticalCurvePvi> pvis;
};

struct ProfileVerticalCurveGripEdit {
    domain::profile::VerticalCurveGripRole role = domain::profile::VerticalCurveGripRole::StartPoint;
    std::size_t index = 0;
    double station = 0.0;
    double elevation = 0.0;
    double radius = 0.0;
};

using ProfileVerticalCurveEditResult = domain::profile::ProfileVerticalCurveEditResult;

class ProfileVerticalCurveEditService {
public:
    ProfileVerticalCurveEditResult applyDialogEdit(
        domain::profile::ProfileVerticalCurveData& data,
        const ProfileVerticalCurveDialogEdit& edit) const;

    ProfileVerticalCurveEditResult applyGripMove(
        domain::profile::ProfileVerticalCurveData& data,
        const ProfileVerticalCurveGripEdit& edit) const;

    ProfileVerticalCurveEditResult addPvi(
        domain::profile::ProfileVerticalCurveData& data,
        double station,
        double elevation,
        double radius) const;

    ProfileVerticalCurveEditResult deletePvi(
        domain::profile::ProfileVerticalCurveData& data,
        std::size_t pviIndex) const;
};

} // namespace roadproto::application::profile
