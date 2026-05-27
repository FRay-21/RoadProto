#pragma once

#include "domain/cross_section/SectionDrawingConfigModel.h"

#include <string>
#include <vector>

namespace roadproto::cad_adapter::objectarx::cross_section {

struct SectionDrawingConfigComponentOption {
    std::wstring code;
    std::wstring displayName;
    roadproto::domain::cross_section::SectionDrawingComponentTypeSelection selection;
};

struct SectionDrawingConfigDialogRequest {
    std::wstring drawingHandle;
    std::wstring roadModelHandle;
    std::wstring responsePath;
    roadproto::domain::cross_section::SectionDrawingConfigData config;
    std::vector<SectionDrawingConfigComponentOption> componentOptions;
};

enum class SectionDrawingConfigDialogAction {
    None,
    Draw,
    PickTemplate
};

struct SectionDrawingConfigDialogResponse {
    SectionDrawingConfigDialogAction action = SectionDrawingConfigDialogAction::None;
    bool accepted = false;
    std::wstring drawingHandle;
    std::wstring roadModelHandle;
    std::wstring responsePath;
    int pickRowIndex = -1;
    roadproto::domain::cross_section::SectionDrawingConfigData config;
    std::vector<SectionDrawingConfigComponentOption> componentOptions;
};

bool queueSectionDrawingConfigWpfDialog(
    const SectionDrawingConfigDialogRequest& request,
    std::wstring& errorMessage);

bool readSectionDrawingConfigDialogResponse(
    const std::wstring& responsePath,
    SectionDrawingConfigDialogResponse& response,
    std::wstring& errorMessage);

} // namespace roadproto::cad_adapter::objectarx::cross_section
