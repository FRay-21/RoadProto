#include "cad_adapter/objectarx/profile/ProfileVerticalCurveDialogBridge.h"

#include "acdocman.h"

#include <Windows.h>

#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace roadproto::cad_adapter::objectarx::profile {
namespace {

constexpr int kMaxDialogPviCount = 100000;

std::string wideToUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        output.data(),
        size,
        nullptr,
        nullptr);
    return output;
}

std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0);
    if (size <= 0) {
        return {};
    }

    std::wstring output(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), output.data(), size);
    return output;
}

std::string escapeValue(const std::wstring& value)
{
    const auto utf8 = wideToUtf8(value);
    std::ostringstream escaped;
    escaped << std::uppercase << std::hex;
    for (const unsigned char ch : utf8) {
        if (ch == '%' || ch == '\r' || ch == '\n') {
            escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        } else {
            escaped << static_cast<char>(ch);
        }
    }
    return escaped.str();
}

std::wstring unescapeValue(const std::string& value)
{
    std::string output;
    output.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '%' && i + 2 < value.size()
            && std::isxdigit(static_cast<unsigned char>(value[i + 1]))
            && std::isxdigit(static_cast<unsigned char>(value[i + 2]))) {
            const auto code = value.substr(i + 1, 2);
            output.push_back(static_cast<char>(std::strtoul(code.c_str(), nullptr, 16)));
            i += 2;
        } else {
            output.push_back(value[i]);
        }
    }
    return utf8ToWide(output);
}

std::wstring tempFilePath(const wchar_t* suffix)
{
    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);

    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::wstringstream name;
    name << L"RoadProtoProfileVerticalCurve_" << GetCurrentProcessId() << L"_" << now << suffix;
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

std::wstring pendingRequestPath()
{
    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);

    std::wstringstream name;
    name << L"RoadProtoProfileVerticalCurveDialog_" << GetCurrentProcessId() << L".pending";
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

bool writePendingRequestPath(const std::wstring& requestPath, std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(pendingRequestPath()), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write profile vertical curve dialog pending request path.";
        return false;
    }

    stream << wideToUtf8(requestPath);
    return true;
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, const std::wstring& value)
{
    stream << wideToUtf8(key) << '=' << escapeValue(value) << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, int value)
{
    stream << wideToUtf8(key) << '=' << value << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, double value)
{
    stream << wideToUtf8(key) << '=';
    stream << std::setprecision(17) << value << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, std::size_t value)
{
    stream << wideToUtf8(key) << '=' << value << '\n';
}

bool writeRequestFile(
    const ProfileVerticalCurveDialogRequest& request,
    const std::wstring& requestPath,
    const std::wstring& responsePath,
    std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(requestPath), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write profile vertical curve dialog request file.";
        return false;
    }

    writeKeyValue(stream, L"handle", request.handle);
    writeKeyValue(stream, L"responsePath", responsePath);
    writeKeyValue(stream, L"profileGraphHandle", request.profileGraphHandle);
    writeKeyValue(stream, L"name", request.name);
    writeKeyValue(stream, L"startStation", request.startStation);
    writeKeyValue(stream, L"startElevation", request.startElevation);
    writeKeyValue(stream, L"endStation", request.endStation);
    writeKeyValue(stream, L"endElevation", request.endElevation);
    writeKeyValue(stream, L"currentPviIndex", request.currentPviIndex);
    writeKeyValue(stream, L"pviCount", request.pvis.size());

    for (std::size_t i = 0; i < request.pvis.size(); ++i) {
        const auto prefix = L"pvi." + std::to_wstring(i);
        writeKeyValue(stream, prefix + L".station", request.pvis[i].station);
        writeKeyValue(stream, prefix + L".elevation", request.pvis[i].elevation);
        writeKeyValue(stream, prefix + L".radius", request.pvis[i].radius);
    }

    return true;
}

std::unordered_map<std::wstring, std::wstring> readKeyValueFile(const std::wstring& path)
{
    std::ifstream stream(std::filesystem::path(path), std::ios::binary);
    std::unordered_map<std::wstring, std::wstring> values;
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        auto key = utf8ToWide(line.substr(0, separator));
        if (!key.empty() && key.front() == 0xFEFF) {
            key.erase(key.begin());
        }
        values[key] = unescapeValue(line.substr(separator + 1));
    }
    return values;
}

std::wstring valueOrDefault(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    const std::wstring& fallback = L"")
{
    const auto found = values.find(key);
    return found == values.end() ? fallback : found->second;
}

