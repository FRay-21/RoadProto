#pragma once

#include <string>

namespace roadproto::cad_adapter {

struct LayerSpec {
    std::wstring name;
    int colorIndex = 7;
    bool plottable = true;
};

} // namespace roadproto::cad_adapter
