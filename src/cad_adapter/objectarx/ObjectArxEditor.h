#pragma once

#include "cad_adapter/common/IEditor.h"

namespace roadproto::cad_adapter::objectarx {

class ObjectArxEditor final : public IEditor {
public:
    void writeMessage(const std::wstring& message) override;
    void writeWarning(const std::wstring& message) override;
    void writeError(const std::wstring& message) override;
};

} // namespace roadproto::cad_adapter::objectarx