bool boolValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    bool fallback = false)
{
    const auto value = valueOrDefault(values, key, fallback ? L"1" : L"0");
    return value == L"1" || value == L"true" || value == L"True";
}

bool tryIntValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    int& result)
{
    try {
        std::size_t parsedLength = 0;
        const auto value = valueOrDefault(values, key);
        if (value.empty()) {
            return false;
        }
        result = std::stoi(value, &parsedLength);
        return parsedLength == value.size();
    } catch (...) {
        return false;
    }
}

bool tryDoubleValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    double& result)
{
    try {
        std::size_t parsedLength = 0;
        const auto value = valueOrDefault(values, key);
        if (value.empty()) {
            return false;
        }
        result = std::stod(value, &parsedLength);
        return parsedLength == value.size() && std::isfinite(result);
    } catch (...) {
        return false;
    }
}

void removeFileIfExists(const std::wstring& path)
{
    if (path.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::remove(std::filesystem::path(path), error);
}

} // namespace

bool queueProfileVerticalCurveWpfDialog(
    const ProfileVerticalCurveDialogRequest& request,
    std::wstring& errorMessage)
{
    const auto requestPath = tempFilePath(L".request");
    const auto responsePath = request.responsePath.empty() ? tempFilePath(L".response") : request.responsePath;
    if (!writeRequestFile(request, requestPath, responsePath, errorMessage)) {
        return false;
    }
    if (!writePendingRequestPath(requestPath, errorMessage)) {
        removeFileIfExists(requestPath);
        return false;
    }

    auto* document = acDocManager == nullptr ? nullptr : acDocManager->curDocument();
    if (document == nullptr) {
        removeFileIfExists(pendingRequestPath());
        removeFileIfExists(requestPath);
        removeFileIfExists(responsePath);
        errorMessage = L"No active AutoCAD document is available.";
        return false;
    }

    const std::wstring command = L"RD_PROFILE_VERTICAL_CURVE_SHOW_WPF_DIALOG\n";
    acDocManager->sendStringToExecute(document, command.c_str(), true, false, false);
    return true;
}

bool readProfileVerticalCurveDialogResponse(
    const std::wstring& responsePath,
    ProfileVerticalCurveDialogResponse& response,
    std::wstring& errorMessage)
{
    std::ifstream test(std::filesystem::path(responsePath), std::ios::binary);
    if (!test) {
        errorMessage = L"Cannot open profile vertical curve dialog response file.";
        return false;
    }
    test.close();

    const auto values = readKeyValueFile(responsePath);
    response.accepted = boolValue(values, L"accepted", false);
    response.handle = valueOrDefault(values, L"handle");
    response.name = valueOrDefault(values, L"name", L"\u7ad6\u66f2\u7ebf");
    if (!response.accepted) {
        removeFileIfExists(responsePath);
        return true;
    }

    if (response.handle.empty()) {
        errorMessage = L"Profile vertical curve dialog response handle is empty.";
        return false;
    }
    if (!tryDoubleValue(values, L"startStation", response.startStation)
        || !tryDoubleValue(values, L"startElevation", response.startElevation)
        || !tryDoubleValue(values, L"endStation", response.endStation)
        || !tryDoubleValue(values, L"endElevation", response.endElevation)) {
        errorMessage = L"Profile vertical curve dialog response has invalid start/end values.";
        return false;
    }

    int pviCount = 0;
    if (!tryIntValue(values, L"pviCount", pviCount) || pviCount < 0 || pviCount > kMaxDialogPviCount) {
        errorMessage = L"Profile vertical curve dialog response has invalid PVI count.";
        return false;
    }
    response.pvis.clear();
    if (pviCount > 0) {
        response.pvis.reserve(static_cast<std::size_t>(pviCount));
    }
    for (int i = 0; i < pviCount; ++i) {
        const auto prefix = L"pvi." + std::to_wstring(i);
        roadproto::domain::profile::VerticalCurvePvi pvi;
        if (!tryDoubleValue(values, prefix + L".station", pvi.station)
            || !tryDoubleValue(values, prefix + L".elevation", pvi.elevation)
            || !tryDoubleValue(values, prefix + L".radius", pvi.radius)) {
            errorMessage = L"Profile vertical curve dialog response has invalid PVI values.";
            return false;
        }
        response.pvis.push_back(pvi);
    }

    removeFileIfExists(responsePath);
    return true;
}

} // namespace roadproto::cad_adapter::objectarx::profile
