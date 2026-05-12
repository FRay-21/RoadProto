#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::profile {

core::CommandProcedure profileVerticalCurveCreateCommandProcedure();
core::CommandProcedure profileVerticalCurveEditHandleCommandProcedure();
core::CommandProcedure profileVerticalCurveApplyDialogFileCommandProcedure();
core::CommandProcedure profileVerticalCurveAddPviCommandProcedure();
core::CommandProcedure profileVerticalCurveDeletePviCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::profile
