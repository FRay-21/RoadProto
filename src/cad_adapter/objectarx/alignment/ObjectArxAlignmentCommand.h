#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx {

core::CommandProcedure alignmentCenterlineCreateCommandProcedure();
core::CommandProcedure alignmentCurveParamEditCommandProcedure();
core::CommandProcedure alignmentCenterlineExportIcdCommandProcedure();
core::CommandProcedure alignmentCenterlineImportIcdCommandProcedure();
core::CommandProcedure alignmentCenterlineEditHandleCommandProcedure();
core::CommandProcedure alignmentApplyDialogFileCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx
