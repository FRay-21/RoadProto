#include "domain/alignment/AlignmentElementChainBuilder.h"

#include "domain/alignment/StationFormatter.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace roadproto::domain::alignment {
namespace {

constexpr double kEpsilon = 1e-9;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

Vec2 operator+(Vec2 a, Vec2 b)
{
    return {a.x + b.x, a.y + b.y};
}

Vec2 operator-(Vec2 a, Vec2 b)
{
    return {a.x - b.x, a.y - b.y};
}

Vec2 operator*(Vec2 a, double scale)
{
    return {a.x * scale, a.y * scale};
}

AlignmentPoint2d toPoint(Vec2 value)
{
    return {value.x, value.y};
}

Vec2 toVec(const AlignmentPoint2d& point)
{
    return {point.x, point.y};
}

Vec2 fromHeading(double heading)
{
    return {std::cos(heading), std::sin(heading)};
}

Vec2 leftNormal(Vec2 value)
{
    return {-value.y, value.x};
}

Vec2 rotate(Vec2 value, double angle)
{
    const auto c = std::cos(angle);
    const auto s = std::sin(angle);
    return {value.x * c - value.y * s, value.x * s + value.y * c};
}

double directionSign(CurveTurnDirection direction)
{
    return direction == CurveTurnDirection::Left ? 1.0 : -1.0;
}

double headingAt(double startHeading, double startCurvature, double endCurvature, double length, double station)
{
    if (length <= kEpsilon) {
        return startHeading;
    }
    const auto s = std::clamp(station, 0.0, length);
    const auto curvatureSlope = (endCurvature - startCurvature) / length;
    return startHeading + startCurvature * s + 0.5 * curvatureSlope * s * s;
}

AlignmentPoint2d pointAtSamples(const std::vector<AlignmentSamplePoint>& samples, double station)
{
    if (samples.empty()) {
        return {};
    }
    if (station <= samples.front().station) {
        return samples.front().point;
    }
    if (station >= samples.back().station) {
        return samples.back().point;
    }

    for (std::size_t i = 1; i < samples.size(); ++i) {
        const auto& previous = samples[i - 1];
        const auto& next = samples[i];
        if (station > next.station) {
            continue;
        }
        const auto span = next.station - previous.station;
        const auto t = span <= kEpsilon ? 0.0 : (station - previous.station) / span;
        return AlignmentPoint2d{
            previous.point.x + (next.point.x - previous.point.x) * t,
            previous.point.y + (next.point.y - previous.point.y) * t};
    }
    return samples.back().point;
}

std::vector<AlignmentSamplePoint> sampleLine(
    const AlignmentPoint2d& start,
    double heading,
    double startStation,
    double elementLength,
    AlignmentPoint2d& end)
{
    const auto divisions = std::max(1, static_cast<int>(std::ceil(elementLength / 10.0)));
    std::vector<AlignmentSamplePoint> samples;
    samples.reserve(static_cast<std::size_t>(divisions) + 1);
    const auto tangent = fromHeading(heading);
    for (int i = 0; i <= divisions; ++i) {
        const auto s = elementLength * static_cast<double>(i) / static_cast<double>(divisions);
        samples.push_back({toPoint(toVec(start) + tangent * s), startStation + s});
    }
    end = samples.back().point;
    return samples;
}

std::vector<AlignmentSamplePoint> sampleArc(
    const AlignmentPoint2d& start,
    double heading,
    double startStation,
    double elementLength,
    double signedCurvature,
    AlignmentPoint2d& end)
{
    const auto divisions = std::max(6, static_cast<int>(std::ceil(elementLength / 5.0)));
    std::vector<AlignmentSamplePoint> samples;
    samples.reserve(static_cast<std::size_t>(divisions) + 1);

    if (std::fabs(signedCurvature) <= kEpsilon) {
        return sampleLine(start, heading, startStation, elementLength, end);
    }

    const auto radius = 1.0 / std::fabs(signedCurvature);
    const auto turnSign = signedCurvature > 0.0 ? 1.0 : -1.0;
    const auto tangent = fromHeading(heading);
    const auto center = toVec(start) + leftNormal(tangent) * (turnSign * radius);
    const auto startVector = toVec(start) - center;

    for (int i = 0; i <= divisions; ++i) {
        const auto s = elementLength * static_cast<double>(i) / static_cast<double>(divisions);
        samples.push_back({
            toPoint(center + rotate(startVector, signedCurvature * s)),
            startStation + s});
    }
    end = samples.back().point;
    return samples;
}

std::vector<AlignmentSamplePoint> sampleCurvatureTransition(
    const AlignmentPoint2d& start,
    double heading,
    double startStation,
    double elementLength,
    double startCurvature,
    double endCurvature,
    AlignmentPoint2d& end)
{
    const auto divisions = std::max(8, static_cast<int>(std::ceil(elementLength / 2.0)));
    std::vector<AlignmentSamplePoint> samples;
    samples.reserve(static_cast<std::size_t>(divisions) + 1);
    samples.push_back({start, startStation});

    Vec2 current = toVec(start);
    double previousS = 0.0;
    for (int i = 1; i <= divisions; ++i) {
        const auto s = elementLength * static_cast<double>(i) / static_cast<double>(divisions);
        const auto ds = s - previousS;
        const auto midS = previousS + ds * 0.5;
        const auto midHeading = headingAt(heading, startCurvature, endCurvature, elementLength, midS);
        current = current + fromHeading(midHeading) * ds;
        samples.push_back({toPoint(current), startStation + s});
        previousS = s;
    }

    end = samples.back().point;
    return samples;
}

void appendStationLabels(HorizontalAlignment& alignment, double interval)
{
    if (interval <= 0.0 || alignment.elements.empty()) {
        return;
    }

    std::vector<AlignmentSamplePoint> samples;
    for (const auto& element : alignment.elements) {
        samples.insert(samples.end(), element.samples.begin(), element.samples.end());
    }
    if (samples.empty()) {
        return;
    }

    const auto startStation = samples.front().station;
    for (double station = startStation; station <= alignment.totalLength + 1e-6; station += interval) {
        alignment.stationLabels.push_back({
            pointAtSamples(samples, station),
            station,
            StationFormatter::format(station)});
    }
}

void appendFeaturePoint(
    HorizontalAlignment& alignment,
    AlignmentFeaturePointType type,
    std::size_t curveIndex,
    const AlignmentPoint2d& point,
    double station)
{
    alignment.featurePoints.push_back({type, curveIndex, point, station});
}

bool validateElement(const AlignmentChainElementInput& element, std::wstring& errorMessage)
{
    if (element.length <= 0.0) {
        errorMessage = L"线形单元长度必须大于 0。";
        return false;
    }
    if (element.type == AlignmentElementType::CircularArc && element.radius <= 0.0) {
        errorMessage = L"圆曲线半径必须大于 0。";
        return false;
    }
    if (element.type == AlignmentElementType::PartialSpiral
        && std::fabs(element.endCurvature - element.startCurvature) <= kEpsilon) {
        errorMessage = L"不完整缓和曲线的起终曲率不能相同。";
        return false;
    }
    return true;
}

} // namespace

