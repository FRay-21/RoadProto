#include "cad_adapter/objectarx/cross_section/PavementLayerTemplateDialogBridge.h"

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
#include <utility>

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

constexpr int kMaxDialogLayers = 1000;

std::string wideToUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string output(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), output.data(), size, nullptr, nullptr);
    return output;
}

std::wstring utf8ToWide(const std::string& value)
{
    if (value.empty()) {
        return {};
    }
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
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
    name << L"RoadProtoPavementLayerTemplate_" << GetCurrentProcessId() << L"_" << now << suffix;
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

std::wstring pendingRequestPath()
{
    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);
    std::wstringstream name;
    name << L"RoadProtoPavementLayerTemplateDialog_" << GetCurrentProcessId() << L".pending";
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

bool writePendingRequestPath(const std::wstring& requestPath, std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(pendingRequestPath()), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write pavement layer template dialog pending request path.";
        return false;
    }
    stream << wideToUtf8(requestPath);
    return true;
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, const std::wstring& value)
{
    stream << wideToUtf8(key) << '=' << escapeValue(value) << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, const wchar_t* value)
{
    writeKeyValue(stream, key, std::wstring(value == nullptr ? L"" : value));
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, bool value)
{
    stream << wideToUtf8(key) << '=' << (value ? 1 : 0) << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, double value)
{
    stream << wideToUtf8(key) << '=' << std::setprecision(17) << value << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, std::size_t value)
{
    stream << wideToUtf8(key) << '=' << value << '\n';
}

bool writeRequestFile(
    const PavementLayerTemplateDialogRequest& request,
    const std::wstring& requestPath,
    const std::wstring& responsePath,
    std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(requestPath), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write pavement layer template dialog request file.";
        return false;
    }

    writeKeyValue(stream, L"handle", request.handle);
    writeKeyValue(stream, L"responsePath", responsePath);
    writeKeyValue(stream, L"insertionX", request.insertionPoint.x);
    writeKeyValue(stream, L"insertionY", request.insertionPoint.y);
    writeKeyValue(stream, L"insertionZ", request.insertionPoint.z);
    writeKeyValue(stream, L"templateName", request.data.properties.name);
    writeKeyValue(stream, L"displayScale", request.data.properties.displayScale);
    writeKeyValue(stream, L"previewWidth", request.data.properties.previewWidth);
    writeKeyValue(stream, L"layerCount", request.data.layers.size());

    for (std::size_t i = 0; i < request.data.layers.size(); ++i) {
        const auto& layer = request.data.layers[i];
        const auto prefix = L"layer." + std::to_wstring(i);
        writeKeyValue(stream, prefix + L".type", roadproto::domain::cross_section::pavementLayerTypeCode(layer.type));
        writeKeyValue(stream, prefix + L".name", layer.name);
        writeKeyValue(stream, prefix + L".uniformThickness", layer.uniformThickness);
        writeKeyValue(stream, prefix + L".thickness", layer.thickness);
        writeKeyValue(stream, prefix + L".innerThickness", layer.innerThickness);
        writeKeyValue(stream, prefix + L".outerThickness", layer.outerThickness);
        writeKeyValue(stream, prefix + L".innerWidening", layer.innerWidening);
        writeKeyValue(stream, prefix + L".outerWidening", layer.outerWidening);
        writeKeyValue(stream, prefix + L".innerSlope", layer.innerSlope);
        writeKeyValue(stream, prefix + L".outerSlope", layer.outerSlope);
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

int intValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    int fallback = 0)
{
    try {
        std::size_t parsedLength = 0;
        const auto value = valueOrDefault(values, key);
        if (value.empty()) {
            return fallback;
        }
        const auto parsed = std::stoi(value, &parsedLength);
        return parsedLength == value.size() ? parsed : fallback;
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
        std::size_t parsedLength = 0;
        const auto value = valueOrDefault(values, key);
        if (value.empty()) {
            return fallback;
        }
        const auto parsed = std::stod(value, &parsedLength);
        return parsedLength == value.size() && std::isfinite(parsed) ? parsed : fallback;
    } catch (...) {
        return fallback;
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

bool queuePavementLayerTemplateWpfDialog(
    const PavementLayerTemplateDialogRequest& request,
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

    const std::wstring command = L"RD_SECTION_PAVEMENT_LAYER_TEMPLATE_SHOW_WPF_DIALOG\n";
    acDocManager->sendStringToExecute(document, command.c_str(), true, false, false);
    return true;
}

bool readPavementLayerTemplateDialogResponse(
    const std::wstring& responsePath,
    PavementLayerTemplateDialogResponse& response,
    std::wstring& errorMessage)
{
    std::ifstream test(std::filesystem::path(responsePath), std::ios::binary);
    if (!test) {
        errorMessage = L"Cannot open pavement layer template dialog response file.";
        return false;
    }
    test.close();

    const auto values = readKeyValueFile(responsePath);
    response.accepted = boolValue(values, L"accepted", false);
    response.handle = valueOrDefault(values, L"handle");
    response.insertionPoint = AcGePoint3d(
        doubleValue(values, L"insertionX", 0.0),
        doubleValue(values, L"insertionY", 0.0),
        doubleValue(values, L"insertionZ", 0.0));
    response.data.properties.name = valueOrDefault(values, L"templateName", L"\u8def\u9762\u7ed3\u6784\u5c42\u6a21\u677f");
    response.data.properties.displayScale = doubleValue(values, L"displayScale", 10.0);
    response.data.properties.previewWidth = doubleValue(values, L"previewWidth", 3.75);

    if (!response.accepted) {
        removeFileIfExists(responsePath);
        return true;
    }

    const auto layerCount = intValue(values, L"layerCount", 0);
    if (layerCount < 0 || layerCount > kMaxDialogLayers) {
        errorMessage = L"Pavement layer template dialog response has invalid layer count.";
        return false;
    }

    response.data.layers.clear();
    response.data.layers.reserve(static_cast<std::size_t>(layerCount));
    for (int i = 0; i < layerCount; ++i) {
        const auto prefix = L"layer." + std::to_wstring(i);
        roadproto::domain::cross_section::PavementLayerTemplateLayer layer;
        layer.type = roadproto::domain::cross_section::pavementLayerTypeFromCode(
            valueOrDefault(values, prefix + L".type", L"UpperSurface"));
        layer.name = valueOrDefault(values, prefix + L".name");
        layer.uniformThickness = boolValue(values, prefix + L".uniformThickness", true);
        layer.thickness = doubleValue(values, prefix + L".thickness", 0.04);
        layer.innerThickness = doubleValue(values, prefix + L".innerThickness", layer.thickness);
        layer.outerThickness = doubleValue(values, prefix + L".outerThickness", layer.thickness);
        layer.innerWidening = doubleValue(values, prefix + L".innerWidening", 0.0);
        layer.outerWidening = doubleValue(values, prefix + L".outerWidening", 0.0);
        layer.innerSlope = doubleValue(values, prefix + L".innerSlope", 0.0);
        layer.outerSlope = doubleValue(values, prefix + L".outerSlope", 0.0);
        response.data.layers.push_back(std::move(layer));
    }

    if (!roadproto::domain::cross_section::PavementLayerTemplateRules::normalize(response.data, errorMessage)) {
        return false;
    }

    removeFileIfExists(responsePath);
    return true;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
