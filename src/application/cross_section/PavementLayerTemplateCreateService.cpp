#include "application/cross_section/PavementLayerTemplateCreateService.h"

namespace roadproto::application::cross_section {

PavementLayerTemplateCreateResult PavementLayerTemplateCreateService::create(
    const PavementLayerTemplateCreateInput& input) const
{
    PavementLayerTemplateCreateResult result;
    result.templateData = domain::cross_section::PavementLayerTemplateDefaults::create();
    result.templateData.properties.displayScale = input.displayScale;
    result.templateData.properties.previewWidth = input.previewWidth;
    if (!input.name.empty()) {
        result.templateData.properties.name = input.name;
    }

    std::wstring errorMessage;
    if (!domain::cross_section::PavementLayerTemplateRules::normalize(result.templateData, errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    result.succeeded = true;
    return result;
}

} // namespace roadproto::application::cross_section
