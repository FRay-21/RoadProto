#pragma once

#include <optional>
#include <string>
#include <vector>

namespace roadproto::domain::profile {

enum class VerticalCurvePointRole {
    Start,
    Pvi,
    End,
};

enum class VerticalCurveType {
    TangentOnly,
    Crest,
    Sag,
};

enum class VerticalCurveGripRole {
    StartPoint,
    EndPoint,
    PviPoint,
    RadiusPoint,
};

struct VerticalCurveControlPoint {
    VerticalCurvePointRole role = VerticalCurvePointRole::Pvi;
    double station = 0.0;
    double elevation = 0.0;
};

struct VerticalCurvePvi {
    double station = 0.0;
    double elevation = 0.0;
    double radius = 1000.0;
    bool radiusLocked = false;
};

struct VerticalCurveProperties {
    std::wstring name = L"\u7ad6\u66f2\u7ebf";
    int designLineColorIndex = 4;
    int tangentLineColorIndex = 7;
    int keyPointColorIndex = 2;
    double designLineWidth = 0.35;
    double sampleInterval = 5.0;
    bool showLabels = true;
    bool showTangentLines = true;
};

struct VerticalCurveKeyPoint {
    double station = 0.0;
    double elevation = 0.0;
    bool isHighPoint = false;
};

struct VerticalCurveElement {
    std::size_t pviIndex = 0;
    VerticalCurveType type = VerticalCurveType::TangentOnly;
    double pviStation = 0.0;
    double pviElevation = 0.0;
    double i1 = 0.0;
    double i2 = 0.0;
    double gradeDifference = 0.0;
    double radius = 0.0;
    double length = 0.0;
    double tangentLength = 0.0;
    double bvcStation = 0.0;
    double bvcElevation = 0.0;
    double evcStation = 0.0;
    double evcElevation = 0.0;
    std::optional<VerticalCurveKeyPoint> highLowPoint;
};

struct VerticalCurveSamplePoint {
    double station = 0.0;
    double elevation = 0.0;
};

struct ProfileVerticalCurveData {
    std::wstring profileGraphHandle;
    std::vector<VerticalCurveControlPoint> controlPoints;
    std::vector<VerticalCurvePvi> pvis;
    VerticalCurveProperties properties;
    int version = 1;
};

} // namespace roadproto::domain::profile
