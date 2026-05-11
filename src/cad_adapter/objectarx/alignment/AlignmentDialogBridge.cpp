#include "cad_adapter/objectarx/alignment/AlignmentDialogBridge.h"

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

namespace roadproto::cad_adapter::objectarx {
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
    MultiByteToWideChar(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        output.data(),
        size);
    return output;
}

std::wstring modeText(AlignmentDialogMode mode)
{
    switch (mode) {
    case AlignmentDialogMode::Properties:
        return L"properties";
    case AlignmentDialogMode::Curve:
        return L"curve";
    case AlignmentDialogMode::Full:
    default:
        return L"full";
    }
}

AlignmentDialogMode parseMode(const std::wstring& value)
{
    if (value == L"properties") {
        return AlignmentDialogMode::Properties;
    }
    if (value == L"curve") {
        return AlignmentDialogMode::Curve;
    }
    return AlignmentDialogMode::Full;
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
    name << L"RoadProtoAlignment_" << GetCurrentProcessId() << L"_" << now << suffix;
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

std::wstring toCommandPath(std::wstring path)
{
    std::replace(path.begin(), path.end(), L'\\', L'/');
    return path;
}

double parameterOrDefault(double value, double fallback)
{
    return value > 0.0 ? value : fallback;
}

domain::alignment::HorizontalCurveParameters parametersForRequest(const AlignmentDialogRequest& request)
{
    if (!request.alignment.curveParameters.empty()) {
        if (request.curveIndex < request.alignment.curveParameters.size()) {
            return request.alignment.curveParameters[request.curveIndex];
        }
        return request.alignment.curveParameters.front();
    }
    return {};
}

std::vector<domain::alignment::HorizontalCurveParameters> curveParametersForRequest(
    const AlignmentDialogRequest& request)
{
    if (!request.alignment.curveParameters.empty()) {
        return request.alignment.curveParameters;
    }

    const auto curveCount =
        request.alignment.controlPoints.size() >= 3 ? request.alignment.controlPoints.size() - 2 : 1;
    return std::vector<domain::alignment::HorizontalCurveParameters>(curveCount, parametersForRequest(request));
}

std::wstring curveParameterKey(std::size_t index, const wchar_t* name)
{
    return L"curve." + std::to_wstring(index) + L"." + name;
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, const std::wstring& value)
{
    stream << wideToUtf8(key) << '=' << escapeValue(value) << '\n';
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
    const AlignmentDialogRequest& request,
    const std::wstring& requestPath,
    const std::wstring& responsePath,
    std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(requestPath), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write alignment dialog request file.";
        return false;
    }

    const auto& properties = request.alignment.properties;
    const auto parameters = parametersForRequest(request);
    const auto curveParameters = curveParametersForRequest(request);
    writeKeyValue(stream, L"mode", modeText(request.mode));
    writeKeyValue(stream, L"handle", request.handle);
    writeKeyValue(stream, L"responsePath", responsePath);
    writeKeyValue(stream, L"deleteOnCancel", request.deleteOnCancel);
    writeKeyValue(stream, L"curveIndex", request.curveIndex);
    writeKeyValue(stream, L"roadName", properties.roadName.empty() ? L"K1" : properties.roadName);
    writeKeyValue(stream, L"roadGradeIndex", static_cast<std::size_t>(properties.roadGrade));
    writeKeyValue(stream, L"linkedTerrainEnabled", properties.linkedTerrainEnabled);
    writeKeyValue(stream, L"linkedTerrainHandle", properties.linkedTerrainHandle);
    writeKeyValue(stream, L"stationLabelInterval", parameterOrDefault(properties.stationLabelInterval, 100.0));
    writeKeyValue(stream, L"tangentIn", parameters.tangentIn);
    writeKeyValue(stream, L"tangentOut", parameters.tangentOut);
    writeKeyValue(stream, L"ls1", parameters.ls1);
    writeKeyValue(stream, L"radius", parameters.radius);
    writeKeyValue(stream, L"ls2", parameters.ls2);
    writeKeyValue(stream, L"curveCount", curveParameters.size());
    for (std::size_t index = 0; index < curveParameters.size(); ++index) {
        const auto& curve = curveParameters[index];
        writeKeyValue(stream, curveParameterKey(index, L"tangentIn"), curve.tangentIn);
        writeKeyValue(stream, curveParameterKey(index, L"tangentOut"), curve.tangentOut);
        writeKeyValue(stream, curveParameterKey(index, L"ls1"), curve.ls1);
        writeKeyValue(stream, curveParameterKey(index, L"radius"), curve.radius);
        writeKeyValue(stream, curveParameterKey(index, L"ls2"), curve.ls2);
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

std::size_t sizeValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    const std::wstring& key,
    std::size_t fallback = 0)
{
    try {
        return static_cast<std::size_t>(std::stoull(valueOrDefault(values, key, std::to_wstring(fallback))));
    } catch (...) {
        return fallback;
    }
}

domain::alignment::HorizontalCurveParameters curveParameterValue(
    const std::unordered_map<std::wstring, std::wstring>& values,
    std::size_t index,
    const domain::alignment::HorizontalCurveParameters& fallback)
{
    domain::alignment::HorizontalCurveParameters parameters;
    parameters.tangentIn = doubleValue(values, curveParameterKey(index, L"tangentIn"), fallback.tangentIn);
    parameters.tangentOut = doubleValue(values, curveParameterKey(index, L"tangentOut"), fallback.tangentOut);
    parameters.ls1 = doubleValue(values, curveParameterKey(index, L"ls1"), fallback.ls1);
    parameters.radius = doubleValue(values, curveParameterKey(index, L"radius"), fallback.radius);
    parameters.ls2 = doubleValue(values, curveParameterKey(index, L"ls2"), fallback.ls2);
    return parameters;
}

} // namespace

