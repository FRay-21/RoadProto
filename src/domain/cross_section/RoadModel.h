#pragma once

#include "domain/alignment/AlignmentGeometry.h"
#include "domain/cross_section/PavementLayerTemplateModel.h"
#include "domain/cross_section/SlopeTemplateModel.h"
#include "domain/cross_section/SubgradeTemplateModel.h"
#include "domain/profile/ProfileVerticalCurveModel.h"
#include "domain/terrain/TerrainTinData.h"

#include <cstddef>
#include <functional>
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

struct RoadModelSlopeTemplateReference {
    std::wstring templateHandle;
    std::wstring templateName;
};

struct RoadModelSlopeTemplateGroup {
    double startStation = 0.0;
    double endStation = 0.0;
    std::vector<RoadModelSlopeTemplateReference> templates;
};

enum class RoadModelStructureType {
    Bridge,
    Tunnel,
};

enum class RoadModelStructureSideRange {
    Left,
    Right,
    Both,
};

struct RoadModelStructureRange {
    double startStation = 0.0;
    double endStation = 0.0;
    RoadModelStructureType type = RoadModelStructureType::Bridge;
    RoadModelStructureSideRange sideRange = RoadModelStructureSideRange::Both;
};

struct RoadModelSlopeConfig {
    double leftSearchWidth = 50.0;
    double rightSearchWidth = 50.0;
    std::vector<RoadModelSlopeTemplateGroup> leftGroups;
    std::vector<RoadModelSlopeTemplateGroup> rightGroups;
};

struct RoadModelConfig {
    std::wstring roadCenterlineHandle;
    std::wstring profileVerticalCurveHandle;
    double sampleInterval = 10.0;
    // Assignments are ordered from high to low priority; resolve returns the first matching row.
    std::vector<RoadModelTemplateAssignment> assignments;
    std::vector<RoadModelStructureRange> structures;
    RoadModelSlopeConfig slopeConfig;
};

struct RoadModelTemplateSource {
    std::wstring templateHandle;
    SubgradeTemplateData data;
};

struct RoadModelSlopeTemplateSource {
    std::wstring templateHandle;
    SlopeTemplateData data;
};

struct RoadModelPavementLayerTemplateSource {
    std::wstring templateHandle;
    PavementLayerTemplateData data;
};

struct RoadModelTerrainSurface {
    std::vector<terrain::TinPoint> points;
    std::vector<terrain::TinTriangle> triangles;
};

using RoadModelProgressCallback = std::function<void(int percent, const std::wstring& message)>;