AlignmentChainElementInput AlignmentChainElementInput::line(double length)
{
    AlignmentChainElementInput element;
    element.type = AlignmentElementType::Line;
    element.length = length;
    return element;
}

AlignmentChainElementInput AlignmentChainElementInput::circularArc(
    double length,
    double radius,
    CurveTurnDirection direction)
{
    AlignmentChainElementInput element;
    element.type = AlignmentElementType::CircularArc;
    element.length = length;
    element.radius = radius;
    element.startRadius = radius;
    element.endRadius = radius;
    element.startCurvature = radius <= 0.0 ? 0.0 : directionSign(direction) / radius;
    element.endCurvature = element.startCurvature;
    return element;
}

AlignmentChainElementInput AlignmentChainElementInput::partialSpiral(
    double length,
    double startRadius,
    double endRadius,
    CurveTurnDirection direction)
{
    AlignmentChainElementInput element;
    element.type = AlignmentElementType::PartialSpiral;
    element.length = length;
    element.startRadius = startRadius;
    element.endRadius = endRadius;
    element.startCurvature = startRadius <= 0.0 ? 0.0 : directionSign(direction) / startRadius;
    element.endCurvature = endRadius <= 0.0 ? 0.0 : directionSign(direction) / endRadius;
    return element;
}

