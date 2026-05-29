#include "domain/quantity/PavementQuantityTable.h"

#include "domain/alignment/StationFormatter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <optional>
#include <sstream>

namespace roadproto::domain::quantity {
namespace {

constexpr double kStationTolerance = 1.0e-7;

bool isFinite(double value)
{
    return std::isfinite(value);
}

bool sameStation(double lhs, double rhs)
{
    return std::fabs(lhs - rhs) <= kStationTolerance;
}

std::wstring normalizeLayerName(const std::wstring& name)
{
    return name.empty() ? L"路面结构层" : name;
}

std::wstring normalizeComponentName(const std::wstring& name)
{
    return name.empty() ? L"未分部件" : name;
}

std::wstring displayNameForLayerSample(
    const PavementQuantityLayerSample& sample,
    PavementQuantityAggregationMode aggregationMode)
{
    const auto layerName = normalizeLayerName(sample.layerName);
    if (aggregationMode == PavementQuantityAggregationMode::ByComponentAndLayer) {
        return normalizeComponentName(sample.componentName) + L"-" + layerName;
    }
    return layerName;
}

PavementQuantitySectionSample normalizeSample(
    const PavementQuantitySectionSample& sample,
    PavementQuantityAggregationMode aggregationMode)
{
    PavementQuantitySectionSample normalized;
    normalized.station = sample.station;

    std::vector<std::wstring> order;
    std::map<std::wstring, PavementQuantityLayerSample> byName;
    for (const auto& layer : sample.layers) {
        const auto name = displayNameForLayerSample(layer, aggregationMode);
        if (byName.find(name) == byName.end()) {
            order.push_back(name);
            byName[name] = PavementQuantityLayerSample{name, 0.0, 0.0};
        }
        byName[name].projectedWidth += layer.projectedWidth;
        byName[name].sectionArea += layer.sectionArea;
    }

    normalized.layers.reserve(order.size());
    for (const auto& name : order) {
        normalized.layers.push_back(byName[name]);
    }
    return normalized;
}

std::optional<PavementQuantityLayerSample> findLayer(
    const PavementQuantitySectionSample& sample,
    const std::wstring& layerName)
{
    const auto found = std::find_if(
        sample.layers.begin(),
        sample.layers.end(),
        [&](const auto& layer) {
            return layer.layerName == layerName;
        });
    if (found == sample.layers.end()) {
        return std::nullopt;
    }
    return *found;
}

double layerValueAt(
    const std::vector<PavementQuantitySectionSample>& samples,
    const std::wstring& layerName,
    double station,
    bool useProjectedWidth)
{
    if (samples.empty()) {
        return 0.0;
    }

    if (station <= samples.front().station + kStationTolerance) {
        const auto layer = findLayer(samples.front(), layerName);
        return layer.has_value() ? (useProjectedWidth ? layer->projectedWidth : layer->sectionArea) : 0.0;
    }
    if (station >= samples.back().station - kStationTolerance) {
        const auto layer = findLayer(samples.back(), layerName);
        return layer.has_value() ? (useProjectedWidth ? layer->projectedWidth : layer->sectionArea) : 0.0;
    }

    for (std::size_t i = 1; i < samples.size(); ++i) {
        const auto& previous = samples[i - 1];
        const auto& next = samples[i];
        if (sameStation(station, previous.station)) {
            const auto layer = findLayer(previous, layerName);
            return layer.has_value() ? (useProjectedWidth ? layer->projectedWidth : layer->sectionArea) : 0.0;
        }
        if (sameStation(station, next.station)) {
            const auto layer = findLayer(next, layerName);
            return layer.has_value() ? (useProjectedWidth ? layer->projectedWidth : layer->sectionArea) : 0.0;
        }
        if (previous.station < station && station < next.station) {
            const auto previousLayer = findLayer(previous, layerName);
            const auto nextLayer = findLayer(next, layerName);
            const auto previousValue = previousLayer.has_value()
                ? (useProjectedWidth ? previousLayer->projectedWidth : previousLayer->sectionArea)
                : 0.0;
            const auto nextValue = nextLayer.has_value()
                ? (useProjectedWidth ? nextLayer->projectedWidth : nextLayer->sectionArea)
                : 0.0;
            const auto ratio = (station - previous.station) / (next.station - previous.station);
            return previousValue + (nextValue - previousValue) * ratio;
        }
    }

    return 0.0;
}

struct ActiveSegmentKind {
    PavementQuantitySegmentType type = PavementQuantitySegmentType::Normal;
    int structureIndex = -1;
};

ActiveSegmentKind segmentKindAt(
    double station,
    const std::vector<PavementQuantityStructureRange>& structures)
{
    for (std::size_t i = 0; i < structures.size(); ++i) {
        const auto& structure = structures[i];
        if (station >= structure.startStation - kStationTolerance
            && station <= structure.endStation + kStationTolerance) {
            return ActiveSegmentKind{structure.type, static_cast<int>(i)};
        }
    }
    return ActiveSegmentKind{};
}

bool sameSegmentKind(const ActiveSegmentKind& lhs, const ActiveSegmentKind& rhs)
{
    return lhs.type == rhs.type && lhs.structureIndex == rhs.structureIndex;
}

std::vector<double> sortedUniqueBreakpoints(std::vector<double> stations)
{
    std::sort(stations.begin(), stations.end());
    std::vector<double> result;
    for (const auto station : stations) {
        if (!isFinite(station)) {
            continue;
        }
        if (result.empty() || !sameStation(result.back(), station)) {
            result.push_back(station);
        }
    }
    return result;
}

std::string wideToUtf8(const std::wstring& value)
{
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
        std::uint32_t codePoint = static_cast<std::uint32_t>(value[i]);
        if (codePoint >= 0xD800 && codePoint <= 0xDBFF && i + 1 < value.size()) {
            const auto low = static_cast<std::uint32_t>(value[i + 1]);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                ++i;
            }
        }

