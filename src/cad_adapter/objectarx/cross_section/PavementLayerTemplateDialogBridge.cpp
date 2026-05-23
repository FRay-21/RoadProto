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
#include <locale>
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

void writeKeyValue(std::ostream& stream, const std::wstring& key, int value)
{
    stream << wideToUtf8(key) << '=' << value << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, double value)
{
    const auto previousLocale = stream.getloc();
    stream.imbue(std::locale::classic());
    stream << wideToUtf8(key) << '=' << std::setprecision(17) << value << '\n';
    stream.imbue(previousLocale);
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
    writeKeyValue(
        stream,
        L"displayMode",
        roadproto::domain::cross_section::PavementLayerTemplateRules::displayModeCode(request.data.properties.displayMode));
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
        writeKeyValue(stream, prefix + L".colorR", layer.color.r);
        writeKeyValue(stream, prefix + L".colorG", layer.color.g);
        writeKeyValue(stream, prefix + L".colorB", layer.color.b);
        writeKeyValue(stream, prefix + L".hatchPattern", layer.hatchPattern);
        writeKeyValue(stream, prefix + L".hatchAngle", layer.hatchAngle);
        writeKeyValue(stream, prefix + L".hatchScale", layer.hatchScale);
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

bool parseIntStrict(const std::wstring& value, int& result)
{
    std::wistringstream parsed(value);
    parsed.imbue(std::locale::classic());
    parsed >> result;
    return parsed && parsed.eof();
}

bool parseDoubleStrict(const std::wstring& value, double& result)
{
    std::wistringstream parsed(value);
    parsed.imbue(std::locale::classic());
    parsed >> result;
    return parsed && parsed.eof() && std::isfinite(result);
}

bool requiredValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    std::wstring& result,
    std::wstring& errorMessage)
{
    const auto found = values.find(key);
    if (found == values.end()) {
        errorMessage = L"Missing pavement layer template dialog field: " + key;
        return false;
    }
    result = found->second;
    return true;
}

bool requiredIntValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    int& result,
    std::wstring& errorMessage)
{
    std::wstring value;
    if (!requiredValue(values, key, value, errorMessage)) {
        return false;
    }
    if (!parseIntStrict(value, result)) {
        errorMessage = L"Invalid pavement layer template numeric field: " + key;
        return false;
    }
    return true;
}

bool requiredDoubleValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    double& result,
    std::wstring& errorMessage)
{
    std::wstring value;
    if (!requiredValue(values, key, value, errorMessage)) {
        return false;
    }
    if (!parseDoubleStrict(value, result)) {
        errorMessage = L"Invalid pavement layer template numeric field: " + key;
        return false;
    }
    return true;
}

bool requiredBoolValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    bool& result,
    std::wstring& errorMessage)
{
    std::wstring value;
    if (!requiredValue(values, key, value, errorMessage)) {
        return false;
    }
    if (value == L"1" || value == L"true" || value == L"True") {
        result = true;
        return true;
    }
    if (value == L"0" || value == L"false" || value == L"False") {
        result = false;
        return true;
    }
    errorMessage = L"Invalid pavement layer template boolean field: " + key;
    return false;
}

bool requiredPavementLayerType(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    roadproto::domain::cross_section::PavementLayerType& result,
    std::wstring& errorMessage)
{
    std::wstring value;
    if (!requiredValue(values, key, value, errorMessage)) {
        return false;
    }
    result = roadproto::domain::cross_section::pavementLayerTypeFromCode(value);
    if (value != roadproto::domain::cross_section::pavementLayerTypeCode(result)) {
        errorMessage = L"Unknown pavement layer type code: " + value;
        return false;
    }
    return true;
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
    if (!requiredBoolValue(values, L"accepted", response.accepted, errorMessage)) {
        return false;
    }

    if (!response.accepted) {
        response.handle = valueOrDefault(values, L"handle");
        removeFileIfExists(responsePath);
        return true;
    }

    double insertionX = 0.0;
    double insertionY = 0.0;
    double insertionZ = 0.0;
    if (!requiredDoubleValue(values, L"insertionX", insertionX, errorMessage)
        || !requiredDoubleValue(values, L"insertionY", insertionY, errorMessage)
        || !requiredDoubleValue(values, L"insertionZ", insertionZ, errorMessage)) {
        return false;
    }
    response.insertionPoint = AcGePoint3d(insertionX, insertionY, insertionZ);

    if (!requiredValue(values, L"handle", response.handle, errorMessage)
        || !requiredValue(values, L"templateName", response.data.properties.name, errorMessage)
        || !requiredDoubleValue(values, L"displayScale", response.data.properties.displayScale, errorMessage)
        || !requiredDoubleValue(values, L"previewWidth", response.data.properties.previewWidth, errorMessage)) {
        return false;
    }
    response.data.properties.displayMode =
        roadproto::domain::cross_section::PavementLayerTemplateRules::displayModeFromCode(
            valueOrDefault(values, L"displayMode", L"Color"));

    int layerCount = 0;
    if (!requiredIntValue(values, L"layerCount", layerCount, errorMessage)) {
        return false;
    }
    if (layerCount < 0 || layerCount > kMaxDialogLayers) {
        errorMessage = L"Pavement layer template dialog response has invalid layer count.";
        return false;
    }

    response.data.layers.clear();
    response.data.layers.reserve(static_cast<std::size_t>(layerCount));
    for (int i = 0; i < layerCount; ++i) {
        const auto prefix = L"layer." + std::to_wstring(i);
        roadproto::domain::cross_section::PavementLayerTemplateLayer layer;
        if (!requiredPavementLayerType(values, prefix + L".type", layer.type, errorMessage)
            || !requiredValue(values, prefix + L".name", layer.name, errorMessage)
            || !requiredBoolValue(values, prefix + L".uniformThickness", layer.uniformThickness, errorMessage)
            || !requiredDoubleValue(values, prefix + L".thickness", layer.thickness, errorMessage)
            || !requiredDoubleValue(values, prefix + L".innerThickness", layer.innerThickness, errorMessage)
            || !requiredDoubleValue(values, prefix + L".outerThickness", layer.outerThickness, errorMessage)
            || !requiredDoubleValue(values, prefix + L".innerWidening", layer.innerWidening, errorMessage)
            || !requiredDoubleValue(values, prefix + L".outerWidening", layer.outerWidening, errorMessage)
            || !requiredDoubleValue(values, prefix + L".innerSlope", layer.innerSlope, errorMessage)
            || !requiredDoubleValue(values, prefix + L".outerSlope", layer.outerSlope, errorMessage)
            || !requiredIntValue(values, prefix + L".colorR", layer.color.r, errorMessage)
            || !requiredIntValue(values, prefix + L".colorG", layer.color.g, errorMessage)
            || !requiredIntValue(values, prefix + L".colorB", layer.color.b, errorMessage)
            || !requiredDoubleValue(values, prefix + L".hatchAngle", layer.hatchAngle, errorMessage)
            || !requiredDoubleValue(values, prefix + L".hatchScale", layer.hatchScale, errorMessage)) {
            return false;
        }
        layer.hatchPattern = valueOrDefault(values, prefix + L".hatchPattern", L"SOLID");
        response.data.layers.push_back(std::move(layer));
    }

    if (!roadproto::domain::cross_section::PavementLayerTemplateRules::normalize(response.data, errorMessage)) {
        return false;
    }

    removeFileIfExists(responsePath);
    return true;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
