#include "cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.h"

#include "acdocman.h"

#include <Windows.h>

#include <chrono>
#include <cerrno>
#include <cctype>
#include <cwctype>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

using roadproto::domain::cross_section::SectionDrawingComponentTypeSelection;
using roadproto::domain::cross_section::SectionDrawingConfigData;
using roadproto::domain::cross_section::SectionDrawingConfigRules;
using roadproto::domain::cross_section::SectionClearTableConfigRow;
using roadproto::domain::cross_section::SectionPavementLayerConfigRow;

constexpr int kMaxConfigRows = 10000;
constexpr int kMaxConfigComponents = 1000;
constexpr int kMaxComponentOptions = 1000;

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
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        output.data(),
        size);
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
    name << L"RoadProtoSectionDrawingConfig_" << GetCurrentProcessId() << L"_" << now << suffix;
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

std::wstring pendingRequestPath()
{
    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);

    std::wstringstream name;
    name << L"RoadProtoSectionDrawingConfig_" << GetCurrentProcessId() << L".pending";
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, const std::wstring& value)
{
    stream << wideToUtf8(key) << '=' << escapeValue(value) << '\n';
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

std::wstring joinComponentCodes(const std::vector<SectionDrawingComponentTypeSelection>& selections)
{
    std::wstring result;
    for (std::size_t i = 0; i < selections.size(); ++i) {
        if (i > 0) {
            result += L";";
        }
        result += SectionDrawingConfigRules::componentSelectionCode(selections[i]);
    }
    return result;
}

bool writeRequestFile(
    const SectionDrawingConfigDialogRequest& request,
    const std::wstring& requestPath,
    const std::wstring& responsePath,
    std::wstring& errorMessage)
{
    if (request.componentOptions.size() > static_cast<std::size_t>(kMaxComponentOptions)
        || request.config.pavementRows.size() > static_cast<std::size_t>(kMaxConfigRows)
        || request.config.clearTableRows.size() > static_cast<std::size_t>(kMaxConfigRows)) {
        errorMessage = L"Section drawing config dialog request is too large.";
        return false;
    }

    std::ofstream stream(std::filesystem::path(requestPath), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write section drawing config dialog request file.";
        return false;
    }

    writeKeyValue(stream, L"drawingHandle", request.drawingHandle);
    writeKeyValue(stream, L"roadModelHandle", request.roadModelHandle);
    writeKeyValue(stream, L"responsePath", responsePath);
    writeKeyValue(stream, L"configPath", request.config.configPath);
    writeKeyValue(stream, L"componentOptionCount", request.componentOptions.size());
    for (std::size_t i = 0; i < request.componentOptions.size(); ++i) {
        const auto prefix = L"componentOption." + std::to_wstring(i);
        writeKeyValue(stream, prefix + L".code", request.componentOptions[i].code);
        writeKeyValue(stream, prefix + L".displayName", request.componentOptions[i].displayName);
    }

    writeKeyValue(stream, L"pavementRowCount", request.config.pavementRows.size());
    for (std::size_t i = 0; i < request.config.pavementRows.size(); ++i) {
        const auto& row = request.config.pavementRows[i];
        if (row.componentTypes.size() > static_cast<std::size_t>(kMaxConfigComponents)) {
            errorMessage = L"Section drawing config row has too many component types.";
            return false;
        }

        const auto prefix = L"pavementRow." + std::to_wstring(i);
        writeKeyValue(stream, prefix + L".startStation", row.startStation);
        writeKeyValue(stream, prefix + L".endStation", row.endStation);
        writeKeyValue(stream, prefix + L".componentCodes", joinComponentCodes(row.componentTypes));
        writeKeyValue(stream, prefix + L".templateHandle", row.templateHandle);
        writeKeyValue(stream, prefix + L".templateName", row.templateName);
    }

    writeKeyValue(stream, L"clearTableRowCount", request.config.clearTableRows.size());
    for (std::size_t i = 0; i < request.config.clearTableRows.size(); ++i) {
        const auto& row = request.config.clearTableRows[i];
        const auto prefix = L"clearTableRow." + std::to_wstring(i);
        writeKeyValue(stream, prefix + L".startStation", row.startStation);
        writeKeyValue(stream, prefix + L".endStation", row.endStation);
        writeKeyValue(stream, prefix + L".leftSlopeRatio", row.leftSlopeRatio);
        writeKeyValue(stream, prefix + L".rightSlopeRatio", row.rightSlopeRatio);
        writeKeyValue(stream, prefix + L".thickness", row.thickness);
        writeKeyValue(stream, prefix + L".scope", SectionDrawingConfigRules::clearTableScopeCode(row.scope));
        writeKeyValue(stream, prefix + L".clearCut", row.clearCut);
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
    const auto value = valueOrDefault(values, key);
    if (value.empty()) {
        return fallback;
    }
    wchar_t* end = nullptr;
    const auto parsed = std::wcstol(value.c_str(), &end, 10);
    return end == value.c_str() ? fallback : static_cast<int>(parsed);
}

std::optional<double> doubleValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key)
{
    const auto value = valueOrDefault(values, key);
    if (value.empty()) {
        return std::nullopt;
    }

    errno = 0;
    wchar_t* end = nullptr;
    const double parsed = std::wcstod(value.c_str(), &end);
    if (end == value.c_str() || errno == ERANGE || !std::isfinite(parsed)) {
        return std::nullopt;
    }

    while (end != nullptr && *end != L'\0') {
        if (std::iswspace(*end) == 0) {
            return std::nullopt;
        }
        ++end;
    }

    return parsed;
}

std::vector<std::wstring> splitWide(const std::wstring& value, wchar_t delimiter)
{
    std::vector<std::wstring> parts;
    std::wstring current;
    for (const auto ch : value) {
        if (ch == delimiter) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

SectionDrawingConfigDialogAction actionFromCode(const std::wstring& code)
{
    if (code == L"draw") {
        return SectionDrawingConfigDialogAction::Draw;
    }
    if (code == L"pickTemplate") {
        return SectionDrawingConfigDialogAction::PickTemplate;
    }
    return SectionDrawingConfigDialogAction::None;
}

bool writePendingRequestPath(const std::wstring& requestPath, std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(pendingRequestPath()), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write section drawing config pending request path.";
        return false;
    }

    stream << wideToUtf8(requestPath);
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

bool queueSectionDrawingConfigWpfDialog(
    const SectionDrawingConfigDialogRequest& request,
    std::wstring& errorMessage)
{
    const auto requestPath = tempFilePath(L".request");
    const auto responsePath = request.responsePath.empty() ? tempFilePath(L".response") : request.responsePath;
    if (!writeRequestFile(request, requestPath, responsePath, errorMessage)) {
        removeFileIfExists(requestPath);
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

    const std::wstring command = L"RD_SECTION_DRAWING_CONFIG_SHOW_WPF_DIALOG\n";
    acDocManager->sendStringToExecute(document, command.c_str(), true, false, false);
    return true;
}

bool readSectionDrawingConfigDialogResponse(
    const std::wstring& responsePath,
    SectionDrawingConfigDialogResponse& response,
    std::wstring& errorMessage)
{
    std::ifstream test(std::filesystem::path(responsePath), std::ios::binary);
    if (!test) {
        errorMessage = L"Cannot open section drawing config dialog response file.";
        return false;
    }
    test.close();

    const auto values = readKeyValueFile(responsePath);
    SectionDrawingConfigDialogResponse parsed;
    parsed.action = actionFromCode(valueOrDefault(values, L"action"));
    parsed.accepted = boolValue(values, L"accepted", false);
    parsed.drawingHandle = valueOrDefault(values, L"drawingHandle");
    parsed.roadModelHandle = valueOrDefault(values, L"roadModelHandle");
    parsed.responsePath = valueOrDefault(values, L"responsePath");
    parsed.pickRowIndex = intValue(values, L"pickRowIndex", -1);
    parsed.config.configPath = valueOrDefault(values, L"configPath");

    const auto componentOptionCount = intValue(values, L"componentOptionCount", 0);
    if (componentOptionCount < 0 || componentOptionCount > kMaxComponentOptions) {
        errorMessage = L"Section drawing config response component option count is invalid.";
        return false;
    }
    parsed.componentOptions.reserve(static_cast<std::size_t>(componentOptionCount));
    for (int i = 0; i < componentOptionCount; ++i) {
        const auto prefix = L"componentOption." + std::to_wstring(i);
        SectionDrawingConfigComponentOption option;
        option.code = valueOrDefault(values, prefix + L".code");
        option.displayName = valueOrDefault(values, prefix + L".displayName");
        const auto selection = SectionDrawingConfigRules::componentSelectionFromText(option.code);
        if (!selection.has_value()) {
            errorMessage = L"Section drawing config response component option code is invalid.";
            return false;
        }
        option.selection = *selection;
        parsed.componentOptions.push_back(std::move(option));
    }

    const auto rowCount = intValue(values, L"pavementRowCount", 0);
    if (rowCount < 0 || rowCount > kMaxConfigRows) {
        errorMessage = L"Section drawing config response row count is invalid.";
        return false;
    }
    parsed.config.pavementRows.reserve(static_cast<std::size_t>(rowCount));
    for (int i = 0; i < rowCount; ++i) {
        const auto prefix = L"pavementRow." + std::to_wstring(i);
        SectionPavementLayerConfigRow row;
        const auto startStation = doubleValue(values, prefix + L".startStation");
        const auto endStation = doubleValue(values, prefix + L".endStation");
        if (!startStation.has_value() || !endStation.has_value()) {
            errorMessage = L"Section drawing config response station value is invalid.";
            return false;
        }
        row.startStation = *startStation;
        row.endStation = *endStation;
        row.templateHandle = valueOrDefault(values, prefix + L".templateHandle");
        row.templateName = valueOrDefault(values, prefix + L".templateName");

        const auto componentCodes = splitWide(valueOrDefault(values, prefix + L".componentCodes"), L';');
        if (componentCodes.size() > static_cast<std::size_t>(kMaxConfigComponents)) {
            errorMessage = L"Section drawing config response component count is invalid.";
            return false;
        }
        for (const auto& componentCode : componentCodes) {
            if (componentCode.empty()) {
                continue;
            }
            const auto selection = SectionDrawingConfigRules::componentSelectionFromText(componentCode);
            if (!selection.has_value()) {
                errorMessage = L"Section drawing config response component code is invalid.";
                return false;
            }
            row.componentTypes.push_back(*selection);
        }

        parsed.config.pavementRows.push_back(std::move(row));
    }

    const auto clearTableRowCount = intValue(values, L"clearTableRowCount", 0);
    if (clearTableRowCount < 0 || clearTableRowCount > kMaxConfigRows) {
        errorMessage = L"Section drawing config response clear table row count is invalid.";
        return false;
    }
    parsed.config.clearTableRows.reserve(static_cast<std::size_t>(clearTableRowCount));
    for (int i = 0; i < clearTableRowCount; ++i) {
        const auto prefix = L"clearTableRow." + std::to_wstring(i);
        SectionClearTableConfigRow row;
        const auto startStation = doubleValue(values, prefix + L".startStation");
        const auto endStation = doubleValue(values, prefix + L".endStation");
        const auto leftSlopeRatio = doubleValue(values, prefix + L".leftSlopeRatio");
        const auto rightSlopeRatio = doubleValue(values, prefix + L".rightSlopeRatio");
        const auto thickness = doubleValue(values, prefix + L".thickness");
        if (!startStation.has_value()
            || !endStation.has_value()
            || !leftSlopeRatio.has_value()
            || !rightSlopeRatio.has_value()
            || !thickness.has_value()) {
            errorMessage = L"Section drawing config response clear table value is invalid.";
            return false;
        }
        const auto scope = SectionDrawingConfigRules::clearTableScopeFromText(
            valueOrDefault(values, prefix + L".scope", L"Both"));
        if (!scope.has_value()) {
            errorMessage = L"Section drawing config response clear table scope is invalid.";
            return false;
        }
        row.startStation = *startStation;
        row.endStation = *endStation;
        row.leftSlopeRatio = *leftSlopeRatio;
        row.rightSlopeRatio = *rightSlopeRatio;
        row.thickness = *thickness;
        row.scope = *scope;
        row.clearCut = boolValue(values, prefix + L".clearCut", true);
        parsed.config.clearTableRows.push_back(std::move(row));
    }

    if (!SectionDrawingConfigRules::normalize(parsed.config, errorMessage)) {
        return false;
    }

    response = std::move(parsed);
    removeFileIfExists(responsePath);
    return true;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
