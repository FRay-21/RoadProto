#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbcolor.h"
#include "dbproxy.h"
#include "rxregsvc.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <utility>
#include <vector>

using roadproto::domain::cross_section::PavementLayerSectionLayer;
using roadproto::domain::cross_section::PavementLayerTemplateData;
using roadproto::domain::cross_section::PavementLayerTemplateDisplayColor;
using roadproto::domain::cross_section::PavementLayerTemplateDisplayMode;
using roadproto::domain::cross_section::PavementLayerTemplateLayer;
using roadproto::domain::cross_section::PavementLayerTemplateRules;
using roadproto::domain::cross_section::PavementLayerType;
using roadproto::domain::cross_section::PavementSubgradeMoistureType;
using roadproto::domain::cross_section::PavementSubgradeSoilGroup;
using roadproto::domain::cross_section::SubgradeSide;
using roadproto::domain::cross_section::pavementSubgradeMoistureTypeCode;
using roadproto::domain::cross_section::pavementSubgradeMoistureTypeFromCode;
using roadproto::domain::cross_section::pavementSubgradeSoilGroupCode;
using roadproto::domain::cross_section::pavementSubgradeSoilGroupFromCode;
using roadproto::domain::cross_section::pavementSurfaceTypeCode;
using roadproto::domain::cross_section::pavementSurfaceTypeFromCode;

ACRX_DXF_DEFINE_MEMBERS(
    DnPavementLayerTemplateEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNPAVEMENTLAYERTEMPLATEENTITY,
    "RoadProto Pavement Layer Template");

