#include "cad_adapter/objectarx/cross_section/SubgradeTemplateDialogBridge.h"

#include "acdocman.h"
#include "gept3dar.h"

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

constexpr int kMaxDialogComponents = 1000;
constexpr int kMaxDialogTableRows = 10000;

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
    name << L"RoadProtoSubgradeTemplate_" << GetCurrentProcessId() << L"_" << now << suffix;
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

std::wstring pendingRequestPath()
{
    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);

    std::wstringstream name;
    name << L"RoadProtoSubgradeTemplateDialog_" << GetCurrentProcessId() << L".pending";
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

bool writePendingRequestPath(const std::wstring& requestPath, std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(pendingRequestPath()), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write subgrade template dialog pending request path.";
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
    stream << wideToUtf8(key) << '=' << std::setprecision(17) << value << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, std::size_t value)
{
    stream << wideToUtf8(key) << '=' << value << '\n';
}

void writeStationRows(
    std::ostream& stream,
    const std::wstring& prefix,
    const std::vector<roadproto::domain::cross_section::SubgradeStationValue>& rows)
{
    writeKeyValue(stream, prefix + L"Count", rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto rowPrefix = prefix + L"." + std::to_wstring(i);
        writeKeyValue(stream, rowPrefix + L".station", rows[i].station);
        writeKeyValue(stream, rowPrefix + L".value", rows[i].value);
    }
}

bool writeRequestFile(
    const SubgradeTemplateDialogRequest& request,
    const std::wstring& requestPath,
    const std::wstring& responsePath,
    std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(requestPath), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write subgrade template dialog request file.";
        return false;
    }

    writeKeyValue(stream, L"handle", request.handle);
    writeKeyValue(stream, L"responsePath", responsePath);
    writeKeyValue(stream, L"insertionX", request.insertionPoint.x);
    writeKeyValue(stream, L"insertionY", request.insertionPoint.y);
    writeKeyValue(stream, L"insertionZ", request.insertionPoint.z);
    writeKeyValue(stream, L"templateName", request.data.properties.name);
    writeKeyValue(stream, L"displayScale", request.data.properties.displayScale);
    writeKeyValue(
        stream,
        L"roadGrade",
        roadproto::domain::cross_section::roadGradeCode(request.data.properties.roadGrade));
    writeKeyValue(stream, L"roadCenterlineHandle", request.data.roadCenterlineHandle);
    writeKeyValue(stream, L"componentCount", request.data.components.size());

    for (std::size_t i = 0; i < request.data.components.size(); ++i) {
        const auto& component = request.data.components[i];
        const auto prefix = L"component." + std::to_wstring(i);
        writeKeyValue(stream, prefix + L".side", roadproto::domain::cross_section::subgradeSideCode(component.side));
        writeKeyValue(stream, prefix + L".type", roadproto::domain::cross_section::subgradeComponentTypeCode(component.type));
        writeKeyValue(stream, prefix + L".width", component.width);
        writeKeyValue(stream, prefix + L".height", component.height);
        writeKeyValue(stream, prefix + L".fixedSlope", component.fixedSlope);
        writeKeyValue(stream, prefix + L".slopeMode", roadproto::domain::cross_section::subgradeSlopeModeCode(component.slopeMode));
        writeKeyValue(stream, prefix + L".colorR", component.color.r);
        writeKeyValue(stream, prefix + L".colorG", component.color.g);
        writeKeyValue(stream, prefix + L".colorB", component.color.b);
        writeStationRows(stream, prefix + L".widening", component.wideningTable);
        writeStationRows(stream, prefix + L".slopeTable", component.variableSlopeTable);
        writeKeyValue(stream, prefix + L".pavementLayerLinked", component.pavementLayerLinked);
        writeKeyValue(stream, prefix + L".pavementLayerHandle", component.pavementLayerHandle);
        writeKeyValue(stream, prefix + L".pavementLayerThickness", component.pavementLayerThickness);
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

double doubleValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    double fallback = 0.0)
{
    double result = fallback;
    return tryDoubleValue(values, key, result) ? result : fallback;
}

