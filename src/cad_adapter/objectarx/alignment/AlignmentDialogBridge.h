#pragma once

#include "domain/alignment/HorizontalAlignmentBuilder.h"

#include <string>
#include <vector>

namespace roadproto::cad_adapter::objectarx {

enum class AlignmentDialogMode {
    Properties,
    Curve,
    Full,
};

enum class AlignmentDialogAction {
    None,
    PickTerrain,
};

struct AlignmentDialogRequest {
    AlignmentDialogMode mode = AlignmentDialogMode::Full;
    std::wstring handle;
    domain::alignment::HorizontalAlignment alignment;
    std::size_t curveIndex = 0;
    bool deleteOnCancel = false;
};

struct AlignmentDialogResponse {
    AlignmentDialogAction action = AlignmentDialogAction::None;
    bool accepted = false;
    AlignmentDialogMode mode = AlignmentDialogMode::Full;
    std::wstring handle;
    domain::alignment::RoadCenterlineProperties properties;
    domain::alignment::HorizontalCurveParameters parameters;
    std::vector<domain::alignment::HorizontalCurveParameters> curveParameters;
    std::size_t curveIndex = 0;
    bool deleteOnCancel = false;
};

bool queueAlignmentWpfDialog(const AlignmentDialogRequest& request, std::wstring& errorMessage);

bool readAlignmentDialogResponse(
    const std::wstring& responsePath,
    AlignmentDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx
