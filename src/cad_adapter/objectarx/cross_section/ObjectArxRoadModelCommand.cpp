#include "cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h"

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

void roadModelCreateCommand()
{
}

void roadModelEditCommand()
{
}

void roadModelEditHandleCommand()
{
}

void roadModelApplyDialogFileCommand()
{
}

} // namespace

core::CommandProcedure roadModelCreateCommandProcedure()
{
    return &roadModelCreateCommand;
}

core::CommandProcedure roadModelEditCommandProcedure()
{
    return &roadModelEditCommand;
}

core::CommandProcedure roadModelEditHandleCommandProcedure()
{
    return &roadModelEditHandleCommand;
}

core::CommandProcedure roadModelApplyDialogFileCommandProcedure()
{
    return &roadModelApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