namespace {

constexpr Adesk::Int16 kEntityVersion = 5;
constexpr Adesk::Int32 kMaxLayers = 1000;
constexpr double kMinAxisLength = 1.0e-9;
constexpr double kMaxAxisLength = 1.0e9;
constexpr double kMinAxisSine = 1.0e-6;
constexpr double kExtentsPadding = 8.0;
constexpr double kPreviewFillOpacity = 74.0 / 255.0;
constexpr int kPreviewBackgroundR = 21;
constexpr int kPreviewBackgroundG = 26;
constexpr int kPreviewBackgroundB = 32;
constexpr Adesk::UInt8 kOpaqueTransparencyAlpha = 255;
constexpr double kPi = 3.14159265358979323846;

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

bool readBool(AcDbDwgFiler* filer)
{
    Adesk::Int8 value = 0;
    filer->readInt8(&value);
    return value != 0;
}

void writeBool(AcDbDwgFiler* filer, bool value)
{
    filer->writeInt8(value ? 1 : 0);
}

void writeMoistureTypes(
    AcDbDwgFiler* filer,
    const std::vector<PavementSubgradeMoistureType>& values)
{
    filer->writeInt32(static_cast<Adesk::Int32>(values.size()));
    for (const auto value : values) {
        writeWideString(filer, pavementSubgradeMoistureTypeCode(value));
    }
}

std::vector<PavementSubgradeMoistureType> readMoistureTypes(AcDbDwgFiler* filer)
{
    Adesk::Int32 count = 0;
    filer->readInt32(&count);
    std::vector<PavementSubgradeMoistureType> values;
    if (count < 0 || count > kMaxLayers) {
        return values;
    }
    values.reserve(static_cast<std::size_t>(count));
    for (Adesk::Int32 i = 0; i < count; ++i) {
        const auto code = readWideString(filer);
        const auto parsed = pavementSubgradeMoistureTypeFromCode(code);
        if (code == pavementSubgradeMoistureTypeCode(parsed)) {
            values.push_back(parsed);
        }
    }
    return values;
}

void writeSoilGroups(
    AcDbDwgFiler* filer,
    const std::vector<PavementSubgradeSoilGroup>& values)
{
    filer->writeInt32(static_cast<Adesk::Int32>(values.size()));
    for (const auto value : values) {
        writeWideString(filer, pavementSubgradeSoilGroupCode(value));
    }
}

std::vector<PavementSubgradeSoilGroup> readSoilGroups(AcDbDwgFiler* filer)
{
    Adesk::Int32 count = 0;
    filer->readInt32(&count);
    std::vector<PavementSubgradeSoilGroup> values;
    if (count < 0 || count > kMaxLayers) {
        return values;
    }
    values.reserve(static_cast<std::size_t>(count));
    for (Adesk::Int32 i = 0; i < count; ++i) {
        const auto code = readWideString(filer);
        const auto parsed = pavementSubgradeSoilGroupFromCode(code);
        if (code == pavementSubgradeSoilGroupCode(parsed)) {
            values.push_back(parsed);
        }
    }
    return values;
}

bool isFinitePoint(const AcGePoint3d& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool isFiniteVector(const AcGeVector3d& vector)
{
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

bool areAxesUsable(const AcGeVector3d& xAxis, const AcGeVector3d& yAxis)
{
    if (!isFiniteVector(xAxis) || !isFiniteVector(yAxis)) {
        return false;
    }

    const auto xLength = xAxis.length();
    const auto yLength = yAxis.length();
    if (!std::isfinite(xLength) || !std::isfinite(yLength)
        || xLength < kMinAxisLength || yLength < kMinAxisLength
        || xLength > kMaxAxisLength || yLength > kMaxAxisLength) {
        return false;
    }

    const auto crossLength = xAxis.crossProduct(yAxis).length();
    return std::isfinite(crossLength)
        && crossLength / (xLength * yLength) >= kMinAxisSine;
}

void resetDefaultAxes(AcGeVector3d& xAxis, AcGeVector3d& yAxis)
{
    xAxis = AcGeVector3d::kXAxis;
    yAxis = AcGeVector3d::kYAxis;
}

bool isValidPavementLayerTypeValue(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(PavementLayerType::UpperSurface)
        || value == static_cast<Adesk::Int32>(PavementLayerType::MiddleSurface)
        || value == static_cast<Adesk::Int32>(PavementLayerType::LowerSurface)
        || value == static_cast<Adesk::Int32>(PavementLayerType::Base)
        || value == static_cast<Adesk::Int32>(PavementLayerType::Subbase)
        || value == static_cast<Adesk::Int32>(PavementLayerType::Cushion)
        || value == static_cast<Adesk::Int32>(PavementLayerType::AsphaltSeal)
        || value == static_cast<Adesk::Int32>(PavementLayerType::ApproachSlab);
}

Acad::ErrorStatus checkFilerStatus(AcDbDwgFiler* filer)
{
    const auto status = filer == nullptr ? Acad::eInvalidInput : filer->filerStatus();
    if (status != Acad::eOk) {
        return status;
    }
    return Acad::eOk;
}

double drawingScale(const PavementLayerTemplateData& data)
{
    return PavementLayerTemplateRules::isSupportedDisplayScale(data.properties.displayScale)
        ? data.properties.displayScale
        : 10.0;
}

double previewWidth(const PavementLayerTemplateData& data)
{
    return PavementLayerTemplateRules::isSupportedPreviewWidth(data.properties.previewWidth)
        ? data.properties.previewWidth
        : 3.0;
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

AcGeVector3d textDirection(const AcGeVector3d& xAxis)
{
    auto direction = xAxis;
    if (direction.isZeroLength()) {
        return AcGeVector3d::kXAxis;
    }
    direction.normalize();
    return direction;
}

AcGeVector3d textNormal(const AcGeVector3d& xAxis, const AcGeVector3d& yAxis)
{
    auto normal = xAxis.crossProduct(yAxis);
    if (normal.isZeroLength()) {
        return AcGeVector3d::kZAxis;
    }
    normal.normalize();
    return normal;
}

void makePreviewTextStyle(AcGiTextStyle& textStyle, double height)
{
    textStyle.setTextSize(height);
    textStyle.setXScale(1.0);
    textStyle.setObliquingAngle(0.0);
    textStyle.setFont(L"SimSun",
        Adesk::kFalse,
        Adesk::kFalse,
        kChineseSimpCharset,
        Autodesk::AutoCAD::PAL::FontUtils::FontPitch::kDefault,
        Autodesk::AutoCAD::PAL::FontUtils::FontFamily::kDoNotCare);
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

int blendPreviewFillChannel(int source, int background)
{
    const auto blended = background + (std::clamp(source, 0, 255) - background) * kPreviewFillOpacity;
    return static_cast<int>(std::round(blended));
}

AcCmEntityColor pavementLayerStrokeColor(const PavementLayerTemplateDisplayColor& color)
{
    return colorFromRgb(
        static_cast<Adesk::UInt8>(std::clamp(color.r, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.g, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.b, 0, 255)));
}

AcCmEntityColor pavementLayerFillColor(const PavementLayerTemplateDisplayColor& color)
{
    return colorFromRgb(
        blendPreviewFillChannel(color.r, kPreviewBackgroundR),
        blendPreviewFillChannel(color.g, kPreviewBackgroundG),
        blendPreviewFillChannel(color.b, kPreviewBackgroundB));
}

void drawText(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    double x,
    double y,
    const std::wstring& text,
    double height)
{
    if (worldDraw == nullptr || text.empty() || height <= 0.0 || !std::isfinite(height)) {
        return;
    }

    AcGiTextStyle textStyle;
    makePreviewTextStyle(textStyle, height);
    worldDraw->geometry().text(
        sectionPoint(origin, xAxis, yAxis, x, y),
        textNormal(xAxis, yAxis),
        textDirection(xAxis),
        text.c_str(),
        static_cast<Adesk::Int32>(text.size()),
        Adesk::kFalse,
        textStyle);
}

double estimateTextWidth(const std::wstring& text, double height)
{
    if (text.empty() || height <= 0.0 || !std::isfinite(height)) {
        return 0.0;
    }

    double width = 0.0;
    for (const auto character : text) {
        width += character <= 0x7F ? height * 0.58 : height;
    }
    return width;
}

bool sameSectionPoint(
    const roadproto::domain::cross_section::PavementLayerSectionPoint& first,
    const roadproto::domain::cross_section::PavementLayerSectionPoint& second)
{
    constexpr double tolerance = 1.0e-9;
    return std::fabs(first.offset - second.offset) <= tolerance
        && std::fabs(first.elevation - second.elevation) <= tolerance;
}

struct SectionPoint2d {
    double x = 0.0;
    double y = 0.0;
};

SectionPoint2d makeSectionPoint(double x, double y)
{
    return SectionPoint2d{x, y};
}

SectionPoint2d scaledSectionPoint(
    const roadproto::domain::cross_section::PavementLayerSectionPoint& point,
    double scale)
{
    return SectionPoint2d{point.offset * scale, point.elevation * scale};
}

double dot(const SectionPoint2d& first, const SectionPoint2d& second)
{
    return first.x * second.x + first.y * second.y;
}

SectionPoint2d operator+(const SectionPoint2d& first, const SectionPoint2d& second)
{
    return SectionPoint2d{first.x + second.x, first.y + second.y};
}

SectionPoint2d operator-(const SectionPoint2d& first, const SectionPoint2d& second)
{
    return SectionPoint2d{first.x - second.x, first.y - second.y};
}

SectionPoint2d operator*(const SectionPoint2d& point, double scale)
{
    return SectionPoint2d{point.x * scale, point.y * scale};
}

double length(const SectionPoint2d& vector)
{
    return std::hypot(vector.x, vector.y);
}

SectionPoint2d normalized(const SectionPoint2d& vector)
{
    const auto vectorLength = length(vector);
    return vectorLength <= 1.0e-9
        ? SectionPoint2d{1.0, 0.0}
        : SectionPoint2d{vector.x / vectorLength, vector.y / vectorLength};
}

SectionPoint2d perpendicular(const SectionPoint2d& vector)
{
    return SectionPoint2d{-vector.y, vector.x};
}

void drawSectionLine(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const SectionPoint2d& start,
    const SectionPoint2d& end)
{
    if (length(end - start) <= 1.0e-9) {
        return;
    }

    AcGePoint3d line[2] = {
        sectionPoint(origin, xAxis, yAxis, start.x, start.y),
        sectionPoint(origin, xAxis, yAxis, end.x, end.y)};
    worldDraw->geometry().polyline(2, line);
}

bool pointInPolygon(const SectionPoint2d& point, const std::vector<SectionPoint2d>& polygon)
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
    const std::vector<SectionPoint2d>& polygon,
    const SectionPoint2d& direction,
    const SectionPoint2d& normal,
    double offset)
{
    std::vector<SectionPoint2d> intersections;
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
    drawSectionLine(worldDraw, origin, xAxis, yAxis, intersections.front(), intersections.back());
}

void drawHatchFamily(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const std::vector<SectionPoint2d>& polygon,
    SectionPoint2d direction,
    double spacing)
{
    direction = normalized(direction);
    const auto normal = perpendicular(direction);
    auto minDistance = dot(polygon.front(), normal);
    auto maxDistance = minDistance;
    for (const auto& point : polygon) {
        minDistance = std::min(minDistance, dot(point, normal));
        maxDistance = std::max(maxDistance, dot(point, normal));
    }
    for (auto offset = minDistance - spacing; offset <= maxDistance + spacing; offset += spacing) {
        drawClippedHatchLine(worldDraw, origin, xAxis, yAxis, polygon, direction, normal, offset);
    }
}

double safeHatchScale(double hatchScale)
{
    return std::isfinite(hatchScale) && hatchScale > 0.0 ? hatchScale : 1.0;
}

SectionPoint2d hatchDirectionFromAngle(double hatchAngle)
{
    const auto angleRadians = (std::isfinite(hatchAngle) ? hatchAngle : 0.0) * kPi / 180.0;
    return normalized(makeSectionPoint(std::cos(angleRadians), std::sin(angleRadians)));
}

void drawLayerPreviewFill(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const PavementLayerSectionLayer& layer,
    double scale,
    PavementLayerTemplateDisplayMode displayMode)
{
    if (displayMode == PavementLayerTemplateDisplayMode::Hatch) {
        return;
    }

    const auto color = pavementLayerFillColor(layer.color);
    worldDraw->subEntityTraits().setTrueColor(color);
    worldDraw->subEntityTraits().setTransparency(AcCmTransparency(kOpaqueTransparencyAlpha));
    worldDraw->subEntityTraits().setFillType(kAcGiFillAlways);

    AcGePoint3d fillPoints[4] = {
        sectionPoint(origin, xAxis, yAxis, layer.topInner.offset * scale, layer.topInner.elevation * scale),
        sectionPoint(origin, xAxis, yAxis, layer.topOuter.offset * scale, layer.topOuter.elevation * scale),
        sectionPoint(origin, xAxis, yAxis, layer.bottomOuter.offset * scale, layer.bottomOuter.elevation * scale),
        sectionPoint(origin, xAxis, yAxis, layer.bottomInner.offset * scale, layer.bottomInner.elevation * scale)};
    worldDraw->geometry().polygon(4, fillPoints);
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
}

void drawLayerHatchPattern(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const PavementLayerSectionLayer& sectionLayer,
    const PavementLayerTemplateLayer& layer,
    double scale,
    PavementLayerTemplateDisplayMode displayMode)
{
    if (displayMode == PavementLayerTemplateDisplayMode::Color || layer.hatchPattern == L"SOLID") {
        return;
    }

    const auto polygon = std::vector<SectionPoint2d>{
        scaledSectionPoint(sectionLayer.topInner, scale),
        scaledSectionPoint(sectionLayer.topOuter, scale),
        scaledSectionPoint(sectionLayer.bottomOuter, scale),
        scaledSectionPoint(sectionLayer.bottomInner, scale)};
    const auto color = displayMode == PavementLayerTemplateDisplayMode::HatchAndColor
        ? pavementLayerStrokeColor(sectionLayer.color)
        : colorFromRgb(205, 213, 222);
    worldDraw->subEntityTraits().setTrueColor(color);
    worldDraw->subEntityTraits().setTransparency(AcCmTransparency(kOpaqueTransparencyAlpha));
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);

    const auto spacing = std::max(0.35, 0.12 * scale * safeHatchScale(layer.hatchScale));
    if (layer.hatchPattern == L"DOTS") {
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
        const auto dotSize = std::max(0.08, 0.015 * scale);
        for (auto x = minX; x <= maxX; x += spacing) {
            for (auto y = minY; y <= maxY; y += spacing) {
                const auto point = makeSectionPoint(x, y);
                if (pointInPolygon(point, polygon)) {
                    drawSectionLine(
                        worldDraw,
                        origin,
                        xAxis,
                        yAxis,
                        makeSectionPoint(x - dotSize, y),
                        makeSectionPoint(x + dotSize, y));
                }
            }
        }
        return;
    }

    const auto primaryDirection = hatchDirectionFromAngle(layer.hatchAngle);
    drawHatchFamily(worldDraw, origin, xAxis, yAxis, polygon, primaryDirection, spacing);
    if (layer.hatchPattern == L"CROSS"
        || layer.hatchPattern == L"ANSI32"
        || layer.hatchPattern == L"ANSI37"
        || layer.hatchPattern == L"ANSI38"
        || layer.hatchPattern == L"GRAVEL"
        || layer.hatchPattern == L"EARTH"
        || layer.hatchPattern == L"AR-CONC") {
        drawHatchFamily(worldDraw, origin, xAxis, yAxis, polygon, perpendicular(primaryDirection), spacing);
    }
    if (layer.hatchPattern == L"GRAVEL" || layer.hatchPattern == L"EARTH" || layer.hatchPattern == L"AR-CONC") {
        drawHatchFamily(worldDraw, origin, xAxis, yAxis, polygon, hatchDirectionFromAngle(layer.hatchAngle + 45.0), spacing * 1.4);
    }
}

void drawLayerEdge(AcGiWorldDraw* worldDraw, const AcGePoint3d& start, const AcGePoint3d& end)
{
    if ((end - start).length() <= 1.0e-9) {
        return;
    }

    AcGePoint3d line[2] = {start, end};
    worldDraw->geometry().polyline(2, line);
}

void drawLayerEdges(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const PavementLayerSectionLayer& layer,
    double scale,
    bool drawTopEdge)
{
    const auto color = pavementLayerStrokeColor(layer.color);
    worldDraw->subEntityTraits().setTrueColor(color);
    worldDraw->subEntityTraits().setTransparency(AcCmTransparency(kOpaqueTransparencyAlpha));
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    const AcGePoint3d topInner =
        sectionPoint(origin, xAxis, yAxis, layer.topInner.offset * scale, layer.topInner.elevation * scale);
    const AcGePoint3d topOuter =
        sectionPoint(origin, xAxis, yAxis, layer.topOuter.offset * scale, layer.topOuter.elevation * scale);
    const AcGePoint3d bottomOuter =
        sectionPoint(origin, xAxis, yAxis, layer.bottomOuter.offset * scale, layer.bottomOuter.elevation * scale);
    const AcGePoint3d bottomInner =
        sectionPoint(origin, xAxis, yAxis, layer.bottomInner.offset * scale, layer.bottomInner.elevation * scale);

    if (drawTopEdge) {
        drawLayerEdge(worldDraw, topInner, topOuter);
    }
    drawLayerEdge(worldDraw, topOuter, bottomOuter);
    drawLayerEdge(worldDraw, bottomOuter, bottomInner);
    drawLayerEdge(worldDraw, bottomInner, topInner);
}

struct SectionBounds {
    bool initialized = false;
    double minX = 0.0;
    double maxX = 0.0;
    double minY = 0.0;
    double maxY = 0.0;
};

void addBounds(SectionBounds& bounds, double x, double y)
{
    if (!bounds.initialized) {
        bounds.initialized = true;
        bounds.minX = bounds.maxX = x;
        bounds.minY = bounds.maxY = y;
        return;
    }

    bounds.minX = std::min(bounds.minX, x);
    bounds.maxX = std::max(bounds.maxX, x);
    bounds.minY = std::min(bounds.minY, y);
    bounds.maxY = std::max(bounds.maxY, y);
}

SectionBounds calculateSectionContentBounds(const std::vector<PavementLayerSectionLayer>& layers, double scale)
{
    SectionBounds bounds;
    for (const auto& layer : layers) {
        addBounds(bounds, layer.topInner.offset * scale, layer.topInner.elevation * scale);
        addBounds(bounds, layer.topOuter.offset * scale, layer.topOuter.elevation * scale);
        addBounds(bounds, layer.bottomInner.offset * scale, layer.bottomInner.elevation * scale);
        addBounds(bounds, layer.bottomOuter.offset * scale, layer.bottomOuter.elevation * scale);
    }
    if (!bounds.initialized) {
        addBounds(bounds, 0.0, 0.0);
        addBounds(bounds, 5.0 * scale, 0.0);
    }
    return bounds;
}

void drawTemplateName(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const std::wstring& templateName,
    const SectionBounds& contentBounds,
    double scale)
{
    if (templateName.empty()) {
        return;
    }

    const auto titleHeight = std::max(0.9, 0.085 * scale);
    const auto templateNameTextWidth = estimateTextWidth(templateName, titleHeight);
    const auto centerX = (contentBounds.minX + contentBounds.maxX) * 0.5;
    const auto templateNameX = centerX - templateNameTextWidth * 0.5;
    const auto templateNameY = contentBounds.maxY + std::max(1.1, 0.24 * scale);

    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    worldDraw->subEntityTraits().setTransparency(AcCmTransparency(kOpaqueTransparencyAlpha));
    worldDraw->subEntityTraits().setTrueColor(colorFromRgb(255, 255, 255));
    drawText(worldDraw, origin, xAxis, yAxis, templateNameX, templateNameY, templateName, titleHeight);
}

SectionBounds calculateBounds(const PavementLayerTemplateData& data)
{
    const auto section = PavementLayerTemplateRules::buildSection(
        data,
        previewWidth(data),
        SubgradeSide::Right,
        0.0,
        0.0);
    const auto scale = drawingScale(data);
    auto bounds = calculateSectionContentBounds(section.layers, scale);
    bounds.minX -= kExtentsPadding;
    bounds.maxX += kExtentsPadding;
    bounds.minY -= kExtentsPadding;
    bounds.maxY += kExtentsPadding;
    return bounds;
}

void markGraphicsModifiedIfResident(AcDbEntity& entity)
{
    if (!entity.objectId().isNull()) {
        entity.recordGraphicsModified(true);
    }
}

} // namespace

DnPavementLayerTemplateEntity::DnPavementLayerTemplateEntity()
    : templateData_(roadproto::domain::cross_section::PavementLayerTemplateDefaults::create())
    , insertionPoint_(AcGePoint3d::kOrigin)
    , xAxis_(AcGeVector3d::kXAxis)
    , yAxis_(AcGeVector3d::kYAxis)
{
}

Acad::ErrorStatus DnPavementLayerTemplateEntity::setTemplateData(const PavementLayerTemplateData& data)
{
    assertWriteEnabled();
    auto normalized = data;
    std::wstring errorMessage;
    if (!PavementLayerTemplateRules::normalize(normalized, errorMessage)) {
        return Acad::eInvalidInput;
    }
    templateData_ = std::move(normalized);
    markGraphicsModifiedIfResident(*this);
    return Acad::eOk;
}

const PavementLayerTemplateData& DnPavementLayerTemplateEntity::templateData() const
{
    return templateData_;
}

void DnPavementLayerTemplateEntity::setInsertionPoint(const AcGePoint3d& point)
{
    assertWriteEnabled();
    insertionPoint_ = point;
    markGraphicsModifiedIfResident(*this);
}

AcGePoint3d DnPavementLayerTemplateEntity::insertionPoint() const
{
    assertReadEnabled();
    return insertionPoint_;
}

Acad::ErrorStatus DnPavementLayerTemplateEntity::dwgInFields(AcDbDwgFiler* filer)
{
    assertWriteEnabled();
    auto status = AcDbEntity::dwgInFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    Adesk::Int16 version = 0;
    filer->readInt16(&version);
    status = checkFilerStatus(filer);
    if (status != Acad::eOk) {
        return checkFilerStatus(filer);
    }
    if (version < 1 || version > kEntityVersion) {
        return version > kEntityVersion ? Acad::eMakeMeProxy : Acad::eInvalidInput;
    }

    PavementLayerTemplateData data;
    data.properties.name = readWideString(filer);
    filer->readDouble(&data.properties.displayScale);
    filer->readDouble(&data.properties.previewWidth);
    if (version >= 3) {
        data.properties.displayMode = PavementLayerTemplateRules::displayModeFromCode(readWideString(filer));
    }
    if (version >= 5) {
        data.properties.showAllGeneralParameters = readBool(filer);
        data.properties.structureCode = readWideString(filer);
        data.properties.subgradeMoistureTypes = readMoistureTypes(filer);
        data.properties.pavementType = pavementSurfaceTypeFromCode(readWideString(filer));
        data.properties.subgradeSoilGroups = readSoilGroups(filer);
        data.properties.designDeflection = readWideString(filer);
        data.properties.cumulativeAxleLoads = readWideString(filer);
    }

    Adesk::Int32 layerCount = 0;
    filer->readInt32(&layerCount);
    status = checkFilerStatus(filer);
    if (status != Acad::eOk) {
        return checkFilerStatus(filer);
    }
    if (layerCount < 0 || layerCount > kMaxLayers) {
        return Acad::eInvalidInput;
    }

    data.layers.reserve(static_cast<std::size_t>(layerCount));
    for (Adesk::Int32 i = 0; i < layerCount; ++i) {
        PavementLayerTemplateLayer layer;
        Adesk::Int32 type = 0;
        filer->readInt32(&type);
        status = checkFilerStatus(filer);
        if (status != Acad::eOk) {
            return checkFilerStatus(filer);
        }
        if (!isValidPavementLayerTypeValue(type)) {
            return Acad::eInvalidInput;
        }
        layer.type = static_cast<PavementLayerType>(type);
        layer.name = readWideString(filer);
        layer.uniformThickness = readBool(filer);
        filer->readDouble(&layer.thickness);
        filer->readDouble(&layer.innerThickness);
        filer->readDouble(&layer.outerThickness);
        filer->readDouble(&layer.innerWidening);
        filer->readDouble(&layer.outerWidening);
        filer->readDouble(&layer.innerSlope);
        filer->readDouble(&layer.outerSlope);
        if (version >= 2) {
            Adesk::Int32 colorR = -1;
            Adesk::Int32 colorG = -1;
            Adesk::Int32 colorB = -1;
            filer->readInt32(&colorR);
            filer->readInt32(&colorG);
            filer->readInt32(&colorB);
            layer.color = {
                static_cast<int>(colorR),
                static_cast<int>(colorG),
                static_cast<int>(colorB)};
        }
        if (version >= 3) {
            layer.hatchPattern = readWideString(filer);
        }
        if (version >= 4) {
            filer->readDouble(&layer.hatchAngle);
            filer->readDouble(&layer.hatchScale);
        }
        status = checkFilerStatus(filer);
        if (status != Acad::eOk) {
            return checkFilerStatus(filer);
        }
        data.layers.push_back(std::move(layer));
    }

    filer->readPoint3d(&insertionPoint_);
    filer->readVector3d(&xAxis_);
    filer->readVector3d(&yAxis_);
    status = checkFilerStatus(filer);
    if (status != Acad::eOk) {
        return checkFilerStatus(filer);
    }
    if (!isFinitePoint(insertionPoint_)) {
        return Acad::eInvalidInput;
    }
    if (!areAxesUsable(xAxis_, yAxis_)) {
        resetDefaultAxes(xAxis_, yAxis_);
    }

    std::wstring errorMessage;
    if (!PavementLayerTemplateRules::normalize(data, errorMessage)) {
        return Acad::eInvalidInput;
    }

    const auto finalStatus = checkFilerStatus(filer);
    if (finalStatus != Acad::eOk) {
        return finalStatus;
    }
    templateData_ = std::move(data);
    return finalStatus;
}

Acad::ErrorStatus DnPavementLayerTemplateEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }
    if (!isFinitePoint(insertionPoint_) || !areAxesUsable(xAxis_, yAxis_)
        || templateData_.layers.size() > static_cast<std::size_t>(kMaxLayers)) {
        return Acad::eInvalidInput;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, templateData_.properties.name);
    filer->writeDouble(templateData_.properties.displayScale);
    filer->writeDouble(templateData_.properties.previewWidth);
    writeWideString(filer, PavementLayerTemplateRules::displayModeCode(templateData_.properties.displayMode));
    writeBool(filer, templateData_.properties.showAllGeneralParameters);
    writeWideString(filer, templateData_.properties.structureCode);
    writeMoistureTypes(filer, templateData_.properties.subgradeMoistureTypes);
    writeWideString(filer, pavementSurfaceTypeCode(templateData_.properties.pavementType));
    writeSoilGroups(filer, templateData_.properties.subgradeSoilGroups);
    writeWideString(filer, templateData_.properties.designDeflection);
    writeWideString(filer, templateData_.properties.cumulativeAxleLoads);
    filer->writeInt32(static_cast<Adesk::Int32>(templateData_.layers.size()));
    for (const auto& layer : templateData_.layers) {
        filer->writeInt32(static_cast<Adesk::Int32>(layer.type));
        writeWideString(filer, layer.name);
        writeBool(filer, layer.uniformThickness);
        filer->writeDouble(layer.thickness);
        filer->writeDouble(layer.innerThickness);
        filer->writeDouble(layer.outerThickness);
        filer->writeDouble(layer.innerWidening);
        filer->writeDouble(layer.outerWidening);
        filer->writeDouble(layer.innerSlope);
        filer->writeDouble(layer.outerSlope);
        filer->writeInt32(static_cast<Adesk::Int32>(layer.color.r));
        filer->writeInt32(static_cast<Adesk::Int32>(layer.color.g));
        filer->writeInt32(static_cast<Adesk::Int32>(layer.color.b));
        writeWideString(filer, layer.hatchPattern);
        filer->writeDouble(layer.hatchAngle);
        filer->writeDouble(layer.hatchScale);
    }
    filer->writePoint3d(insertionPoint_);
    filer->writeVector3d(xAxis_);
    filer->writeVector3d(yAxis_);
    return filer->filerStatus();
}

