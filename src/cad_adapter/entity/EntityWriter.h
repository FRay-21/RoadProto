#pragma once

#include <string>

namespace roadproto::cad_adapter {

struct EntityWriteResult {
    bool succeeded = false;
    std::wstring cadEntityHandle;
    std::wstring message;
};

} // namespace roadproto::cad_adapter
