#include "domain/alignment/IcdAlignmentFile.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace roadproto::domain::alignment {
namespace {

constexpr double kEpsilon = 1e-9;
constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

std::wstring widenAscii(const std::string& value)
{
    return std::wstring(value.begin(), value.end());
}

std::string narrowAscii(const std::wstring& value)
{
    std::string result;
    result.reserve(value.size());
    for (const auto ch : value) {
        result.push_back(ch <= 0x7F ? static_cast<char>(ch) : '?');
    }
    return result;
}

std::string trim(std::string value)
{
    const auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char ch) { return !isSpace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](char ch) { return !isSpace(ch); }).base(), value.end());
    return value;
}

std::string stripComment(const std::string& line)
{
    const auto pos = line.find("//");
    return trim(pos == std::string::npos ? line : line.substr(0, pos));
}

std::vector<std::string> splitFields(const std::string& line)
{
    std::string normalized = line;
    for (auto& ch : normalized) {
        if (ch == ',' || ch == '\t') {
            ch = ' ';
        }
    }

    std::vector<std::string> fields;
    std::istringstream stream(normalized);
    std::string field;
    while (stream >> field) {
        fields.push_back(field);
    }
    return fields;
}

double parseDouble(const std::string& value)
{
    std::size_t parsed = 0;
    const auto result = std::stod(value, &parsed);
    if (parsed != value.size()) {
        throw std::runtime_error("invalid number");
    }
    return result;
}

int parseInt(const std::string& value)
{
    std::size_t parsed = 0;
    const auto result = std::stoi(value, &parsed);
    if (parsed != value.size()) {
        throw std::runtime_error("invalid integer");
    }
    return result;
}

double parseStationValue(const std::string& text)
{
    const auto stationPart = text.substr(0, text.find('_'));
    return parseDouble(stationPart);
}

double signedCurvatureFromIcdTurn(int turn, double radius)
{
    if (radius <= 0.0) {
        return 0.0;
    }
    if (turn == 1) {
        return -1.0 / radius;
    }
    if (turn == -1) {
        return 1.0 / radius;
    }
    return 0.0;
}

int icdTurnFromCurvature(double curvature)
{
    return curvature < 0.0 ? 1 : -1;
}

double radiusFromCurvature(double curvature)
{
    return std::fabs(curvature) <= kEpsilon ? 0.0 : 1.0 / std::fabs(curvature);
}

IcdAlignmentUnit parseUnit(const std::vector<std::string>& fields)
{
    if (fields.empty()) {
        throw std::runtime_error("empty icd unit");
    }

    const auto rawType = parseInt(fields[0]);
    IcdAlignmentUnit unit;
    unit.type = static_cast<IcdUnitType>(rawType);

    switch (unit.type) {
    case IcdUnitType::Line:
        if (fields.size() < 2) {
            throw std::runtime_error("line unit requires length");
        }
        unit.length = parseDouble(fields[1]);
        if (fields.size() >= 3) {
            unit.hasEndHeading = true;
            unit.endHeading = parseDouble(fields[2]);
        }
        break;
    case IcdUnitType::CircularArc:
        if (fields.size() < 4) {
            throw std::runtime_error("arc unit requires radius, length and turn");
        }
        unit.radius = parseDouble(fields[1]);
        unit.length = parseDouble(fields[2]);
        unit.turn = parseInt(fields[3]);
        break;
    case IcdUnitType::SpiralIn:
        if (fields.size() < 4) {
            throw std::runtime_error("spiral-in unit requires A, end radius and turn");
        }
        unit.a = parseDouble(fields[1]);
        unit.endRadius = parseDouble(fields[2]);
        unit.turn = parseInt(fields[3]);
        unit.length = unit.endRadius <= 0.0 ? 0.0 : (unit.a * unit.a) / unit.endRadius;
        break;
    case IcdUnitType::SpiralOut:
        if (fields.size() < 4) {
            throw std::runtime_error("spiral-out unit requires A, start radius and turn");
        }
        unit.a = parseDouble(fields[1]);
        unit.startRadius = parseDouble(fields[2]);
        unit.turn = parseInt(fields[3]);
        unit.length = unit.startRadius <= 0.0 ? 0.0 : (unit.a * unit.a) / unit.startRadius;
        break;
    case IcdUnitType::PartialSpiralLargeToSmall:
    case IcdUnitType::PartialSpiralSmallToLarge:
        if (fields.size() < 5) {
            throw std::runtime_error("partial spiral unit requires A, start radius, end radius and turn");
        }
        unit.a = parseDouble(fields[1]);
        unit.startRadius = parseDouble(fields[2]);
        unit.endRadius = parseDouble(fields[3]);
        unit.turn = parseInt(fields[4]);
        if (unit.startRadius > 0.0 && unit.endRadius > 0.0) {
            unit.length = std::fabs((unit.a * unit.a) / unit.endRadius - (unit.a * unit.a) / unit.startRadius);
        }
        break;
    default:
        throw std::runtime_error("unsupported icd unit type");
    }

    return unit;
}

