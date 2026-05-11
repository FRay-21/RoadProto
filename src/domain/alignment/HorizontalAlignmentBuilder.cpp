#include "domain/alignment/HorizontalAlignmentBuilder.h"

#include "domain/alignment/StationFormatter.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace roadproto::domain::alignment {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kEpsilon = 1e-8;

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

Vec2 toVec(const AlignmentPoint2d& point)
{
    return {point.x, point.y};
}

AlignmentPoint2d toPoint(Vec2 value)
{
    return {value.x, value.y};
}

double length(Vec2 value)
{
    return std::sqrt(value.x * value.x + value.y * value.y);
}

Vec2 normalized(Vec2 value)
{
    const auto len = length(value);
    if (len <= kEpsilon) {
        return {};
    }
    return {value.x / len, value.y / len};
}

double dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

double cross(Vec2 a, Vec2 b)
{
    return a.x * b.y - a.y * b.x;
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

AlignmentPoint2d pointAt(const std::vector<AlignmentSamplePoint>& samples, double station)
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

double clothoidLocalX(double stationOnSpiral, double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength <= 0.0) {
        return 0.0;
    }
    const auto s = std::clamp(stationOnSpiral, 0.0, spiralLength);
    const auto a2 = radius * spiralLength;
    const auto a4 = a2 * a2;
    const auto a8 = a4 * a4;
    const auto a12 = a8 * a4;
    return s
        - std::pow(s, 5.0) / (40.0 * a4)
        + std::pow(s, 9.0) / (3456.0 * a8)
        - std::pow(s, 13.0) / (599040.0 * a12);
}

double clothoidLocalY(double stationOnSpiral, double radius, double spiralLength)
{
    if (radius <= 0.0 || spiralLength <= 0.0) {
        return 0.0;
    }
    const auto s = std::clamp(stationOnSpiral, 0.0, spiralLength);
    const auto a2 = radius * spiralLength;
    const auto a6 = a2 * a2 * a2;
    const auto a10 = a6 * a2 * a2;
    return std::pow(s, 3.0) / (6.0 * a2)
        - std::pow(s, 7.0) / (336.0 * a6)
        + std::pow(s, 11.0) / (42240.0 * a10);
}

AlignmentPoint2d clothoidPoint(
    const AlignmentPoint2d& start,
    double heading,
    double turnSign,
    double stationOnSpiral,
    double radius,
    double spiralLength)
{
    const auto tangent = fromHeading(heading);
    const auto normal = leftNormal(tangent);
    const auto x = clothoidLocalX(stationOnSpiral, radius, spiralLength);
    const auto y = clothoidLocalY(stationOnSpiral, radius, spiralLength) * turnSign;
    return toPoint(toVec(start) + tangent * x + normal * y);
}

std::vector<AlignmentSamplePoint> sampleLine(
    const AlignmentPoint2d& start,
    const AlignmentPoint2d& end,
    double startStation)
{
    const auto segmentLength = distance(start, end);
    const auto divisions = std::max(1, static_cast<int>(std::ceil(segmentLength / 10.0)));
    std::vector<AlignmentSamplePoint> samples;
    samples.reserve(static_cast<std::size_t>(divisions) + 1);
    for (int i = 0; i <= divisions; ++i) {
        const auto t = static_cast<double>(i) / static_cast<double>(divisions);
        samples.push_back({
            AlignmentPoint2d{start.x + (end.x - start.x) * t, start.y + (end.y - start.y) * t},
            startStation + segmentLength * t});
    }
    return samples;
}

std::vector<AlignmentSamplePoint> sampleSpiralIn(
    const AlignmentPoint2d& start,
    double heading,
    double startStation,
    double elementLength,
    double radius,
    double turnSign,
    AlignmentPoint2d& end,
    double& endHeading)
{
    const auto divisions = std::max(6, static_cast<int>(std::ceil(std::max(elementLength, 1.0) / 5.0)));
    std::vector<AlignmentSamplePoint> samples;
    samples.reserve(static_cast<std::size_t>(divisions) + 1);
    for (int i = 0; i <= divisions; ++i) {
        const auto s = elementLength * static_cast<double>(i) / static_cast<double>(divisions);
        samples.push_back({clothoidPoint(start, heading, turnSign, s, radius, elementLength), startStation + s});
    }
    end = samples.back().point;
    endHeading = heading + turnSign * clothoidTangentAngleAt(elementLength, radius, elementLength);
    return samples;
}

std::vector<AlignmentSamplePoint> sampleSpiralOutFromEnd(
    const AlignmentPoint2d& end,
    double outgoingHeading,
    double startStation,
    double elementLength,
    double radius,
    double turnSign,
    AlignmentPoint2d& start,
    double& startHeading)
{
    const auto divisions = std::max(6, static_cast<int>(std::ceil(std::max(elementLength, 1.0) / 5.0)));
    const auto reverseHeading = outgoingHeading + kPi;
    std::vector<AlignmentSamplePoint> samples;
    samples.reserve(static_cast<std::size_t>(divisions) + 1);
    for (int i = 0; i <= divisions; ++i) {
        const auto u = elementLength * static_cast<double>(i) / static_cast<double>(divisions);
        const auto reverseStation = elementLength - u;
        samples.push_back({
            clothoidPoint(end, reverseHeading, -turnSign, reverseStation, radius, elementLength),
            startStation + u});
    }
    start = samples.front().point;
    startHeading = outgoingHeading - turnSign * clothoidTangentAngleAt(elementLength, radius, elementLength);
    return samples;
}

std::vector<AlignmentSamplePoint> sampleCircularArc(
    const AlignmentPoint2d& start,
    const AlignmentPoint2d& expectedEnd,
    double startHeading,
    double startStation,
    double arcLength,
    double radius,
    double turnSign)
{
    const auto divisions = std::max(6, static_cast<int>(std::ceil(std::max(arcLength, 1.0) / 5.0)));
    const auto tangent = fromHeading(startHeading);
    const auto center = toVec(start) + leftNormal(tangent) * (turnSign * radius);
    const auto startVector = toVec(start) - center;
    const auto angle = radius <= 0.0 ? 0.0 : arcLength / radius;

    std::vector<AlignmentSamplePoint> samples;
    samples.reserve(static_cast<std::size_t>(divisions) + 1);
    for (int i = 0; i <= divisions; ++i) {
        const auto t = static_cast<double>(i) / static_cast<double>(divisions);
        samples.push_back({toPoint(center + rotate(startVector, turnSign * angle * t)), startStation + arcLength * t});
    }
    if (!samples.empty()) {
        samples.back().point = expectedEnd;
    }
    return samples;
}

void appendStationLabels(HorizontalAlignment& alignment, double interval)
{
    if (interval <= 0.0 || alignment.elements.empty()) {
        return;
    }

    std::vector<AlignmentSamplePoint> allSamples;
    for (const auto& element : alignment.elements) {
        allSamples.insert(allSamples.end(), element.samples.begin(), element.samples.end());
    }
    if (allSamples.empty()) {
        return;
    }

    for (double station = 0.0; station <= alignment.totalLength + 1e-6; station += interval) {
        alignment.stationLabels.push_back(
            StationLabel{pointAt(allSamples, station), station, StationFormatter::format(station)});
    }
}

void appendElement(
    HorizontalAlignment& alignment,
    AlignmentElementType type,
    std::size_t curveIndex,
    const AlignmentPoint2d& start,
    const AlignmentPoint2d& end,
    double startStation,
    double elementLength,
    double radius,
    double spiralLength,
    double startHeading,
    double endHeading,
    double startCurvature,
    double endCurvature,
    std::vector<AlignmentSamplePoint> samples)
{
    HorizontalAlignmentElement element;
    element.type = type;
    element.curveIndex = curveIndex;
    element.start = start;
    element.end = end;
    element.startStation = startStation;
    element.length = elementLength;
    element.radius = radius;
    element.spiralLength = spiralLength;
    element.startHeading = startHeading;
    element.endHeading = endHeading;
    element.startCurvature = startCurvature;
    element.endCurvature = endCurvature;
    element.samples = std::move(samples);
    alignment.elements.push_back(std::move(element));
}

struct CurveLayout {
    std::size_t curveIndex = 0;
    std::size_t piPointIndex = 0;
    AlignmentPoint2d pi;
    Vec2 inDir;
    Vec2 outDir;
    double inHeading = 0.0;
    double outHeading = 0.0;
    double signedTurnAngle = 0.0;
    double turnAngle = 0.0;
    double turnSign = 1.0;
    HorizontalCurveParameters params;
    double beta1 = 0.0;
    double beta2 = 0.0;
    double arcLength = 0.0;
    double tangentIn = 0.0;
    double tangentOut = 0.0;
    AlignmentPoint2d ts;
    AlignmentPoint2d st;
};

