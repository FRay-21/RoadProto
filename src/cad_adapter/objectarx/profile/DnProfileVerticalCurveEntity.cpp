#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"

#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "domain/profile/ProfileVerticalCurveCalculator.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbapserv.h"
#include "dbproxy.h"
#include "rxregsvc.h"

#include <algorithm>
#include <cmath>
#include <utility>

using roadproto::domain::profile::ProfileVerticalCurveCalculator;
using roadproto::domain::profile::ProfileVerticalCurveData;
using roadproto::domain::profile::VerticalCurveControlPoint;
using roadproto::domain::profile::VerticalCurvePointRole;
using roadproto::domain::profile::VerticalCurvePvi;

ACRX_DXF_DEFINE_MEMBERS(
    DnProfileVerticalCurveEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNPROFILEVERTICALCURVEENTITY,
    "RoadProto Profile Vertical Curve");

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;
constexpr Adesk::Int32 kMaxPersistedCount = 100000;

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

bool isValidPointRole(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(VerticalCurvePointRole::Start)
        || value == static_cast<Adesk::Int32>(VerticalCurvePointRole::Pvi)
        || value == static_cast<Adesk::Int32>(VerticalCurvePointRole::End);
}

AcDb::LineWeight lineWeightFromMillimeters(double width)
{
    if (!std::isfinite(width) || width <= 0.0) {
        return AcDb::kLnWtByLayer;
    }

    const auto clampedWidth = std::clamp(width, 0.0, 2.11);
    const auto hundredthsOfMillimeter = static_cast<int>(std::round(clampedWidth * 100.0));
    return AcDbDatabase::getNearestLineWeight(hundredthsOfMillimeter);
}

void markGraphicsModifiedIfResident(AcDbEntity& entity)
{
    if (!entity.objectId().isNull()) {
        entity.recordGraphicsModified(true);
    }
}

} // namespace

DnProfileVerticalCurveEntity::DnProfileVerticalCurveEntity() = default;

void DnProfileVerticalCurveEntity::setCurveData(const ProfileVerticalCurveData& data)
{
    assertWriteEnabled();
    curveData_ = data;
    markGraphicsModifiedIfResident(*this);
}

const ProfileVerticalCurveData& DnProfileVerticalCurveEntity::curveData() const
{
    return curveData_;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::dwgInFields(AcDbDwgFiler* filer)
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
    curveData_.version = version;

    curveData_.profileGraphHandle = readWideString(filer);

    curveData_.properties.name = readWideString(filer);
    filer->readInt32(&curveData_.properties.designLineColorIndex);
    filer->readInt32(&curveData_.properties.tangentLineColorIndex);
    filer->readInt32(&curveData_.properties.keyPointColorIndex);
    filer->readDouble(&curveData_.properties.designLineWidth);
    filer->readDouble(&curveData_.properties.sampleInterval);

    Adesk::Int32 controlPointCount = 0;
    filer->readInt32(&controlPointCount);
    if (controlPointCount < 0 || controlPointCount > kMaxPersistedCount) {
        return Acad::eInvalidInput;
    }
    curveData_.controlPoints.clear();
    curveData_.controlPoints.reserve(controlPointCount > 0 ? static_cast<std::size_t>(controlPointCount) : 0);
    for (Adesk::Int32 i = 0; i < controlPointCount; ++i) {
        VerticalCurveControlPoint point;
        Adesk::Int32 role = 0;
        filer->readInt32(&role);
        if (!isValidPointRole(role)) {
            return Acad::eInvalidInput;
        }
        point.role = static_cast<VerticalCurvePointRole>(role);
        filer->readDouble(&point.station);
        filer->readDouble(&point.elevation);
        curveData_.controlPoints.push_back(point);
    }

    Adesk::Int32 pviCount = 0;
    filer->readInt32(&pviCount);
    if (pviCount < 0 || pviCount > kMaxPersistedCount) {
        return Acad::eInvalidInput;
    }
    curveData_.pvis.clear();
    curveData_.pvis.reserve(pviCount > 0 ? static_cast<std::size_t>(pviCount) : 0);
    for (Adesk::Int32 i = 0; i < pviCount; ++i) {
        VerticalCurvePvi pvi;
        filer->readDouble(&pvi.station);
        filer->readDouble(&pvi.elevation);
        filer->readDouble(&pvi.radius);
        pvi.radiusLocked = readBool(filer);
        curveData_.pvis.push_back(pvi);
    }

    return filer->filerStatus();
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, curveData_.profileGraphHandle);

    writeWideString(filer, curveData_.properties.name);
    filer->writeInt32(static_cast<Adesk::Int32>(curveData_.properties.designLineColorIndex));
    filer->writeInt32(static_cast<Adesk::Int32>(curveData_.properties.tangentLineColorIndex));
    filer->writeInt32(static_cast<Adesk::Int32>(curveData_.properties.keyPointColorIndex));
    filer->writeDouble(curveData_.properties.designLineWidth);
    filer->writeDouble(curveData_.properties.sampleInterval);

    filer->writeInt32(static_cast<Adesk::Int32>(curveData_.controlPoints.size()));
    for (const auto& point : curveData_.controlPoints) {
        filer->writeInt32(static_cast<Adesk::Int32>(point.role));
        filer->writeDouble(point.station);
        filer->writeDouble(point.elevation);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(curveData_.pvis.size()));
    for (const auto& pvi : curveData_.pvis) {
        filer->writeDouble(pvi.station);
        filer->writeDouble(pvi.elevation);
        filer->writeDouble(pvi.radius);
        writeBool(filer, pvi.radiusLocked);
    }

    return filer->filerStatus();
}

Adesk::Boolean DnProfileVerticalCurveEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr) {
        return Adesk::kTrue;
    }

    const auto samples = ProfileVerticalCurveCalculator::sample(curveData_, curveData_.properties.sampleInterval);
    if (!samples.succeeded || samples.points.size() < 2) {
        return Adesk::kTrue;
    }

    const auto color = curveData_.properties.designLineColorIndex > 0
        ? static_cast<Adesk::UInt16>(curveData_.properties.designLineColorIndex)
        : static_cast<Adesk::UInt16>(1);
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    worldDraw->subEntityTraits().setColor(color);
    worldDraw->subEntityTraits().setLineWeight(lineWeightFromMillimeters(curveData_.properties.designLineWidth));

    for (std::size_t i = 1; i < samples.points.size(); ++i) {
        AcGePoint3d line[2] = {
            AcGePoint3d(samples.points[i - 1].station, samples.points[i - 1].elevation, 0.0),
            AcGePoint3d(samples.points[i].station, samples.points[i].elevation, 0.0)};
        worldDraw->geometry().polyline(2, line);
    }
    worldDraw->subEntityTraits().setLineWeight(AcDb::kLnWtByLayer);

    return Adesk::kTrue;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    const auto samples = ProfileVerticalCurveCalculator::sample(curveData_, curveData_.properties.sampleInterval);
    if (!samples.succeeded || samples.points.empty()) {
        return Acad::eInvalidExtents;
    }

    for (const auto& point : samples.points) {
        extents.addPoint(AcGePoint3d(point.station, point.elevation, 0.0));
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::subTransformBy(const AcGeMatrix3d&)
{
    assertWriteEnabled();
    return Acad::eOk;
}

namespace roadproto::cad_adapter::objectarx {

void initializeProfileVerticalCurveEntityClass()
{
    DnProfileVerticalCurveEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeProfileVerticalCurveEntityClass()
{
    deleteAcRxClass(DnProfileVerticalCurveEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
