#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbproxy.h"
#include "rxregsvc.h"

#include <algorithm>
#include <limits>
#include <utility>

using roadproto::domain::cross_section::RoadModelComponentLine;
using roadproto::domain::cross_section::RoadModelData;
using roadproto::domain::cross_section::RoadModelLineKey;
using roadproto::domain::cross_section::RoadModelPoint3d;
using roadproto::domain::cross_section::RoadModelTemplateAssignment;
using roadproto::domain::cross_section::SubgradeComponentType;
using roadproto::domain::cross_section::SubgradeSide;

ACRX_DXF_DEFINE_MEMBERS(
    DnRoadModelEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNROADMODELENTITY,
    "RoadProto Road Model");

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;
constexpr Adesk::Int32 kMaxAssignments = 10000;
constexpr Adesk::Int32 kMaxComponentLines = 100000;
constexpr Adesk::Int32 kMaxLinePoints = 100000;
constexpr Adesk::Int32 kMaxTotalPoints = 1000000;

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

bool readCount(AcDbDwgFiler* filer, Adesk::Int32 maxValue, Adesk::Int32& count)
{
    count = 0;
    filer->readInt32(&count);
    return count >= 0 && count <= maxValue;
}

bool canWriteInt32(std::size_t value)
{
    return value <= static_cast<std::size_t>(std::numeric_limits<Adesk::Int32>::max());
}

void readPoint(AcDbDwgFiler* filer, RoadModelPoint3d& point)
{
    filer->readDouble(&point.x);
    filer->readDouble(&point.y);
    filer->readDouble(&point.z);
}

void writePoint(AcDbDwgFiler* filer, const RoadModelPoint3d& point)
{
    filer->writeDouble(point.x);
    filer->writeDouble(point.y);
    filer->writeDouble(point.z);
}

void markGraphicsModifiedIfResident(AcDbEntity& entity)
{
    if (!entity.objectId().isNull()) {
        entity.recordGraphicsModified(true);
    }
}

AcCmEntityColor roadModelLineColor(const roadproto::domain::cross_section::SubgradeTemplateRgbColor& color)
{
    return AcCmEntityColor(
        static_cast<Adesk::UInt8>(std::clamp(color.r, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.g, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.b, 0, 255)));
}

bool validatePersistedSizes(const RoadModelData& data)
{
    if (data.config.assignments.size() > static_cast<std::size_t>(kMaxAssignments)
        || data.componentLines.size() > static_cast<std::size_t>(kMaxComponentLines)) {
        return false;
    }

    std::size_t totalPoints = 0;
    for (const auto& line : data.componentLines) {
        if (line.points.size() > static_cast<std::size_t>(kMaxLinePoints)) {
            return false;
        }
        totalPoints += line.points.size();
        if (totalPoints > static_cast<std::size_t>(kMaxTotalPoints)
            || !canWriteInt32(line.key.componentIndex)
            || !canWriteInt32(line.key.boundaryIndex)) {
            return false;
        }
    }

    return true;
}

void readAssignment(AcDbDwgFiler* filer, RoadModelTemplateAssignment& row)
{
    filer->readDouble(&row.startStation);
    filer->readDouble(&row.endStation);
    row.templateHandle = readWideString(filer);
    row.templateName = readWideString(filer);
}

void writeAssignment(AcDbDwgFiler* filer, const RoadModelTemplateAssignment& row)
{
    filer->writeDouble(row.startStation);
    filer->writeDouble(row.endStation);
    writeWideString(filer, row.templateHandle);
    writeWideString(filer, row.templateName);
}

Acad::ErrorStatus readComponentLine(
    AcDbDwgFiler* filer,
    RoadModelComponentLine& line,
    Adesk::Int32& totalPointCount)
{
    line.key.templateHandle = readWideString(filer);

    Adesk::Int32 side = 0;
    Adesk::Int32 type = 0;
    Adesk::Int32 componentIndex = 0;
    Adesk::Int32 boundaryIndex = 0;
    filer->readInt32(&side);
    filer->readInt32(&type);
    filer->readInt32(&componentIndex);
    filer->readInt32(&boundaryIndex);
    if (componentIndex < 0 || boundaryIndex < 0) {
        return Acad::eInvalidInput;
    }

    line.key.side = side == static_cast<Adesk::Int32>(SubgradeSide::Left)
        ? SubgradeSide::Left
        : SubgradeSide::Right;
    line.key.componentType = static_cast<SubgradeComponentType>(type);
    line.key.componentIndex = static_cast<std::size_t>(componentIndex);
    line.key.boundaryIndex = static_cast<std::size_t>(boundaryIndex);

    filer->readInt32(&line.color.r);
    filer->readInt32(&line.color.g);
    filer->readInt32(&line.color.b);

    Adesk::Int32 pointCount = 0;
    if (!readCount(filer, kMaxLinePoints, pointCount)) {
        return Acad::eInvalidInput;
    }
    if (totalPointCount > kMaxTotalPoints - pointCount) {
        return Acad::eInvalidInput;
    }
    totalPointCount += pointCount;

    line.points.clear();
    line.points.reserve(static_cast<std::size_t>(pointCount));
    for (Adesk::Int32 i = 0; i < pointCount; ++i) {
        RoadModelPoint3d point;
        readPoint(filer, point);
        line.points.push_back(point);
    }

    return Acad::eOk;
}

