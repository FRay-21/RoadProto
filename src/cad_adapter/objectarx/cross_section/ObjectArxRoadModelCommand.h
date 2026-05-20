#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::cross_section {

core::CommandProcedure roadModelCreateCommandProcedure();
core::CommandProcedure roadModelEditCommandProcedure();
core::CommandProcedure roadModelEditHandleCommandProcedure();
core::CommandProcedure roadModelApplyDialogFileCommandProcedure();
core::CommandProcedure roadModelViewSectionCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx::cross_section