struct TangentLengths {
    double in = 0.0;
    double out = 0.0;
};

TangentLengths transitionTangentLengths(
    double turnAngle,
    double radius,
    double spiralLength1,
    double spiralLength2)
{
    const auto fallback = defaultSpiralTangentLength(turnAngle, radius, spiralLength1, spiralLength2);
    if (radius <= 0.0 || turnAngle <= kEpsilon) {
        return {fallback, fallback};
    }

    const auto sinTurn = std::sin(turnAngle);
    if (std::fabs(sinTurn) <= kEpsilon) {
        return {fallback, fallback};
    }

    const auto beta1 = clothoidTangentAngleAt(spiralLength1, radius, spiralLength1);
    const auto beta2 = clothoidTangentAngleAt(spiralLength2, radius, spiralLength2);
    const auto x1 = clothoidEndX(radius, spiralLength1);
    const auto y1 = clothoidEndY(radius, spiralLength1);
    const auto x2 = clothoidEndX(radius, spiralLength2);
    const auto y2 = clothoidEndY(radius, spiralLength2);

    const auto arcEndY = y1 + radius * (std::cos(beta1) - std::cos(turnAngle - beta2));
    const auto tangentOutMinusSpiralX = (arcEndY - y2 * std::cos(turnAngle)) / sinTurn;
    const auto tangentOut = tangentOutMinusSpiralX + x2;

    const auto arcEndX = x1 + radius * (std::sin(turnAngle - beta2) - std::sin(beta1));
    const auto tangentIn =
        arcEndX - (tangentOutMinusSpiralX * std::cos(turnAngle) - y2 * std::sin(turnAngle));

    if (!std::isfinite(tangentIn) || !std::isfinite(tangentOut) || tangentIn <= 0.0 || tangentOut <= 0.0) {
        return {fallback, fallback};
    }
    return {tangentIn, tangentOut};
}

HorizontalCurveParameters curveParametersFor(
    const HorizontalAlignmentInput& input,
    std::size_t curveIndex)
{
    if (input.curveParameters.empty()) {
        return input.defaultParameters;
    }
    if (curveIndex < input.curveParameters.size()) {
        return input.curveParameters[curveIndex];
    }
    if (input.curveParameters.size() == 1) {
        return input.curveParameters.front();
    }
    return input.defaultParameters;
}

