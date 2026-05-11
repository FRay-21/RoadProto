#pragma once

#include <string>

namespace roadproto::cad_adapter {

class IEditor {
public:
    virtual ~IEditor() = default;

    virtual void writeMessage(const std::wstring& message) = 0;
    virtual void writeWarning(const std::wstring& message) = 0;
    virtual void writeError(const std::wstring& message) = 0;
};

class NullEditor final : public IEditor {
public:
    void writeMessage(const std::wstring&) override {}
    void writeWarning(const std::wstring&) override {}
    void writeError(const std::wstring&) override {}
};

} // namespace roadproto::cad_adapter
