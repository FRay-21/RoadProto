#pragma once

#include "domain/cross_section/PavementLayerTemplateModel.h"

#include <string>

namespace roadproto::application::cross_section {

struct PavementLayerTemplateCreateInput {
    std::wstring name;
    double displayScale = 10.0;
    double previewWidth = 3.75;
};

struct PavementLayerTemplateCreateResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::cross_section::PavementLayerTemplateData templateData;
};

class PavementLayerTemplateCreateService {
public:
    PavementLayerTemplateCreateResult create(const PavementLayerTemplateCreateInput& input) const;
};

} // namespace roadproto::application::cross_section
