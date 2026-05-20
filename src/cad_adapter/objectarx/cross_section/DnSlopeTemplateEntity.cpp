#include "cad_adapter/objectarx/cross_section/DnSlopeTemplateEntity.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbproxy.h"
#include "rxregsvc.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

using roadproto::domain::cross_section::SlopeComponentType;
using roadproto::domain::cross_section::SlopeGeometryConstraintMode;
using roadproto::domain::cross_section::SlopeTemplateComponent;
using roadproto::domain::cross_section::SlopeTemplateData;
using roadproto::domain::cross_section::SlopeTemplateKind;
using roadproto::domain::cross_section::SlopeTemplateRgbColor;
using roadproto::domain::cross_section::SlopeTemplateRules;

ACRX_DXF_DEFINE_MEMBERS(
    DnSlopeTemplateEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNSLOPETEMPLATEENTITY,
    "RoadProto Slope Template");

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;
constexpr Adesk::Int32 kMaxComponents = 1000;
constexpr double kMinAxisLength = 1.0e-9;
constexpr double kMaxAxisLength = 1.0e9;
constexpr double kMinAxisSine = 1.0e-6;
constexpr double kExtentsPadding = 8.0;

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

bool readCount(AcDbDwgFiler* filer, Adesk::Int32 maxValue, Adesk::Int32& count)
{
    count = 0;
    filer->readInt32(&count);
    return count >= 0 && count <= maxValue;
}