bool isTerminator(const std::vector<std::string>& fields)
{
    if (fields.empty()) {
        return false;
    }
    return parseInt(fields[0]) == 0;
}

std::string formatDouble(double value)
{
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(6) << value;
    return stream.str();
}

double normalizeAngle(double angle)
{
    auto normalized = std::fmod(angle, kTwoPi);
    if (normalized < 0.0) {
        normalized += kTwoPi;
    }
    return normalized;
}

AlignmentPoint2d cadPointFromIcdEngineeringPoint(const AlignmentPoint2d& point)
{
    return {point.y, point.x};
}

AlignmentPoint2d icdEngineeringPointFromCadPoint(const AlignmentPoint2d& point)
{
    return {point.y, point.x};
}

double cadHeadingFromIcdEngineeringAngle(double angle)
{
    return normalizeAngle(kPi / 2.0 - angle);
}

double icdEngineeringAngleFromCadHeading(double heading)
{
    return normalizeAngle(kPi / 2.0 - heading);
}

AlignmentChainElementInput chainElementFromUnit(const IcdAlignmentUnit& unit)
{
    switch (unit.type) {
    case IcdUnitType::Line:
        return AlignmentChainElementInput::line(unit.length);
    case IcdUnitType::CircularArc:
        return AlignmentChainElementInput::circularArc(
            unit.length,
            unit.radius,
            unit.turn == 1 ? CurveTurnDirection::Right : CurveTurnDirection::Left);
    case IcdUnitType::SpiralIn:
        return AlignmentChainElementInput::curvatureTransition(
            unit.length,
            0.0,
            signedCurvatureFromIcdTurn(unit.turn, unit.endRadius));
    case IcdUnitType::SpiralOut:
        return AlignmentChainElementInput::curvatureTransition(
            unit.length,
            signedCurvatureFromIcdTurn(unit.turn, unit.startRadius),
            0.0);
    case IcdUnitType::PartialSpiralLargeToSmall:
    case IcdUnitType::PartialSpiralSmallToLarge:
        return AlignmentChainElementInput::curvatureTransition(
            unit.length,
            signedCurvatureFromIcdTurn(unit.turn, unit.startRadius),
            signedCurvatureFromIcdTurn(unit.turn, unit.endRadius));
    default:
        return AlignmentChainElementInput::line(unit.length);
    }
}

AlignmentCurveCombinationType classifyCombination(const std::vector<IcdAlignmentUnit>& units)
{
    for (const auto& unit : units) {
        if (unit.type != IcdUnitType::PartialSpiralLargeToSmall
            && unit.type != IcdUnitType::PartialSpiralSmallToLarge) {
            continue;
        }

        const auto startCurvature = signedCurvatureFromIcdTurn(unit.turn, unit.startRadius);
        const auto endCurvature = signedCurvatureFromIcdTurn(unit.turn, unit.endRadius);
        if (startCurvature * endCurvature < 0.0) {
            return AlignmentCurveCombinationType::SCurve;
        }
        return AlignmentCurveCombinationType::OvalCurve;
    }
    return AlignmentCurveCombinationType::ElementChain;
}

