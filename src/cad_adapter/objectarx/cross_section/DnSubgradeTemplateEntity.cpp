#include "cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbproxy.h"
#include "rxregsvc.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

using roadproto::domain::cross_section::RoadGrade;
using roadproto::domain::cross_section::SubgradeComponentType;
using roadproto::domain::cross_section::SubgradeSide;
using roadproto::domain::cross_section::SubgradeSlopeMode;
using roadproto::domain::cross_section::SubgradeStationValue;
using roadproto::domain::cross_section::SubgradeTemplateComponent;
using roadproto::domain::cross_section::SubgradeTemplateData;
using roadproto::domain::cross_section::SubgradeTemplateRgbColor;
using roadproto::domain::cross_section::SubgradeTemplateRules;

ACRX_DXF_DEFINE_MEMBERS(
    DnSubgradeTemplateEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNSUBGRADETEMPLATEENTITY,
    "RoadProto Subgrade Template");

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;
constexpr Adesk::Int32 kMaxComponents = 1000;
constexpr Adesk::Int32 kMaxTableRows = 10000;
constexpr double kMinAxisLength = 1.0e-9;
constexpr double kMaxAxisLength = 1.0e9;
constexpr double kMinAxisSine = 1.0e-6;
constexpr double kExtentsPadding = 12.0;

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
    xAxis = AcGeVector3d(1.0, 0.0, 0.0);
    yAxis = AcGeVector3d(0.0, 1.0, 0.0);
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

double drawingScale(const SubgradeTemplateData& data)
{
    return SubgradeTemplateRules::isSupportedDisplayScale(data.properties.displayScale)
        ? data.properties.displayScale
        : 10.0;
}

AcCmEntityColor entityColor(const SubgradeTemplateRgbColor& color)
{
    AcCmEntityColor result;
    result.setRGB(
        static_cast<Adesk::UInt8>(std::clamp(color.r, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.g, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.b, 0, 255)));
    return result;
}

void drawLine(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    double x1,
    double y1,
    double x2,
    double y2)
{
    AcGePoint3d points[2] = {
        sectionPoint(origin, xAxis, yAxis, x1, y1),
        sectionPoint(origin, xAxis, yAxis, x2, y2)};
    worldDraw->geometry().polyline(2, points);
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
    if (text.empty()) {
        return;
    }

    worldDraw->geometry().text(
        sectionPoint(origin, xAxis, yAxis, x, y),
        textNormal(xAxis, yAxis),
        textDirection(xAxis),
        height,
        1.0,
        0.0,
        text.c_str());
}

double displaySlope(const SubgradeTemplateComponent& component)
{
    if (component.slopeMode == SubgradeSlopeMode::Fixed) {
        return std::isfinite(component.fixedSlope) ? component.fixedSlope : 0.0;
    }

    for (const auto& row : component.variableSlopeTable) {
        if (std::isfinite(row.value)) {
            return row.value;
        }
    }

    return 0.0;
}

std::wstring componentLabel(const SubgradeTemplateComponent& component)
{
    std::wostringstream stream;
    stream << roadproto::domain::cross_section::subgradeComponentTypeDisplayName(component.type)
           << L" 宽 " << std::fixed << std::setprecision(2) << component.width
           << L" 坡 " << std::setprecision(3) << displaySlope(component);
    return stream.str();
}

struct SectionBounds {
    double minX = -5.0;
    double maxX = 5.0;
    double minY = -5.0;
    double maxY = 5.0;
};

void addBounds(SectionBounds& bounds, double x, double y)
{
    bounds.minX = std::min(bounds.minX, x);
    bounds.maxX = std::max(bounds.maxX, x);
    bounds.minY = std::min(bounds.minY, y);
    bounds.maxY = std::max(bounds.maxY, y);
}

void walkSideForBounds(
    const SubgradeTemplateData& data,
    SubgradeSide side,
    SectionBounds& bounds)
{
    const auto scale = drawingScale(data);
    const auto sign = side == SubgradeSide::Left ? -1.0 : 1.0;
    double x = 0.0;
    double y = 0.0;
    for (const auto& component : data.components) {
        if (component.side != side) {
            continue;
        }
        const auto width = std::max(0.0, component.width) * scale;
        const auto yStart = y + component.height * scale;
        const auto yEnd = yStart + displaySlope(component) * width;
        const auto xEnd = x + sign * width;
        const auto thickness = std::max(0.15 * scale, 0.5);
        const auto bottom = std::min(yStart, yEnd) - thickness;
        addBounds(bounds, x, yStart);
        addBounds(bounds, xEnd, yEnd);
        addBounds(bounds, x, bottom);
        addBounds(bounds, xEnd, bottom);
        x = xEnd;
        y = yEnd;
    }
}

