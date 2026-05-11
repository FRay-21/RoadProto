#include "cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h"

namespace roadproto::cad_adapter::objectarx::profile {
namespace {

void runProfileGradeGraphCreateCommand()
{
}

void runProfileGradeGraphEditHandleCommand()
{
}

void runProfileApplyDialogFileCommand()
{
}

} // namespace

core::CommandProcedure profileGradeGraphCreateCommandProcedure()
{
    return &runProfileGradeGraphCreateCommand;
}

core::CommandProcedure profileGradeGraphEditHandleCommandProcedure()
{
    return &runProfileGradeGraphEditHandleCommand;
}

core::CommandProcedure profileApplyDialogFileCommandProcedure()
{
    return &runProfileApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::profile