IcdAlignmentUnit unitFromElement(const HorizontalAlignmentElement& element)
{
    IcdAlignmentUnit unit;
    switch (element.type) {
    case AlignmentElementType::Line:
        unit.type = IcdUnitType::Line;
        unit.length = element.length;
        break;
    case AlignmentElementType::CircularArc:
        unit.type = IcdUnitType::CircularArc;
        unit.radius = element.radius > 0.0 ? element.radius : radiusFromCurvature(element.startCurvature);
        unit.length = element.length;
        unit.turn = icdTurnFromCurvature(element.startCurvature);
        break;
    case AlignmentElementType::SpiralIn:
        unit.type = IcdUnitType::SpiralIn;
        unit.endRadius = element.radius > 0.0 ? element.radius : radiusFromCurvature(element.endCurvature);
        unit.length = element.length;
        unit.a = std::sqrt(unit.endRadius * element.length);
        unit.turn = icdTurnFromCurvature(element.endCurvature);
        break;
    case AlignmentElementType::SpiralOut:
        unit.type = IcdUnitType::SpiralOut;
        unit.startRadius = element.radius > 0.0 ? element.radius : radiusFromCurvature(element.startCurvature);
        unit.length = element.length;
        unit.a = std::sqrt(unit.startRadius * element.length);
        unit.turn = icdTurnFromCurvature(element.startCurvature);
        break;
    case AlignmentElementType::PartialSpiral: {
        const auto startRadius = radiusFromCurvature(element.startCurvature);
        const auto endRadius = radiusFromCurvature(element.endCurvature);
        if (std::fabs(element.startCurvature) <= kEpsilon && std::fabs(element.endCurvature) > kEpsilon) {
            unit.type = IcdUnitType::SpiralIn;
            unit.endRadius = endRadius;
            unit.length = element.length;
            unit.a = std::sqrt(unit.endRadius * element.length);
            unit.turn = icdTurnFromCurvature(element.endCurvature);
            break;
        }
        if (std::fabs(element.endCurvature) <= kEpsilon && std::fabs(element.startCurvature) > kEpsilon) {
            unit.type = IcdUnitType::SpiralOut;
            unit.startRadius = startRadius;
            unit.length = element.length;
            unit.a = std::sqrt(unit.startRadius * element.length);
            unit.turn = icdTurnFromCurvature(element.startCurvature);
            break;
        }
        unit.type = std::fabs(element.endCurvature) > std::fabs(element.startCurvature)
            ? IcdUnitType::PartialSpiralLargeToSmall
            : IcdUnitType::PartialSpiralSmallToLarge;
        unit.startRadius = startRadius;
        unit.endRadius = endRadius;
        unit.length = element.length;
        const auto curvatureDelta = std::fabs(element.endCurvature - element.startCurvature);
        unit.a = curvatureDelta <= kEpsilon ? 0.0 : std::sqrt(element.length / curvatureDelta);
        unit.turn = icdTurnFromCurvature(std::fabs(element.endCurvature) > kEpsilon ? element.endCurvature : element.startCurvature);
        break;
    }
    default:
        break;
    }
    return unit;
}

} // namespace

IcdAlignmentReadResult IcdAlignmentFile::read(const std::wstring& path) const
{
    IcdAlignmentReadResult result;
    std::ifstream stream{std::filesystem::path(path)};
    if (!stream) {
        result.errorMessage = L"无法打开 ICD 文件。";
        return result;
    }

    std::vector<std::vector<std::string>> records;
    std::string line;
    while (std::getline(stream, line)) {
        auto stripped = stripComment(line);
        if (stripped.empty()) {
            continue;
        }
        records.push_back(splitFields(stripped));
    }

    if (records.size() < 3) {
        result.errorMessage = L"ICD 文件记录不足。";
        return result;
    }

    try {
        result.document.startStationText = widenAscii(records[0].front());
        result.document.startStation = parseStationValue(records[0].front());

        if (records[1].size() < 3) {
            result.errorMessage = L"ICD 起点坐标记录无效。";
            return result;
        }
        result.document.startPoint = AlignmentPoint2d{parseDouble(records[1][0]), parseDouble(records[1][1])};
        result.document.startHeading = parseDouble(records[1][2]);

        for (std::size_t i = 2; i < records.size(); ++i) {
            if (isTerminator(records[i])) {
                result.succeeded = true;
                return result;
            }
            result.document.units.push_back(parseUnit(records[i]));
        }
    } catch (const std::exception& error) {
        result.errorMessage = L"读取 ICD 文件失败: " + widenAscii(error.what());
        return result;
    }

    result.errorMessage = L"ICD 文件缺少结束符。";
    return result;
}

