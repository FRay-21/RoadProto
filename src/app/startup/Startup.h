#pragma once

#include "cad_adapter/common/IEditor.h"

namespace roadproto::app {

bool initialize(cad_adapter::IEditor& editor);
bool shutdown();

} // namespace roadproto::app
