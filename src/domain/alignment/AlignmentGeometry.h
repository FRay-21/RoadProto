#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace roadproto::domain::alignment {

struct AlignmentPoint2d {
    double x = 0.0;
    double y = 0.0;
};

enum class RoadGrade {
    Expressway,
    FirstClass,
    SecondClass,
    ThirdClass,
    FourthClass,
    UrbanExpressway,
    UrbanArterial,
    UrbanSubArterial,
    UrbanBranch,
    Other,
    Unclassified,
};

struct RoadCenterlineProperties {
    std::wstring roadName = L"K1";
    RoadGrade roadGrade = RoadGrade::Other;
    bool linkedTerrainEnabled = false;
    std::wstring linkedTerrainHandle;
    double stationLabelInterval = 100.0;
};

struct HorizontalCurveParameters {
    double tangentIn = 0.0;
    double tangentOut = 0.0;
    double ls1 = 20.0;
    double radius = 80.0;
    double ls2 = 20.0;
};

enum class AlignmentElementType {
    Line,
    SpiralIn,
    CircularArc,
    SpiralOut,
    PartialSpiral,
};

enum class AlignmentCurveCombinationType {
    BasicFiveElement,
    CCurve,
    OvalCurve,
    SCurve,
    ElementChain,
};

enum class CurveTurnDirection {
    Left,
    Right,
};

enum class AlignmentFeaturePointType {
    Start,
    End,
    PI,
    TS,
    SC,
    CS,
    ST,
    TC,
    CT,
    TangentIntersection,
    ArcMid,
};

struct AlignmentSamplePoint {
    AlignmentPoint2d point;
    double station = 0.0;
};

struct HorizontalAlignmentElement {
    AlignmentElementType type = AlignmentElementType::Line;
    std::size_t curveIndex = 0;
    AlignmentPoint2d start;
    AlignmentPoint2d end;
    double startStation = 0.0;
    double length = 0.0;
    double radius = 0.0;
    double spiralLength = 0.0;
    double startHeading = 0.0;
    double endHeading = 0.0;
    double startCurvature = 0.0;
    double endCurvature = 0.0;
    std::vector<AlignmentSamplePoint> samples;
};

struct HorizontalAlignmentFeaturePoint {
    AlignmentFeaturePointType type = AlignmentFeaturePointType::Start;
    std::size_t curveIndex = 0;
    AlignmentPoint2d point;
    double station = 0.0;
};

struct StationLabel {
    AlignmentPoint2d point;
    double station = 0.0;
    std::wstring text;
};

struct HorizontalAlignment {
    RoadCenterlineProperties properties;
    AlignmentCurveCombinationType combinationType = AlignmentCurveCombinationType::BasicFiveElement;
    std::vector<AlignmentPoint2d> controlPoints;
    std::vector<HorizontalCurveParameters> curveParameters;
    std::vector<HorizontalAlignmentElement> elements;
    std::vector<HorizontalAlignmentFeaturePoint> featurePoints;
    std::vector<StationLabel> stationLabels;
    double totalLength = 0.0;
};

double distance(const AlignmentPoint2d& a, const AlignmentPoint2d& b);
double clothoidA(double radius, double spiralLength);
double clothoidCurvatureAt(double stationOnSpiral, double radius, double spiralLength);
double clothoidTangentAngleAt(double stationOnSpiral, double radius, double spiralLength);
double clothoidEndX(double radius, double spiralLength);
double clothoidEndY(double radius, double spiralLength);
double spiralShiftP(double radius, double spiralLength);
double spiralTangentOffsetM(double radius, double spiralLength);
double defaultSpiralTangentLength(double turnAngle, double radius, double spiralLength1, double spiralLength2);
std::wstring roadGradeToDisplayName(RoadGrade grade);
RoadGrade roadGradeFromIndex(std::size_t index);
std::vector<std::wstring> roadGradeDisplayNames();

} // namespace roadproto::domain::alignment