Adesk::Boolean DnPavementLayerTemplateEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr || !areAxesUsable(xAxis_, yAxis_)) {
        return Adesk::kTrue;
    }

    const auto section = PavementLayerTemplateRules::buildSection(
        templateData_,
        previewWidth(templateData_),
        SubgradeSide::Right,
        0.0,
        0.0);
    if (!section.succeeded) {
        return Adesk::kTrue;
    }

    const auto scale = drawingScale(templateData_);
    const auto displayMode = templateData_.properties.displayMode;
    for (std::size_t layerIndex = 0; layerIndex < section.layers.size(); ++layerIndex) {
        drawLayerPreviewFill(worldDraw, insertionPoint_, xAxis_, yAxis_, section.layers[layerIndex], scale, displayMode);
    }
    for (std::size_t layerIndex = 0; layerIndex < section.layers.size() && layerIndex < templateData_.layers.size(); ++layerIndex) {
        drawLayerHatchPattern(
            worldDraw,
            insertionPoint_,
            xAxis_,
            yAxis_,
            section.layers[layerIndex],
            templateData_.layers[layerIndex],
            scale,
            displayMode);
    }
    for (std::size_t layerIndex = 0; layerIndex < section.layers.size(); ++layerIndex) {
        const auto& layer = section.layers[layerIndex];
        const bool drawTopEdge = layerIndex == 0
            || !sameSectionPoint(layer.topInner, section.layers[layerIndex - 1].bottomInner)
            || !sameSectionPoint(layer.topOuter, section.layers[layerIndex - 1].bottomOuter);
        drawLayerEdges(worldDraw, insertionPoint_, xAxis_, yAxis_, layer, scale, drawTopEdge);
    }
    const auto contentBounds = calculateSectionContentBounds(section.layers, scale);
    drawTemplateName(
        worldDraw,
        insertionPoint_,
        xAxis_,
        yAxis_,
        templateData_.properties.name,
        contentBounds,
        scale);

    return Adesk::kTrue;
}

