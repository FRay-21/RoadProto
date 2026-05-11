#include "cad_adapter/objectarx/ObjectArxEditor.h"

#include "acutads.h"

namespace roadproto::cad_adapter::objectarx {

namespace {

void printLine(const wchar_t* prefix, const std::wstring& message)
{
    acutPrintf(L"\n%s%s", prefix, message.c_str());
}

} // namespace

void ObjectArxEditor::writeMessage(const std::wstring& message)
{
    printLine(L"", message);
}

void ObjectArxEditor::writeWarning(const std::wstring& message)
{
    printLine(L"[RoadProto warning] ", message);
}

void ObjectArxEditor::writeError(const std::wstring& message)
{
    printLine(L"[RoadProto error] ", message);
}

} // namespace roadproto::cad_adapter::objectarx
