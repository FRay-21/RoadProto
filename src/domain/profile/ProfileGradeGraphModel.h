#pragma once

#include <string>
#include <vector>

namespace roadproto::domain::profile {

enum class ProfileGroundSourceType {
    TerrainTin,
    DmxFile,
};

struct ProfileGroundSample {
    double station = 0.0;
    double elevation = 0.0;
    std::wstring rawStationText;
    int breakChainIndex = 0;
};

struct ProfileGradeGraphProperties {
    std::wstring graphName = L"拉坡图";
    int groundLineColorIndex = 4;
    double groundLineWidth = 1.0;
    double sampleInterval = 10.0;
    double verticalScale = 10.0;
    double gridSpacing = 10.0;
};

struct ProfileGradeGraphData {
    ProfileGroundSourceType sourceType = ProfileGroundSourceType::TerrainTin;
    std::wstring roadCenterlineHandle;
    std::wstring terrainTinHandle;
    std::wstring dmxFilePath;
    ProfileGradeGraphProperties properties;
    std::vector<ProfileGroundSample> groundSamples;
};

} // namespace roadproto::domain::profile