AlignmentChainElementInput AlignmentChainElementInput::curvatureTransition(
    double length,
    double startCurvature,
    double endCurvature)
{
    AlignmentChainElementInput element;
    element.type = AlignmentElementType::PartialSpiral;
    element.length = length;
    element.startCurvature = startCurvature;
    element.endCurvature = endCurvature;
    element.startRadius = std::fabs(startCurvature) <= kEpsilon ? 0.0 : 1.0 / std::fabs(startCurvature);
    element.endRadius = std::fabs(endCurvature) <= kEpsilon ? 0.0 : 1.0 / std::fabs(endCurvature);
    return element;
}

HorizontalAlignmentBuildResult AlignmentElementChainBuilder::build(const AlignmentElementChainInput& input) const
{
    HorizontalAlignmentBuildResult result;
    if (input.properties.stationLabelInterval <= 0.0) {
        result.errorMessage = L"桩号标注间距必须大于 0。";
        return result;
    }
    if (input.elements.empty()) {
        result.errorMessage = L"元素链至少需要一个线形单元。";
        return result;
    }

    HorizontalAlignment alignment;
    alignment.properties = input.properties;
    alignment.combinationType = input.combinationType;

    auto currentPoint = input.startPoint;
    auto currentHeading = input.startHeading;
    auto station = input.startStation;
    appendFeaturePoint(alignment, AlignmentFeaturePointType::Start, 0, currentPoint, station);

    for (std::size_t index = 0; index < input.elements.size(); ++index) {
        const auto& source = input.elements[index];
        std::wstring errorMessage;
        if (!validateElement(source, errorMessage)) {
            result.errorMessage = errorMessage;
            return result;
        }

        AlignmentPoint2d endPoint;
        std::vector<AlignmentSamplePoint> samples;
        if (source.type == AlignmentElementType::Line) {
            samples = sampleLine(currentPoint, currentHeading, station, source.length, endPoint);
        } else if (source.type == AlignmentElementType::CircularArc) {
            samples = sampleArc(currentPoint, currentHeading, station, source.length, source.startCurvature, endPoint);
        } else if (source.type == AlignmentElementType::PartialSpiral) {
            samples = sampleCurvatureTransition(
                currentPoint,
                currentHeading,
                station,
                source.length,
                source.startCurvature,
                source.endCurvature,
                endPoint);
        } else {
            result.errorMessage = L"元素链暂不支持该线形单元。";
            return result;
        }

        HorizontalAlignmentElement element;
        element.type = source.type;
        element.curveIndex = source.curveIndex == 0 ? index : source.curveIndex;
        element.start = currentPoint;
        element.end = endPoint;
        element.startStation = station;
        element.length = source.length;
        element.radius = source.radius;
        element.spiralLength = source.type == AlignmentElementType::PartialSpiral ? source.length : 0.0;
        element.startHeading = currentHeading;
        element.endHeading = headingAt(
            currentHeading,
            source.startCurvature,
            source.endCurvature,
            source.length,
            source.length);
        element.startCurvature = source.startCurvature;
        element.endCurvature = source.endCurvature;
        element.samples = std::move(samples);
        alignment.elements.push_back(std::move(element));

        const auto endStation = station + source.length;
        if (source.type == AlignmentElementType::CircularArc) {
            appendFeaturePoint(alignment, AlignmentFeaturePointType::ArcMid, index, pointAtSamples(alignment.elements.back().samples, station + source.length * 0.5), station + source.length * 0.5);
            appendFeaturePoint(alignment, AlignmentFeaturePointType::CT, index, endPoint, endStation);
        } else if (source.type == AlignmentElementType::PartialSpiral) {
            appendFeaturePoint(alignment, AlignmentFeaturePointType::SC, index, endPoint, endStation);
        }

        currentPoint = endPoint;
        currentHeading = alignment.elements.back().endHeading;
        station = endStation;
    }

    alignment.totalLength = station;
    appendFeaturePoint(
        alignment,
        AlignmentFeaturePointType::End,
        alignment.elements.empty() ? 0 : alignment.elements.back().curveIndex,
        currentPoint,
        station);
    appendStationLabels(alignment, input.properties.stationLabelInterval);

    result.succeeded = true;
    result.alignment = std::move(alignment);
    return result;
}

} // namespace roadproto::domain::alignment
