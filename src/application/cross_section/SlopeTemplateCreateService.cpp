#include "application/cross_section/SlopeTemplateCreateService.h"

namespace roadproto::application::cross_section {

SlopeTemplateCreateResult SlopeTemplateCreateService::create(
    const SlopeTemplateCreateInput& input) const
{
    SlopeTemplateCreateResult result;
    result.templateData =
        domain::cross_section::SlopeTemplateDefaults::create(input.kind);
    result.templateData.properties.displayScale = input.displayScale;
    if (!input.name.empty()) {
        result.templateData.properties.name = input.name;
    }

    std::wstring errorMessage;
    if (!domain::cross_section::SlopeTemplateRules::normalize(result.templateData, errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    result.succeeded = true;
    return result;
}

} // namespace roadproto::application::cross_section
