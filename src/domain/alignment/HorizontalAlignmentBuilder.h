#pragma once

#include "domain/alignment/AlignmentGeometry.h"

#include <string>

namespace roadproto::domain::alignment {

struct HorizontalAlignmentInput {
    RoadCenterlineProperties properties;
    std::vector<AlignmentPoint2d> controlPoints;
    HorizontalCurveParameters defaultParameters;
    std::vector<HorizontalCurveParameters> curveParameters;
};

struct HorizontalAlignmentBuildResult {
    bool succeeded = false;
    std::wstring errorMessage;
    HorizontalAlignment alignment;
};

class HorizontalAlignmentBuilder {
public:
    HorizontalAlignmentBuildResult build(const HorizontalAlignmentInput& input) const;

private:
    static bool validate(const HorizontalAlignmentInput& input, std::wstring& errorMessage);
};

} // namespace roadproto::domain::alignment
