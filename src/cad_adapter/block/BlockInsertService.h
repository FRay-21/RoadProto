#pragma once

#include <string>

namespace roadproto::cad_adapter {

struct BlockInsertSpec {
    std::wstring blockName;
    double scale = 1.0;
    double rotationRadians = 0.0;
};

} // namespace roadproto::cad_adapter