        if (codePoint <= 0x7F) {
            result.push_back(static_cast<char>(codePoint));
        } else if (codePoint <= 0x7FF) {
            result.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else if (codePoint <= 0xFFFF) {
            result.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
            result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        } else {
            result.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
            result.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
            result.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
        }
    }
    return result;
}

std::wstring xmlEscape(const std::wstring& value)
{
    std::wstring escaped;
    for (const auto ch : value) {
        switch (ch) {
        case L'&':
            escaped += L"&amp;";
            break;
        case L'<':
            escaped += L"&lt;";
            break;
        case L'>':
            escaped += L"&gt;";
            break;
        case L'"':
            escaped += L"&quot;";
            break;
        case L'\'':
            escaped += L"&apos;";
            break;
        default:
            escaped.push_back(ch);
            break;
        }
    }
    return escaped;
}

std::wstring formatNumber(double value)
{
    std::wostringstream stream;
    stream.setf(std::ios::fixed, std::ios::floatfield);
    stream << std::setprecision(3) << value;
    auto text = stream.str();
    while (text.size() > 1 && text.back() == L'0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == L'.') {
        text.pop_back();
    }
    return text;
}

std::wstring stationRangeText(double startStation, double endStation)
{
    return roadproto::domain::alignment::StationFormatter::format(startStation)
        + L" - "
        + roadproto::domain::alignment::StationFormatter::format(endStation);
}

void writeCellStart(std::wostream& stream, const wchar_t* styleId)
{
    stream << L"<Cell";
    if (styleId != nullptr && styleId[0] != L'\0') {
        stream << L" ss:StyleID=\"" << styleId << L"\"";
    }
    stream << L">";
}

void writeStringCell(std::wostream& stream, const std::wstring& value, const wchar_t* styleId)
{
    writeCellStart(stream, styleId);
    stream << L"<Data ss:Type=\"String\">" << xmlEscape(value) << L"</Data></Cell>\n";
}

void writeNumberCell(std::wostream& stream, double value, const wchar_t* styleId)
{
    writeCellStart(stream, styleId);
    stream << L"<Data ss:Type=\"Number\">" << formatNumber(value) << L"</Data></Cell>\n";
}

void writeWorkbookStyles(std::wostream& stream)
{
    stream << L"<Styles>\n";
    stream << L"<Style ss:ID=\"TitleText\"><Alignment ss:Horizontal=\"Center\" ss:Vertical=\"Center\" ss:WrapText=\"1\"/>"
        << L"<Font ss:FontName=\"宋体\" ss:Size=\"10\" ss:Bold=\"1\"/></Style>\n";
    stream << L"<Style ss:ID=\"HeaderText\"><Alignment ss:Horizontal=\"Center\" ss:Vertical=\"Center\" ss:WrapText=\"1\"/>"
        << L"<Font ss:FontName=\"宋体\" ss:Size=\"10\" ss:Bold=\"1\"/></Style>\n";
    stream << L"<Style ss:ID=\"BodyText\"><Alignment ss:Horizontal=\"Center\" ss:Vertical=\"Center\" ss:WrapText=\"1\"/>"
        << L"<Font ss:FontName=\"宋体\" ss:Size=\"10\"/></Style>\n";
    stream << L"<Style ss:ID=\"LatinText\"><Alignment ss:Horizontal=\"Center\" ss:Vertical=\"Center\" ss:WrapText=\"1\"/>"
        << L"<Font ss:FontName=\"Times New Roman\" ss:Size=\"10\"/></Style>\n";
    stream << L"<Style ss:ID=\"NumberText\"><Alignment ss:Horizontal=\"Center\" ss:Vertical=\"Center\" ss:WrapText=\"1\"/>"
        << L"<Font ss:FontName=\"Times New Roman\" ss:Size=\"10\"/></Style>\n";
    stream << L"</Styles>\n";
}

void writeTableColumns(std::wostream& stream, std::size_t layerCount)
{
    stream << L"<Column ss:Width=\"45\"/>\n";
    stream << L"<Column ss:Width=\"135\"/>\n";
    stream << L"<Column ss:Width=\"70\"/>\n";
    for (std::size_t i = 0; i < layerCount * 2; ++i) {
        stream << L"<Column ss:Width=\"120\"/>\n";
    }
}

} // namespace

