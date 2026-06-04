#pragma once

#include "application/agent/AgentToolRequest.h"
#include "domain/cross_section/SubgradeTemplateModel.h"

#include <string>

namespace roadproto::application::agent {

struct SubgradeTemplateToolDataResult {
    bool succeeded = false;
    std::wstring errorMessage;
    domain::cross_section::SubgradeTemplateData data;
};

SubgradeTemplateToolDataResult buildSubgradeTemplateToolData(
    const AgentSubgradeTemplateCreateArguments& arguments,
    std::wstring& errorMessage);

} // namespace roadproto::application::agent