bool queueAlignmentWpfDialog(const AlignmentDialogRequest& request, std::wstring& errorMessage)
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
    const std::wstring command = L"RD_ALIGN_SHOW_WPF_DIALOG \"" + commandPath + L"\"\n\n";
    acDocManager->sendStringToExecute(document, command.c_str(), true, false, false);
    return true;
}

bool readAlignmentDialogResponse(
    const std::wstring& responsePath,
    AlignmentDialogResponse& response,
    std::wstring& errorMessage)
{
    std::ifstream test(std::filesystem::path(responsePath), std::ios::binary);
    if (!test) {
        errorMessage = L"Cannot open alignment dialog response file.";
        return false;
    }
    test.close();

    const auto values = readKeyValueFile(responsePath);
    response.accepted = boolValue(values, L"accepted", false);
    response.mode = parseMode(valueOrDefault(values, L"mode", L"full"));
    response.handle = valueOrDefault(values, L"handle");
    response.deleteOnCancel = boolValue(values, L"deleteOnCancel", false);
    response.curveIndex = sizeValue(values, L"curveIndex", 0);

    response.properties.roadName = valueOrDefault(values, L"roadName", L"K1");
    response.properties.roadGrade =
        domain::alignment::roadGradeFromIndex(sizeValue(values, L"roadGradeIndex", 9));
    response.properties.linkedTerrainEnabled = boolValue(values, L"linkedTerrainEnabled", false);
    response.properties.linkedTerrainHandle = valueOrDefault(values, L"linkedTerrainHandle");
    response.properties.stationLabelInterval = doubleValue(values, L"stationLabelInterval", 100.0);

    response.parameters.tangentIn = doubleValue(values, L"tangentIn", 0.0);
    response.parameters.tangentOut = doubleValue(values, L"tangentOut", 0.0);
    response.parameters.ls1 = doubleValue(values, L"ls1", 20.0);
    response.parameters.radius = doubleValue(values, L"radius", 80.0);
    response.parameters.ls2 = doubleValue(values, L"ls2", 20.0);

    const auto curveCount = sizeValue(values, L"curveCount", 0);
    response.curveParameters.clear();
    response.curveParameters.reserve(curveCount);
    for (std::size_t index = 0; index < curveCount; ++index) {
        response.curveParameters.push_back(curveParameterValue(values, index, response.parameters));
    }
    return true;
}

} // namespace roadproto::cad_adapter::objectarx