SectionBounds calculateBounds(const SubgradeTemplateData& data)
{
    SectionBounds bounds;
    const auto scale = drawingScale(data);
    addBounds(bounds, 0.0, -2.0 * scale);
    addBounds(bounds, 0.0, 2.0 * scale);
    walkSideForBounds(data, SubgradeSide::Left, bounds);
    walkSideForBounds(data, SubgradeSide::Right, bounds);
    bounds.minX -= kExtentsPadding;
    bounds.maxX += kExtentsPadding;
    bounds.minY -= kExtentsPadding;
    bounds.maxY += kExtentsPadding + 3.0 * scale;
    return bounds;
}

void drawComponent(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const SubgradeTemplateComponent& component,
    double scale,
    double& x,
    double& y)
{
    const auto sign = component.side == SubgradeSide::Left ? -1.0 : 1.0;
    const auto width = std::max(0.0, component.width) * scale;
    const auto yStart = y + component.height * scale;
    const auto yEnd = yStart + displaySlope(component) * width;
    const auto xEnd = x + sign * width;
    const auto thickness = std::max(0.15 * scale, 0.5);
    const auto bottomStart = yStart - thickness;
    const auto bottomEnd = yEnd - thickness;

    worldDraw->subEntityTraits().setTrueColor(entityColor(component.color));
    worldDraw->subEntityTraits().setFillType(kAcGiFillAlways);
    AcGePoint3d polygon[4] = {
        sectionPoint(origin, xAxis, yAxis, x, yStart),
        sectionPoint(origin, xAxis, yAxis, xEnd, yEnd),
        sectionPoint(origin, xAxis, yAxis, xEnd, bottomEnd),
        sectionPoint(origin, xAxis, yAxis, x, bottomStart)};
    worldDraw->geometry().polygon(4, polygon);

    worldDraw->subEntityTraits().setColor(7);
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    drawLine(worldDraw, origin, xAxis, yAxis, x, yStart, xEnd, yEnd);
    drawLine(worldDraw, origin, xAxis, yAxis, xEnd, yEnd, xEnd, bottomEnd);
    drawLine(worldDraw, origin, xAxis, yAxis, xEnd, bottomEnd, x, bottomStart);
    drawLine(worldDraw, origin, xAxis, yAxis, x, bottomStart, x, yStart);

    if (width > scale * 0.5) {
        worldDraw->subEntityTraits().setColor(7);
        const auto labelX = (x + xEnd) * 0.5 - scale * 0.2;
        const auto labelY = std::max(yStart, yEnd) + 0.25 * scale;
        drawText(worldDraw, origin, xAxis, yAxis, labelX, labelY, componentLabel(component), std::max(1.8, 0.22 * scale));
    }

    x = xEnd;
    y = yEnd;
}

void drawSide(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const SubgradeTemplateData& data,
    SubgradeSide side)
{
    const auto scale = drawingScale(data);
    double x = 0.0;
    double y = 0.0;
    for (const auto& component : data.components) {
        if (component.side == side) {
            drawComponent(worldDraw, origin, xAxis, yAxis, component, scale, x, y);
        }
    }
}

void readStationRows(
    AcDbDwgFiler* filer,
    std::vector<SubgradeStationValue>& rows)
{
    Adesk::Int32 count = 0;
    filer->readInt32(&count);
    rows.clear();
    if (count < 0 || count > kMaxTableRows) {
        return;
    }
    rows.reserve(static_cast<std::size_t>(count));
    for (Adesk::Int32 i = 0; i < count; ++i) {
        SubgradeStationValue row;
        filer->readDouble(&row.station);
        filer->readDouble(&row.value);
        rows.push_back(row);
    }
}

void writeStationRows(
    AcDbDwgFiler* filer,
    const std::vector<SubgradeStationValue>& rows)
{
    filer->writeInt32(static_cast<Adesk::Int32>(std::min<std::size_t>(rows.size(), kMaxTableRows)));
    for (std::size_t i = 0; i < rows.size() && i < static_cast<std::size_t>(kMaxTableRows); ++i) {
        filer->writeDouble(rows[i].station);
        filer->writeDouble(rows[i].value);
    }
}