const wchar_t* pavementQuantitySegmentTypeDisplayName(PavementQuantitySegmentType type)
{
    switch (type) {
    case PavementQuantitySegmentType::Bridge:
        return L"桥梁段";
    case PavementQuantitySegmentType::Tunnel:
        return L"隧道段";
    case PavementQuantitySegmentType::Normal:
    default:
        return L"普通段";
    }
}

PavementQuantityTable PavementQuantityTableCalculator::build(
    const std::vector<PavementQuantitySectionSample>& samples,
    const std::vector<PavementQuantityStructureRange>& structures,
    std::wstring& errorMessage)
{
    return build(samples, structures, PavementQuantityAggregationMode::ByLayerType, errorMessage);
}

PavementQuantityTable PavementQuantityTableCalculator::build(
    const std::vector<PavementQuantitySectionSample>& samples,
    const std::vector<PavementQuantityStructureRange>& structures,
    PavementQuantityAggregationMode aggregationMode,
    std::wstring& errorMessage)
{
    errorMessage.clear();
    PavementQuantityTable table;

    if (samples.size() < 2) {
        errorMessage = L"路面工程量统计至少需要两个横断面。";
        return table;
    }

    std::vector<PavementQuantitySectionSample> normalizedSamples;
    normalizedSamples.reserve(samples.size());
    for (const auto& sample : samples) {
        if (!isFinite(sample.station)) {
            errorMessage = L"横断面桩号必须是有限数值。";
            return table;
        }
        auto normalized = normalizeSample(sample, aggregationMode);
        for (const auto& layer : normalized.layers) {
            if (!isFinite(layer.projectedWidth) || !isFinite(layer.sectionArea)
                || layer.projectedWidth < 0.0 || layer.sectionArea < 0.0) {
                errorMessage = L"路面结构层宽度和截面积必须是非负有限数值。";
                return table;
            }
            if (std::find(table.layerNames.begin(), table.layerNames.end(), layer.layerName) == table.layerNames.end()) {
                table.layerNames.push_back(layer.layerName);
            }
        }
        normalizedSamples.push_back(std::move(normalized));
    }

    std::sort(
        normalizedSamples.begin(),
        normalizedSamples.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.station < rhs.station;
        });

    for (std::size_t i = 1; i < normalizedSamples.size(); ++i) {
        if (sameStation(normalizedSamples[i - 1].station, normalizedSamples[i].station)) {
            errorMessage = L"横断面桩号不能重复。";
            return table;
        }
    }

    if (table.layerNames.empty()) {
        errorMessage = L"选中的横断面没有可统计的路面结构层。";
        return table;
    }

    std::vector<PavementQuantityStructureRange> normalizedStructures;
    normalizedStructures.reserve(structures.size());
    const auto minStation = normalizedSamples.front().station;
    const auto maxStation = normalizedSamples.back().station;
    for (const auto& structure : structures) {
        if (!isFinite(structure.startStation) || !isFinite(structure.endStation)
            || structure.endStation < structure.startStation) {
            errorMessage = L"构造物桩号范围无效。";
            return table;
        }
        if (structure.endStation <= minStation + kStationTolerance
            || structure.startStation >= maxStation - kStationTolerance) {
            continue;
        }
        normalizedStructures.push_back(
            PavementQuantityStructureRange{
                std::clamp(structure.startStation, minStation, maxStation),
                std::clamp(structure.endStation, minStation, maxStation),
                structure.type});
    }

    std::vector<double> breakpoints;
    breakpoints.reserve(normalizedSamples.size() + normalizedStructures.size() * 2);
    for (const auto& sample : normalizedSamples) {
        breakpoints.push_back(sample.station);
    }
    for (const auto& structure : normalizedStructures) {
        breakpoints.push_back(structure.startStation);
        breakpoints.push_back(structure.endStation);
    }
    breakpoints = sortedUniqueBreakpoints(std::move(breakpoints));

    std::vector<ActiveSegmentKind> rowKinds;
    for (std::size_t i = 1; i < breakpoints.size(); ++i) {
        const auto start = breakpoints[i - 1];
        const auto end = breakpoints[i];
        const auto length = end - start;
        if (length <= kStationTolerance) {
            continue;
        }

        const auto kind = segmentKindAt((start + end) * 0.5, normalizedStructures);
        if (table.rows.empty() || !sameSegmentKind(rowKinds.back(), kind)
            || !sameStation(table.rows.back().endStation, start)) {
            PavementQuantitySegmentRow row;
            row.sequence = static_cast<int>(table.rows.size() + 1);
            row.startStation = start;
            row.endStation = end;
            row.type = kind.type;
            row.totals.resize(table.layerNames.size());
            table.rows.push_back(std::move(row));
            rowKinds.push_back(kind);
        } else {
            table.rows.back().endStation = end;
        }

        auto& row = table.rows.back();
        for (std::size_t layerIndex = 0; layerIndex < table.layerNames.size(); ++layerIndex) {
            const auto& layerName = table.layerNames[layerIndex];
            const auto startWidth = layerValueAt(normalizedSamples, layerName, start, true);
            const auto endWidth = layerValueAt(normalizedSamples, layerName, end, true);
            const auto startArea = layerValueAt(normalizedSamples, layerName, start, false);
            const auto endArea = layerValueAt(normalizedSamples, layerName, end, false);
            row.totals[layerIndex].projectedArea += (startWidth + endWidth) * 0.5 * length;
            row.totals[layerIndex].volume += (startArea + endArea) * 0.5 * length;
        }
    }

    if (table.rows.empty()) {
        errorMessage = L"横断面桩号范围不足，无法形成统计区间。";
    }

    return table;
}

