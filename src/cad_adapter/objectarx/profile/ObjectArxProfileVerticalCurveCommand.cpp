#include "cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.h"

namespace roadproto::cad_adapter::objectarx::profile {
namespace {

void runProfileVerticalCurveCreateCommand()
{
}

void runProfileVerticalCurveEditHandleCommand()
{
}

void runProfileVerticalCurveApplyDialogFileCommand()
{
}

void runProfileVerticalCurveAddPviCommand()
{
}

void runProfileVerticalCurveDeletePviCommand()
{
}

} // namespace

core::CommandProcedure profileVerticalCurveCreateCommandProcedure()
{
    return &runProfileVerticalCurveCreateCommand;
}

core::CommandProcedure profileVerticalCurveEditHandleCommandProcedure()
{
    return &runProfileVerticalCurveEditHandleCommand;
}

core::CommandProcedure profileVerticalCurveApplyDialogFileCommandProcedure()
{
    return &runProfileVerticalCurveApplyDialogFileCommand;
}

core::CommandProcedure profileVerticalCurveAddPviCommandProcedure()
{
    return &runProfileVerticalCurveAddPviCommand;
}

core::CommandProcedure profileVerticalCurveDeletePviCommandProcedure()
{
    return &runProfileVerticalCurveDeletePviCommand;
}

} // namespace roadproto::cad_adapter::objectarx::profile