void markGraphicsModifiedIfResident(AcDbEntity& entity)
{
    if (!entity.objectId().isNull()) {
        entity.recordGraphicsModified(true);
    }
}

} // namespace

DnSubgradeTemplateEntity::DnSubgradeTemplateEntity()
    : insertionPoint_(0.0, 0.0, 0.0)
    , xAxis_(1.0, 0.0, 0.0)
    , yAxis_(0.0, 1.0, 0.0)
{
}

void DnSubgradeTemplateEntity::setTemplateData(const SubgradeTemplateData& data)
{
    assertWriteEnabled();
    templateData_ = data;
    std::wstring errorMessage;
    SubgradeTemplateRules::normalize(templateData_, errorMessage);
    markGraphicsModifiedIfResident(*this);
}

const SubgradeTemplateData& DnSubgradeTemplateEntity::templateData() const
{
    return templateData_;
}

void DnSubgradeTemplateEntity::setInsertionPoint(const AcGePoint3d& point)
{
    assertWriteEnabled();
    insertionPoint_ = point;
    markGraphicsModifiedIfResident(*this);
}

AcGePoint3d DnSubgradeTemplateEntity::insertionPoint() const
{
    return insertionPoint_;
}

Acad::ErrorStatus DnSubgradeTemplateEntity::dwgInFields(AcDbDwgFiler* filer)
{
    assertWriteEnabled();
    auto status = AcDbEntity::dwgInFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    Adesk::Int16 version = 0;
    filer->readInt16(&version);
    if (version > kEntityVersion) {
        return Acad::eMakeMeProxy;
    }

    filer->readDouble(&insertionPoint_.x);
    filer->readDouble(&insertionPoint_.y);
    filer->readDouble(&insertionPoint_.z);
    filer->readDouble(&xAxis_.x);
    filer->readDouble(&xAxis_.y);
    filer->readDouble(&xAxis_.z);
    filer->readDouble(&yAxis_.x);
    filer->readDouble(&yAxis_.y);
    filer->readDouble(&yAxis_.z);
    if (!areAxesUsable(xAxis_, yAxis_)) {
        resetDefaultAxes(xAxis_, yAxis_);
    }

    templateData_.properties.name = readWideString(filer);
    filer->readDouble(&templateData_.properties.displayScale);
    Adesk::Int32 roadGrade = 0;
    filer->readInt32(&roadGrade);
    templateData_.properties.roadGrade = static_cast<RoadGrade>(roadGrade);
    templateData_.roadCenterlineHandle = readWideString(filer);

    Adesk::Int32 componentCount = 0;
    filer->readInt32(&componentCount);
    if (componentCount < 0 || componentCount > kMaxComponents) {
        return Acad::eInvalidInput;
    }

    templateData_.components.clear();
    templateData_.components.reserve(static_cast<std::size_t>(componentCount));
    for (Adesk::Int32 i = 0; i < componentCount; ++i) {
        SubgradeTemplateComponent component;
        Adesk::Int32 side = 0;
        Adesk::Int32 type = 0;
        Adesk::Int32 slopeMode = 0;
        filer->readInt32(&side);
        filer->readInt32(&type);
        filer->readDouble(&component.width);
        filer->readDouble(&component.height);
        filer->readDouble(&component.fixedSlope);
        filer->readInt32(&slopeMode);
        filer->readInt32(&component.color.r);
        filer->readInt32(&component.color.g);
        filer->readInt32(&component.color.b);
        readStationRows(filer, component.wideningTable);
        readStationRows(filer, component.variableSlopeTable);
        component.pavementLayerLinked = readBool(filer);
        component.pavementLayerHandle = readWideString(filer);
        filer->readDouble(&component.pavementLayerThickness);

        component.side = side == static_cast<Adesk::Int32>(SubgradeSide::Left)
            ? SubgradeSide::Left
            : SubgradeSide::Right;
        component.type = static_cast<SubgradeComponentType>(type);
        component.slopeMode = slopeMode == static_cast<Adesk::Int32>(SubgradeSlopeMode::VariableByStation)
            ? SubgradeSlopeMode::VariableByStation
            : SubgradeSlopeMode::Fixed;
        templateData_.components.push_back(std::move(component));
    }

    std::wstring errorMessage;
    SubgradeTemplateRules::normalize(templateData_, errorMessage);
    return filer->filerStatus();
}