void writeLineKey(AcDbDwgFiler* filer, const RoadModelLineKey& key)
{
    writeWideString(filer, key.templateHandle);
    filer->writeInt32(static_cast<Adesk::Int32>(key.side));
    filer->writeInt32(static_cast<Adesk::Int32>(key.componentType));
    filer->writeInt32(static_cast<Adesk::Int32>(key.componentIndex));
    filer->writeInt32(static_cast<Adesk::Int32>(key.boundaryIndex));
}

void writeComponentLine(AcDbDwgFiler* filer, const RoadModelComponentLine& line)
{
    writeLineKey(filer, line.key);
    filer->writeInt32(line.color.r);
    filer->writeInt32(line.color.g);
    filer->writeInt32(line.color.b);
    filer->writeInt32(static_cast<Adesk::Int32>(line.points.size()));
    for (const auto& point : line.points) {
        writePoint(filer, point);
    }
}

} // namespace

DnRoadModelEntity::DnRoadModelEntity() = default;

void DnRoadModelEntity::setRoadModelData(const RoadModelData& data)
{
    assertWriteEnabled();
    data_ = data;
    markGraphicsModifiedIfResident(*this);
}

const RoadModelData& DnRoadModelEntity::roadModelData() const
{
    return data_;
}

Acad::ErrorStatus DnRoadModelEntity::dwgInFields(AcDbDwgFiler* filer)
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

    RoadModelData readData;
    readData.version = version;
    readData.config.roadCenterlineHandle = readWideString(filer);
    readData.config.profileVerticalCurveHandle = readWideString(filer);
    filer->readDouble(&readData.config.sampleInterval);

    Adesk::Int32 assignmentCount = 0;
    if (!readCount(filer, kMaxAssignments, assignmentCount)) {
        return Acad::eInvalidInput;
    }
    readData.config.assignments.reserve(static_cast<std::size_t>(assignmentCount));
    for (Adesk::Int32 i = 0; i < assignmentCount; ++i) {
        RoadModelTemplateAssignment row;
        readAssignment(filer, row);
        readData.config.assignments.push_back(std::move(row));
    }

    Adesk::Int32 lineCount = 0;
    if (!readCount(filer, kMaxComponentLines, lineCount)) {
        return Acad::eInvalidInput;
    }
    readData.componentLines.reserve(static_cast<std::size_t>(lineCount));
    Adesk::Int32 totalPointCount = 0;
    for (Adesk::Int32 i = 0; i < lineCount; ++i) {
        RoadModelComponentLine line;
        status = readComponentLine(filer, line, totalPointCount);
        if (status != Acad::eOk) {
            return status;
        }
        readData.componentLines.push_back(std::move(line));
    }

    data_ = std::move(readData);
    return filer->filerStatus();
}

Acad::ErrorStatus DnRoadModelEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }
    if (!validatePersistedSizes(data_)) {
        return Acad::eInvalidInput;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, data_.config.roadCenterlineHandle);
    writeWideString(filer, data_.config.profileVerticalCurveHandle);
    filer->writeDouble(data_.config.sampleInterval);

    filer->writeInt32(static_cast<Adesk::Int32>(data_.config.assignments.size()));
    for (const auto& row : data_.config.assignments) {
        writeAssignment(filer, row);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(data_.componentLines.size()));
    for (const auto& line : data_.componentLines) {
        writeComponentLine(filer, line);
    }

    return filer->filerStatus();
}

Adesk::Boolean DnRoadModelEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr) {
        return Adesk::kTrue;
    }

    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    for (const auto& line : data_.componentLines) {
        if (line.points.size() < 2) {
            continue;
        }

        worldDraw->subEntityTraits().setTrueColor(roadModelLineColor(line.color));
        for (std::size_t i = 1; i < line.points.size(); ++i) {
            AcGePoint3d segment[2] = {
                AcGePoint3d(line.points[i - 1].x, line.points[i - 1].y, line.points[i - 1].z),
                AcGePoint3d(line.points[i].x, line.points[i].y, line.points[i].z)};
            worldDraw->geometry().polyline(2, segment);
        }
    }

    return Adesk::kTrue;
}

Acad::ErrorStatus DnRoadModelEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();

    bool hasPoint = false;
    for (const auto& line : data_.componentLines) {
        for (const auto& point : line.points) {
            const AcGePoint3d cadPoint(point.x, point.y, point.z);
            extents.addPoint(cadPoint);
            hasPoint = true;
        }
    }

    return hasPoint ? Acad::eOk : Acad::eInvalidExtents;
}

Acad::ErrorStatus DnRoadModelEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    assertWriteEnabled();

    for (auto& line : data_.componentLines) {
        for (auto& point : line.points) {
            AcGePoint3d cadPoint(point.x, point.y, point.z);
            cadPoint.transformBy(transform);
            point.x = cadPoint.x;
            point.y = cadPoint.y;
            point.z = cadPoint.z;
        }
    }

    markGraphicsModifiedIfResident(*this);
    return Acad::eOk;
}

namespace roadproto::cad_adapter::objectarx {

void initializeRoadModelEntityClass()
{
    DnRoadModelEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeRoadModelEntityClass()
{
    deleteAcRxClass(DnRoadModelEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