bool prepareCurveLayouts(
    const HorizontalAlignmentInput& input,
    std::vector<CurveLayout>& layouts,
    std::wstring& errorMessage)
{
    layouts.clear();
    layouts.reserve(input.controlPoints.size() - 2);

    for (std::size_t piPointIndex = 1; piPointIndex + 1 < input.controlPoints.size(); ++piPointIndex) {
        const auto& previous = input.controlPoints[piPointIndex - 1];
        const auto& pi = input.controlPoints[piPointIndex];
        const auto& next = input.controlPoints[piPointIndex + 1];

        CurveLayout layout;
        layout.curveIndex = piPointIndex - 1;
        layout.piPointIndex = piPointIndex;
        layout.pi = pi;
        layout.inDir = normalized(toVec(pi) - toVec(previous));
        layout.outDir = normalized(toVec(next) - toVec(pi));
        layout.inHeading = std::atan2(layout.inDir.y, layout.inDir.x);
        layout.outHeading = std::atan2(layout.outDir.y, layout.outDir.x);
        layout.signedTurnAngle = std::atan2(cross(layout.inDir, layout.outDir), dot(layout.inDir, layout.outDir));
        layout.turnAngle = std::fabs(layout.signedTurnAngle);

        if (layout.turnAngle <= kEpsilon || std::fabs(kPi - layout.turnAngle) <= kEpsilon) {
            errorMessage = L"控制点链中存在近似共线交点，无法生成五单元平曲线。";
            return false;
        }

        layout.params = curveParametersFor(input, layout.curveIndex);
        if (layout.params.radius <= 0.0 || layout.params.ls1 < 0.0 || layout.params.ls2 < 0.0) {
            errorMessage = L"平曲线参数无效。";
            return false;
        }

        layout.beta1 = clothoidTangentAngleAt(layout.params.ls1, layout.params.radius, layout.params.ls1);
        layout.beta2 = clothoidTangentAngleAt(layout.params.ls2, layout.params.radius, layout.params.ls2);
        const auto circularAngle = layout.turnAngle - layout.beta1 - layout.beta2;
        if (circularAngle <= kEpsilon) {
            errorMessage = L"缓和曲线长度过长，圆曲线长度不足。";
            return false;
        }

        layout.arcLength = layout.params.radius * circularAngle;
        const auto tangentLengths = transitionTangentLengths(
            layout.turnAngle,
            layout.params.radius,
            layout.params.ls1,
            layout.params.ls2);
        layout.tangentIn = tangentLengths.in;
        layout.tangentOut = tangentLengths.out;
        layout.turnSign = layout.signedTurnAngle < 0.0 ? -1.0 : 1.0;
        layout.ts = toPoint(toVec(pi) - layout.inDir * layout.tangentIn);
        layout.st = toPoint(toVec(pi) + layout.outDir * layout.tangentOut);
        layouts.push_back(layout);
    }

    for (std::size_t segmentIndex = 0; segmentIndex + 1 < input.controlPoints.size(); ++segmentIndex) {
        double requiredLength = 0.0;
        if (segmentIndex > 0) {
            requiredLength += layouts[segmentIndex - 1].tangentOut;
        }
        if (segmentIndex < layouts.size()) {
            requiredLength += layouts[segmentIndex].tangentIn;
        }

        const auto segmentLength = distance(input.controlPoints[segmentIndex], input.controlPoints[segmentIndex + 1]);
        if (requiredLength >= segmentLength - kEpsilon) {
            errorMessage = L"相邻交点的切线长度不足，请减小半径、缓和曲线长度或调整控制点。";
            return false;
        }
    }

    return true;
}

