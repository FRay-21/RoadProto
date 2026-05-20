#pragma once

#include "domain/cross_section/SlopeTemplateModel.h"

#include <string>

namespace roadproto::application::cross_section {

struct SlopeTemplateCreateInput {
    std::wstring name;
    double displayScale = 10.0;
    domain::cross_section::SlopeTemplateKind kind =
        domain::cross_section::SlopeTemplateKind::Fill;
};

struct SlopeTemplateCreateResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::cross_section::SlopeTemplateData templateData;
};

class SlopeTemplateCreateService {
public:
    SlopeTemplateCreateResult create(const SlopeTemplateCreateInput& input) const;
};

} // namespace roadproto::application::cross_section
