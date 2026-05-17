#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::cross_section {

core::CommandProcedure subgradeTemplateCreateCommandProcedure();
core::CommandProcedure subgradeTemplateEditHandleCommandProcedure();
core::CommandProcedure subgradeTemplateApplyDialogFileCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::cross_section