bool IcdAlignmentFile::write(const std::wstring& path, const IcdAlignmentDocument& document, std::wstring& errorMessage) const
{
    std::ofstream stream{std::filesystem::path(path), std::ios::trunc};
    if (!stream) {
        errorMessage = L"无法创建 ICD 文件。";
        return false;
    }

    stream << narrowAscii(document.startStationText.empty()
            ? widenAscii(formatDouble(document.startStation))
            : document.startStationText)
           << "\n";
    stream << formatDouble(document.startPoint.x)
           << ","
           << formatDouble(document.startPoint.y)
           << ","
           << formatDouble(document.startHeading)
           << "\n";

    for (const auto& unit : document.units) {
        switch (unit.type) {
        case IcdUnitType::Line:
            stream << "1," << formatDouble(unit.length);
            if (unit.hasEndHeading) {
                stream << "," << formatDouble(unit.endHeading);
            }
            stream << "\n";
            break;
        case IcdUnitType::CircularArc:
            stream << "2," << formatDouble(unit.radius) << "," << formatDouble(unit.length) << "," << unit.turn << "\n";
            break;
        case IcdUnitType::SpiralIn:
            stream << "3," << formatDouble(unit.a) << "," << formatDouble(unit.endRadius) << "," << unit.turn << "\n";
            break;
        case IcdUnitType::SpiralOut:
            stream << "4," << formatDouble(unit.a) << "," << formatDouble(unit.startRadius) << "," << unit.turn << "\n";
            break;
        case IcdUnitType::PartialSpiralLargeToSmall:
            stream << "5," << formatDouble(unit.a) << "," << formatDouble(unit.startRadius) << "," << formatDouble(unit.endRadius) << "," << unit.turn << "\n";
            break;
        case IcdUnitType::PartialSpiralSmallToLarge:
            stream << "6," << formatDouble(unit.a) << "," << formatDouble(unit.startRadius) << "," << formatDouble(unit.endRadius) << "," << unit.turn << "\n";
            break;
        default:
            errorMessage = L"ICD 单元类型不受支持。";
            return false;
        }
    }
    stream << "0,0,0\n";
    return true;
}

IcdAlignmentDocument IcdAlignmentFile::documentFromAlignment(const HorizontalAlignment& alignment) const
{
    IcdAlignmentDocument document;
    if (!alignment.elements.empty()) {
        document.startStation = alignment.elements.front().startStation;
        document.startStationText = widenAscii(formatDouble(document.startStation));
        document.startPoint = icdEngineeringPointFromCadPoint(alignment.elements.front().start);
        document.startHeading = icdEngineeringAngleFromCadHeading(alignment.elements.front().startHeading);
    }

    document.units.reserve(alignment.elements.size());
    for (const auto& element : alignment.elements) {
        document.units.push_back(unitFromElement(element));
    }
    return document;
}

HorizontalAlignmentBuildResult IcdAlignmentFile::alignmentFromDocument(
    const IcdAlignmentDocument& document,
    const RoadCenterlineProperties& properties) const
{
    AlignmentElementChainInput input;
    input.properties = properties;
    input.combinationType = classifyCombination(document.units);
    input.startPoint = cadPointFromIcdEngineeringPoint(document.startPoint);
    input.startHeading = cadHeadingFromIcdEngineeringAngle(document.startHeading);
    input.startStation = document.startStation;
    input.elements.reserve(document.units.size());
    for (const auto& unit : document.units) {
        input.elements.push_back(chainElementFromUnit(unit));
    }

    AlignmentElementChainBuilder builder;
    return builder.build(input);
}

} // namespace roadproto::domain::alignment
