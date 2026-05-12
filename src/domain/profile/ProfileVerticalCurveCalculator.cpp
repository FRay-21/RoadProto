#include "domain/profile/ProfileVerticalCurveCalculator.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::profile {
namespace {

constexpr double kEpsilon = 1e-9;

struct IndexedPvi {
    std::size_t originalIndex = 0;
    VerticalCurvePvi pvi;
};

bool isFinite(double value)
{
    return std::isfinite(value);
}

double slopeBetween(const VerticalCurveControlPoint& a, const VerticalCurveControlPoint& b)
{
    return (b.elevation - a.elevation) / (b.station - a.station);
}

bool samePoint(const VerticalCurveControlPoint& point, const VerticalCurvePvi& pvi)
{
    return std::fabs(point.station - pvi.station) < kEpsilon &&
        std::fabs(point.elevation - pvi.elevation) < kEpsilon;
}

std::vector<VerticalCurvePvi> sortedPvis(const std::vector<VerticalCurvePvi>& pvis)
{
    auto sorted = pvis;
    std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.station < rhs.station;
    });
    return sorted;
}

std::vector<IndexedPvi> sortedPvisWithSourceIndex(const std::vector<VerticalCurvePvi>& pvis)
{
    std::vector<IndexedPvi> sorted;
    sorted.reserve(pvis.size());
    for (std::size_t i = 0; i < pvis.size(); ++i) {
        sorted.push_back(IndexedPvi{i, pvis[i]});
    }
    std::sort(sorted.begin(), sorted.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.pvi.station < rhs.pvi.station;
    });
    return sorted;
}

bool hasOnlyStartAndEnd(const std::vector<VerticalCurveControlPoint>& points)
{
    if (points.size() != 2) {
        return false;
    }
    const bool firstPair = points[0].role == VerticalCurvePointRole::Start &&
        points[1].role == VerticalCurvePointRole::End;
    const bool secondPair = points[1].role == VerticalCurvePointRole::Start &&
        points[0].role == VerticalCurvePointRole::End;
    return firstPair || secondPair;
}

std::vector<VerticalCurveControlPoint> buildOrderedControlPoints(const ProfileVerticalCurveData& data)
{
    std::vector<VerticalCurveControlPoint> points;
    if (!data.pvis.empty() && hasOnlyStartAndEnd(data.controlPoints)) {
        auto startIt = std::find_if(data.controlPoints.begin(), data.controlPoints.end(), [](const auto& point) {
            return point.role == VerticalCurvePointRole::Start;
        });
        auto endIt = std::find_if(data.controlPoints.begin(), data.controlPoints.end(), [](const auto& point) {
            return point.role == VerticalCurvePointRole::End;
        });
        points.push_back(*startIt);
        for (const auto& pvi : sortedPvis(data.pvis)) {
            points.push_back(VerticalCurveControlPoint{VerticalCurvePointRole::Pvi, pvi.station, pvi.elevation});
        }
        points.push_back(*endIt);
    } else {
        points = data.controlPoints;
    }

    std::sort(points.begin(), points.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.station < rhs.station;
    });
    return points;
}

void rebuildControlPointsFromPvis(ProfileVerticalCurveData& data)
{
    if (data.controlPoints.size() < 2) {
        return;
    }

    auto ordered = data.controlPoints;
    std::sort(ordered.begin(), ordered.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.station < rhs.station;
    });

    VerticalCurveControlPoint start = ordered.front();
    VerticalCurveControlPoint end = ordered.back();
    for (const auto& point : ordered) {
        if (point.role == VerticalCurvePointRole::Start) {
            start = point;
            break;
        }
    }
    for (const auto& point : ordered) {
        if (point.role == VerticalCurvePointRole::End) {
            end = point;
            break;
        }
    }
    start.role = VerticalCurvePointRole::Start;
    end.role = VerticalCurvePointRole::End;

    data.controlPoints.clear();
    data.controlPoints.push_back(start);
    for (const auto& pvi : sortedPvis(data.pvis)) {
        data.controlPoints.push_back(VerticalCurveControlPoint{VerticalCurvePointRole::Pvi, pvi.station, pvi.elevation});
    }
    data.controlPoints.push_back(end);
    std::sort(data.controlPoints.begin(), data.controlPoints.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.station < rhs.station;
    });
}

