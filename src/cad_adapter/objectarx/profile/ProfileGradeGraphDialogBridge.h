#pragma once

#include "domain/profile/ProfileGradeGraphModel.h"

#include <cstddef>
#include <string>

namespace roadproto::cad_adapter::objectarx::profile {

struct ProfileGradeGraphDialogRequest {
    std::wstring handle;
    std::wstring roadCenterlineHandle;
    std::wstring terrainTinHandle;
    std::wstring graphName;
    roadproto::domain::profile::ProfileGroundSourceType sourceType =
        roadproto::domain::profile::ProfileGroundSourceType::DmxFile;
    std::wstring dmxFilePath;
    int groundLineColorIndex = 4;
    double groundLineWidth = 1.0;
    double groundLinePrecision = 10.0;
    double verticalScale = 10.0;
    double gridSpacing = 10.0;
    std::size_t sampleCount = 0;
};

struct ProfileGradeGraphDialogResponse {
    bool accepted = false;
    std::wstring handle;
    std::wstring graphName;
    int groundLineColorIndex = 4;
    double groundLineWidth = 1.0;
    double groundLinePrecision = 10.0;
    double verticalScale = 10.0;
    double gridSpacing = 10.0;
    bool updateGroundLineRequested = false;
};

bool queueProfileGradeGraphWpfDialog(
    const ProfileGradeGraphDialogRequest& request,
    std::wstring& errorMessage);

bool readProfileGradeGraphDialogResponse(
    const std::wstring& responsePath,
    ProfileGradeGraphDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::profile
