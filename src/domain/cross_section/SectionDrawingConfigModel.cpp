#include "domain/cross_section/SectionDrawingConfigModel.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwctype>
#include <iomanip>
#include <sstream>

namespace roadproto::domain::cross_section {
namespace {

constexpr double kStationTolerance = 1.0e-7;

bool isFinite(double value)
{
    return std::isfinite(value);
}

std::wstring trim(std::wstring value)
{
    while (!value.empty() && std::iswspace(value.front()) != 0) {
        value.erase(value.begin());
    }
    while (!value.empty() && std::iswspace(value.back()) != 0) {
        value.pop_back();
    }
    return value;
}

void appendUtf8CodePoint(std::string& output, unsigned int codePoint)
{
    if (codePoint <= 0x7F) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

std::string wideToUtf8(const std::wstring& value)
{
    std::string output;
    for (std::size_t i = 0; i < value.size(); ++i) {
        unsigned int codePoint = static_cast<unsigned int>(value[i]);
        if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 1 < value.size()) {
            const auto low = static_cast<unsigned int>(value[i + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }
        appendUtf8CodePoint(output, codePoint);
    }
    return output;
}

std::wstring utf8ToWide(const std::string& value)
{
    std::wstring output;
    for (std::size_t i = 0; i < value.size();) {
        const auto first = static_cast<unsigned char>(value[i]);
        unsigned int codePoint = 0;
        std::size_t advance = 1;

        if (first < 0x80) {
            codePoint = first;
        } else if ((first & 0xE0) == 0xC0 && i + 1 < value.size()) {
            const auto second = static_cast<unsigned char>(value[i + 1]);
            codePoint = ((first & 0x1F) << 6) | (second & 0x3F);
            advance = 2;
        } else if ((first & 0xF0) == 0xE0 && i + 2 < value.size()) {
            const auto second = static_cast<unsigned char>(value[i + 1]);
            const auto third = static_cast<unsigned char>(value[i + 2]);
            codePoint = ((first & 0x0F) << 12) | ((second & 0x3F) << 6) | (third & 0x3F);
            advance = 3;
        } else if ((first & 0xF8) == 0xF0 && i + 3 < value.size()) {
            const auto second = static_cast<unsigned char>(value[i + 1]);
            const auto third = static_cast<unsigned char>(value[i + 2]);
            const auto fourth = static_cast<unsigned char>(value[i + 3]);
            codePoint = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((third & 0x3F) << 6)
                | (fourth & 0x3F);
            advance = 4;
        } else {
            codePoint = L'?';
        }

        if (codePoint <= 0xFFFF) {
            output.push_back(static_cast<wchar_t>(codePoint));
        } else {
            codePoint -= 0x10000;
            output.push_back(static_cast<wchar_t>(0xD800 + ((codePoint >> 10) & 0x3FF)));
            output.push_back(static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)));
        }
        i += advance;
    }
    return output;
}

std::string csvEscape(const std::wstring& value)
{
    const auto utf8 = wideToUtf8(value);
    if (utf8.find_first_of(",\"\r\n") == std::string::npos) {
        return utf8;
    }

    std::string escaped = "\"";
    for (const auto ch : utf8) {
        if (ch == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(ch);
        }
    }
    escaped += "\"";
    return escaped;
}

std::vector<std::string> parseCsvLine(const std::string& line)
{
    std::vector<std::string> cells;
    std::string cell;
    bool quoted = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const auto ch = line[i];
        if (quoted && ch == '"' && i + 1 < line.size() && line[i + 1] == '"') {
            cell.push_back('"');
            ++i;
        } else if (ch == '"') {
            quoted = !quoted;
        } else if (!quoted && ch == ',') {
            cells.push_back(cell);
            cell.clear();
        } else {
            cell.push_back(ch);
        }
    }
    cells.push_back(cell);
    return cells;
}

std::vector<std::string> splitCsvRecords(std::string text)
{
    if (text.size() >= 3
        && static_cast<unsigned char>(text[0]) == 0xEF
        && static_cast<unsigned char>(text[1]) == 0xBB
        && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }

    std::vector<std::string> records;
    std::string record;
    bool quoted = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const auto ch = text[i];
        if (quoted && ch == '"' && i + 1 < text.size() && text[i + 1] == '"') {
            record.push_back(ch);
            record.push_back(text[i + 1]);
            ++i;
        } else if (ch == '"') {
            quoted = !quoted;
            record.push_back(ch);
        } else if (!quoted && (ch == '\n' || ch == '\r')) {
            if (!record.empty()) {
                records.push_back(record);
                record.clear();
            }
            if (ch == '\r' && i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;
            }
        } else {
            record.push_back(ch);
        }
    }
    if (!record.empty()) {
        records.push_back(record);
    }
    return records;
}

