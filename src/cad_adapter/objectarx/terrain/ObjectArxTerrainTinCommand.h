#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx {

core::CommandProcedure terrainTinCreateCommandProcedure();
core::CommandProcedure terrainTinEditCommandProcedure();
core::CommandProcedure terrainTinEditHandleCommandProcedure();
core::CommandProcedure terrainTinExportCommandProcedure();
core::CommandProcedure terrainTinImportCommandProcedure();

} // namespace roadproto::cad_adapter::objectarx