HorizontalAlignmentBuildResult buildAlignmentChain(const HorizontalAlignmentInput& input)
{
    HorizontalAlignmentBuildResult result;
    HorizontalAlignment alignment;
    alignment.properties = input.properties;
    alignment.controlPoints = input.controlPoints;

    std::wstring errorMessage;
    std::vector<CurveLayout> layouts;
    if (!prepareCurveLayouts(input, layouts, errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    double station = 0.0;
    AlignmentPoint2d currentPoint = input.controlPoints.front();
    alignment.featurePoints.push_back({AlignmentFeaturePointType::Start, 0, currentPoint, station});

    for (const auto& layout : layouts) {
        const auto lineSamples = sampleLine(currentPoint, layout.ts, station);
        appendElement(
            alignment,
            AlignmentElementType::Line,
            layout.curveIndex,
            currentPoint,
            layout.ts,
            station,
            distance(currentPoint, layout.ts),
            0.0,
            0.0,
            layout.inHeading,
            layout.inHeading,
            0.0,
            0.0,
            lineSamples);
        station += alignment.elements.back().length;

        const auto tsStation = station;
        AlignmentPoint2d sc;
        double scHeading = layout.inHeading;
        auto spiralInSamples = sampleSpiralIn(
            layout.ts,
            layout.inHeading,
            station,
            layout.params.ls1,
            layout.params.radius,
            layout.turnSign,
            sc,
            scHeading);
        appendElement(
            alignment,
            AlignmentElementType::SpiralIn,
            layout.curveIndex,
            layout.ts,
            sc,
            station,
            layout.params.ls1,
            layout.params.radius,
            layout.params.ls1,
            layout.inHeading,
            scHeading,
            0.0,
            layout.turnSign / layout.params.radius,
            spiralInSamples);
        station += layout.params.ls1;

        const auto scStation = station;
        AlignmentPoint2d cs;
        double csHeading = layout.outHeading;
        auto spiralOutSamples = sampleSpiralOutFromEnd(
            layout.st,
            layout.outHeading,
            station + layout.arcLength,
            layout.params.ls2,
            layout.params.radius,
            layout.turnSign,
            cs,
            csHeading);

        auto arcSamples = sampleCircularArc(
            sc,
            cs,
            scHeading,
            station,
            layout.arcLength,
            layout.params.radius,
            layout.turnSign);
        appendElement(
            alignment,
            AlignmentElementType::CircularArc,
            layout.curveIndex,
            sc,
            cs,
            station,
            layout.arcLength,
            layout.params.radius,
            0.0,
            scHeading,
            csHeading,
            layout.turnSign / layout.params.radius,
            layout.turnSign / layout.params.radius,
            arcSamples);
        station += layout.arcLength;

        const auto csStation = station;
        appendElement(
            alignment,
            AlignmentElementType::SpiralOut,
            layout.curveIndex,
            cs,
            layout.st,
            station,
            layout.params.ls2,
            layout.params.radius,
            layout.params.ls2,
            csHeading,
            layout.outHeading,
            layout.turnSign / layout.params.radius,
            0.0,
            spiralOutSamples);
        station += layout.params.ls2;

        const auto stStation = station;
        currentPoint = layout.st;

        HorizontalCurveParameters storedParams = layout.params;
        storedParams.tangentIn = layout.tangentIn;
        storedParams.tangentOut = layout.tangentOut;
        alignment.curveParameters.push_back(storedParams);

        alignment.featurePoints.push_back({AlignmentFeaturePointType::PI, layout.curveIndex, layout.pi, tsStation});
        alignment.featurePoints.push_back({AlignmentFeaturePointType::TS, layout.curveIndex, layout.ts, tsStation});
        alignment.featurePoints.push_back({AlignmentFeaturePointType::SC, layout.curveIndex, sc, scStation});
        alignment.featurePoints.push_back({AlignmentFeaturePointType::CS, layout.curveIndex, cs, csStation});
        alignment.featurePoints.push_back({AlignmentFeaturePointType::ST, layout.curveIndex, layout.st, stStation});
        alignment.featurePoints.push_back(
            {AlignmentFeaturePointType::ArcMid,
             layout.curveIndex,
             pointAt(alignment.elements[alignment.elements.size() - 2].samples, scStation + layout.arcLength / 2.0),
             scStation + layout.arcLength / 2.0});
    }

    const auto& finalPoint = input.controlPoints.back();
    const auto finalLineSamples = sampleLine(currentPoint, finalPoint, station);
    const auto finalCurveIndex = layouts.empty() ? 0 : layouts.back().curveIndex;
    const auto finalHeading = std::atan2(finalPoint.y - currentPoint.y, finalPoint.x - currentPoint.x);
    appendElement(
        alignment,
        AlignmentElementType::Line,
        finalCurveIndex,
        currentPoint,
        finalPoint,
        station,
        distance(currentPoint, finalPoint),
        0.0,
        0.0,
        finalHeading,
        finalHeading,
        0.0,
        0.0,
        finalLineSamples);
    station += alignment.elements.back().length;

    alignment.totalLength = station;
    alignment.featurePoints.push_back({AlignmentFeaturePointType::End, finalCurveIndex, finalPoint, alignment.totalLength});
    appendStationLabels(alignment, alignment.properties.stationLabelInterval);

    result.succeeded = true;
    result.alignment = std::move(alignment);
    return result;
}

} // namespace

bool HorizontalAlignmentBuilder::validate(const HorizontalAlignmentInput& input, std::wstring& errorMessage)
{
    if (input.controlPoints.size() < 3) {
        errorMessage = L"平面布线至少需要起点、交点和第三点。";
        return false;
    }
    if (input.defaultParameters.radius <= 0.0) {
        errorMessage = L"圆曲线半径必须大于 0。";
        return false;
    }
    if (input.defaultParameters.ls1 < 0.0 || input.defaultParameters.ls2 < 0.0) {
        errorMessage = L"缓和曲线长度不能为负。";
        return false;
    }
    if (input.properties.stationLabelInterval <= 0.0) {
        errorMessage = L"桩号标注间距必须大于 0。";
        return false;
    }
    for (std::size_t i = 1; i < input.controlPoints.size(); ++i) {
        if (distance(input.controlPoints[i - 1], input.controlPoints[i]) <= kEpsilon) {
            errorMessage = L"控制点不能重合。";
            return false;
        }
    }
    return true;
}

HorizontalAlignmentBuildResult HorizontalAlignmentBuilder::build(const HorizontalAlignmentInput& input) const
{
    std::wstring errorMessage;
    if (!validate(input, errorMessage)) {
        HorizontalAlignmentBuildResult result;
        result.errorMessage = errorMessage;
        return result;
    }

    return buildAlignmentChain(input);
}

} // namespace roadproto::domain::alignment