Acad::ErrorStatus DnSubgradeTemplateEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    auto xAxis = xAxis_;
    auto yAxis = yAxis_;
    if (!areAxesUsable(xAxis, yAxis)) {
        resetDefaultAxes(xAxis, yAxis);
    }

    filer->writeInt16(kEntityVersion);
    filer->writeDouble(insertionPoint_.x);
    filer->writeDouble(insertionPoint_.y);
    filer->writeDouble(insertionPoint_.z);
    filer->writeDouble(xAxis.x);
    filer->writeDouble(xAxis.y);
    filer->writeDouble(xAxis.z);
    filer->writeDouble(yAxis.x);
    filer->writeDouble(yAxis.y);
    filer->writeDouble(yAxis.z);

    writeWideString(filer, templateData_.properties.name);
    filer->writeDouble(templateData_.properties.displayScale);
    filer->writeInt32(static_cast<Adesk::Int32>(templateData_.properties.roadGrade));
    writeWideString(filer, templateData_.roadCenterlineHandle);

    filer->writeInt32(static_cast<Adesk::Int32>(std::min<std::size_t>(templateData_.components.size(), kMaxComponents)));
    for (std::size_t i = 0; i < templateData_.components.size() && i < static_cast<std::size_t>(kMaxComponents); ++i) {
        const auto& component = templateData_.components[i];
        filer->writeInt32(static_cast<Adesk::Int32>(component.side));
        filer->writeInt32(static_cast<Adesk::Int32>(component.type));
        filer->writeDouble(component.width);
        filer->writeDouble(component.height);
        filer->writeDouble(component.fixedSlope);
        filer->writeInt32(static_cast<Adesk::Int32>(component.slopeMode));
        filer->writeInt32(component.color.r);
        filer->writeInt32(component.color.g);
        filer->writeInt32(component.color.b);
        writeStationRows(filer, component.wideningTable);
        writeStationRows(filer, component.variableSlopeTable);
        writeBool(filer, component.pavementLayerLinked);
        writeWideString(filer, component.pavementLayerHandle);
        filer->writeDouble(component.pavementLayerThickness);
    }

    return filer->filerStatus();
}

Adesk::Boolean DnSubgradeTemplateEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr || !areAxesUsable(xAxis_, yAxis_)) {
        return Adesk::kTrue;
    }

    const auto bounds = calculateBounds(templateData_);
    const auto scale = drawingScale(templateData_);

    worldDraw->subEntityTraits().setColor(1);
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    drawLine(worldDraw, insertionPoint_, xAxis_, yAxis_, 0.0, bounds.minY, 0.0, bounds.maxY - 2.0 * scale);
    drawText(worldDraw, insertionPoint_, xAxis_, yAxis_, 0.3 * scale, bounds.maxY - 2.5 * scale, L"CL", std::max(2.0, 0.28 * scale));

    drawSide(worldDraw, insertionPoint_, xAxis_, yAxis_, templateData_, SubgradeSide::Left);
    drawSide(worldDraw, insertionPoint_, xAxis_, yAxis_, templateData_, SubgradeSide::Right);

    worldDraw->subEntityTraits().setColor(2);
    const auto title = templateData_.properties.name.empty() ? L"Subgrade Template" : templateData_.properties.name;
    drawText(worldDraw, insertionPoint_, xAxis_, yAxis_, bounds.minX, bounds.maxY - scale, title, std::max(2.5, 0.32 * scale));

    return Adesk::kTrue;
}

Acad::ErrorStatus DnSubgradeTemplateEntity::subGetGeomExtents(AcDbExtents& extents) const
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

Acad::ErrorStatus DnSubgradeTemplateEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    assertWriteEnabled();
    auto transformedInsertionPoint = insertionPoint_;
    auto transformedXAxis = xAxis_;
    auto transformedYAxis = yAxis_;
    transformedInsertionPoint.transformBy(transform);
    transformedXAxis.transformBy(transform);
    transformedYAxis.transformBy(transform);
    if (!areAxesUsable(transformedXAxis, transformedYAxis)) {
        return Acad::eInvalidInput;
    }

    insertionPoint_ = transformedInsertionPoint;
    xAxis_ = transformedXAxis;
    yAxis_ = transformedYAxis;
    markGraphicsModifiedIfResident(*this);
    return Acad::eOk;
}

namespace roadproto::cad_adapter::objectarx {

void initializeSubgradeTemplateEntityClass()
{
    DnSubgradeTemplateEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeSubgradeTemplateEntityClass()
{
    deleteAcRxClass(DnSubgradeTemplateEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
