#include "domain/profile/ProfileVerticalCurveDisplayPlanner.h"

#include "domain/profile/ProfileVerticalCurveCalculator.h"

#include <algorithm>
#include <cmath>

namespace roadproto::domain::profile {
namespace {

constexpr double kEpsilon = 1.0e-9;
constexpr int kStraightDesignLineColorIndex = 4;
constexpr int kCurveDesignLineColorIndex = 2;
constexpr int kDefaultCurveTangentLineColorIndex = 7;

bool isFinite(double value)
{
    return std::isfinite(value) != 0;
}

int tangentLineColorIndex(const ProfileVerticalCurveData& data)
{
    return data.properties.tangentLineColorIndex > 0
        ? data.properties.tangentLineColorIndex
        : kDefaultCurveTangentLineColorIndex;
}

void addStation(std::vector<double>& stations, double station)
{
    if (isFinite(station)) {
        stations.push_back(station);
    }
}

std::vector<double> uniqueSortedStations(std::vector<double> stations)
{
    std::sort(stations.begin(), stations.end());

    std::vector<double> result;
    result.reserve(stations.size());
    for (const auto station : stations) {
        if (result.empty() || std::fabs(station - result.back()) > kEpsilon) {
            result.push_back(station);
        }
    }

    return result;
}

bool isCurveInterval(
    const std::vector<VerticalCurveElement>& elements,
    double startStation,
    double endStation)
{
    const double midpoint = (startStation + endStation) / 2.0;
    return std::any_of(elements.begin(), elements.end(), [midpoint](const auto& element) {
        return element.bvcStation - kEpsilon <= midpoint && midpoint <= element.evcStation + kEpsilon;
    });
}

bool addDesignLineSegment(
    ProfileVerticalCurveDisplayPlan& plan,
    const ProfileVerticalCurveBuildResult& built,
    double startStation,
    double endStation)
{
    if (endStation <= startStation + kEpsilon) {
        return true;
    }

    const auto startElevation = ProfileVerticalCurveCalculator::elevationAt(built, startStation);
    if (!startElevation.succeeded) {
        plan.errorMessage = startElevation.errorMessage;
        return false;
    }

    const auto endElevation = ProfileVerticalCurveCalculator::elevationAt(built, endStation);
    if (!endElevation.succeeded) {
        plan.errorMessage = endElevation.errorMessage;
        return false;
    }

    const bool curveInterval = isCurveInterval(built.elements, startStation, endStation);
    plan.segments.push_back(VerticalCurveDisplaySegment{
        curveInterval
            ? VerticalCurveDisplaySegmentRole::CurveDesignLine
            : VerticalCurveDisplaySegmentRole::StraightDesignLine,
        startStation,
        startElevation.value,
        endStation,
        endElevation.value,
        curveInterval ? kCurveDesignLineColorIndex : kStraightDesignLineColorIndex});
    return true;
}

void addCurveTangentSegments(
    ProfileVerticalCurveDisplayPlan& plan,
    const ProfileVerticalCurveData& data,
    const ProfileVerticalCurveBuildResult& built)
{
    if (!data.properties.showTangentLines) {
        return;
    }

    const auto colorIndex = tangentLineColorIndex(data);
    for (const auto& element : built.elements) {
        plan.segments.push_back(VerticalCurveDisplaySegment{
            VerticalCurveDisplaySegmentRole::CurveTangentLine,
            element.bvcStation,
            element.bvcElevation,
            element.pviStation,
            element.pviElevation,
            colorIndex});
        plan.segments.push_back(VerticalCurveDisplaySegment{
            VerticalCurveDisplaySegmentRole::CurveTangentLine,
            element.pviStation,
            element.pviElevation,
            element.evcStation,
            element.evcElevation,
            colorIndex});
    }
}

} // namespace

ProfileVerticalCurveDisplayPlan ProfileVerticalCurveDisplayPlanner::build(const ProfileVerticalCurveData& data)
{
    ProfileVerticalCurveDisplayPlan plan;
    if (!isFinite(data.properties.sampleInterval) || data.properties.sampleInterval <= 0.0) {
        plan.errorMessage = L"Sample interval must be positive.";
        return plan;
    }

    const auto built = ProfileVerticalCurveCalculator::rebuild(data);
    if (!built.succeeded) {
        plan.errorMessage = built.errorMessage;
        return plan;
    }
    if (built.orderedControlPoints.size() < 2) {
        plan.errorMessage = L"At least two control points are required.";
        return plan;
    }

    const double startStation = built.orderedControlPoints.front().station;
    const double endStation = built.orderedControlPoints.back().station;
    std::vector<double> stations;
    addStation(stations, startStation);
    addStation(stations, endStation);

    double station = startStation;
    for (int guard = 0; station + data.properties.sampleInterval < endStation - kEpsilon; ++guard) {
        if (guard > 100000) {
            plan.errorMessage = L"Too many vertical curve display samples.";
            return plan;
        }

        const double nextStation = station + data.properties.sampleInterval;
        if (nextStation <= station + kEpsilon) {
            plan.errorMessage = L"Sample interval does not advance station.";
            return plan;
        }

        addStation(stations, nextStation);
        station = nextStation;
    }

    for (const auto& element : built.elements) {
        addStation(stations, element.bvcStation);
        addStation(stations, element.evcStation);
    }

    stations = uniqueSortedStations(std::move(stations));
    for (std::size_t i = 1; i < stations.size(); ++i) {
        if (!addDesignLineSegment(plan, built, stations[i - 1], stations[i])) {
            return plan;
        }
    }

    addCurveTangentSegments(plan, data, built);
    plan.succeeded = true;
    return plan;
}

} // namespace roadproto::domain::profile
