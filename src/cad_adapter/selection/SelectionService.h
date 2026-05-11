#pragma once

#include <string>

namespace roadproto::cad_adapter {

struct SelectionFilter {
    std::wstring prompt;
    std::wstring entityType;
};

} // namespace roadproto::cad_adapter