ProfileVerticalCurveEditResult validateEdit(ProfileVerticalCurveData& data, const ProfileVerticalCurveData& original)
{
    const auto built = ProfileVerticalCurveCalculator::rebuild(data);
    if (!built.succeeded) {
        data = original;
        return ProfileVerticalCurveEditResult{false, false, built.errorMessage};
    }
    return ProfileVerticalCurveEditResult{true, true, L""};
}

bool findBracketingControlPoints(
    const std::vector<VerticalCurveControlPoint>& points,
    double station,
    std::size_t& previousIndex,
    std::size_t& nextIndex)
{
    for (std::size_t i = 0; i + 1 < points.size(); ++i) {
        if (points[i].station < station + kEpsilon && station < points[i + 1].station + kEpsilon) {
            previousIndex = i;
            nextIndex = i + 1;
            return true;
        }
    }
    return false;
}

const VerticalCurveElement* findCurveAt(const ProfileVerticalCurveBuildResult& built, double station)
{
    for (const auto& element : built.elements) {
        if (element.bvcStation - kEpsilon <= station && station <= element.evcStation + kEpsilon) {
            return &element;
        }
    }
    return nullptr;
}

ProfileVerticalCurveQueryResult queryTangent(
    const ProfileVerticalCurveBuildResult& built,
    double station,
    bool queryGrade)
{
    const auto& points = built.orderedControlPoints;
    if (points.size() < 2) {
        return ProfileVerticalCurveQueryResult{false, L"At least two control points are required.", 0.0};
    }

    std::size_t previousIndex = 0;
    std::size_t nextIndex = 1;
    if (station <= points.front().station) {
        previousIndex = 0;
        nextIndex = 1;
    } else if (station >= points.back().station) {
        previousIndex = points.size() - 2;
        nextIndex = points.size() - 1;
    } else if (!findBracketingControlPoints(points, station, previousIndex, nextIndex)) {
        return ProfileVerticalCurveQueryResult{false, L"Station is outside the vertical curve definition.", 0.0};
    }

    const auto& previous = points[previousIndex];
    const auto& next = points[nextIndex];
    const double grade = slopeBetween(previous, next);
    if (queryGrade) {
        return ProfileVerticalCurveQueryResult{true, L"", grade};
    }
    return ProfileVerticalCurveQueryResult{true, L"", previous.elevation + grade * (station - previous.station)};
}

ProfileVerticalCurveQueryResult failIfInvalidQuery(const ProfileVerticalCurveBuildResult& built, double station)
{
    if (!built.succeeded) {
        return ProfileVerticalCurveQueryResult{false, built.errorMessage, 0.0};
    }
    if (!isFinite(station)) {
        return ProfileVerticalCurveQueryResult{false, L"Station must be finite.", 0.0};
    }
    return ProfileVerticalCurveQueryResult{true, L"", 0.0};
}

} // namespace

