#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::cross_section {

core::CommandProcedure pavementLayerTemplateCreateCommandProcedure();
core::CommandProcedure pavementLayerTemplateEditHandleCommandProcedure();
core::CommandProcedure pavementLayerTemplateApplyDialogFileCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::cross_section
