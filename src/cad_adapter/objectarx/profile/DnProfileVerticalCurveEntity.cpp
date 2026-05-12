#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"

#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "application/profile/ProfileVerticalCurveEditService.h"
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
using roadproto::domain::profile::VerticalCurveGripRole;
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
constexpr double kExtentsPadding = 2.0;
constexpr double kRadiusGripScale = 0.01;

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

bool resolveObjectIdFromHandle(const std::wstring& handleText, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr || handleText.empty()) {
        return false;
    }

    const AcDbHandle handle(handleText.c_str());
    return database->getAcDbObjectId(entityId, false, handle) == Acad::eOk && !entityId.isNull();
}

bool loadGraphFrame(const std::wstring& handleText, ProfileGradeGraphDrawingFrame& frame)
{
    AcDbObjectId graphId;
    if (!resolveObjectIdFromHandle(handleText, graphId)) {
        return false;
    }

    DnProfileGradeGraphEntity* graphEntity = nullptr;
    if (acdbOpenObject(graphEntity, graphId, AcDb::kForRead) != Acad::eOk || graphEntity == nullptr) {
        return false;
    }

    frame = graphEntity->drawingFrame();
    graphEntity->close();
    return frame.valid;
}

AcGePoint3d mapDesignPoint(const ProfileGradeGraphDrawingFrame& frame, double station, double elevation)
{
    return frame.insertionPoint
        + frame.xAxis * (station - frame.minStation)
        + frame.yAxis * ((elevation - frame.baseElevation) * frame.verticalScale);
}

bool unmapDesignPoint(
    const ProfileGradeGraphDrawingFrame& frame,
    const AcGePoint3d& point,
    double& station,
    double& elevation)
{
    if (!frame.valid || frame.xAxis.isZeroLength() || frame.yAxis.isZeroLength()
        || !std::isfinite(frame.verticalScale) || frame.verticalScale <= 0.0) {
        return false;
    }

    auto xDirection = frame.xAxis;
    auto yDirection = frame.yAxis;
    xDirection.normalize();
    yDirection.normalize();

    const auto vector = point - frame.insertionPoint;
    station = frame.minStation + vector.dotProduct(xDirection);
    elevation = frame.baseElevation + vector.dotProduct(yDirection) / frame.verticalScale;
    return std::isfinite(station) && std::isfinite(elevation);
}

bool mapCurvePoint(
    const ProfileGradeGraphDrawingFrame& frame,
    double station,
    double elevation,
    AcGePoint3d& point)
{
    if (!frame.valid || !std::isfinite(station) || !std::isfinite(elevation)) {
        return false;
    }
    point = mapDesignPoint(frame, station, elevation);
    return true;
}

AcGePoint3d radiusGripPoint(const ProfileGradeGraphDrawingFrame& frame, const VerticalCurvePvi& pvi)
{
    const auto radiusOffset = std::max(1.0, pvi.radius * kRadiusGripScale);
    return mapDesignPoint(frame, pvi.station + radiusOffset, pvi.elevation);
}

