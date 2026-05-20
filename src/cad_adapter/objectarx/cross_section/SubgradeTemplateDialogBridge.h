#pragma once

#include "domain/cross_section/SubgradeTemplateModel.h"

#include "gept3dar.h"

#include <string>

namespace roadproto::cad_adapter::objectarx::cross_section {

enum class SubgradeTemplateDialogAction {
    None,
    PickPavementLayerTemplate
};

struct SubgradeTemplateDialogRequest {
    SubgradeTemplateDialogAction action = SubgradeTemplateDialogAction::None;
    int pickComponentIndex = -1;
    std::wstring handle;
    std::wstring responsePath;
    AcGePoint3d insertionPoint;
    roadproto::domain::cross_section::SubgradeTemplateData data;
};

struct SubgradeTemplateDialogResponse {
    SubgradeTemplateDialogAction action = SubgradeTemplateDialogAction::None;
    int pickComponentIndex = -1;
    bool accepted = false;
    std::wstring handle;
    AcGePoint3d insertionPoint;
    roadproto::domain::cross_section::SubgradeTemplateData data;
};

bool queueSubgradeTemplateWpfDialog(
    const SubgradeTemplateDialogRequest& request,
    std::wstring& errorMessage);

bool readSubgradeTemplateDialogResponse(
    const std::wstring& responsePath,
    SubgradeTemplateDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