std::vector<std::wstring> splitWide(const std::wstring& text, wchar_t separator)
{
    std::vector<std::wstring> result;
    std::wstring current;
    for (const auto ch : text) {
        if (ch == separator) {
            current = trim(current);
            if (!current.empty()) {
                result.push_back(current);
            }
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    current = trim(current);
    if (!current.empty()) {
        result.push_back(current);
    }
    return result;
}

std::string formatStation(double value)
{
    std::ostringstream stream;
    stream << std::setprecision(15) << value;
    auto text = stream.str();
    if (text.find('.') != std::string::npos) {
        while (!text.empty() && text.back() == '0') {
            text.pop_back();
        }
        if (!text.empty() && text.back() == '.') {
            text.pop_back();
        }
    }
    return text.empty() ? "0" : text;
}

std::optional<double> parseStation(const std::string& text)
{
    try {
        std::size_t consumed = 0;
        const auto value = std::stod(text, &consumed);
        if (consumed != text.size() || !isFinite(value)) {
            return std::nullopt;
        }
        return value;
    } catch (...) {
        return std::nullopt;
    }
}

std::wstring sideCode(SubgradeSide side)
{
    return side == SubgradeSide::Left ? L"Left" : L"Right";
}

std::wstring sideDisplayName(SubgradeSide side)
{
    return side == SubgradeSide::Left ? L"\u5de6\u4fa7" : L"\u53f3\u4fa7";
}

std::optional<SubgradeSide> sideFromText(const std::wstring& text)
{
    const auto value = trim(text);
    if (value == L"Left" || value == L"\u5de6\u4fa7") {
        return SubgradeSide::Left;
    }
    if (value == L"Right" || value == L"\u53f3\u4fa7") {
        return SubgradeSide::Right;
    }
    return std::nullopt;
}

bool isValidClearTableScope(SectionClearTableScope scope)
{
    return scope == SectionClearTableScope::Left
        || scope == SectionClearTableScope::Right
        || scope == SectionClearTableScope::Both;
}

std::wstring componentTypeDisplayName(SubgradeComponentType type)
{
    switch (type) {
    case SubgradeComponentType::Median:
        return L"\u4e2d\u5206\u5e26";
    case SubgradeComponentType::TravelLane:
        return L"\u884c\u8f66\u9053";
    case SubgradeComponentType::HardShoulder:
        return L"\u786c\u8def\u80a9";
    case SubgradeComponentType::EarthShoulder:
        return L"\u571f\u8def\u80a9";
    case SubgradeComponentType::SideMedian:
        return L"\u4fa7\u5206\u5e26";
    case SubgradeComponentType::Sidewalk:
        return L"\u4eba\u884c\u9053";
    case SubgradeComponentType::BikeLane:
        return L"\u6162\u8f66\u9053";
    case SubgradeComponentType::CurbStrip:
        return L"\u8def\u7f18\u5e26";
    default:
        return L"\u884c\u8f66\u9053";
    }
}

std::optional<SubgradeComponentType> componentTypeFromDisplayName(const std::wstring& text)
{
    const SubgradeComponentType allTypes[] = {
        SubgradeComponentType::Median,
        SubgradeComponentType::TravelLane,
        SubgradeComponentType::HardShoulder,
        SubgradeComponentType::EarthShoulder,
        SubgradeComponentType::SideMedian,
        SubgradeComponentType::Sidewalk,
        SubgradeComponentType::BikeLane,
        SubgradeComponentType::CurbStrip,
    };

    for (const auto type : allTypes) {
        if (text == componentTypeDisplayName(type)) {
            return type;
        }
    }
    return std::nullopt;
}

bool isBlankRecord(const std::string& record)
{
    return std::all_of(record.begin(), record.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
}

bool isExpectedCsvHeader(const std::vector<std::string>& cells)
{
    return cells.size() == 5
        && utf8ToWide(cells[0]) == L"\u8d77\u70b9\u6869\u53f7"
        && utf8ToWide(cells[1]) == L"\u7ec8\u70b9\u6869\u53f7"
        && utf8ToWide(cells[2]) == L"\u8def\u57fa\u7c7b\u578b"
        && utf8ToWide(cells[3]) == L"\u6a21\u677fHandle"
        && utf8ToWide(cells[4]) == L"\u6a21\u677f\u540d\u79f0";
}

} // namespace

bool SectionDrawingConfigRules::normalize(SectionDrawingConfigData& data, std::wstring& errorMessage)
{
    errorMessage.clear();
    data.version = std::max(1, data.version);

    for (auto& row : data.pavementRows) {
        if (!isFinite(row.startStation) || !isFinite(row.endStation)) {
            errorMessage = L"Section drawing config stations must be finite.";
            return false;
        }
        if (row.endStation < row.startStation) {
            std::swap(row.startStation, row.endStation);
        }

        row.templateHandle = trim(row.templateHandle);
        row.templateName = trim(row.templateName);

        std::vector<SectionDrawingComponentTypeSelection> unique;
        for (const auto& selection : row.componentTypes) {
            const auto exists = std::any_of(unique.begin(), unique.end(), [&](const auto& candidate) {
                return candidate.side == selection.side && candidate.componentType == selection.componentType;
            });
            if (!exists) {
                unique.push_back(selection);
            }
        }
        row.componentTypes = std::move(unique);
    }

    for (auto& row : data.clearTableRows) {
        if (!isFinite(row.startStation)
            || !isFinite(row.endStation)
            || !isFinite(row.leftSlopeRatio)
            || !isFinite(row.rightSlopeRatio)
            || !isFinite(row.thickness)
            || row.leftSlopeRatio <= 0.0
            || row.rightSlopeRatio <= 0.0
            || row.thickness <= 0.0
            || !isValidClearTableScope(row.scope)) {
            errorMessage = L"Section drawing clear table rows must use finite stations, positive slope ratios, and positive thickness.";
            return false;
        }
        if (row.endStation < row.startStation) {
            std::swap(row.startStation, row.endStation);
        }
    }

    return true;
}

std::optional<SectionDrawingResolvedPavementRow> SectionDrawingConfigRules::resolvePavementRow(
    const SectionDrawingConfigData& data,
    double station)
{
    if (!isFinite(station)) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < data.pavementRows.size(); ++i) {
        const auto& row = data.pavementRows[i];
        if (trim(row.templateHandle).empty()) {
            continue;
        }
        if (station >= row.startStation - kStationTolerance && station <= row.endStation + kStationTolerance) {
            return SectionDrawingResolvedPavementRow{i, row};
        }
    }
    return std::nullopt;
}

std::optional<SectionDrawingResolvedPavementRow> SectionDrawingConfigRules::resolvePavementRow(
    const SectionDrawingConfigData& data,
    double station,
    SubgradeSide side,
    SubgradeComponentType componentType)
{
    if (!isFinite(station)) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < data.pavementRows.size(); ++i) {
        const auto& row = data.pavementRows[i];
        if (trim(row.templateHandle).empty()) {
            continue;
        }
        if (station < row.startStation - kStationTolerance || station > row.endStation + kStationTolerance) {
            continue;
        }
        if (matchesComponent(row, side, componentType)) {
            return SectionDrawingResolvedPavementRow{i, row};
        }
    }
    return std::nullopt;
}

