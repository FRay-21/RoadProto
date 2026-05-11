#pragma once

#include "domain/alignment/HorizontalAlignmentBuilder.h"

namespace roadproto::domain::alignment {

struct AlignmentChainElementInput {
    AlignmentElementType type = AlignmentElementType::Line;
    double length = 0.0;
    double radius = 0.0;
    double startRadius = 0.0;
    double endRadius = 0.0;
    double startCurvature = 0.0;
    double endCurvature = 0.0;
    std::size_t curveIndex = 0;

    static AlignmentChainElementInput line(double length);
    static AlignmentChainElementInput circularArc(
        double length,
        double radius,
        CurveTurnDirection direction);
    static AlignmentChainElementInput partialSpiral(
        double length,
        double startRadius,
        double endRadius,
        CurveTurnDirection direction);
    static AlignmentChainElementInput curvatureTransition(
        double length,
        double startCurvature,
        double endCurvature);
};

struct AlignmentElementChainInput {
    RoadCenterlineProperties properties;
    AlignmentCurveCombinationType combinationType = AlignmentCurveCombinationType::ElementChain;
    AlignmentPoint2d startPoint;
    double startHeading = 0.0;
    double startStation = 0.0;
    std::vector<AlignmentChainElementInput> elements;
};

class AlignmentElementChainBuilder {
public:
    HorizontalAlignmentBuildResult build(const AlignmentElementChainInput& input) const;
};

} // namespace roadproto::domain::alignment