bool isFiniteVector(const AcGeVector3d& vector)
{
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

bool isFinitePoint(const AcGePoint3d& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
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

double drawingScale(const SlopeTemplateData& data)
{
    return SlopeTemplateRules::isSupportedDisplayScale(data.properties.displayScale)
        ? data.properties.displayScale
        : 10.0;
}

AcCmEntityColor slopeTemplateEntityColor(const SlopeTemplateRgbColor& color)
{
    AcCmEntityColor result;
    result.setRGB(
        static_cast<Adesk::UInt8>(std::clamp(color.r, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.g, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.b, 0, 255)));
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

bool canWriteInt32(std::size_t value)
{
    return value <= static_cast<std::size_t>(std::numeric_limits<Adesk::Int32>::max());
}

bool isValidKindValue(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(SlopeTemplateKind::Fill)
        || value == static_cast<Adesk::Int32>(SlopeTemplateKind::Cut);
}

bool isValidComponentTypeValue(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(SlopeComponentType::FillSlope)
        || value == static_cast<Adesk::Int32>(SlopeComponentType::CutSlope)
        || value == static_cast<Adesk::Int32>(SlopeComponentType::Berm);
}

bool isValidConstraintModeValue(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(SlopeGeometryConstraintMode::SlopeAndHeight)
        || value == static_cast<Adesk::Int32>(SlopeGeometryConstraintMode::SlopeAndWidth)
        || value == static_cast<Adesk::Int32>(SlopeGeometryConstraintMode::HeightAndWidth);
}

bool validateTemplateDataForPersistence(const SlopeTemplateData& data)
{
    if (data.components.size() > static_cast<std::size_t>(kMaxComponents)
        || !std::isfinite(data.properties.displayScale)) {
        return false;
    }
    for (const auto& component : data.components) {
        if (!std::isfinite(component.slope)
            || !std::isfinite(component.height)
            || !std::isfinite(component.width)
            || !std::isfinite(component.groundSearchHeightIncrement)) {
            return false;
        }
    }
    return true;
}

Acad::ErrorStatus readComponent(AcDbDwgFiler* filer, SlopeTemplateComponent& component)
{
    Adesk::Int32 type = 0;
    Adesk::Int32 mode = 0;
    filer->readInt32(&type);
    filer->readInt32(&mode);
    if (!isValidComponentTypeValue(type) || !isValidConstraintModeValue(mode)) {
        return Acad::eInvalidInput;
    }
    component.type = static_cast<SlopeComponentType>(type);
    component.constraintMode = static_cast<SlopeGeometryConstraintMode>(mode);
    filer->readDouble(&component.slope);
    filer->readDouble(&component.height);
    filer->readDouble(&component.width);
    filer->readDouble(&component.groundSearchHeightIncrement);
    filer->readInt32(&component.color.r);
    filer->readInt32(&component.color.g);
    filer->readInt32(&component.color.b);
    return Acad::eOk;
}

void writeComponent(AcDbDwgFiler* filer, const SlopeTemplateComponent& component)
{
    filer->writeInt32(static_cast<Adesk::Int32>(component.type));
    filer->writeInt32(static_cast<Adesk::Int32>(component.constraintMode));
    filer->writeDouble(component.slope);
    filer->writeDouble(component.height);
    filer->writeDouble(component.width);
    filer->writeDouble(component.groundSearchHeightIncrement);
    filer->writeInt32(component.color.r);
    filer->writeInt32(component.color.g);
    filer->writeInt32(component.color.b);
}

struct SectionBounds {
    double minX = -3.0;
    double maxX = 3.0;
    double minY = -3.0;
    double maxY = 3.0;
};

void addBounds(SectionBounds& bounds, double x, double y)
{
    bounds.minX = std::min(bounds.minX, x);
    bounds.maxX = std::max(bounds.maxX, x);
    bounds.minY = std::min(bounds.minY, y);
    bounds.maxY = std::max(bounds.maxY, y);
}

SectionBounds calculateBounds(const SlopeTemplateData& data)
{
    SectionBounds bounds;
    const auto scale = drawingScale(data);
    double x = 0.0;
    double y = 0.0;
    for (const auto& component : data.components) {
        const auto geometry = SlopeTemplateRules::resolveGeometry(component);
        if (!geometry.succeeded) {
            continue;
        }
        const double nextX = x + geometry.width * scale;
        const double nextY = y + geometry.elevationDelta * scale;
        addBounds(bounds, x, y);
        addBounds(bounds, nextX, nextY);
        x = nextX;
        y = nextY;
    }
    return bounds;
}

void markGraphicsModifiedIfResident(AcDbEntity& entity)
{
    if (!entity.objectId().isNull()) {
        entity.recordGraphicsModified(true);
    }
}

} // namespace

DnSlopeTemplateEntity::DnSlopeTemplateEntity()
    : templateData_(roadproto::domain::cross_section::SlopeTemplateDefaults::create(SlopeTemplateKind::Fill))
    , insertionPoint_(AcGePoint3d::kOrigin)
    , xAxis_(AcGeVector3d::kXAxis)
    , yAxis_(AcGeVector3d::kYAxis)
{
}

void DnSlopeTemplateEntity::setTemplateData(const SlopeTemplateData& data)
{
    assertWriteEnabled();
    templateData_ = data;
    markGraphicsModifiedIfResident(*this);
}

const SlopeTemplateData& DnSlopeTemplateEntity::templateData() const
{
    return templateData_;
}

void DnSlopeTemplateEntity::setInsertionPoint(const AcGePoint3d& point)
{
    assertWriteEnabled();
    insertionPoint_ = point;
    markGraphicsModifiedIfResident(*this);
}

AcGePoint3d DnSlopeTemplateEntity::insertionPoint() const
{
    assertReadEnabled();
    return insertionPoint_;
}

Acad::ErrorStatus DnSlopeTemplateEntity::dwgInFields(AcDbDwgFiler* filer)
{
    assertWriteEnabled();
    auto status = AcDbEntity::dwgInFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    Adesk::Int16 version = 0;
    filer->readInt16(&version);
    if (version < 0) {
        return Acad::eInvalidInput;
    }
    if (version > kEntityVersion) {
        return Acad::eMakeMeProxy;
    }

    SlopeTemplateData data;
    data.properties.name = readWideString(filer);
    filer->readDouble(&data.properties.displayScale);
    Adesk::Int32 kind = 0;
    filer->readInt32(&kind);
    if (!isValidKindValue(kind)) {
        return Acad::eInvalidInput;
    }
    data.properties.kind = static_cast<SlopeTemplateKind>(kind);
    data.properties.stopAtGround = readBool(filer);
    data.properties.repeatLastGroupUntilGround = readBool(filer);

    Adesk::Int32 componentCount = 0;
    if (!readCount(filer, kMaxComponents, componentCount)) {
        return Acad::eInvalidInput;
    }
    data.components.reserve(static_cast<std::size_t>(componentCount));
    for (Adesk::Int32 i = 0; i < componentCount; ++i) {
        SlopeTemplateComponent component;
        status = readComponent(filer, component);
        if (status != Acad::eOk) {
            return status;
        }
        data.components.push_back(component);
    }

    filer->readPoint3d(&insertionPoint_);
    filer->readVector3d(&xAxis_);
    filer->readVector3d(&yAxis_);
    if (!isFinitePoint(insertionPoint_)) {
        return Acad::eInvalidInput;
    }
    if (!areAxesUsable(xAxis_, yAxis_)) {
        resetDefaultAxes(xAxis_, yAxis_);
    }

    std::wstring errorMessage;
    if (!SlopeTemplateRules::normalize(data, errorMessage) || !validateTemplateDataForPersistence(data)) {
        return Acad::eInvalidInput;
    }
    templateData_ = std::move(data);
    return filer->filerStatus();
}

Acad::ErrorStatus DnSlopeTemplateEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }
    if (!validateTemplateDataForPersistence(templateData_)
        || !isFinitePoint(insertionPoint_)
        || !areAxesUsable(xAxis_, yAxis_)) {
        return Acad::eInvalidInput;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, templateData_.properties.name);
    filer->writeDouble(templateData_.properties.displayScale);
    filer->writeInt32(static_cast<Adesk::Int32>(templateData_.properties.kind));
    writeBool(filer, templateData_.properties.stopAtGround);
    writeBool(filer, templateData_.properties.repeatLastGroupUntilGround);
    filer->writeInt32(static_cast<Adesk::Int32>(templateData_.components.size()));
    for (const auto& component : templateData_.components) {
        writeComponent(filer, component);
    }
    filer->writePoint3d(insertionPoint_);
    filer->writeVector3d(xAxis_);
    filer->writeVector3d(yAxis_);
    return filer->filerStatus();
}