bool SectionDrawingConfigRules::matchesComponent(
    const SectionPavementLayerConfigRow& row,
    SubgradeSide side,
    SubgradeComponentType componentType)
{
    return std::any_of(row.componentTypes.begin(), row.componentTypes.end(), [&](const auto& selection) {
        return selection.side == side && selection.componentType == componentType;
    });
}

std::optional<SectionDrawingResolvedClearTableRow> SectionDrawingConfigRules::resolveClearTableRow(
    const SectionDrawingConfigData& data,
    double station,
    SubgradeSide side,
    bool isCutSection)
{
    if (!isFinite(station)) {
        return std::nullopt;
    }

    for (std::size_t i = 0; i < data.clearTableRows.size(); ++i) {
        const auto& row = data.clearTableRows[i];
        if (station < row.startStation - kStationTolerance || station > row.endStation + kStationTolerance) {
            continue;
        }
        if (isCutSection && !row.clearCut) {
            continue;
        }
        if (matchesClearTableScope(row.scope, side)) {
            return SectionDrawingResolvedClearTableRow{i, row};
        }
    }
    return std::nullopt;
}

bool SectionDrawingConfigRules::matchesClearTableScope(SectionClearTableScope scope, SubgradeSide side)
{
    return scope == SectionClearTableScope::Both
        || (scope == SectionClearTableScope::Left && side == SubgradeSide::Left)
        || (scope == SectionClearTableScope::Right && side == SubgradeSide::Right);
}

