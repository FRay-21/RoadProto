#include "cad_adapter/objectarx/ObjectArxRibbonAdapter.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include "acdocman.h"
#include <algorithm>
#include <cwctype>
#include <sstream>
#include <string>

namespace roadproto::cad_adapter::objectarx {

namespace {

std::wstring toUpper(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towupper(ch));
    });
    return value;
}

bool isCoreConsole()
{
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) {
        return false;
    }
    return toUpper(path).find(L"ACCORECONSOLE.EXE") != std::wstring::npos;
}

std::wstring parentDirectory(const std::wstring& path)
{
    const auto index = path.find_last_of(L"\\/");
    return index == std::wstring::npos ? L"" : path.substr(0, index);
}

std::wstring fileName(const std::wstring& path)
{
    const auto index = path.find_last_of(L"\\/");
    return index == std::wstring::npos ? path : path.substr(index + 1);
}

std::wstring currentModulePath()
{
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&currentModulePath),
            &module)) {
        return L"";
    }

    wchar_t buffer[MAX_PATH] = {};
    if (GetModuleFileNameW(module, buffer, MAX_PATH) == 0) {
        return L"";
    }
    return buffer;
}

std::wstring findManagedRibbonPlugin()
{
    const auto modulePath = currentModulePath();
    const auto arxDirectory = parentDirectory(modulePath);
    if (arxDirectory.empty()) {
        return L"";
    }

    const auto configuration = fileName(arxDirectory);
    const auto x64Directory = parentDirectory(arxDirectory);
    const auto artifactsDirectory = parentDirectory(x64Directory);
    if (configuration.empty() || artifactsDirectory.empty()) {
        return L"";
    }

    const auto candidate = artifactsDirectory
        + L"\\managed\\" + configuration + L"\\net48\\RoadProto.Terrain.UI.dll";
    if (GetFileAttributesW(candidate.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return candidate;
    }
    return L"";
}

std::wstring lispStringPath(std::wstring path)
{
    std::replace(path.begin(), path.end(), L'\\', L'/');
    return path;
}

bool queueManagedRibbonLoad()
{
    if (isCoreConsole() || acDocManager == nullptr || acDocManager->curDocument() == nullptr) {
        return false;
    }

    const auto pluginPath = findManagedRibbonPlugin();
    if (pluginPath.empty()) {
        return false;
    }

    const auto command =
        L"(command \"_.NETLOAD\" \"" + lispStringPath(pluginPath)
        + L"\")(command \"DN_ROADPROTO_SHOW_RIBBON\") ";
    return acDocManager->sendStringToExecute(
        acDocManager->curDocument(),
        command.c_str(),
        true,
        false,
        false) == Acad::eOk;
}

} // namespace

bool ObjectArxRibbonAdapter::install(const ui::RibbonModel& ribbonModel, IEditor& editor) const
{
    const auto queuedManagedRibbon = queueManagedRibbonLoad();
    std::wstringstream stream;
    stream << L"Ribbon model prepared: " << ribbonModel.tab().title
           << L", panels: " << ribbonModel.tab().panels.size()
           << L". Visible AutoCAD Ribbon "
           << (queuedManagedRibbon
               ? L"load has been queued from RoadProto.Terrain.UI.dll."
               : L"is provided by RoadProto.Terrain.UI.dll; load it with NETLOAD if it is not visible.");
    editor.writeMessage(stream.str());
    return true;
}

} // namespace roadproto::cad_adapter::objectarx
