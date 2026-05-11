#include "domain/alignment/AlignmentGripEditService.h"

#include "domain/alignment/HorizontalAlignmentBuilder.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace roadproto::domain::alignment {
namespace {

constexpr double kGripMinimumLength = 1.0;

struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

Vec2 operator-(Vec2 a, Vec2 b)
{
    return {a.x - b.x, a.y - b.y};
}

Vec2 operator*(Vec2 value, double scale)
{
    return {value.x * scale, value.y * scale};
}

Vec2 toVec(const AlignmentPoint2d& point)
{
    return {point.x, point.y};
}

double length(Vec2 value)
{
    return std::sqrt(value.x * value.x + value.y * value.y);
}

Vec2 normalized(Vec2 value)
{
    const auto len = length(value);
    if (len <= 1e-9) {
        return {};
    }
    return {value.x / len, value.y / len};
}

double dot(Vec2 a, Vec2 b)
{
    return a.x * b.x + a.y * b.y;
}

double clampPositive(double value)
{
    return std::max(value, kGripMinimumLength);
}

double clampScale(double value)
{
    return std::clamp(value, 0.25, 4.0);
}

void resetDerivedTangents(HorizontalAlignment& alignment, std::size_t curveIndex)
{
    if (curveIndex >= alignment.curveParameters.size()) {
        return;
    }
    alignment.curveParameters[curveIndex].tangentIn = 0.0;
    alignment.curveParameters[curveIndex].tangentOut = 0.0;
}

void resetTangentsAffectedByControlPoint(HorizontalAlignment& alignment, std::size_t controlPointIndex)
{
    if (alignment.controlPoints.size() < 3) {
        return;
    }
    const auto curveCount = alignment.controlPoints.size() - 2;
    for (std::size_t curveIndex = 0; curveIndex < curveCount; ++curveIndex) {
        if (controlPointIndex >= curveIndex && controlPointIndex <= curveIndex + 2) {
            resetDerivedTangents(alignment, curveIndex);
        }
    }
}

bool rebuildFromStoredData(HorizontalAlignment& alignment)
{
    HorizontalAlignmentInput input;
    input.properties = alignment.properties;
    input.controlPoints = alignment.controlPoints;
    if (!alignment.curveParameters.empty()) {
        input.defaultParameters = alignment.curveParameters.front();
        input.curveParameters = alignment.curveParameters;
    }

    HorizontalAlignmentBuilder builder;
    const auto rebuilt = builder.build(input);
    if (!rebuilt.succeeded) {
        return false;
    }
    alignment = rebuilt.alignment;
    return true;
}

bool applySingleGripOffset(HorizontalAlignment& alignment, std::size_t gripIndex, Vec2 delta)
{
    if (gripIndex < alignment.controlPoints.size()) {
        alignment.controlPoints[gripIndex].x += delta.x;
        alignment.controlPoints[gripIndex].y += delta.y;
        resetTangentsAffectedByControlPoint(alignment, gripIndex);
        return true;
    }

    const auto featureIndex = gripIndex - alignment.controlPoints.size();
    if (featureIndex >= alignment.featurePoints.size()) {
        return false;
    }

    const auto feature = alignment.featurePoints[featureIndex];
    const auto curveIndex = feature.curveIndex;

    if (feature.type == AlignmentFeaturePointType::Start && !alignment.controlPoints.empty()) {
        alignment.controlPoints.front().x += delta.x;
        alignment.controlPoints.front().y += delta.y;
        resetTangentsAffectedByControlPoint(alignment, 0);
        return true;
    }
    if (feature.type == AlignmentFeaturePointType::End && !alignment.controlPoints.empty()) {
        alignment.controlPoints.back().x += delta.x;
        alignment.controlPoints.back().y += delta.y;
        resetTangentsAffectedByControlPoint(alignment, alignment.controlPoints.size() - 1);
        return true;
    }
    if ((feature.type == AlignmentFeaturePointType::PI
            || feature.type == AlignmentFeaturePointType::TangentIntersection)
        && curveIndex + 1 < alignment.controlPoints.size()) {
        alignment.controlPoints[curveIndex + 1].x += delta.x;
        alignment.controlPoints[curveIndex + 1].y += delta.y;
        resetTangentsAffectedByControlPoint(alignment, curveIndex + 1);
        return true;
    }

    if (curveIndex + 2 >= alignment.controlPoints.size()) {
        return false;
    }
    if (alignment.curveParameters.size() <= curveIndex) {
        alignment.curveParameters.resize(curveIndex + 1);
    }

    auto& parameters = alignment.curveParameters[curveIndex];
    const auto previous = alignment.controlPoints[curveIndex];
    const auto pi = alignment.controlPoints[curveIndex + 1];
    const auto next = alignment.controlPoints[curveIndex + 2];
    const auto inDir = normalized(toVec(pi) - toVec(previous));
    const auto outDir = normalized(toVec(next) - toVec(pi));
    const auto tsScaleBase = std::max(distance(feature.point, pi), kGripMinimumLength);

    switch (feature.type) {
    case AlignmentFeaturePointType::TS: {
        const auto scale = clampScale(1.0 + dot(delta, inDir * -1.0) / tsScaleBase);
        parameters.radius = clampPositive(parameters.radius * scale);
        parameters.ls1 = std::max(0.0, parameters.ls1 * scale);
        parameters.ls2 = std::max(0.0, parameters.ls2 * scale);
        resetDerivedTangents(alignment, curveIndex);
        return true;
    }
    case AlignmentFeaturePointType::ST: {
        const auto scale = clampScale(1.0 + dot(delta, outDir) / tsScaleBase);
        parameters.radius = clampPositive(parameters.radius * scale);
        parameters.ls1 = std::max(0.0, parameters.ls1 * scale);
        parameters.ls2 = std::max(0.0, parameters.ls2 * scale);
        resetDerivedTangents(alignment, curveIndex);
        return true;
    }
    case AlignmentFeaturePointType::SC:
    case AlignmentFeaturePointType::TC:
        parameters.ls1 = std::max(0.0, parameters.ls1 + dot(delta, inDir));
        resetDerivedTangents(alignment, curveIndex);
        return true;
    case AlignmentFeaturePointType::CS:
    case AlignmentFeaturePointType::CT:
        parameters.ls2 = std::max(0.0, parameters.ls2 - dot(delta, outDir));
        resetDerivedTangents(alignment, curveIndex);
        return true;
    case AlignmentFeaturePointType::ArcMid: {
        const auto radiusDir = normalized(toVec(feature.point) - toVec(pi));
        parameters.radius = clampPositive(parameters.radius + dot(delta, radiusDir));
        resetDerivedTangents(alignment, curveIndex);
        return true;
    }
    default:
        return false;
    }
}

std::pair<int, std::size_t> semanticTargetKeyForGripIndex(
    const HorizontalAlignment& alignment,
    std::size_t gripIndex)
{
    if (gripIndex < alignment.controlPoints.size()) {
        return {0, gripIndex};
    }

    const auto featureIndex = gripIndex - alignment.controlPoints.size();
    if (featureIndex >= alignment.featurePoints.size()) {
        return {3, gripIndex};
    }

    const auto& feature = alignment.featurePoints[featureIndex];
    switch (feature.type) {
    case AlignmentFeaturePointType::Start:
        return {0, 0};
    case AlignmentFeaturePointType::End:
        return {0, alignment.controlPoints.empty() ? 0 : alignment.controlPoints.size() - 1};
    case AlignmentFeaturePointType::PI:
    case AlignmentFeaturePointType::TangentIntersection:
        return {0, feature.curveIndex + 1};
    default:
        return {1, feature.curveIndex * 32 + static_cast<std::size_t>(feature.type)};
    }
}

} // namespace

AlignmentGripEditResult AlignmentGripEditService::applyGripOffsets(
    HorizontalAlignment& alignment,
    const std::vector<std::size_t>& gripIndices,
    double offsetX,
    double offsetY) const
{
    AlignmentGripEditResult result;
    if (gripIndices.empty() || (std::fabs(offsetX) <= 1e-12 && std::fabs(offsetY) <= 1e-12)) {
        return result;
    }

    const auto originalAlignment = alignment;
    const Vec2 delta{offsetX, offsetY};
    std::vector<std::pair<int, std::size_t>> appliedTargets;
    for (const auto gripIndex : gripIndices) {
        const auto targetKey = semanticTargetKeyForGripIndex(originalAlignment, gripIndex);
        if (std::find(appliedTargets.begin(), appliedTargets.end(), targetKey) != appliedTargets.end()) {
            continue;
        }
        appliedTargets.push_back(targetKey);
        result.changed = applySingleGripOffset(alignment, gripIndex, delta) || result.changed;
    }

    if (!result.changed) {
        return result;
    }

    if (!rebuildFromStoredData(alignment)) {
        alignment = originalAlignment;
        result.succeeded = false;
        result.changed = false;
    }
    return result;
}

} // namespace roadproto::domain::alignment