SectionClearTableEdgeSlopeRatios SectionDrawingConfigRules::clearTableEdgeSlopeRatios(
    const SectionClearTableConfigRow& row,
    SubgradeSide side)
{
    const auto outerSlopeRatio = side == SubgradeSide::Left
        ? row.leftSlopeRatio
        : row.rightSlopeRatio;
    double innerSlopeRatio = 0.0;
    if (row.scope == SectionClearTableScope::Left && side == SubgradeSide::Left) {
        innerSlopeRatio = row.rightSlopeRatio;
    } else if (row.scope == SectionClearTableScope::Right && side == SubgradeSide::Right) {
        innerSlopeRatio = row.leftSlopeRatio;
    }
    return SectionClearTableEdgeSlopeRatios{innerSlopeRatio, outerSlopeRatio};
}

std::wstring SectionDrawingConfigRules::componentSelectionCode(const SectionDrawingComponentTypeSelection& selection)
{
    return sideCode(selection.side) + L":" + std::wstring(subgradeComponentTypeCode(selection.componentType));
}

std::wstring SectionDrawingConfigRules::componentSelectionDisplayName(
    const SectionDrawingComponentTypeSelection& selection)
{
    return sideDisplayName(selection.side) + componentTypeDisplayName(selection.componentType);
}

std::optional<SectionDrawingComponentTypeSelection> SectionDrawingConfigRules::componentSelectionFromText(
    const std::wstring& text)
{
    const auto value = trim(text);
    const auto separator = value.find(L':');
    if (separator != std::wstring::npos) {
        const auto side = sideFromText(value.substr(0, separator));
        if (!side.has_value()) {
            return std::nullopt;
        }

        const auto code = trim(value.substr(separator + 1));
        const auto componentType = subgradeComponentTypeFromCode(code, SubgradeComponentType::TravelLane);
        if (std::wstring(subgradeComponentTypeCode(componentType)) != code) {
            return std::nullopt;
        }
        return SectionDrawingComponentTypeSelection{*side, componentType};
    }

    const auto leftPrefix = std::wstring(L"\u5de6\u4fa7");
    const auto rightPrefix = std::wstring(L"\u53f3\u4fa7");
    if (value.rfind(leftPrefix, 0) == 0) {
        const auto componentType = componentTypeFromDisplayName(value.substr(leftPrefix.size()));
        if (componentType.has_value()) {
            return SectionDrawingComponentTypeSelection{SubgradeSide::Left, *componentType};
        }
    }
    if (value.rfind(rightPrefix, 0) == 0) {
        const auto componentType = componentTypeFromDisplayName(value.substr(rightPrefix.size()));
        if (componentType.has_value()) {
            return SectionDrawingComponentTypeSelection{SubgradeSide::Right, *componentType};
        }
    }

    return std::nullopt;
}

std::wstring SectionDrawingConfigRules::clearTableScopeCode(SectionClearTableScope scope)
{
    switch (scope) {
    case SectionClearTableScope::Left:
        return L"Left";
    case SectionClearTableScope::Right:
        return L"Right";
    case SectionClearTableScope::Both:
    default:
        return L"Both";
    }
}

