#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::profile {

core::CommandProcedure profileGradeGraphCreateCommandProcedure();
core::CommandProcedure profileGradeGraphEditHandleCommandProcedure();
core::CommandProcedure profileApplyDialogFileCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::profile