Acad::ErrorStatus DnPavementLayerTemplateEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    if (!areAxesUsable(xAxis_, yAxis_)) {
        return Acad::eInvalidExtents;
    }

    const auto bounds = calculateBounds(templateData_);
    extents.addPoint(sectionPoint(insertionPoint_, xAxis_, yAxis_, bounds.minX, bounds.minY));
    extents.addPoint(sectionPoint(insertionPoint_, xAxis_, yAxis_, bounds.minX, bounds.maxY));
    extents.addPoint(sectionPoint(insertionPoint_, xAxis_, yAxis_, bounds.maxX, bounds.minY));
    extents.addPoint(sectionPoint(insertionPoint_, xAxis_, yAxis_, bounds.maxX, bounds.maxY));
    return Acad::eOk;
}

Acad::ErrorStatus DnPavementLayerTemplateEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    assertWriteEnabled();
    auto newPoint = insertionPoint_;
    auto newXAxis = xAxis_;
    auto newYAxis = yAxis_;
    newPoint.transformBy(transform);
    newXAxis.transformBy(transform);
    newYAxis.transformBy(transform);
    if (!isFinitePoint(newPoint) || !areAxesUsable(newXAxis, newYAxis)) {
        return Acad::eInvalidInput;
    }

    insertionPoint_ = newPoint;
    xAxis_ = newXAxis;
    yAxis_ = newYAxis;
    markGraphicsModifiedIfResident(*this);
    return Acad::eOk;
}

Acad::ErrorStatus DnPavementLayerTemplateEntity::subGetGripPoints(
    AcGePoint3dArray& gripPoints,
    AcDbIntArray&,
    AcDbIntArray&) const
{
    assertReadEnabled();
    gripPoints.append(insertionPoint_);
    return Acad::eOk;
}

Acad::ErrorStatus DnPavementLayerTemplateEntity::subMoveGripPointsAt(
    const AcDbIntArray& indices,
    const AcGeVector3d& offset)
{
    assertWriteEnabled();
    if (indices.length() == 0 || offset.isZeroLength()) {
        return Acad::eOk;
    }
    insertionPoint_ += offset;
    recordGraphicsModified(true);
    return Acad::eOk;
}

namespace roadproto::cad_adapter::objectarx {

void initializePavementLayerTemplateEntityClass()
{
    DnPavementLayerTemplateEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializePavementLayerTemplateEntityClass()
{
    deleteAcRxClass(DnPavementLayerTemplateEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
