#pragma once

#include <string>

namespace roadproto::cad_adapter {

struct TextAnnotationSpec {
    std::wstring text;
    double height = 2.5;
    double rotationRadians = 0.0;
};

} // namespace roadproto::cad_adapter
