#pragma once

#include "domain/cross_section/SubgradeTemplateModel.h"

#include <string>

namespace roadproto::application::cross_section {

struct SubgradeTemplateCreateInput {
    std::wstring name;
    double displayScale = 10.0;
    domain::cross_section::RoadGrade roadGrade =
        domain::cross_section::RoadGrade::Expressway;
};

struct SubgradeTemplateCreateResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::cross_section::SubgradeTemplateData templateData;
};

class SubgradeTemplateCreateService {
public:
    SubgradeTemplateCreateResult create(const SubgradeTemplateCreateInput& input) const;
};

} // namespace roadproto::application::cross_section
