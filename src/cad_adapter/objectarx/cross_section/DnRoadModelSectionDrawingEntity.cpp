#include "cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbcolor.h"
#include "dbproxy.h"
#include "rxregsvc.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <utility>

using roadproto::domain::cross_section::SectionDrawingComponentTypeSelection;
using roadproto::domain::cross_section::SectionDrawingConfigData;
using roadproto::domain::cross_section::SectionDrawingConfigRules;
using roadproto::domain::cross_section::SectionPavementLayerConfigRow;
using roadproto::domain::cross_section::SubgradeComponentType;
using roadproto::domain::cross_section::SubgradeSide;
using roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingData;
using roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingFace;
using roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingLine;
using roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingPoint;

ACRX_DXF_DEFINE_MEMBERS(
    DnRoadModelSectionDrawingEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNROADMODELSECTIONDRAWINGENTITY,
    "RoadProto Road Model Section Drawing");

namespace {

constexpr Adesk::Int16 kEntityVersion = 4;
constexpr Adesk::Int32 kMaxLines = 10000;
constexpr Adesk::Int32 kMaxFaces = 10000;
constexpr Adesk::Int32 kMaxPointsPerItem = 1000;
constexpr Adesk::Int32 kMaxConfigRows = 10000;
constexpr Adesk::Int32 kMaxConfigComponents = 1000;
constexpr double kPi = 3.14159265358979323846;
constexpr double kStationLabelBand = 4.0;
constexpr double kTextHeight = 2.0;
constexpr Adesk::UInt8 kOpaqueTransparencyAlpha = 255;

std::wstring readWideString(AcDbDwgFiler* filer)
{
    ACHAR* value = nullptr;
    filer->readString(&value);
    std::wstring result = value == nullptr ? L"" : value;
    acutDelString(value);
    return result;
}

void writeWideString(AcDbDwgFiler* filer, const std::wstring& value)
{
    filer->writeString(value.c_str());
}

bool canWriteInt32(std::size_t value)
{
    return value <= static_cast<std::size_t>(std::numeric_limits<Adesk::Int32>::max());
}

bool readCount(AcDbDwgFiler* filer, Adesk::Int32 maxValue, Adesk::Int32& count)
{
    count = 0;
    filer->readInt32(&count);
    return filer->filerStatus() == Acad::eOk && count >= 0 && count <= maxValue;
}

bool isValidSubgradeSide(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(SubgradeSide::Left)
        || value == static_cast<Adesk::Int32>(SubgradeSide::Right);
}

bool isValidSubgradeComponentType(Adesk::Int32 value)
{
    return value >= static_cast<Adesk::Int32>(SubgradeComponentType::Median)
        && value <= static_cast<Adesk::Int32>(SubgradeComponentType::CurbStrip);
}

bool validateSectionDrawingConfig(const SectionDrawingConfigData& config)
{
    if (!canWriteInt32(config.pavementRows.size())
        || config.pavementRows.size() > static_cast<std::size_t>(kMaxConfigRows)) {
        return false;
    }

    for (const auto& row : config.pavementRows) {
        if (!std::isfinite(row.startStation)
            || !std::isfinite(row.endStation)
            || !canWriteInt32(row.componentTypes.size())
            || row.componentTypes.size() > static_cast<std::size_t>(kMaxConfigComponents)) {
            return false;
        }

        for (const auto& component : row.componentTypes) {
            const auto side = static_cast<Adesk::Int32>(component.side);
            const auto componentType = static_cast<Adesk::Int32>(component.componentType);
            if (!isValidSubgradeSide(side) || !isValidSubgradeComponentType(componentType)) {
                return false;
            }
        }
    }

    return true;
}

void writeSectionDrawingConfig(AcDbDwgFiler* filer, const SectionDrawingConfigData& config)
{
    filer->writeInt32(static_cast<Adesk::Int32>(config.version));
    writeWideString(filer, config.configPath);
    filer->writeInt32(static_cast<Adesk::Int32>(config.pavementRows.size()));
    for (const auto& row : config.pavementRows) {
        filer->writeDouble(row.startStation);
        filer->writeDouble(row.endStation);
        filer->writeInt32(static_cast<Adesk::Int32>(row.componentTypes.size()));
        for (const auto& component : row.componentTypes) {
            filer->writeInt32(static_cast<Adesk::Int32>(component.side));
            filer->writeInt32(static_cast<Adesk::Int32>(component.componentType));
        }
        writeWideString(filer, row.templateHandle);
        writeWideString(filer, row.templateName);
    }
}

bool readSectionDrawingConfig(AcDbDwgFiler* filer, SectionDrawingConfigData& config, std::wstring& errorMessage)
{
    SectionDrawingConfigData data;
    Adesk::Int32 version = 1;
    filer->readInt32(&version);
    data.version = static_cast<int>(version);
    data.configPath = readWideString(filer);

    Adesk::Int32 rowCount = 0;
    if (!readCount(filer, kMaxConfigRows, rowCount)) {
        return false;
    }
    data.pavementRows.reserve(static_cast<std::size_t>(rowCount));
    for (Adesk::Int32 i = 0; i < rowCount; ++i) {
        SectionPavementLayerConfigRow row;
        filer->readDouble(&row.startStation);
        filer->readDouble(&row.endStation);

        Adesk::Int32 componentCount = 0;
        if (!readCount(filer, kMaxConfigComponents, componentCount)) {
            return false;
        }
        row.componentTypes.reserve(static_cast<std::size_t>(componentCount));
        for (Adesk::Int32 j = 0; j < componentCount; ++j) {
            Adesk::Int32 side = 0;
            Adesk::Int32 componentType = 0;
            filer->readInt32(&side);
            filer->readInt32(&componentType);
            if (!isValidSubgradeSide(side) || !isValidSubgradeComponentType(componentType)) {
                return false;
            }
            row.componentTypes.push_back(
                SectionDrawingComponentTypeSelection{
                    static_cast<SubgradeSide>(side),
                    static_cast<SubgradeComponentType>(componentType)});
        }
        row.templateHandle = readWideString(filer);
        row.templateName = readWideString(filer);
        data.pavementRows.push_back(std::move(row));
    }

    if (!SectionDrawingConfigRules::normalize(data, errorMessage)) {
        return false;
    }
    if (!validateSectionDrawingConfig(data)) {
        return false;
    }
    config = std::move(data);
    return true;
}

bool isFinitePoint(const RoadModelSectionDrawingPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool isFiniteCadPoint(const AcGePoint3d& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool areAxesUsable(const AcGeVector3d& xAxis, const AcGeVector3d& yAxis)
{
    return xAxis.length() > 1.0e-9
        && yAxis.length() > 1.0e-9
        && xAxis.crossProduct(yAxis).length() > 1.0e-9;
}

bool validateDrawingData(const RoadModelSectionDrawingData& data)
{
    if (!isFiniteCadPoint(data.insertionPoint)
        || !std::isfinite(data.station)
        || !std::isfinite(data.width)
        || !std::isfinite(data.height)
        || data.width <= 0.0
        || data.height <= kStationLabelBand
        || !canWriteInt32(data.lines.size())
        || !canWriteInt32(data.faces.size())
        || data.lines.size() > static_cast<std::size_t>(kMaxLines)
        || data.faces.size() > static_cast<std::size_t>(kMaxFaces)) {
        return false;
    }

    for (const auto& line : data.lines) {
        if (!canWriteInt32(line.points.size()) || line.points.size() > static_cast<std::size_t>(kMaxPointsPerItem)) {
            return false;
        }
        for (const auto& point : line.points) {
            if (!isFinitePoint(point)) {
                return false;
            }
        }
    }

    for (const auto& face : data.faces) {
        if (!std::isfinite(face.hatchAngle)
            || !std::isfinite(face.hatchScale)
            || !canWriteInt32(face.points.size())
            || face.points.size() < 3
            || face.points.size() > static_cast<std::size_t>(kMaxPointsPerItem)) {
            return false;
        }
        for (const auto& point : face.points) {
            if (!isFinitePoint(point)) {
                return false;
            }
        }
    }

    return true;
}

AcCmEntityColor colorFromRgb(int r, int g, int b)
{
    AcCmEntityColor result;
    result.setRGB(
        static_cast<Adesk::UInt8>(std::clamp(r, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(g, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(b, 0, 255)));
    return result;
}

AcGePoint3d sectionPoint(
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    double x,
    double y)
{
    return origin + xAxis * x + yAxis * y;
}

struct FaceGripIndex {
    std::size_t faceIndex = 0;
    std::size_t pointIndex = 0;
};

std::optional<FaceGripIndex> faceGripIndex(int gripIndex, const RoadModelSectionDrawingData& data)
{
    if (gripIndex < 0) {
        return std::nullopt;
    }
    auto remaining = static_cast<std::size_t>(gripIndex);
    for (std::size_t faceIndex = 0; faceIndex < data.faces.size(); ++faceIndex) {
        const auto count = data.faces[faceIndex].points.size();
        if (remaining < count) {
            return FaceGripIndex{faceIndex, remaining};
        }
        remaining -= count;
    }
    return std::nullopt;
}

AcGeVector3d textNormal(const AcGeVector3d& xAxis, const AcGeVector3d& yAxis)
{
    auto normal = xAxis.crossProduct(yAxis);
    return normal.length() <= 1.0e-9 ? AcGeVector3d::kZAxis : normal.normal();
}

AcGeVector3d textDirection(const AcGeVector3d& xAxis)
{
    auto direction = xAxis;
    return direction.length() <= 1.0e-9 ? AcGeVector3d::kXAxis : direction.normal();
}

double estimateTextWidth(const std::wstring& text, double height)
{
    double width = 0.0;
    for (const auto character : text) {
        width += character <= 0x7F ? height * 0.58 : height;
    }
    return width;
}

struct LocalPoint {
    double x = 0.0;
    double y = 0.0;
};

LocalPoint makeLocalPoint(double x, double y)
{
    return LocalPoint{x, y};
}

LocalPoint toLocalPoint(const RoadModelSectionDrawingPoint& point)
{
    return LocalPoint{point.x, point.y};
}

double dot(const LocalPoint& first, const LocalPoint& second)
{
    return first.x * second.x + first.y * second.y;
}

LocalPoint operator+(const LocalPoint& first, const LocalPoint& second)
{
    return LocalPoint{first.x + second.x, first.y + second.y};
}

LocalPoint operator-(const LocalPoint& first, const LocalPoint& second)
{
    return LocalPoint{first.x - second.x, first.y - second.y};
}

LocalPoint operator*(const LocalPoint& point, double scale)
{
    return LocalPoint{point.x * scale, point.y * scale};
}

double length(const LocalPoint& point)
{
    return std::hypot(point.x, point.y);
}

LocalPoint normalized(const LocalPoint& point)
{
    const auto pointLength = length(point);
    return pointLength <= 1.0e-9 ? LocalPoint{1.0, 0.0} : LocalPoint{point.x / pointLength, point.y / pointLength};
}

LocalPoint perpendicular(const LocalPoint& point)
{
    return LocalPoint{-point.y, point.x};
}

void drawLocalLine(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const LocalPoint& start,
    const LocalPoint& end)
{
    if (worldDraw == nullptr || length(end - start) <= 1.0e-9) {
        return;
    }

    AcGePoint3d points[2] = {
        sectionPoint(origin, xAxis, yAxis, start.x, start.y),
        sectionPoint(origin, xAxis, yAxis, end.x, end.y)};
    worldDraw->geometry().polyline(2, points);
}

bool pointInPolygon(const LocalPoint& point, const std::vector<LocalPoint>& polygon)
{
    bool inside = false;
    for (std::size_t i = 0, j = polygon.empty() ? 0 : polygon.size() - 1; i < polygon.size(); j = i++) {
        const auto& current = polygon[i];
        const auto& previous = polygon[j];
        const bool crosses = (current.y > point.y) != (previous.y > point.y);
        if (crosses) {
            const auto x = (previous.x - current.x) * (point.y - current.y) / (previous.y - current.y) + current.x;
            if (point.x < x) {
                inside = !inside;
            }
        }
    }
    return inside;
}

void drawClippedHatchLine(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const std::vector<LocalPoint>& polygon,
    const LocalPoint& direction,
    const LocalPoint& normal,
    double offset)
{
    std::vector<LocalPoint> intersections;
    for (std::size_t i = 0; i < polygon.size(); ++i) {
        const auto& first = polygon[i];
        const auto& second = polygon[(i + 1) % polygon.size()];
        const auto firstDistance = dot(first, normal) - offset;
        const auto secondDistance = dot(second, normal) - offset;
        if (std::fabs(firstDistance) <= 1.0e-9) {
            intersections.push_back(first);
        }
        if (firstDistance * secondDistance < 0.0) {
            const auto t = firstDistance / (firstDistance - secondDistance);
            intersections.push_back(first + (second - first) * t);
        }
    }
    if (intersections.size() < 2) {
        return;
    }

    std::sort(intersections.begin(), intersections.end(), [&](const auto& first, const auto& second) {
        return dot(first, direction) < dot(second, direction);
    });
    drawLocalLine(worldDraw, origin, xAxis, yAxis, intersections.front(), intersections.back());
}

void drawHatchFamily(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const std::vector<LocalPoint>& polygon,
    LocalPoint direction,
    double spacing)
{
    if (polygon.empty() || spacing <= 1.0e-9) {
        return;
    }

    direction = normalized(direction);
    const auto normal = perpendicular(direction);
    auto minDistance = dot(polygon.front(), normal);
    auto maxDistance = minDistance;
    for (const auto& point : polygon) {
        minDistance = std::min(minDistance, dot(point, normal));
        maxDistance = std::max(maxDistance, dot(point, normal));
    }

    int drawn = 0;
    for (auto offset = minDistance - spacing; offset <= maxDistance + spacing && drawn < 20000; offset += spacing, ++drawn) {
        drawClippedHatchLine(worldDraw, origin, xAxis, yAxis, polygon, direction, normal, offset);
    }
}

double safeHatchScale(double hatchScale)
{
    return std::isfinite(hatchScale) && hatchScale > 0.0 ? hatchScale : 1.0;
}

LocalPoint hatchDirectionFromAngle(double hatchAngle)
{
    const auto angleRadians = (std::isfinite(hatchAngle) ? hatchAngle : 0.0) * kPi / 180.0;
    return normalized(makeLocalPoint(std::cos(angleRadians), std::sin(angleRadians)));
}

std::vector<LocalPoint> facePolygon(const RoadModelSectionDrawingFace& face)
{
    std::vector<LocalPoint> polygon;
    polygon.reserve(face.points.size());
    for (const auto& point : face.points) {
        polygon.push_back(toLocalPoint(point));
    }
    return polygon;
}

void drawFaceFill(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const RoadModelSectionDrawingFace& face)
{
    if (face.points.size() < 3) {
        return;
    }

    worldDraw->subEntityTraits().setTrueColor(colorFromRgb(face.colorR, face.colorG, face.colorB));
    worldDraw->subEntityTraits().setTransparency(AcCmTransparency(static_cast<Adesk::UInt8>(90)));
    worldDraw->subEntityTraits().setFillType(kAcGiFillAlways);

    std::vector<AcGePoint3d> geometry;
    for (const auto& point : face.points) {
        geometry.push_back(sectionPoint(origin, xAxis, yAxis, point.x, point.y));
    }
    worldDraw->geometry().polygon(static_cast<Adesk::Int32>(geometry.size()), geometry.data());
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    worldDraw->subEntityTraits().setTransparency(AcCmTransparency(kOpaqueTransparencyAlpha));
}

void drawFaceHatch(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const RoadModelSectionDrawingFace& face)
{
    if (face.hatchPattern.empty() || face.hatchPattern == L"SOLID") {
        return;
    }

    auto polygon = facePolygon(face);
    if (polygon.size() < 3) {
        return;
    }

    worldDraw->subEntityTraits().setTrueColor(colorFromRgb(face.colorR, face.colorG, face.colorB));
    worldDraw->subEntityTraits().setTransparency(AcCmTransparency(kOpaqueTransparencyAlpha));
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);

    const auto spacing = std::max(0.25, 0.45 * safeHatchScale(face.hatchScale));
    if (face.hatchPattern == L"DOTS") {
        auto minX = polygon.front().x;
        auto maxX = polygon.front().x;
        auto minY = polygon.front().y;
        auto maxY = polygon.front().y;
        for (const auto& point : polygon) {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }
        const auto dotSize = std::max(0.05, spacing * 0.12);
        for (auto x = minX; x <= maxX; x += spacing) {
            for (auto y = minY; y <= maxY; y += spacing) {
                const auto point = makeLocalPoint(x, y);
                if (pointInPolygon(point, polygon)) {
                    drawLocalLine(
                        worldDraw,
                        origin,
                        xAxis,
                        yAxis,
                        makeLocalPoint(x - dotSize, y),
                        makeLocalPoint(x + dotSize, y));
                }
            }
        }
        return;
    }

    const auto primaryDirection = hatchDirectionFromAngle(face.hatchAngle);
    drawHatchFamily(worldDraw, origin, xAxis, yAxis, polygon, primaryDirection, spacing);
    if (face.hatchPattern == L"CROSS"
        || face.hatchPattern == L"ANSI32"
        || face.hatchPattern == L"ANSI37"
        || face.hatchPattern == L"ANSI38"
        || face.hatchPattern == L"GRAVEL"
        || face.hatchPattern == L"EARTH"
        || face.hatchPattern == L"AR-CONC") {
        drawHatchFamily(worldDraw, origin, xAxis, yAxis, polygon, perpendicular(primaryDirection), spacing);
    }
    if (face.hatchPattern == L"GRAVEL" || face.hatchPattern == L"EARTH" || face.hatchPattern == L"AR-CONC") {
        drawHatchFamily(worldDraw, origin, xAxis, yAxis, polygon, hatchDirectionFromAngle(face.hatchAngle + 45.0), spacing * 1.4);
    }
}

void drawLinework(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const RoadModelSectionDrawingLine& line)
{
    if (line.points.size() < 2) {
        return;
    }

    worldDraw->subEntityTraits().setTrueColor(colorFromRgb(line.colorR, line.colorG, line.colorB));
    worldDraw->subEntityTraits().setTransparency(AcCmTransparency(kOpaqueTransparencyAlpha));
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    for (std::size_t i = 1; i < line.points.size(); ++i) {
        drawLocalLine(
            worldDraw,
            origin,
            xAxis,
            yAxis,
            toLocalPoint(line.points[i - 1]),
            toLocalPoint(line.points[i]));
    }
}

void setWhiteFrameAndStationLabelTraits(AcGiWorldDraw* worldDraw)
{
    worldDraw->subEntityTraits().setColor(7);
    worldDraw->subEntityTraits().setTransparency(AcCmTransparency(kOpaqueTransparencyAlpha));
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
}

void drawFrame(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    double width,
    double height)
{
    setWhiteFrameAndStationLabelTraits(worldDraw);
    AcGePoint3d frame[5] = {
        sectionPoint(origin, xAxis, yAxis, 0.0, kStationLabelBand),
        sectionPoint(origin, xAxis, yAxis, width, kStationLabelBand),
        sectionPoint(origin, xAxis, yAxis, width, height),
        sectionPoint(origin, xAxis, yAxis, 0.0, height),
        sectionPoint(origin, xAxis, yAxis, 0.0, kStationLabelBand)};
    worldDraw->geometry().polyline(5, frame);
}

void drawStationLabel(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    double width,
    const std::wstring& stationLabel)
{
    if (stationLabel.empty()) {
        return;
    }

    AcGiTextStyle textStyle;
    textStyle.setTextSize(kTextHeight);
    textStyle.setXScale(1.0);
    textStyle.setObliquingAngle(0.0);
    const auto textX = std::max(0.0, (width - estimateTextWidth(stationLabel, kTextHeight)) * 0.5);
    setWhiteFrameAndStationLabelTraits(worldDraw);
    worldDraw->geometry().text(
        sectionPoint(origin, xAxis, yAxis, textX, 0.75),
        textNormal(xAxis, yAxis),
        textDirection(xAxis),
        stationLabel.c_str(),
        static_cast<Adesk::Int32>(stationLabel.size()),
        Adesk::kFalse,
        textStyle);
}

} // namespace

DnRoadModelSectionDrawingEntity::DnRoadModelSectionDrawingEntity()
    : xAxis_(AcGeVector3d::kXAxis)
    , yAxis_(AcGeVector3d::kYAxis)
{
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::setDrawingData(const RoadModelSectionDrawingData& data)
{
    assertWriteEnabled();
    auto updated = data;
    std::wstring errorMessage;
    if (!SectionDrawingConfigRules::normalize(updated.config, errorMessage)
        || !validateSectionDrawingConfig(updated.config)
        || !validateDrawingData(updated)) {
        return Acad::eInvalidInput;
    }

    data_ = std::move(updated);
    xAxis_ = AcGeVector3d::kXAxis;
    yAxis_ = AcGeVector3d::kYAxis;
    recordGraphicsModified(true);
    return Acad::eOk;
}

const RoadModelSectionDrawingData& DnRoadModelSectionDrawingEntity::drawingData() const
{
    assertReadEnabled();
    return data_;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::setSectionDrawingConfig(const SectionDrawingConfigData& config)
{
    assertWriteEnabled();
    auto updated = data_;
    updated.config = config;
    std::wstring errorMessage;
    if (!SectionDrawingConfigRules::normalize(updated.config, errorMessage)
        || !validateSectionDrawingConfig(updated.config)
        || !validateDrawingData(updated)) {
        return Acad::eInvalidInput;
    }

    data_ = std::move(updated);
    recordGraphicsModified(true);
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::replaceFaces(std::vector<RoadModelSectionDrawingFace> faces)
{
    assertWriteEnabled();
    auto updated = data_;
    updated.faces = std::move(faces);
    if (!validateDrawingData(updated)) {
        return Acad::eInvalidInput;
    }

    data_ = std::move(updated);
    recordGraphicsModified(true);
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::dwgInFields(AcDbDwgFiler* filer)
{
    assertWriteEnabled();
    const auto status = AcDbEntity::dwgInFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    Adesk::Int16 version = 0;
    filer->readInt16(&version);
    if (version < 1 || version > kEntityVersion) {
        return Acad::eMakeMeProxy;
    }

    RoadModelSectionDrawingData data;
    data.roadModelHandle = readWideString(filer);
    data.roadCenterlineHandle = readWideString(filer);
    data.stationLabel = readWideString(filer);
    filer->readDouble(&data.station);
    filer->readPoint3d(&data.insertionPoint);
    filer->readDouble(&data.width);
    filer->readDouble(&data.height);

    Adesk::Int32 faceCount = 0;
    if (!readCount(filer, kMaxFaces, faceCount)) {
        return Acad::eInvalidInput;
    }
    data.faces.reserve(static_cast<std::size_t>(faceCount));
    for (Adesk::Int32 i = 0; i < faceCount; ++i) {
        RoadModelSectionDrawingFace face;
        if (version >= 2) {
            face.layerName = readWideString(filer);
        }
        if (version >= 3) {
            face.componentName = readWideString(filer);
        }
        if (version >= 4) {
            face.faceId = readWideString(filer);
            face.sourceTemplateHandle = readWideString(filer);
            Adesk::Int32 rowIndex = -1;
            filer->readInt32(&rowIndex);
            face.sourceConfigRowIndex = static_cast<int>(rowIndex);
            Adesk::Int32 manualEdited = 0;
            filer->readInt32(&manualEdited);
            face.manualEdited = manualEdited != 0;
        }
        Adesk::Int32 colorValue = 0;
        filer->readInt32(&colorValue);
        face.colorR = static_cast<int>(colorValue);
        filer->readInt32(&colorValue);
        face.colorG = static_cast<int>(colorValue);
        filer->readInt32(&colorValue);
        face.colorB = static_cast<int>(colorValue);
        face.hatchPattern = readWideString(filer);
        filer->readDouble(&face.hatchAngle);
        filer->readDouble(&face.hatchScale);

        Adesk::Int32 pointCount = 0;
        if (!readCount(filer, kMaxPointsPerItem, pointCount)) {
            return Acad::eInvalidInput;
        }
        face.points.reserve(static_cast<std::size_t>(pointCount));
        for (Adesk::Int32 j = 0; j < pointCount; ++j) {
            RoadModelSectionDrawingPoint point;
            filer->readDouble(&point.x);
            filer->readDouble(&point.y);
            face.points.push_back(point);
        }
        data.faces.push_back(std::move(face));
    }

    Adesk::Int32 lineCount = 0;
    if (!readCount(filer, kMaxLines, lineCount)) {
        return Acad::eInvalidInput;
    }
    data.lines.reserve(static_cast<std::size_t>(lineCount));
    for (Adesk::Int32 i = 0; i < lineCount; ++i) {
        RoadModelSectionDrawingLine line;
        Adesk::Int32 value = 0;
        filer->readInt32(&value);
        line.kind = static_cast<int>(value);
        filer->readInt32(&value);
        line.colorR = static_cast<int>(value);
        filer->readInt32(&value);
        line.colorG = static_cast<int>(value);
        filer->readInt32(&value);
        line.colorB = static_cast<int>(value);

        Adesk::Int32 pointCount = 0;
        if (!readCount(filer, kMaxPointsPerItem, pointCount)) {
            return Acad::eInvalidInput;
        }
        line.points.reserve(static_cast<std::size_t>(pointCount));
        for (Adesk::Int32 j = 0; j < pointCount; ++j) {
            RoadModelSectionDrawingPoint point;
            filer->readDouble(&point.x);
            filer->readDouble(&point.y);
            line.points.push_back(point);
        }
        data.lines.push_back(std::move(line));
    }

    if (version >= 4) {
        std::wstring configError;
        if (!readSectionDrawingConfig(filer, data.config, configError)) {
            return Acad::eInvalidInput;
        }
    }

    AcGeVector3d xAxis = AcGeVector3d::kXAxis;
    AcGeVector3d yAxis = AcGeVector3d::kYAxis;
    filer->readVector3d(&xAxis);
    filer->readVector3d(&yAxis);

    const auto finalStatus = filer->filerStatus();
    if (finalStatus != Acad::eOk) {
        return finalStatus;
    }
    if (!validateSectionDrawingConfig(data.config) || !validateDrawingData(data) || !areAxesUsable(xAxis, yAxis)) {
        return Acad::eInvalidInput;
    }

    xAxis_ = xAxis;
    yAxis_ = yAxis;
    data_ = std::move(data);
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    const auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }
    if (!validateSectionDrawingConfig(data_.config)
        || !validateDrawingData(data_)
        || !areAxesUsable(xAxis_, yAxis_)) {
        return Acad::eInvalidInput;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, data_.roadModelHandle);
    writeWideString(filer, data_.roadCenterlineHandle);
    writeWideString(filer, data_.stationLabel);
    filer->writeDouble(data_.station);
    filer->writePoint3d(data_.insertionPoint);
    filer->writeDouble(data_.width);
    filer->writeDouble(data_.height);

    filer->writeInt32(static_cast<Adesk::Int32>(data_.faces.size()));
    for (const auto& face : data_.faces) {
        writeWideString(filer, face.layerName);
        writeWideString(filer, face.componentName);
        writeWideString(filer, face.faceId);
        writeWideString(filer, face.sourceTemplateHandle);
        filer->writeInt32(static_cast<Adesk::Int32>(face.sourceConfigRowIndex));
        filer->writeInt32(face.manualEdited ? 1 : 0);
        filer->writeInt32(static_cast<Adesk::Int32>(face.colorR));
        filer->writeInt32(static_cast<Adesk::Int32>(face.colorG));
        filer->writeInt32(static_cast<Adesk::Int32>(face.colorB));
        writeWideString(filer, face.hatchPattern);
        filer->writeDouble(face.hatchAngle);
        filer->writeDouble(face.hatchScale);
        filer->writeInt32(static_cast<Adesk::Int32>(face.points.size()));
        for (const auto& point : face.points) {
            filer->writeDouble(point.x);
            filer->writeDouble(point.y);
        }
    }

    filer->writeInt32(static_cast<Adesk::Int32>(data_.lines.size()));
    for (const auto& line : data_.lines) {
        filer->writeInt32(static_cast<Adesk::Int32>(line.kind));
        filer->writeInt32(static_cast<Adesk::Int32>(line.colorR));
        filer->writeInt32(static_cast<Adesk::Int32>(line.colorG));
        filer->writeInt32(static_cast<Adesk::Int32>(line.colorB));
        filer->writeInt32(static_cast<Adesk::Int32>(line.points.size()));
        for (const auto& point : line.points) {
            filer->writeDouble(point.x);
            filer->writeDouble(point.y);
        }
    }

    writeSectionDrawingConfig(filer, data_.config);

    filer->writeVector3d(xAxis_);
    filer->writeVector3d(yAxis_);
    return filer->filerStatus();
}

Adesk::Boolean DnRoadModelSectionDrawingEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr || !validateDrawingData(data_) || !areAxesUsable(xAxis_, yAxis_)) {
        return Adesk::kTrue;
    }

    for (const auto& face : data_.faces) {
        drawFaceFill(worldDraw, data_.insertionPoint, xAxis_, yAxis_, face);
    }
    for (const auto& face : data_.faces) {
        drawFaceHatch(worldDraw, data_.insertionPoint, xAxis_, yAxis_, face);
    }
    for (const auto& line : data_.lines) {
        drawLinework(worldDraw, data_.insertionPoint, xAxis_, yAxis_, line);
    }
    drawFrame(worldDraw, data_.insertionPoint, xAxis_, yAxis_, data_.width, data_.height);
    drawStationLabel(worldDraw, data_.insertionPoint, xAxis_, yAxis_, data_.width, data_.stationLabel);
    return Adesk::kTrue;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    if (!validateDrawingData(data_) || !areAxesUsable(xAxis_, yAxis_)) {
        return Acad::eInvalidExtents;
    }

    extents.addPoint(sectionPoint(data_.insertionPoint, xAxis_, yAxis_, 0.0, 0.0));
    extents.addPoint(sectionPoint(data_.insertionPoint, xAxis_, yAxis_, data_.width, 0.0));
    extents.addPoint(sectionPoint(data_.insertionPoint, xAxis_, yAxis_, data_.width, data_.height));
    extents.addPoint(sectionPoint(data_.insertionPoint, xAxis_, yAxis_, 0.0, data_.height));
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    assertWriteEnabled();
    auto newPoint = data_.insertionPoint;
    auto newXAxis = xAxis_;
    auto newYAxis = yAxis_;
    newPoint.transformBy(transform);
    newXAxis.transformBy(transform);
    newYAxis.transformBy(transform);
    if (!isFiniteCadPoint(newPoint) || !areAxesUsable(newXAxis, newYAxis)) {
        return Acad::eInvalidInput;
    }

    data_.insertionPoint = newPoint;
    xAxis_ = newXAxis;
    yAxis_ = newYAxis;
    recordGraphicsModified(true);
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::subGetGripPoints(
    AcGePoint3dArray& gripPoints,
    AcDbIntArray&,
    AcDbIntArray&) const
{
    assertReadEnabled();
    if (!validateDrawingData(data_) || !areAxesUsable(xAxis_, yAxis_)) {
        return Acad::eOk;
    }

    for (const auto& face : data_.faces) {
        for (const auto& point : face.points) {
            gripPoints.append(sectionPoint(data_.insertionPoint, xAxis_, yAxis_, point.x, point.y));
        }
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadModelSectionDrawingEntity::subMoveGripPointsAt(
    const AcDbIntArray& indices,
    const AcGeVector3d& offset)
{
    assertWriteEnabled();
    if (indices.length() == 0 || offset.isZeroLength() || !validateDrawingData(data_) || !areAxesUsable(xAxis_, yAxis_)) {
        return Acad::eOk;
    }

    const auto localDx = offset.dotProduct(xAxis_) / xAxis_.lengthSqrd();
    const auto localDy = offset.dotProduct(yAxis_) / yAxis_.lengthSqrd();
    if (!std::isfinite(localDx) || !std::isfinite(localDy)) {
        return Acad::eInvalidInput;
    }

    bool moved = false;
    for (int i = 0; i < indices.length(); ++i) {
        const auto grip = faceGripIndex(indices.at(i), data_);
        if (!grip.has_value()) {
            continue;
        }

        auto& face = data_.faces[grip->faceIndex];
        auto& point = face.points[grip->pointIndex];
        const auto nextX = point.x + localDx;
        const auto nextY = point.y + localDy;
        if (!std::isfinite(nextX) || !std::isfinite(nextY)) {
            continue;
        }

        point.x = nextX;
        point.y = nextY;
        face.manualEdited = true;
        moved = true;
    }

    if (moved) {
        recordGraphicsModified(true);
    }
    return Acad::eOk;
}

namespace roadproto::cad_adapter::objectarx {

void initializeRoadModelSectionDrawingEntityClass()
{
    DnRoadModelSectionDrawingEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeRoadModelSectionDrawingEntityClass()
{
    deleteAcRxClass(DnRoadModelSectionDrawingEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