std::wstring SectionDrawingConfigRules::clearTableScopeDisplayName(SectionClearTableScope scope)
{
    switch (scope) {
    case SectionClearTableScope::Left:
        return L"\u5de6\u4fa7";
    case SectionClearTableScope::Right:
        return L"\u53f3\u4fa7";
    case SectionClearTableScope::Both:
    default:
        return L"\u4e24\u4fa7";
    }
}

std::optional<SectionClearTableScope> SectionDrawingConfigRules::clearTableScopeFromText(const std::wstring& text)
{
    const auto value = trim(text);
    if (value == L"Left" || value == L"\u5de6\u4fa7") {
        return SectionClearTableScope::Left;
    }
    if (value == L"Right" || value == L"\u53f3\u4fa7") {
        return SectionClearTableScope::Right;
    }
    if (value == L"Both" || value == L"\u4e24\u4fa7") {
        return SectionClearTableScope::Both;
    }
    return std::nullopt;
}

std::string SectionDrawingConfigCsv::write(const SectionDrawingConfigData& data)
{
    std::ostringstream stream;
    stream << "\xEF\xBB\xBF" << u8"起点桩号,终点桩号,路基类型,模板Handle,模板名称\n";

    for (const auto& row : data.pavementRows) {
        std::wstring components;
        for (std::size_t i = 0; i < row.componentTypes.size(); ++i) {
            if (i > 0) {
                components += L";";
            }
            components += SectionDrawingConfigRules::componentSelectionDisplayName(row.componentTypes[i]);
        }

        stream << formatStation(row.startStation) << ","
               << formatStation(row.endStation) << ","
               << csvEscape(components) << ","
               << csvEscape(row.templateHandle) << ","
               << csvEscape(row.templateName) << "\n";
    }

    return stream.str();
}

std::optional<SectionDrawingConfigData> SectionDrawingConfigCsv::read(
    const std::string& csv,
    const std::wstring& configPath,
    std::wstring& errorMessage)
{
    errorMessage.clear();
    SectionDrawingConfigData data;
    data.configPath = configPath;

    const auto records = splitCsvRecords(csv);
    bool headerConsumed = false;
    std::size_t lineNumber = 0;
    for (const auto& record : records) {
        ++lineNumber;
        if (isBlankRecord(record)) {
            continue;
        }

        const auto cells = parseCsvLine(record);
        if (!headerConsumed) {
            headerConsumed = true;
            if (!isExpectedCsvHeader(cells)) {
                errorMessage = L"Section drawing config CSV header is invalid.";
                return std::nullopt;
            }
            continue;
        }

        if (cells.size() != 5) {
            errorMessage = L"Section drawing config CSV row "
                + std::to_wstring(lineNumber)
                + L" must contain exactly five columns.";
            return std::nullopt;
        }

        const auto startStation = parseStation(cells[0]);
        const auto endStation = parseStation(cells[1]);
        if (!startStation.has_value() || !endStation.has_value()) {
            errorMessage = L"Section drawing config CSV station is invalid.";
            return std::nullopt;
        }

        SectionPavementLayerConfigRow row;
        row.startStation = *startStation;
        row.endStation = *endStation;
        row.templateHandle = trim(utf8ToWide(cells[3]));
        row.templateName = trim(utf8ToWide(cells[4]));

        const auto componentTexts = splitWide(utf8ToWide(cells[2]), L';');
        for (const auto& componentText : componentTexts) {
            const auto selection = SectionDrawingConfigRules::componentSelectionFromText(componentText);
            if (!selection.has_value()) {
                errorMessage = L"Section drawing config CSV component type is invalid.";
                return std::nullopt;
            }
            row.componentTypes.push_back(*selection);
        }

        data.pavementRows.push_back(std::move(row));
    }

    if (!headerConsumed) {
        errorMessage = L"Section drawing config CSV header is missing.";
        return std::nullopt;
    }

    if (!SectionDrawingConfigRules::normalize(data, errorMessage)) {
        return std::nullopt;
    }
    return data;
}

} // namespace roadproto::domain::cross_section
