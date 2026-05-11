#include "cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.h"

#include "acdocman.h"

#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace roadproto::cad_adapter::objectarx::profile {
namespace {

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

std::wstring sourceTypeText(roadproto::domain::profile::ProfileGroundSourceType sourceType)
{
    return sourceType == roadproto::domain::profile::ProfileGroundSourceType::TerrainTin
        ? L"TerrainTin"
        : L"DmxFile";
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
    name << L"RoadProtoProfileGradeGraph_" << GetCurrentProcessId() << L"_" << now << suffix;
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

std::wstring toCommandPath(std::wstring path)
{
    std::replace(path.begin(), path.end(), L'\\', L'/');
    return path;
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

void writeKeyValue(std::ostream& stream, const std::wstring& key, bool value)
{
    stream << wideToUtf8(key) << '=' << (value ? 1 : 0) << '\n';
}

bool writeRequestFile(
    const ProfileGradeGraphDialogRequest& request,
    const std::wstring& requestPath,
    const std::wstring& responsePath,
    std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(requestPath), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write profile grade graph dialog request file.";
        return false;
    }

    writeKeyValue(stream, L"handle", request.handle);
    writeKeyValue(stream, L"responsePath", responsePath);
    writeKeyValue(stream, L"roadCenterlineHandle", request.roadCenterlineHandle);
    writeKeyValue(stream, L"terrainTinHandle", request.terrainTinHandle);
    writeKeyValue(stream, L"graphName", request.graphName);
    writeKeyValue(stream, L"sourceType", sourceTypeText(request.sourceType));
    writeKeyValue(stream, L"dmxFilePath", request.dmxFilePath);
    writeKeyValue(stream, L"groundLineColorIndex", request.groundLineColorIndex);
    writeKeyValue(stream, L"groundLineWidth", request.groundLineWidth);
    writeKeyValue(stream, L"groundLinePrecision", request.groundLinePrecision);
    writeKeyValue(stream, L"verticalScale", request.verticalScale);
    writeKeyValue(stream, L"gridSpacing", request.gridSpacing);
    writeKeyValue(stream, L"sampleCount", request.sampleCount);
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

int intValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    int fallback = 0)
{
    try {
        return std::stoi(valueOrDefault(values, key, std::to_wstring(fallback)));
    } catch (...) {
        return fallback;
    }
}

double doubleValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    double fallback = 0.0)
{
    try {
        return std::stod(valueOrDefault(values, key, std::to_wstring(fallback)));
    } catch (...) {
        return fallback;
    }
}

} // namespace

bool queueProfileGradeGraphWpfDialog(
    const ProfileGradeGraphDialogRequest& request,
    std::wstring& errorMessage)
{
    const auto requestPath = tempFilePath(L".request");
    const auto responsePath = tempFilePath(L".response");
    if (!writeRequestFile(request, requestPath, responsePath, errorMessage)) {
        return false;
    }

    auto* document = acDocManager == nullptr ? nullptr : acDocManager->curDocument();
    if (document == nullptr) {
        errorMessage = L"No active AutoCAD document is available.";
        return false;
    }

    const auto commandPath = toCommandPath(requestPath);
    const std::wstring command = L"RD_PROFILE_SHOW_WPF_DIALOG \"" + commandPath + L"\"\n\n";
    acDocManager->sendStringToExecute(document, command.c_str(), true, false, false);
    return true;
}

bool readProfileGradeGraphDialogResponse(
    const std::wstring& responsePath,
    ProfileGradeGraphDialogResponse& response,
    std::wstring& errorMessage)
{
    std::ifstream test(std::filesystem::path(responsePath), std::ios::binary);
    if (!test) {
        errorMessage = L"Cannot open profile grade graph dialog response file.";
        return false;
    }
    test.close();

    const auto values = readKeyValueFile(responsePath);
    response.accepted = boolValue(values, L"accepted", false);
    response.handle = valueOrDefault(values, L"handle");
    response.graphName = valueOrDefault(values, L"graphName", L"拉坡图");
    response.groundLineColorIndex = intValue(values, L"groundLineColorIndex", 4);
    response.groundLineWidth = doubleValue(values, L"groundLineWidth", 1.0);
    response.groundLinePrecision = doubleValue(values, L"groundLinePrecision", 10.0);
    response.verticalScale = doubleValue(values, L"verticalScale", 10.0);
    response.gridSpacing = doubleValue(values, L"gridSpacing", 10.0);
    response.updateGroundLineRequested = boolValue(values, L"updateGroundLineRequested", false);
    return true;
}

} // namespace roadproto::cad_adapter::objectarx::profile
