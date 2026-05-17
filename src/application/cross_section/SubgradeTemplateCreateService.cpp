#include "application/cross_section/SubgradeTemplateCreateService.h"

namespace roadproto::application::cross_section {

SubgradeTemplateCreateResult SubgradeTemplateCreateService::create(
    const SubgradeTemplateCreateInput& input) const
{
    SubgradeTemplateCreateResult result;
    result.templateData =
        domain::cross_section::SubgradeTemplateDefaults::create(input.roadGrade);
    result.templateData.properties.displayScale = input.displayScale;
    if (!input.name.empty()) {
        result.templateData.properties.name = input.name;
    }

    std::wstring errorMessage;
    if (!domain::cross_section::SubgradeTemplateRules::normalize(result.templateData, errorMessage)) {
        result.errorMessage = errorMessage;
        return result;
    }

    result.succeeded = true;
    return result;
}

} // namespace roadproto::application::cross_section
