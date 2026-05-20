#pragma once

#include "domain/cross_section/SlopeTemplateModel.h"

#include "gept3dar.h"

#include <string>

namespace roadproto::cad_adapter::objectarx::cross_section {

struct SlopeTemplateDialogRequest {
    std::wstring handle;
    std::wstring responsePath;
    AcGePoint3d insertionPoint;
    roadproto::domain::cross_section::SlopeTemplateData data;
};

struct SlopeTemplateDialogResponse {
    bool accepted = false;
    std::wstring handle;
    AcGePoint3d insertionPoint;
    roadproto::domain::cross_section::SlopeTemplateData data;
};

bool queueSlopeTemplateWpfDialog(
    const SlopeTemplateDialogRequest& request,
    std::wstring& errorMessage);

bool readSlopeTemplateDialogResponse(
    const std::wstring& responsePath,
    SlopeTemplateDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
