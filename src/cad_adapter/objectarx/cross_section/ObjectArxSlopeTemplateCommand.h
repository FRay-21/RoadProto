#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::cross_section {

core::CommandProcedure slopeTemplateCreateCommandProcedure();
core::CommandProcedure slopeTemplateEditHandleCommandProcedure();
core::CommandProcedure slopeTemplateApplyDialogFileCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::cross_section