std::vector<roadproto::domain::cross_section::SubgradeStationValue> readStationRows(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& prefix)
{
    const auto count = intValue(values, prefix + L"Count", 0);
    std::vector<roadproto::domain::cross_section::SubgradeStationValue> rows;
    if (count < 0 || count > kMaxDialogTableRows) {
        return rows;
    }

    rows.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const auto rowPrefix = prefix + L"." + std::to_wstring(i);
        roadproto::domain::cross_section::SubgradeStationValue row;
        if (!tryDoubleValue(values, rowPrefix + L".station", row.station)
            || !tryDoubleValue(values, rowPrefix + L".value", row.value)) {
            continue;
        }
        rows.push_back(row);
    }
    return rows;
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

bool queueSubgradeTemplateWpfDialog(
    const SubgradeTemplateDialogRequest& request,
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

    const std::wstring command = L"RD_SECTION_SUBGRADE_TEMPLATE_SHOW_WPF_DIALOG\n";
    acDocManager->sendStringToExecute(document, command.c_str(), true, false, false);
    return true;
}

bool readSubgradeTemplateDialogResponse(
    const std::wstring& responsePath,
    SubgradeTemplateDialogResponse& response,
    std::wstring& errorMessage)
{
    std::ifstream test(std::filesystem::path(responsePath), std::ios::binary);
    if (!test) {
        errorMessage = L"Cannot open subgrade template dialog response file.";
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
    response.data.properties.name = valueOrDefault(values, L"templateName", L"\u9ed8\u8ba4\u8def\u57fa\u6a21\u677f");
    response.data.properties.displayScale = doubleValue(values, L"displayScale", 10.0);
    response.data.properties.roadGrade = roadproto::domain::cross_section::roadGradeFromCode(
        valueOrDefault(values, L"roadGrade", L"Expressway"));
    response.data.roadCenterlineHandle = valueOrDefault(values, L"roadCenterlineHandle");

    if (!response.accepted) {
        removeFileIfExists(responsePath);
        return true;
    }

    const auto componentCount = intValue(values, L"componentCount", 0);
    if (componentCount < 0 || componentCount > kMaxDialogComponents) {
        errorMessage = L"Subgrade template dialog response has invalid component count.";
        return false;
    }

    response.data.components.clear();
    response.data.components.reserve(static_cast<std::size_t>(componentCount));
    for (int i = 0; i < componentCount; ++i) {
        const auto prefix = L"component." + std::to_wstring(i);
        roadproto::domain::cross_section::SubgradeTemplateComponent component;
        component.side = roadproto::domain::cross_section::subgradeSideFromCode(
            valueOrDefault(values, prefix + L".side", L"Right"));
        component.type = roadproto::domain::cross_section::subgradeComponentTypeFromCode(
            valueOrDefault(values, prefix + L".type", L"TravelLane"));
        component.width = doubleValue(values, prefix + L".width", 0.0);
        component.height = doubleValue(values, prefix + L".height", 0.0);
        component.fixedSlope = doubleValue(values, prefix + L".fixedSlope", 0.0);
        component.slopeMode = roadproto::domain::cross_section::subgradeSlopeModeFromCode(
            valueOrDefault(values, prefix + L".slopeMode", L"Fixed"));
        component.color.r = intValue(values, prefix + L".colorR", 120);
        component.color.g = intValue(values, prefix + L".colorG", 120);
        component.color.b = intValue(values, prefix + L".colorB", 120);
        component.wideningTable = readStationRows(values, prefix + L".widening");
        component.variableSlopeTable = readStationRows(values, prefix + L".slopeTable");
        component.pavementLayerLinked = boolValue(values, prefix + L".pavementLayerLinked", false);
        component.pavementLayerHandle = valueOrDefault(values, prefix + L".pavementLayerHandle");
        component.pavementLayerThickness = doubleValue(values, prefix + L".pavementLayerThickness", 0.0);
        response.data.components.push_back(std::move(component));
    }

    if (!roadproto::domain::cross_section::SubgradeTemplateRules::normalize(response.data, errorMessage)) {
        return false;
    }

    removeFileIfExists(responsePath);
    return true;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
