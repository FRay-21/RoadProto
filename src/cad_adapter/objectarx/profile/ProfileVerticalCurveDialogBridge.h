#pragma once

#include "domain/profile/ProfileVerticalCurveModel.h"

#include <string>
#include <vector>

namespace roadproto::cad_adapter::objectarx::profile {

struct ProfileVerticalCurveDialogRequest {
    std::wstring handle;
    std::wstring responsePath;
    std::wstring profileGraphHandle;
    std::wstring name;
    double startStation = 0.0;
    double startElevation = 0.0;
    double endStation = 0.0;
    double endElevation = 0.0;
    int currentPviIndex = 0;
    std::vector<roadproto::domain::profile::VerticalCurvePvi> pvis;
};

struct ProfileVerticalCurveDialogResponse {
    bool accepted = false;
    std::wstring handle;
    std::wstring name;
    double startStation = 0.0;
    double startElevation = 0.0;
    double endStation = 0.0;
    double endElevation = 0.0;
    std::vector<roadproto::domain::profile::VerticalCurvePvi> pvis;
};

bool queueProfileVerticalCurveWpfDialog(
    const ProfileVerticalCurveDialogRequest& request,
    std::wstring& errorMessage);

bool readProfileVerticalCurveDialogResponse(
    const std::wstring& responsePath,
    ProfileVerticalCurveDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::profile
