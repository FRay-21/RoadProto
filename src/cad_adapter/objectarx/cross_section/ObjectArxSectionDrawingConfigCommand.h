#pragma once

#include "core/command/CommandRegistry.h"

namespace roadproto::cad_adapter::objectarx::cross_section {

#ifdef ROADPROTO_TEST_BUILD
inline void runSectionDrawingConfigCommandForTests() {}
inline void runSectionDrawingConfigEditHandleCommandForTests() {}
inline void runSectionDrawingConfigApplyDialogFileCommandForTests() {}

inline roadproto::core::CommandProcedure sectionDrawingConfigCommandProcedure()
{
    return &runSectionDrawingConfigCommandForTests;
}

inline roadproto::core::CommandProcedure sectionDrawingConfigEditHandleCommandProcedure()
{
    return &runSectionDrawingConfigEditHandleCommandForTests;
}

inline roadproto::core::CommandProcedure sectionDrawingConfigApplyDialogFileCommandProcedure()
{
    return &runSectionDrawingConfigApplyDialogFileCommandForTests;
}
#else
roadproto::core::CommandProcedure sectionDrawingConfigCommandProcedure();
roadproto::core::CommandProcedure sectionDrawingConfigEditHandleCommandProcedure();
roadproto::core::CommandProcedure sectionDrawingConfigApplyDialogFileCommandProcedure();
#endif

} // namespace roadproto::cad_adapter::objectarx::cross_section