ProfileVerticalCurveBuildResult ProfileVerticalCurveCalculator::rebuild(const ProfileVerticalCurveData& data)
{
    ProfileVerticalCurveBuildResult result;

    for (const auto& point : data.controlPoints) {
        if (!isFinite(point.station) || !isFinite(point.elevation)) {
            result.errorMessage = L"Control point station and elevation must be finite.";
            return result;
        }
    }
    for (const auto& pvi : data.pvis) {
        if (!isFinite(pvi.station) || !isFinite(pvi.elevation) || !isFinite(pvi.radius)) {
            result.errorMessage = L"PVI station, elevation, and radius must be finite.";
            return result;
        }
        if (pvi.radius < 1.0) {
            result.errorMessage = L"PVI radius must be at least 1.0.";
            return result;
        }
    }

    result.orderedControlPoints = buildOrderedControlPoints(data);
    if (result.orderedControlPoints.size() < 2) {
        result.errorMessage = L"At least two control points are required.";
        return result;
    }

    for (std::size_t i = 1; i < result.orderedControlPoints.size(); ++i) {
        if (result.orderedControlPoints[i].station <= result.orderedControlPoints[i - 1].station + kEpsilon) {
            result.errorMessage = L"Control point stations must be strictly increasing.";
            return result;
        }
    }

    const auto pvis = sortedPvisWithSourceIndex(data.pvis);
    for (const auto& indexedPvi : pvis) {
        const auto& pvi = indexedPvi.pvi;
        auto pointIt = std::find_if(
            result.orderedControlPoints.begin(),
            result.orderedControlPoints.end(),
            [&pvi](const auto& point) {
                return point.role == VerticalCurvePointRole::Pvi && samePoint(point, pvi);
            });
        if (pointIt == result.orderedControlPoints.end()) {
            result.errorMessage = L"Each PVI must have a matching control point.";
            return result;
        }

        const std::size_t controlIndex = static_cast<std::size_t>(
            std::distance(result.orderedControlPoints.begin(), pointIt));
        if (controlIndex == 0 || controlIndex + 1 >= result.orderedControlPoints.size()) {
            result.errorMessage = L"PVI must be between start and end control points.";
            return result;
        }

        const auto& previous = result.orderedControlPoints[controlIndex - 1];
        const auto& next = result.orderedControlPoints[controlIndex + 1];
        const double i1 = slopeBetween(previous, *pointIt);
        const double i2 = slopeBetween(*pointIt, next);
        const double gradeDifference = i2 - i1;
        if (std::fabs(gradeDifference) < kEpsilon) {
            continue;
        }

        VerticalCurveElement element;
        element.pviIndex = indexedPvi.originalIndex;
        element.type = gradeDifference < 0.0 ? VerticalCurveType::Crest : VerticalCurveType::Sag;
        element.pviStation = pvi.station;
        element.pviElevation = pvi.elevation;
        element.i1 = i1;
        element.i2 = i2;
        element.gradeDifference = gradeDifference;
        element.radius = pvi.radius;
        element.length = std::fabs(gradeDifference) * pvi.radius;
        element.tangentLength = element.length / 2.0;
        element.bvcStation = pvi.station - element.tangentLength;
        element.evcStation = pvi.station + element.tangentLength;
        element.bvcElevation = pvi.elevation - i1 * element.tangentLength;
        element.evcElevation = pvi.elevation + i2 * element.tangentLength;

        const double zeroGradeX = -i1 * element.length / gradeDifference;
        if (-kEpsilon <= zeroGradeX && zeroGradeX <= element.length + kEpsilon) {
            VerticalCurveKeyPoint keyPoint;
            keyPoint.station = element.bvcStation + zeroGradeX;
            keyPoint.elevation = element.bvcElevation + i1 * zeroGradeX +
                gradeDifference * zeroGradeX * zeroGradeX / (2.0 * element.length);
            keyPoint.isHighPoint = element.type == VerticalCurveType::Crest;
            element.highLowPoint = keyPoint;
        }

        result.elements.push_back(element);
    }

    std::sort(result.elements.begin(), result.elements.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.bvcStation < rhs.bvcStation;
    });
    result.succeeded = true;
    return result;
}

ProfileVerticalCurveQueryResult ProfileVerticalCurveCalculator::elevationAt(
    const ProfileVerticalCurveBuildResult& built,
    double station)
{
    const auto valid = failIfInvalidQuery(built, station);
    if (!valid.succeeded) {
        return valid;
    }

    if (const auto* element = findCurveAt(built, station)) {
        const double x = station - element->bvcStation;
        const double elevation = element->bvcElevation + element->i1 * x +
            element->gradeDifference * x * x / (2.0 * element->length);
        return ProfileVerticalCurveQueryResult{true, L"", elevation};
    }
    return queryTangent(built, station, false);
}

ProfileVerticalCurveQueryResult ProfileVerticalCurveCalculator::gradeAt(
    const ProfileVerticalCurveBuildResult& built,
    double station)
{
    const auto valid = failIfInvalidQuery(built, station);
    if (!valid.succeeded) {
        return valid;
    }

    if (const auto* element = findCurveAt(built, station)) {
        const double x = station - element->bvcStation;
        return ProfileVerticalCurveQueryResult{
            true,
            L"",
            element->i1 + element->gradeDifference * x / element->length};
    }
    return queryTangent(built, station, true);
}

