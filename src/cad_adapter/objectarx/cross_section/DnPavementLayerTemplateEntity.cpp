#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbproxy.h"
#include "rxregsvc.h"

#include <algorithm>
#include <cmath>
#include <utility>

using roadproto::domain::cross_section::PavementLayerSectionLayer;
using roadproto::domain::cross_section::PavementLayerTemplateData;
using roadproto::domain::cross_section::PavementLayerTemplateLayer;
using roadproto::domain::cross_section::PavementLayerTemplateRules;
using roadproto::domain::cross_section::PavementLayerType;
using roadproto::domain::cross_section::SubgradeSide;

ACRX_DXF_DEFINE_MEMBERS(
    DnPavementLayerTemplateEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNPAVEMENTLAYERTEMPLATEENTITY,
    "RoadProto Pavement Layer Template");

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;
constexpr Adesk::Int32 kMaxLayers = 1000;
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
        || value == static_cast<Adesk::Int32>(PavementLayerType::Cushion);
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
        : 3.75;
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

int colorIndex(PavementLayerType type)
{
    switch (type) {
    case PavementLayerType::UpperSurface:
        return 1;
    case PavementLayerType::MiddleSurface:
        return 30;
    case PavementLayerType::LowerSurface:
        return 2;
    case PavementLayerType::Base:
        return 3;
    case PavementLayerType::Subbase:
        return 4;
    case PavementLayerType::Cushion:
        return 5;
    default:
        return 7;
    }
}

void drawLayerPolygon(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const PavementLayerSectionLayer& layer,
    double scale)
{
    worldDraw->subEntityTraits().setColor(colorIndex(layer.type));
    worldDraw->subEntityTraits().setFillType(kAcGiFillAlways);
    AcGePoint3d polygon[4] = {
        sectionPoint(origin, xAxis, yAxis, layer.topInner.offset * scale, layer.topInner.elevation * scale),
        sectionPoint(origin, xAxis, yAxis, layer.topOuter.offset * scale, layer.topOuter.elevation * scale),
        sectionPoint(origin, xAxis, yAxis, layer.bottomOuter.offset * scale, layer.bottomOuter.elevation * scale),
        sectionPoint(origin, xAxis, yAxis, layer.bottomInner.offset * scale, layer.bottomInner.elevation * scale)};
    worldDraw->geometry().polygon(4, polygon);

    worldDraw->subEntityTraits().setColor(7);
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    AcGePoint3d outline[5] = {polygon[0], polygon[1], polygon[2], polygon[3], polygon[0]};
    worldDraw->geometry().polyline(5, outline);
}

struct SectionBounds {
    double minX = -1.0;
    double maxX = 5.0;
    double minY = -1.0;
    double maxY = 1.0;
};

void addBounds(SectionBounds& bounds, double x, double y)
{
    bounds.minX = std::min(bounds.minX, x);
    bounds.maxX = std::max(bounds.maxX, x);
    bounds.minY = std::min(bounds.minY, y);
    bounds.maxY = std::max(bounds.maxY, y);
}

SectionBounds calculateBounds(const PavementLayerTemplateData& data)
{
    SectionBounds bounds;
    const auto section = PavementLayerTemplateRules::buildSection(
        data,
        previewWidth(data),
        SubgradeSide::Right,
        0.0,
        0.0);
    const auto scale = drawingScale(data);
    for (const auto& layer : section.layers) {
        addBounds(bounds, layer.topInner.offset * scale, layer.topInner.elevation * scale);
        addBounds(bounds, layer.topOuter.offset * scale, layer.topOuter.elevation * scale);
        addBounds(bounds, layer.bottomInner.offset * scale, layer.bottomInner.elevation * scale);
        addBounds(bounds, layer.bottomOuter.offset * scale, layer.bottomOuter.elevation * scale);
    }
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

void DnPavementLayerTemplateEntity::setTemplateData(const PavementLayerTemplateData& data)
{
    assertWriteEnabled();
    templateData_ = data;
    std::wstring errorMessage;
    PavementLayerTemplateRules::normalize(templateData_, errorMessage);
    markGraphicsModifiedIfResident(*this);
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
    if (version < 0) {
        return Acad::eInvalidInput;
    }
    if (version > kEntityVersion) {
        return Acad::eMakeMeProxy;
    }

    PavementLayerTemplateData data;
    data.properties.name = readWideString(filer);
    filer->readDouble(&data.properties.displayScale);
    filer->readDouble(&data.properties.previewWidth);

    Adesk::Int32 layerCount = 0;
    filer->readInt32(&layerCount);
    if (layerCount < 0 || layerCount > kMaxLayers) {
        return Acad::eInvalidInput;
    }

    data.layers.reserve(static_cast<std::size_t>(layerCount));
    for (Adesk::Int32 i = 0; i < layerCount; ++i) {
        PavementLayerTemplateLayer layer;
        Adesk::Int32 type = 0;
        filer->readInt32(&type);
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
        data.layers.push_back(std::move(layer));
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
    if (!PavementLayerTemplateRules::normalize(data, errorMessage)) {
        return Acad::eInvalidInput;
    }

    templateData_ = std::move(data);
    return filer->filerStatus();
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
    for (const auto& layer : section.layers) {
        drawLayerPolygon(worldDraw, insertionPoint_, xAxis_, yAxis_, layer, scale);
    }

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