Adesk::Boolean DnSlopeTemplateEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr) {
        return Adesk::kTrue;
    }

    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    const auto scale = drawingScale(templateData_);
    double x = 0.0;
    double y = 0.0;
    for (const auto& component : templateData_.components) {
        const auto geometry = SlopeTemplateRules::resolveGeometry(component);
        if (!geometry.succeeded) {
            continue;
        }
        const double nextX = x + geometry.width * scale;
        const double nextY = y + geometry.elevationDelta * scale;
        worldDraw->subEntityTraits().setTrueColor(slopeTemplateEntityColor(component.color));
        AcGePoint3d points[2] = {
            sectionPoint(insertionPoint_, xAxis_, yAxis_, x, y),
            sectionPoint(insertionPoint_, xAxis_, yAxis_, nextX, nextY)};
        worldDraw->geometry().polyline(2, points);
        x = nextX;
        y = nextY;
    }

    return Adesk::kTrue;
}

Acad::ErrorStatus DnSlopeTemplateEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    const auto bounds = calculateBounds(templateData_);
    extents.addPoint(sectionPoint(
        insertionPoint_,
        xAxis_,
        yAxis_,
        bounds.minX - kExtentsPadding,
        bounds.minY - kExtentsPadding));
    extents.addPoint(sectionPoint(
        insertionPoint_,
        xAxis_,
        yAxis_,
        bounds.maxX + kExtentsPadding,
        bounds.maxY + kExtentsPadding));
    return Acad::eOk;
}

Acad::ErrorStatus DnSlopeTemplateEntity::subTransformBy(const AcGeMatrix3d& transform)
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

Acad::ErrorStatus DnSlopeTemplateEntity::subGetGripPoints(
    AcGePoint3dArray& gripPoints,
    AcDbIntArray&,
    AcDbIntArray&) const
{
    assertReadEnabled();
    gripPoints.append(insertionPoint_);
    return Acad::eOk;
}

Acad::ErrorStatus DnSlopeTemplateEntity::subMoveGripPointsAt(
    const AcDbIntArray& indices,
    const AcGeVector3d& offset)
{
    assertWriteEnabled();
    if (indices.isEmpty()) {
        return Acad::eOk;
    }
    insertionPoint_ += offset;
    markGraphicsModifiedIfResident(*this);
    return Acad::eOk;
}

namespace roadproto::cad_adapter::objectarx {

void initializeSlopeTemplateEntityClass()
{
    DnSlopeTemplateEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeSlopeTemplateEntityClass()
{
    deleteAcRxClass(DnSlopeTemplateEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