bool PavementQuantityTableXlsWriter::write(
    const std::wstring& path,
    const PavementQuantityTable& table,
    std::wstring& errorMessage)
{
    errorMessage.clear();
    if (path.empty()) {
        errorMessage = L"输出路径不能为空。";
        return false;
    }
    if (table.layerNames.empty() || table.rows.empty()) {
        errorMessage = L"路面工程量统计表没有可写入的数据。";
        return false;
    }

    std::wostringstream xml;
    xml << L"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    xml << L"<?mso-application progid=\"Excel.Sheet\"?>\n";
    xml << L"<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\" "
        << L"xmlns:o=\"urn:schemas-microsoft-com:office:office\" "
        << L"xmlns:x=\"urn:schemas-microsoft-com:office:excel\" "
        << L"xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\">\n";
    writeWorkbookStyles(xml);
    xml << L"<Worksheet ss:Name=\"路面工程量统计表\"><Table>\n";
    writeTableColumns(xml, table.layerNames.size());

    xml << L"<Row><Cell ss:MergeAcross=\""
        << static_cast<int>(table.layerNames.size() * 2 + 2)
        << L"\" ss:StyleID=\"TitleText\"><Data ss:Type=\"String\">路面工程量统计表</Data></Cell></Row>\n";

    xml << L"<Row>\n";
    writeStringCell(xml, L"序号", L"HeaderText");
    writeStringCell(xml, L"起讫桩号", L"HeaderText");
    writeStringCell(xml, L"类型", L"HeaderText");
    for (const auto& layerName : table.layerNames) {
        writeStringCell(xml, layerName + L"面积", L"HeaderText");
    }
    for (const auto& layerName : table.layerNames) {
        writeStringCell(xml, layerName + L"体积", L"HeaderText");
    }
    xml << L"</Row>\n";

    for (const auto& row : table.rows) {
        xml << L"<Row>\n";
        writeNumberCell(xml, static_cast<double>(row.sequence), L"NumberText");
        writeStringCell(xml, stationRangeText(row.startStation, row.endStation), L"LatinText");
        writeStringCell(xml, pavementQuantitySegmentTypeDisplayName(row.type), L"BodyText");
        for (std::size_t i = 0; i < table.layerNames.size(); ++i) {
            const auto value = i < row.totals.size() ? row.totals[i].projectedArea : 0.0;
            writeNumberCell(xml, value, L"NumberText");
        }
        for (std::size_t i = 0; i < table.layerNames.size(); ++i) {
            const auto value = i < row.totals.size() ? row.totals[i].volume : 0.0;
            writeNumberCell(xml, value, L"NumberText");
        }
        xml << L"</Row>\n";
    }

    xml << L"</Table></Worksheet>\n";
    xml << L"</Workbook>\n";

    const std::filesystem::path outputPath(path);
    std::ofstream stream(outputPath, std::ios::binary | std::ios::trunc);
    if (!stream) {
        errorMessage = L"无法创建路面工程量统计表文件。";
        return false;
    }

    const unsigned char bom[] = {0xEF, 0xBB, 0xBF};
    stream.write(reinterpret_cast<const char*>(bom), sizeof(bom));
    const auto utf8 = wideToUtf8(xml.str());
    stream.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
    if (!stream) {
        errorMessage = L"写入路面工程量统计表文件失败。";
        return false;
    }
    return true;
}

} // namespace roadproto::domain::quantity