struct RoadModelWireColor {
    int r = 0;
    int g = 0;
    int b = 0;
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

struct RoadModelSlopeLineKey {
    std::wstring templateHandle;
    SubgradeSide side = SubgradeSide::Right;
    SlopeComponentType componentType = SlopeComponentType::FillSlope;
    std::size_t groupIndex = 0;
    std::size_t templateOrder = 0;
    std::size_t componentIndex = 0;
    std::size_t boundaryIndex = 0;
};

struct RoadModelSlopeComponentLine {
    RoadModelSlopeLineKey key;
    SlopeTemplateRgbColor color;
    std::vector<RoadModelPoint3d> points;
};

struct RoadModelPavementLayerLineKey {
    std::wstring subgradeTemplateHandle;
    std::wstring pavementLayerTemplateHandle;
    SubgradeSide side = SubgradeSide::Right;
    std::size_t componentIndex = 0;
    std::size_t layerIndex = 0;
    std::size_t boundaryIndex = 0;
};

struct RoadModelPavementLayerLine {
    RoadModelPavementLayerLineKey key;
    RoadModelWireColor color;
    std::vector<RoadModelPoint3d> points;
};

struct RoadModelSlopeSectionResult {
    double station = 0.0;
    SubgradeSide side = SubgradeSide::Right;
    bool succeeded = false;
    std::size_t groupIndex = 0;
    std::size_t templateOrder = 0;
    std::wstring templateHandle;
    std::wstring errorMessage;
};

enum class RoadModelSectionNodeKind {
    Subgrade,
    Slope,
    Ground,
    PavementLayer,
};

struct RoadModelSectionNode {
    RoadModelSectionNodeKind kind = RoadModelSectionNodeKind::Subgrade;
    SubgradeSide side = SubgradeSide::Right;
    double offset = 0.0;
    double elevation = 0.0;
    RoadModelPoint3d point;
    RoadModelWireColor color;
    std::wstring label;
    std::wstring componentName;
};

struct RoadModelGroundProfilePoint {
    double offset = 0.0;
    double elevation = 0.0;
};

struct RoadModelSection {
    double station = 0.0;
    bool succeeded = true;
    std::wstring errorMessage;
    std::vector<RoadModelSectionNode> leftNodes;
    std::vector<RoadModelSectionNode> rightNodes;
    std::vector<RoadModelSectionNode> leftPavementLayerNodes;
    std::vector<RoadModelSectionNode> rightPavementLayerNodes;
    std::vector<RoadModelGroundProfilePoint> leftGroundProfile;
    std::vector<RoadModelGroundProfilePoint> rightGroundProfile;
};

enum class RoadModelWireLineKind {
    SectionRib,
    Longitudinal,
    OuterBoundary,
    Transition,
    EndCap,
    PavementLayer,
};

struct RoadModelWireLine {
    RoadModelWireLineKind kind = RoadModelWireLineKind::Longitudinal;
    SubgradeSide side = SubgradeSide::Right;
    RoadModelWireColor color;
    std::vector<RoadModelPoint3d> points;
};

struct RoadModelData {
    RoadModelConfig config;
    std::vector<double> sampledStations;
    std::vector<RoadModelComponentLine> componentLines;
    std::vector<RoadModelSlopeComponentLine> slopeLines;
    std::vector<RoadModelPavementLayerLine> pavementLayerLines;
    std::vector<RoadModelSlopeSectionResult> slopeSectionResults;
    std::vector<RoadModelSection> sections;
    std::vector<RoadModelWireLine> wireLines;
    int version = 1;
};

struct RoadModelBuildInput {
    RoadModelConfig config;
    std::vector<alignment::AlignmentSamplePoint> alignmentSamples;
    profile::ProfileVerticalCurveData verticalCurve;
    std::vector<RoadModelTemplateSource> templates;
    std::vector<RoadModelSlopeTemplateSource> slopeTemplates;
    std::vector<RoadModelPavementLayerTemplateSource> pavementLayerTemplates;
    RoadModelTerrainSurface terrainSurface;
    RoadModelProgressCallback progressCallback;
};

struct RoadModelBuildResult {
    bool succeeded = false;
    std::wstring errorMessage;
    RoadModelData data;
    std::vector<double> sampledStations;
    struct Diagnostics {
        bool terrainIndexEnabled = false;
        std::size_t terrainTriangleCount = 0;
        std::size_t terrainIndexColumns = 0;
        std::size_t terrainIndexRows = 0;
        std::size_t terrainIndexTriangleReferences = 0;
        std::size_t terrainIndexGlobalTriangles = 0;
        std::size_t groundProfileQueryCount = 0;
        std::size_t groundProfileCandidateTotal = 0;
        std::size_t groundProfileCandidateMax = 0;
    } diagnostics;
};

enum class RoadModelSectionPreviewSegmentKind {
    Subgrade,
    Slope,
    Ground,
    PavementLayer,
};

struct RoadModelSectionPreviewPoint {
    double offset = 0.0;
    double elevation = 0.0;
};

struct RoadModelSectionPreviewColor {
    int r = 0;
    int g = 0;
    int b = 0;
};

struct RoadModelSectionPreviewSegment {
    RoadModelSectionPreviewSegmentKind kind = RoadModelSectionPreviewSegmentKind::Subgrade;
    RoadModelSectionPreviewColor color;
    std::wstring label;
    std::wstring componentName;
    std::vector<RoadModelSectionPreviewPoint> points;
};

struct RoadModelSectionPreviewRequest {
    RoadModelData data;
    std::vector<alignment::AlignmentSamplePoint> alignmentSamples;
    RoadModelTerrainSurface terrainSurface;
    double station = 0.0;
};

struct RoadModelSectionPreview {
    bool succeeded = false;
    bool hasGroundLine = false;
    double station = 0.0;
    std::wstring errorMessage;
    std::wstring statusMessage;
    std::vector<RoadModelSectionPreviewSegment> segments;
};

class RoadModelSectionPreviewBuilder {
public:
    static RoadModelSectionPreview build(const RoadModelSectionPreviewRequest& request);
};

class RoadModelRules {
public:
    static bool isSupportedSampleInterval(double sampleInterval);
    static bool isSupportedSearchWidth(double searchWidth);
    static bool validateAssignments(
        const std::vector<RoadModelTemplateAssignment>& assignments,
        std::wstring& errorMessage);
    static bool validateSlopeTemplateGroups(
        const std::vector<RoadModelSlopeTemplateGroup>& groups,
        std::wstring& errorMessage);
    static bool validateStructureRanges(
        const std::vector<RoadModelStructureRange>& structures,
        std::wstring& errorMessage);
};

class RoadModelTemplateResolver {
public:
    explicit RoadModelTemplateResolver(std::vector<RoadModelTemplateAssignment> assignments);

    const RoadModelTemplateAssignment* resolve(double station) const;

private:
    std::vector<RoadModelTemplateAssignment> assignments_;
};

class RoadModelSlopeTemplateGroupResolver {
public:
    explicit RoadModelSlopeTemplateGroupResolver(std::vector<RoadModelSlopeTemplateGroup> groups);

    std::vector<const RoadModelSlopeTemplateGroup*> resolve(double station) const;

private:
    std::vector<RoadModelSlopeTemplateGroup> groups_;
};

class RoadModelStationSampler {
public:
    static std::vector<double> collectStations(
        double alignmentStart,
        double alignmentEnd,
        const profile::ProfileVerticalCurveData& verticalCurve,
        const std::vector<RoadModelTemplateAssignment>& assignments,
        double sampleInterval);
    static std::vector<double> collectStations(
        double alignmentStart,
        double alignmentEnd,
        const profile::ProfileVerticalCurveData& verticalCurve,
        const std::vector<RoadModelTemplateAssignment>& assignments,
        const std::vector<RoadModelStructureRange>& structures,
        double sampleInterval);
};

class RoadModelBuilder {
public:
    static RoadModelBuildResult build(const RoadModelBuildInput& input);
};

} // namespace roadproto::domain::cross_section
