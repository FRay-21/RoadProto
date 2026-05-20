#pragma once

#include "domain/cross_section/PavementLayerTemplateModel.h"

#include "gept3dar.h"

#include <string>

namespace roadproto::cad_adapter::objectarx::cross_section {

struct PavementLayerTemplateDialogRequest {
    std::wstring handle;
    std::wstring responsePath;
    AcGePoint3d insertionPoint;
    roadproto::domain::cross_section::PavementLayerTemplateData data;
};

struct PavementLayerTemplateDialogResponse {
    bool accepted = false;
    std::wstring handle;
    AcGePoint3d insertionPoint;
    roadproto::domain::cross_section::PavementLayerTemplateData data;
};

bool queuePavementLayerTemplateWpfDialog(
    const PavementLayerTemplateDialogRequest& request,
    std::wstring& errorMessage);

bool readPavementLayerTemplateDialogResponse(
    const std::wstring& responsePath,
    PavementLayerTemplateDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
