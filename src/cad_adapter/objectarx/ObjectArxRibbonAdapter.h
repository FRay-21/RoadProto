#pragma once

#include "cad_adapter/common/IEditor.h"
#include "ui/ribbon/RibbonModel.h"

namespace roadproto::cad_adapter::objectarx {

class ObjectArxRibbonAdapter {
public:
    bool install(const ui::RibbonModel& ribbonModel, IEditor& editor) const;
};

} // namespace roadproto::cad_adapter::objectarx