ProfileVerticalCurveSampleResult ProfileVerticalCurveCalculator::sample(const ProfileVerticalCurveData& data, double interval)
{
    ProfileVerticalCurveSampleResult result;
    if (!isFinite(interval) || interval <= 0.0) {
        result.errorMessage = L"Sample interval must be positive.";
        return result;
    }

    const auto built = rebuild(data);
    if (!built.succeeded) {
        result.errorMessage = built.errorMessage;
        return result;
    }

    const double start = built.orderedControlPoints.front().station;
    const double end = built.orderedControlPoints.back().station;
    for (double station = start; station < end; station += interval) {
        const auto elevation = elevationAt(built, station);
        if (!elevation.succeeded) {
            result.errorMessage = elevation.errorMessage;
            return result;
        }
        result.points.push_back(VerticalCurveSamplePoint{station, elevation.value});
    }

    const auto endElevation = elevationAt(built, end);
    if (!endElevation.succeeded) {
        result.errorMessage = endElevation.errorMessage;
        return result;
    }
    if (result.points.empty() || std::fabs(result.points.back().station - end) > kEpsilon) {
        result.points.push_back(VerticalCurveSamplePoint{end, endElevation.value});
    }

    result.succeeded = true;
    return result;
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::moveControlPoint(
    ProfileVerticalCurveData& data,
    std::size_t pointIndex,
    double station,
    double elevation)
{
    if (pointIndex >= data.controlPoints.size()) {
        return ProfileVerticalCurveEditResult{false, false, L"Control point index is out of range."};
    }
    if (!isFinite(station) || !isFinite(elevation)) {
        return ProfileVerticalCurveEditResult{false, false, L"Control point station and elevation must be finite."};
    }

    const auto original = data;
    const auto oldPoint = data.controlPoints[pointIndex];
    data.controlPoints[pointIndex].station = station;
    data.controlPoints[pointIndex].elevation = elevation;
    if (oldPoint.role == VerticalCurvePointRole::Pvi) {
        for (auto& pvi : data.pvis) {
            if (std::fabs(pvi.station - oldPoint.station) < kEpsilon &&
                std::fabs(pvi.elevation - oldPoint.elevation) < kEpsilon) {
                pvi.station = station;
                pvi.elevation = elevation;
                break;
            }
        }
    }
    if (!data.pvis.empty()) {
        rebuildControlPointsFromPvis(data);
    }
    return validateEdit(data, original);
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::movePvi(
    ProfileVerticalCurveData& data,
    std::size_t pviIndex,
    double station,
    double elevation)
{
    if (pviIndex >= data.pvis.size()) {
        return ProfileVerticalCurveEditResult{false, false, L"PVI index is out of range."};
    }
    if (!isFinite(station) || !isFinite(elevation)) {
        return ProfileVerticalCurveEditResult{false, false, L"PVI station and elevation must be finite."};
    }

    const auto original = data;
    data.pvis[pviIndex].station = station;
    data.pvis[pviIndex].elevation = elevation;
    rebuildControlPointsFromPvis(data);
    return validateEdit(data, original);
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::updateRadius(
    ProfileVerticalCurveData& data,
    std::size_t pviIndex,
    double radius)
{
    if (pviIndex >= data.pvis.size()) {
        return ProfileVerticalCurveEditResult{false, false, L"PVI index is out of range."};
    }
    if (!isFinite(radius) || radius < 1.0) {
        return ProfileVerticalCurveEditResult{false, false, L"PVI radius must be at least 1.0."};
    }

    const auto original = data;
    data.pvis[pviIndex].radius = radius;
    return validateEdit(data, original);
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::insertPvi(
    ProfileVerticalCurveData& data,
    double station,
    double elevation,
    double radius)
{
    if (!isFinite(station) || !isFinite(elevation) || !isFinite(radius)) {
        return ProfileVerticalCurveEditResult{false, false, L"PVI station, elevation, and radius must be finite."};
    }
    if (radius < 1.0) {
        return ProfileVerticalCurveEditResult{false, false, L"PVI radius must be at least 1.0."};
    }

    const auto original = data;
    data.pvis.push_back(VerticalCurvePvi{station, elevation, radius});
    rebuildControlPointsFromPvis(data);
    return validateEdit(data, original);
}

ProfileVerticalCurveEditResult ProfileVerticalCurveCalculator::removePvi(
    ProfileVerticalCurveData& data,
    std::size_t pviIndex)
{
    if (pviIndex >= data.pvis.size()) {
        return ProfileVerticalCurveEditResult{false, false, L"PVI index is out of range."};
    }

    const auto original = data;
    data.pvis.erase(data.pvis.begin() + static_cast<std::ptrdiff_t>(pviIndex));
    rebuildControlPointsFromPvis(data);
    return validateEdit(data, original);
}

} // namespace roadproto::domain::profile