bool findControlPoint(
    const ProfileVerticalCurveData& data,
    VerticalCurvePointRole role,
    VerticalCurveControlPoint& controlPoint)
{
    const auto it = std::find_if(data.controlPoints.begin(), data.controlPoints.end(), [role](const auto& point) {
        return point.role == role;
    });
    if (it == data.controlPoints.end()) {
        return false;
    }
    controlPoint = *it;
    return true;
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

bool DnProfileVerticalCurveEntity::mapDesignPointToCad(double station, double elevation, AcGePoint3d& point) const
{
    assertReadEnabled();
    ProfileGradeGraphDrawingFrame frame;
    return loadGraphFrame(curveData_.profileGraphHandle, frame) && mapCurvePoint(frame, station, elevation, point);
}

bool DnProfileVerticalCurveEntity::mapCadPointToDesign(
    const AcGePoint3d& point,
    double& station,
    double& elevation) const
{
    assertReadEnabled();
    ProfileGradeGraphDrawingFrame frame;
    return loadGraphFrame(curveData_.profileGraphHandle, frame)
        && unmapDesignPoint(frame, point, station, elevation);
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
    curveData_.properties.showLabels = readBool(filer);
    curveData_.properties.showTangentLines = readBool(filer);

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
    writeBool(filer, curveData_.properties.showLabels);
    writeBool(filer, curveData_.properties.showTangentLines);

    if (curveData_.controlPoints.size() > static_cast<std::size_t>(kMaxPersistedCount)
        || curveData_.pvis.size() > static_cast<std::size_t>(kMaxPersistedCount)) {
        return Acad::eInvalidInput;
    }

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

    ProfileGradeGraphDrawingFrame frame;
    if (!loadGraphFrame(curveData_.profileGraphHandle, frame)) {
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
        AcGePoint3d startPoint;
        AcGePoint3d endPoint;
        if (!mapCurvePoint(frame, samples.points[i - 1].station, samples.points[i - 1].elevation, startPoint)
            || !mapCurvePoint(frame, samples.points[i].station, samples.points[i].elevation, endPoint)) {
            continue;
        }
        AcGePoint3d line[2] = {
            startPoint,
            endPoint};
        worldDraw->geometry().polyline(2, line);
    }
    worldDraw->subEntityTraits().setLineWeight(AcDb::kLnWtByLayer);

    worldDraw->subEntityTraits().setColor(static_cast<Adesk::UInt16>(
        curveData_.properties.keyPointColorIndex > 0 ? curveData_.properties.keyPointColorIndex : 2));
    for (const auto& point : curveData_.controlPoints) {
        AcGePoint3d cadPoint;
        if (!mapCurvePoint(frame, point.station, point.elevation, cadPoint)) {
            continue;
        }
        AcGePoint3d marker[2] = {
            cadPoint + AcGeVector3d(-0.8, -0.8, 0.0),
            cadPoint + AcGeVector3d(0.8, 0.8, 0.0)};
        worldDraw->geometry().polyline(2, marker);
        marker[0] = cadPoint + AcGeVector3d(-0.8, 0.8, 0.0);
        marker[1] = cadPoint + AcGeVector3d(0.8, -0.8, 0.0);
        worldDraw->geometry().polyline(2, marker);
    }

    return Adesk::kTrue;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    const auto samples = ProfileVerticalCurveCalculator::sample(curveData_, curveData_.properties.sampleInterval);
    if (!samples.succeeded || samples.points.empty()) {
        return Acad::eInvalidExtents;
    }

    ProfileGradeGraphDrawingFrame frame;
    if (!loadGraphFrame(curveData_.profileGraphHandle, frame)) {
        return Acad::eInvalidExtents;
    }

    bool hasPoint = false;
    for (const auto& point : samples.points) {
        AcGePoint3d cadPoint;
        if (mapCurvePoint(frame, point.station, point.elevation, cadPoint)) {
            extents.addPoint(cadPoint);
            extents.addPoint(cadPoint + AcGeVector3d(kExtentsPadding, kExtentsPadding, 0.0));
            extents.addPoint(cadPoint + AcGeVector3d(-kExtentsPadding, -kExtentsPadding, 0.0));
            hasPoint = true;
        }
    }
    return hasPoint ? Acad::eOk : Acad::eInvalidExtents;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::subTransformBy(const AcGeMatrix3d&)
{
    assertWriteEnabled();
    return Acad::eOk;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::subGetGripPoints(
    AcGePoint3dArray& gripPoints,
    AcDbIntArray&,
    AcDbIntArray&) const
{
    assertReadEnabled();

    ProfileGradeGraphDrawingFrame frame;
    if (!loadGraphFrame(curveData_.profileGraphHandle, frame)) {
        return Acad::eOk;
    }

    VerticalCurveControlPoint startPoint;
    if (findControlPoint(curveData_, VerticalCurvePointRole::Start, startPoint)) {
        gripPoints.append(mapDesignPoint(frame, startPoint.station, startPoint.elevation));
    }

    VerticalCurveControlPoint endPoint;
    if (findControlPoint(curveData_, VerticalCurvePointRole::End, endPoint)) {
        gripPoints.append(mapDesignPoint(frame, endPoint.station, endPoint.elevation));
    }

    for (const auto& pvi : curveData_.pvis) {
        gripPoints.append(mapDesignPoint(frame, pvi.station, pvi.elevation));
    }

    for (const auto& pvi : curveData_.pvis) {
        gripPoints.append(radiusGripPoint(frame, pvi));
    }

    return Acad::eOk;
}

Acad::ErrorStatus DnProfileVerticalCurveEntity::subMoveGripPointsAt(
    const AcDbIntArray& indices,
    const AcGeVector3d& offset)
{
    assertWriteEnabled();
    if (indices.length() == 0 || offset.isZeroLength()) {
        return Acad::eOk;
    }

    ProfileGradeGraphDrawingFrame frame;
    if (!loadGraphFrame(curveData_.profileGraphHandle, frame)) {
        return Acad::eInvalidInput;
    }

    auto editedData = curveData_;
    roadproto::application::profile::ProfileVerticalCurveEditService service;
    for (int i = 0; i < indices.length(); ++i) {
        const auto rawIndex = indices.at(i);
        if (rawIndex < 0) {
            return Acad::eInvalidInput;
        }

        const auto index = static_cast<std::size_t>(rawIndex);
        roadproto::application::profile::ProfileVerticalCurveGripEdit edit;
        bool needsStationElevation = true;
        AcGePoint3d currentPoint;
        if (index == 0) {
            VerticalCurveControlPoint point;
            if (!findControlPoint(editedData, VerticalCurvePointRole::Start, point)) {
                return Acad::eInvalidInput;
            }
            edit.role = VerticalCurveGripRole::StartPoint;
            currentPoint = mapDesignPoint(frame, point.station, point.elevation);
        } else if (index == 1) {
            VerticalCurveControlPoint point;
            if (!findControlPoint(editedData, VerticalCurvePointRole::End, point)) {
                return Acad::eInvalidInput;
            }
            edit.role = VerticalCurveGripRole::EndPoint;
            currentPoint = mapDesignPoint(frame, point.station, point.elevation);
        } else if (index < 2 + editedData.pvis.size()) {
            edit.role = VerticalCurveGripRole::PviPoint;
            edit.index = index - 2;
            const auto& pvi = editedData.pvis[edit.index];
            currentPoint = mapDesignPoint(frame, pvi.station, pvi.elevation);
        } else if (index < 2 + editedData.pvis.size() * 2) {
            edit.role = VerticalCurveGripRole::RadiusPoint;
            edit.index = index - 2 - editedData.pvis.size();
            const auto& pvi = editedData.pvis[edit.index];
            edit.radius = std::max(1.0, pvi.radius + offset.length() * 10.0);
            needsStationElevation = false;
        } else {
            return Acad::eInvalidInput;
        }

        if (needsStationElevation) {
            if (!unmapDesignPoint(frame, currentPoint + offset, edit.station, edit.elevation)) {
                return Acad::eInvalidInput;
            }
        }

        const auto result = service.applyGripMove(editedData, edit);
        if (!result.succeeded) {
            return Acad::eInvalidInput;
        }
    }

    curveData_ = editedData;
    markGraphicsModifiedIfResident(*this);
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
