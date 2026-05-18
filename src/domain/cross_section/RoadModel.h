#pragma once

#include "domain/alignment/AlignmentGeometry.h"
#include "domain/cross_section/SubgradeTemplateModel.h"
#include "domain/profile/ProfileVerticalCurveModel.h"

#include <cstddef>
#include <string>
#include <vector>

namespace roadproto::domain::cross_section {

struct RoadModelPoint3d {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct RoadModelTemplateAssignment {
    double startStation = 0.0;
    double endStation = 0.0;
    std::wstring templateHandle;
    std::wstring templateName;
};

struct RoadModelConfig {
    std::wstring roadCenterlineHandle;
    std::wstring profileVerticalCurveHandle;
    double sampleInterval = 10.0;
    // Assignments are ordered from high to low priority; resolve returns the first matching row.
    std::vector<RoadModelTemplateAssignment> assignments;
};

struct RoadModelTemplateSource {
    std::wstring templateHandle;
    SubgradeTemplateData data;
};

struct RoadModelLineKey {
    std::wstring templateHandle;
    SubgradeSide side = SubgradeSide::Right;
    SubgradeComponentType componentType = SubgradeComponentType::TravelLane;
    std::size_t componentIndex = 0;
    std::size_t boundaryIndex = 0;
};

struct RoadModelComponentLine {
    RoadModelLineKey key;
    SubgradeTemplateRgbColor color;
    std::vector<RoadModelPoint3d> points;
};

struct RoadModelData {
    RoadModelConfig config;
    std::vector<RoadModelComponentLine> componentLines;
    int version = 1;
};

struct RoadModelBuildInput {
    RoadModelConfig config;
    std::vector<alignment::AlignmentSamplePoint> alignmentSamples;
    profile::ProfileVerticalCurveData verticalCurve;
    std::vector<RoadModelTemplateSource> templates;
};

struct RoadModelBuildResult {
    bool succeeded = false;
    std::wstring errorMessage;
    RoadModelData data;
    std::vector<double> sampledStations;
};

class RoadModelRules {
public:
    static bool isSupportedSampleInterval(double sampleInterval);
    static bool validateAssignments(
        const std::vector<RoadModelTemplateAssignment>& assignments,
        std::wstring& errorMessage);
};

class RoadModelTemplateResolver {
public:
    explicit RoadModelTemplateResolver(std::vector<RoadModelTemplateAssignment> assignments);

    const RoadModelTemplateAssignment* resolve(double station) const;

private:
    std::vector<RoadModelTemplateAssignment> assignments_;
};

class RoadModelStationSampler {
public:
    static std::vector<double> collectStations(
        const RoadModelConfig& config,
        const std::vector<alignment::AlignmentSamplePoint>& alignmentSamples);
};

class RoadModelBuilder {
public:
    RoadModelBuildResult build(const RoadModelBuildInput& input) const;
};

} // namespace roadproto::domain::cross_section
