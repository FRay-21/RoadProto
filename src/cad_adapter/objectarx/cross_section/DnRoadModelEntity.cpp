#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbproxy.h"
#include "rxregsvc.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

using roadproto::domain::cross_section::RoadModelComponentLine;
using roadproto::domain::cross_section::RoadModelData;
using roadproto::domain::cross_section::RoadModelGroundProfilePoint;
using roadproto::domain::cross_section::RoadModelLineKey;
using roadproto::domain::cross_section::RoadModelPavementLayerLine;
using roadproto::domain::cross_section::RoadModelPavementLayerLineKey;
using roadproto::domain::cross_section::RoadModelPoint3d;
using roadproto::domain::cross_section::RoadModelSection;
using roadproto::domain::cross_section::RoadModelSectionNode;
using roadproto::domain::cross_section::RoadModelSectionNodeKind;
using roadproto::domain::cross_section::RoadModelSlopeComponentLine;
using roadproto::domain::cross_section::RoadModelSlopeLineKey;
using roadproto::domain::cross_section::RoadModelSlopeTemplateGroup;
using roadproto::domain::cross_section::RoadModelSlopeTemplateReference;
using roadproto::domain::cross_section::RoadModelTemplateAssignment;
using roadproto::domain::cross_section::RoadModelWireColor;
using roadproto::domain::cross_section::RoadModelWireLine;
using roadproto::domain::cross_section::RoadModelWireLineKind;
using roadproto::domain::cross_section::SlopeComponentType;
using roadproto::domain::cross_section::SlopeTemplateRgbColor;
using roadproto::domain::cross_section::SubgradeComponentType;
using roadproto::domain::cross_section::SubgradeSide;
using roadproto::domain::cross_section::SubgradeTemplateRgbColor;

ACRX_DXF_DEFINE_MEMBERS(
    DnRoadModelEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNROADMODELENTITY,
    "RoadProto Road Model");

namespace {

constexpr Adesk::Int16 kEntityVersion = 6;
constexpr Adesk::Int32 kMaxAssignments = 10000;
constexpr Adesk::Int32 kMaxSlopeTemplateGroups = 10000;
constexpr Adesk::Int32 kMaxSlopeTemplateReferences = 10000;
constexpr Adesk::Int32 kMaxComponentLines = 100000;
constexpr Adesk::Int32 kMaxRoadModelSections = 100000;
constexpr Adesk::Int32 kMaxRoadModelSectionNodes = 10000;
constexpr Adesk::Int32 kMaxRoadModelGroundProfilePoints = 10000;
constexpr Adesk::Int32 kMaxRoadModelWireLines = 500000;
constexpr Adesk::Int32 kMaxLinePoints = 100000;
constexpr Adesk::Int32 kMaxTotalPoints = 5000000;
constexpr Adesk::Int32 kMaxSampledStations = 100000;

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

bool isFiniteRoadModelPoint(const RoadModelPoint3d& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool transformedPointIsFinite(const AcGePoint3d& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
}

bool validateAllRoadModelPointsFinite(const RoadModelData& data)
{
    for (const auto& line : data.componentLines) {
        for (const auto& point : line.points) {
            if (!isFiniteRoadModelPoint(point)) {
                return false;
            }
        }
    }
    for (const auto& line : data.slopeLines) {
        for (const auto& point : line.points) {
            if (!isFiniteRoadModelPoint(point)) {
                return false;
            }
        }
    }
    for (const auto& line : data.pavementLayerLines) {
        for (const auto& point : line.points) {
            if (!isFiniteRoadModelPoint(point)) {
                return false;
            }
        }
    }
    for (const auto& section : data.sections) {
        for (const auto& node : section.leftNodes) {
            if (!isFiniteRoadModelPoint(node.point)) {
                return false;
            }
        }
        for (const auto& node : section.rightNodes) {
            if (!isFiniteRoadModelPoint(node.point)) {
                return false;
            }
        }
        for (const auto& node : section.leftPavementLayerNodes) {
            if (!isFiniteRoadModelPoint(node.point)) {
                return false;
            }
        }
        for (const auto& node : section.rightPavementLayerNodes) {
            if (!isFiniteRoadModelPoint(node.point)) {
                return false;
            }
        }
    }
    for (const auto& line : data.wireLines) {
        for (const auto& point : line.points) {
            if (!isFiniteRoadModelPoint(point)) {
                return false;
            }
        }
    }

    return true;
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

bool addToTotalPointCount(Adesk::Int32& totalPointCount, Adesk::Int32 pointCount)
{
    if (pointCount < 0 || totalPointCount > kMaxTotalPoints - pointCount) {
        return false;
    }
    totalPointCount += pointCount;
    return true;
}

void readRoadModelWireColor(AcDbDwgFiler* filer, RoadModelWireColor& color)
{
    filer->readInt32(&color.r);
    filer->readInt32(&color.g);
    filer->readInt32(&color.b);
}

void writeRoadModelWireColor(AcDbDwgFiler* filer, const RoadModelWireColor& color)
{
    filer->writeInt32(color.r);
    filer->writeInt32(color.g);
    filer->writeInt32(color.b);
}

void markGraphicsModifiedIfResident(AcDbEntity& entity)
{
    if (!entity.objectId().isNull()) {
        entity.recordGraphicsModified(true);
    }
}

AcCmEntityColor roadModelLineColor(const SubgradeTemplateRgbColor& color)
{
    return AcCmEntityColor(
        static_cast<Adesk::UInt8>(std::clamp(color.r, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.g, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.b, 0, 255)));
}

AcCmEntityColor roadModelLineColor(const SlopeTemplateRgbColor& color)
{
    return AcCmEntityColor(
        static_cast<Adesk::UInt8>(std::clamp(color.r, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.g, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.b, 0, 255)));
}

AcCmEntityColor roadModelLineColor(const RoadModelWireColor& color)
{
    return AcCmEntityColor(
        static_cast<Adesk::UInt8>(std::clamp(color.r, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.g, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(color.b, 0, 255)));
}

bool isValidSubgradeSideValue(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(SubgradeSide::Left)
        || value == static_cast<Adesk::Int32>(SubgradeSide::Right);
}

bool isValidSubgradeComponentTypeValue(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(SubgradeComponentType::Median)
        || value == static_cast<Adesk::Int32>(SubgradeComponentType::TravelLane)
        || value == static_cast<Adesk::Int32>(SubgradeComponentType::HardShoulder)
        || value == static_cast<Adesk::Int32>(SubgradeComponentType::EarthShoulder)
        || value == static_cast<Adesk::Int32>(SubgradeComponentType::SideMedian)
        || value == static_cast<Adesk::Int32>(SubgradeComponentType::Sidewalk)
        || value == static_cast<Adesk::Int32>(SubgradeComponentType::BikeLane)
        || value == static_cast<Adesk::Int32>(SubgradeComponentType::CurbStrip);
}

bool isValidSlopeComponentTypeValue(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(SlopeComponentType::FillSlope)
        || value == static_cast<Adesk::Int32>(SlopeComponentType::CutSlope)
        || value == static_cast<Adesk::Int32>(SlopeComponentType::Berm);
}

bool isValidRoadModelSectionNodeKindValue(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(RoadModelSectionNodeKind::Subgrade)
        || value == static_cast<Adesk::Int32>(RoadModelSectionNodeKind::Slope)
        || value == static_cast<Adesk::Int32>(RoadModelSectionNodeKind::Ground)
        || value == static_cast<Adesk::Int32>(RoadModelSectionNodeKind::PavementLayer);
}

bool isValidRoadModelWireLineKindValue(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(RoadModelWireLineKind::SectionRib)
        || value == static_cast<Adesk::Int32>(RoadModelWireLineKind::Longitudinal)
        || value == static_cast<Adesk::Int32>(RoadModelWireLineKind::OuterBoundary)
        || value == static_cast<Adesk::Int32>(RoadModelWireLineKind::Transition)
        || value == static_cast<Adesk::Int32>(RoadModelWireLineKind::EndCap)
        || value == static_cast<Adesk::Int32>(RoadModelWireLineKind::PavementLayer);
}

bool isValidLineKey(const RoadModelLineKey& key)
{
    return canWriteInt32(key.componentIndex)
        && canWriteInt32(key.boundaryIndex)
        && isValidSubgradeSideValue(static_cast<Adesk::Int32>(key.side))
        && isValidSubgradeComponentTypeValue(static_cast<Adesk::Int32>(key.componentType));
}

bool isValidSlopeLineKey(const RoadModelSlopeLineKey& key)
{
    return canWriteInt32(key.groupIndex)
        && canWriteInt32(key.templateOrder)
        && canWriteInt32(key.componentIndex)
        && canWriteInt32(key.boundaryIndex)
        && isValidSubgradeSideValue(static_cast<Adesk::Int32>(key.side))
        && isValidSlopeComponentTypeValue(static_cast<Adesk::Int32>(key.componentType));
}

bool isValidPavementLayerLineKey(const RoadModelPavementLayerLineKey& key)
{
    return !key.subgradeTemplateHandle.empty()
        && !key.pavementLayerTemplateHandle.empty()
        && canWriteInt32(key.componentIndex)
        && canWriteInt32(key.layerIndex)
        && canWriteInt32(key.boundaryIndex)
        && isValidSubgradeSideValue(static_cast<Adesk::Int32>(key.side));
}

bool isValidSlopeTemplateGroup(const RoadModelSlopeTemplateGroup& group)
{
    if (!std::isfinite(group.startStation) || !std::isfinite(group.endStation) ||
        group.templates.size() > static_cast<std::size_t>(kMaxSlopeTemplateReferences)) {
        return false;
    }
    for (const auto& reference : group.templates) {
        if (reference.templateHandle.empty()) {
            return false;
        }
    }
    return true;
}

bool isValidRoadModelSectionNode(const RoadModelSectionNode& node, SubgradeSide expectedSide)
{
    return node.side == expectedSide
        && isValidRoadModelSectionNodeKindValue(static_cast<Adesk::Int32>(node.kind))
        && isValidSubgradeSideValue(static_cast<Adesk::Int32>(node.side))
        && std::isfinite(node.offset)
        && std::isfinite(node.elevation)
        && isFiniteRoadModelPoint(node.point);
}

bool isValidRoadModelGroundProfile(const std::vector<RoadModelGroundProfilePoint>& profile)
{
    if (profile.size() > static_cast<std::size_t>(kMaxRoadModelGroundProfilePoints)) {
        return false;
    }
    for (const auto& point : profile) {
        if (!std::isfinite(point.offset) || !std::isfinite(point.elevation)) {
            return false;
        }
    }
    return true;
}

bool isValidRoadModelSection(const RoadModelSection& section)
{
    if (!std::isfinite(section.station)
        || section.leftNodes.size() > static_cast<std::size_t>(kMaxRoadModelSectionNodes)
        || section.rightNodes.size() > static_cast<std::size_t>(kMaxRoadModelSectionNodes)
        || section.leftPavementLayerNodes.size() > static_cast<std::size_t>(kMaxRoadModelSectionNodes)
        || section.rightPavementLayerNodes.size() > static_cast<std::size_t>(kMaxRoadModelSectionNodes)
        || !isValidRoadModelGroundProfile(section.leftGroundProfile)
        || !isValidRoadModelGroundProfile(section.rightGroundProfile)) {
        return false;
    }
    for (const auto& node : section.leftNodes) {
        if (!isValidRoadModelSectionNode(node, SubgradeSide::Left)) {
            return false;
        }
    }
    for (const auto& node : section.rightNodes) {
        if (!isValidRoadModelSectionNode(node, SubgradeSide::Right)) {
            return false;
        }
    }
    for (const auto& node : section.leftPavementLayerNodes) {
        if (!isValidRoadModelSectionNode(node, SubgradeSide::Left)) {
            return false;
        }
    }
    for (const auto& node : section.rightPavementLayerNodes) {
        if (!isValidRoadModelSectionNode(node, SubgradeSide::Right)) {
            return false;
        }
    }
    return true;
}

bool isValidPavementLayerLine(const RoadModelPavementLayerLine& line)
{
    if (!isValidPavementLayerLineKey(line.key)
        || line.points.size() > static_cast<std::size_t>(kMaxLinePoints)) {
        return false;
    }
    for (const auto& point : line.points) {
        if (!isFiniteRoadModelPoint(point)) {
            return false;
        }
    }
    return true;
}

bool isValidRoadModelWireLine(const RoadModelWireLine& line)
{
    if (!isValidRoadModelWireLineKindValue(static_cast<Adesk::Int32>(line.kind))
        || !isValidSubgradeSideValue(static_cast<Adesk::Int32>(line.side))
        || line.points.size() > static_cast<std::size_t>(kMaxLinePoints)) {
        return false;
    }
    for (const auto& point : line.points) {
        if (!isFiniteRoadModelPoint(point)) {
            return false;
        }
    }
    return true;
}

bool validateRoadModelDataForPersistence(const RoadModelData& data)
{
    if (data.config.assignments.size() > static_cast<std::size_t>(kMaxAssignments)
        || data.sampledStations.size() > static_cast<std::size_t>(kMaxSampledStations)
        || data.componentLines.size() > static_cast<std::size_t>(kMaxComponentLines)
        || data.slopeLines.size() > static_cast<std::size_t>(kMaxComponentLines)
        || data.pavementLayerLines.size() > static_cast<std::size_t>(kMaxComponentLines)
        || data.sections.size() > static_cast<std::size_t>(kMaxRoadModelSections)
        || data.wireLines.size() > static_cast<std::size_t>(kMaxRoadModelWireLines)
        || !std::isfinite(data.config.sampleInterval)
        || !std::isfinite(data.config.slopeConfig.leftSearchWidth)
        || !std::isfinite(data.config.slopeConfig.rightSearchWidth)
        || data.config.slopeConfig.leftGroups.size() > static_cast<std::size_t>(kMaxSlopeTemplateGroups)
        || data.config.slopeConfig.rightGroups.size() > static_cast<std::size_t>(kMaxSlopeTemplateGroups)) {
        return false;
    }

    for (const auto& row : data.config.assignments) {
        if (!std::isfinite(row.startStation) || !std::isfinite(row.endStation)) {
            return false;
        }
    }
    for (const auto station : data.sampledStations) {
        if (!std::isfinite(station)) {
            return false;
        }
    }
    for (const auto& group : data.config.slopeConfig.leftGroups) {
        if (!isValidSlopeTemplateGroup(group)) {
            return false;
        }
    }
    for (const auto& group : data.config.slopeConfig.rightGroups) {
        if (!isValidSlopeTemplateGroup(group)) {
            return false;
        }
    }

    std::size_t totalPoints = 0;
    for (const auto& line : data.componentLines) {
        if (line.points.size() > static_cast<std::size_t>(kMaxLinePoints)) {
            return false;
        }
        totalPoints += line.points.size();
        if (totalPoints > static_cast<std::size_t>(kMaxTotalPoints)
            || !isValidLineKey(line.key)) {
            return false;
        }
    }
    for (const auto& line : data.slopeLines) {
        if (line.points.size() > static_cast<std::size_t>(kMaxLinePoints)) {
            return false;
        }
        totalPoints += line.points.size();
        if (totalPoints > static_cast<std::size_t>(kMaxTotalPoints)
            || !isValidSlopeLineKey(line.key)) {
            return false;
        }
    }
    for (const auto& line : data.pavementLayerLines) {
        if (!isValidPavementLayerLine(line)) {
            return false;
        }
        totalPoints += line.points.size();
        if (totalPoints > static_cast<std::size_t>(kMaxTotalPoints)) {
            return false;
        }
    }
    for (const auto& section : data.sections) {
        if (!isValidRoadModelSection(section)) {
            return false;
        }
        totalPoints += section.leftNodes.size()
            + section.rightNodes.size()
            + section.leftPavementLayerNodes.size()
            + section.rightPavementLayerNodes.size()
            + section.leftGroundProfile.size()
            + section.rightGroundProfile.size();
        if (totalPoints > static_cast<std::size_t>(kMaxTotalPoints)) {
            return false;
        }
    }
    for (const auto& line : data.wireLines) {
        if (!isValidRoadModelWireLine(line)) {
            return false;
        }
        totalPoints += line.points.size();
        if (totalPoints > static_cast<std::size_t>(kMaxTotalPoints)) {
            return false;
        }
    }

    return validateAllRoadModelPointsFinite(data);
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

void readSlopeTemplateReference(AcDbDwgFiler* filer, RoadModelSlopeTemplateReference& reference)
{
    reference.templateHandle = readWideString(filer);
    reference.templateName = readWideString(filer);
}

void writeSlopeTemplateReference(
    AcDbDwgFiler* filer,
    const RoadModelSlopeTemplateReference& reference)
{
    writeWideString(filer, reference.templateHandle);
    writeWideString(filer, reference.templateName);
}

Acad::ErrorStatus readSlopeTemplateGroup(
    AcDbDwgFiler* filer,
    RoadModelSlopeTemplateGroup& group)
{
    filer->readDouble(&group.startStation);
    filer->readDouble(&group.endStation);

    Adesk::Int32 referenceCount = 0;
    if (!readCount(filer, kMaxSlopeTemplateReferences, referenceCount)) {
        return Acad::eInvalidInput;
    }
    group.templates.clear();
    group.templates.reserve(static_cast<std::size_t>(referenceCount));
    for (Adesk::Int32 i = 0; i < referenceCount; ++i) {
        RoadModelSlopeTemplateReference reference;
        readSlopeTemplateReference(filer, reference);
        if (reference.templateHandle.empty()) {
            return Acad::eInvalidInput;
        }
        group.templates.push_back(std::move(reference));
    }

    return Acad::eOk;
}

void writeSlopeTemplateGroup(AcDbDwgFiler* filer, const RoadModelSlopeTemplateGroup& group)
{
    filer->writeDouble(group.startStation);
    filer->writeDouble(group.endStation);
    filer->writeInt32(static_cast<Adesk::Int32>(group.templates.size()));
    for (const auto& reference : group.templates) {
        writeSlopeTemplateReference(filer, reference);
    }
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
    if (componentIndex < 0 || boundaryIndex < 0
        || !isValidSubgradeSideValue(side)
        || !isValidSubgradeComponentTypeValue(type)) {
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
        if (!isFiniteRoadModelPoint(point)) {
            return Acad::eInvalidInput;
        }
        line.points.push_back(point);
    }

    return Acad::eOk;
}

Acad::ErrorStatus readSlopeComponentLine(
    AcDbDwgFiler* filer,
    RoadModelSlopeComponentLine& line,
    Adesk::Int32& totalPointCount)
{
    line.key.templateHandle = readWideString(filer);

    Adesk::Int32 side = 0;
    Adesk::Int32 type = 0;
    Adesk::Int32 groupIndex = 0;
    Adesk::Int32 templateOrder = 0;
    Adesk::Int32 componentIndex = 0;
    Adesk::Int32 boundaryIndex = 0;
    filer->readInt32(&side);
    filer->readInt32(&type);
    filer->readInt32(&groupIndex);
    filer->readInt32(&templateOrder);
    filer->readInt32(&componentIndex);
    filer->readInt32(&boundaryIndex);
    if (groupIndex < 0 || templateOrder < 0 || componentIndex < 0 || boundaryIndex < 0
        || !isValidSubgradeSideValue(side)
        || !isValidSlopeComponentTypeValue(type)) {
        return Acad::eInvalidInput;
    }

    line.key.side = side == static_cast<Adesk::Int32>(SubgradeSide::Left)
        ? SubgradeSide::Left
        : SubgradeSide::Right;
    line.key.componentType = static_cast<SlopeComponentType>(type);
    line.key.groupIndex = static_cast<std::size_t>(groupIndex);
    line.key.templateOrder = static_cast<std::size_t>(templateOrder);
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
        if (!isFiniteRoadModelPoint(point)) {
            return Acad::eInvalidInput;
        }
        line.points.push_back(point);
    }

    return Acad::eOk;
}

Acad::ErrorStatus readPavementLayerLine(
    AcDbDwgFiler* filer,
    RoadModelPavementLayerLine& line,
    Adesk::Int32& totalPointCount)
{
    line.key.subgradeTemplateHandle = readWideString(filer);
    line.key.pavementLayerTemplateHandle = readWideString(filer);

    Adesk::Int32 side = 0;
    Adesk::Int32 componentIndex = 0;
    Adesk::Int32 layerIndex = 0;
    Adesk::Int32 boundaryIndex = 0;
    filer->readInt32(&side);
    filer->readInt32(&componentIndex);
    filer->readInt32(&layerIndex);
    filer->readInt32(&boundaryIndex);
    if (componentIndex < 0 || layerIndex < 0 || boundaryIndex < 0
        || !isValidSubgradeSideValue(side)) {
        return Acad::eInvalidInput;
    }

    line.key.side = side == static_cast<Adesk::Int32>(SubgradeSide::Left)
        ? SubgradeSide::Left
        : SubgradeSide::Right;
    line.key.componentIndex = static_cast<std::size_t>(componentIndex);
    line.key.layerIndex = static_cast<std::size_t>(layerIndex);
    line.key.boundaryIndex = static_cast<std::size_t>(boundaryIndex);

    readRoadModelWireColor(filer, line.color);

    Adesk::Int32 pointCount = 0;
    if (!readCount(filer, kMaxLinePoints, pointCount)
        || !addToTotalPointCount(totalPointCount, pointCount)
        || !isValidPavementLayerLineKey(line.key)) {
        return Acad::eInvalidInput;
    }

    line.points.clear();
    line.points.reserve(static_cast<std::size_t>(pointCount));
    for (Adesk::Int32 i = 0; i < pointCount; ++i) {
        RoadModelPoint3d point;
        readPoint(filer, point);
        if (!isFiniteRoadModelPoint(point)) {
            return Acad::eInvalidInput;
        }
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

void writeSlopeLineKey(AcDbDwgFiler* filer, const RoadModelSlopeLineKey& key)
{
    writeWideString(filer, key.templateHandle);
    filer->writeInt32(static_cast<Adesk::Int32>(key.side));
    filer->writeInt32(static_cast<Adesk::Int32>(key.componentType));
    filer->writeInt32(static_cast<Adesk::Int32>(key.groupIndex));
    filer->writeInt32(static_cast<Adesk::Int32>(key.templateOrder));
    filer->writeInt32(static_cast<Adesk::Int32>(key.componentIndex));
    filer->writeInt32(static_cast<Adesk::Int32>(key.boundaryIndex));
}

void writePavementLayerLineKey(AcDbDwgFiler* filer, const RoadModelPavementLayerLineKey& key)
{
    writeWideString(filer, key.subgradeTemplateHandle);
    writeWideString(filer, key.pavementLayerTemplateHandle);
    filer->writeInt32(static_cast<Adesk::Int32>(key.side));
    filer->writeInt32(static_cast<Adesk::Int32>(key.componentIndex));
    filer->writeInt32(static_cast<Adesk::Int32>(key.layerIndex));
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

void writeSlopeComponentLine(AcDbDwgFiler* filer, const RoadModelSlopeComponentLine& line)
{
    writeSlopeLineKey(filer, line.key);
    filer->writeInt32(line.color.r);
    filer->writeInt32(line.color.g);
    filer->writeInt32(line.color.b);
    filer->writeInt32(static_cast<Adesk::Int32>(line.points.size()));
    for (const auto& point : line.points) {
        writePoint(filer, point);
    }
}

void writePavementLayerLine(AcDbDwgFiler* filer, const RoadModelPavementLayerLine& line)
{
    writePavementLayerLineKey(filer, line.key);
    writeRoadModelWireColor(filer, line.color);
    filer->writeInt32(static_cast<Adesk::Int32>(line.points.size()));
    for (const auto& point : line.points) {
        writePoint(filer, point);
    }
}

Acad::ErrorStatus readRoadModelSectionNode(
    AcDbDwgFiler* filer,
    SubgradeSide expectedSide,
    RoadModelSectionNode& node,
    Adesk::Int32& totalPointCount)
{
    Adesk::Int32 kind = 0;
    Adesk::Int32 side = 0;
    filer->readInt32(&kind);
    filer->readInt32(&side);
    filer->readDouble(&node.offset);
    filer->readDouble(&node.elevation);
    readPoint(filer, node.point);
    readRoadModelWireColor(filer, node.color);
    node.label = readWideString(filer);

    if (!isValidRoadModelSectionNodeKindValue(kind)
        || !isValidSubgradeSideValue(side)
        || side != static_cast<Adesk::Int32>(expectedSide)
        || !std::isfinite(node.offset)
        || !std::isfinite(node.elevation)
        || !isFiniteRoadModelPoint(node.point)
        || !addToTotalPointCount(totalPointCount, 1)) {
        return Acad::eInvalidInput;
    }

    node.kind = static_cast<RoadModelSectionNodeKind>(kind);
    node.side = expectedSide;
    return Acad::eOk;
}

void writeRoadModelSectionNode(AcDbDwgFiler* filer, const RoadModelSectionNode& node)
{
    filer->writeInt32(static_cast<Adesk::Int32>(node.kind));
    filer->writeInt32(static_cast<Adesk::Int32>(node.side));
    filer->writeDouble(node.offset);
    filer->writeDouble(node.elevation);
    writePoint(filer, node.point);
    writeRoadModelWireColor(filer, node.color);
    writeWideString(filer, node.label);
}

Acad::ErrorStatus readRoadModelGroundProfile(
    AcDbDwgFiler* filer,
    std::vector<RoadModelGroundProfilePoint>& profile,
    Adesk::Int32& totalPointCount)
{
    Adesk::Int32 pointCount = 0;
    if (!readCount(filer, kMaxRoadModelGroundProfilePoints, pointCount)
        || !addToTotalPointCount(totalPointCount, pointCount)) {
        return Acad::eInvalidInput;
    }

    profile.clear();
    profile.reserve(static_cast<std::size_t>(pointCount));
    for (Adesk::Int32 i = 0; i < pointCount; ++i) {
        RoadModelGroundProfilePoint point;
        filer->readDouble(&point.offset);
        filer->readDouble(&point.elevation);
        if (!std::isfinite(point.offset) || !std::isfinite(point.elevation)) {
            return Acad::eInvalidInput;
        }
        profile.push_back(point);
    }
    return Acad::eOk;
}

void writeRoadModelGroundProfile(
    AcDbDwgFiler* filer,
    const std::vector<RoadModelGroundProfilePoint>& profile)
{
    filer->writeInt32(static_cast<Adesk::Int32>(profile.size()));
    for (const auto& point : profile) {
        filer->writeDouble(point.offset);
        filer->writeDouble(point.elevation);
    }
}

Acad::ErrorStatus readRoadModelSection(
    AcDbDwgFiler* filer,
    RoadModelSection& section,
    Adesk::Int16 version,
    Adesk::Int32& totalPointCount)
{
    filer->readDouble(&section.station);

    Adesk::Int32 succeeded = 0;
    filer->readInt32(&succeeded);
    section.errorMessage = readWideString(filer);
    if (!std::isfinite(section.station) || (succeeded != 0 && succeeded != 1)) {
        return Acad::eInvalidInput;
    }
    section.succeeded = succeeded != 0;

    Adesk::Int32 leftNodeCount = 0;
    if (!readCount(filer, kMaxRoadModelSectionNodes, leftNodeCount)) {
        return Acad::eInvalidInput;
    }
    section.leftNodes.clear();
    section.leftNodes.reserve(static_cast<std::size_t>(leftNodeCount));
    for (Adesk::Int32 i = 0; i < leftNodeCount; ++i) {
        RoadModelSectionNode node;
        const auto status = readRoadModelSectionNode(
            filer,
            SubgradeSide::Left,
            node,
            totalPointCount);
        if (status != Acad::eOk) {
            return status;
        }
        section.leftNodes.push_back(std::move(node));
    }

    Adesk::Int32 rightNodeCount = 0;
    if (!readCount(filer, kMaxRoadModelSectionNodes, rightNodeCount)) {
        return Acad::eInvalidInput;
    }
    section.rightNodes.clear();
    section.rightNodes.reserve(static_cast<std::size_t>(rightNodeCount));
    for (Adesk::Int32 i = 0; i < rightNodeCount; ++i) {
        RoadModelSectionNode node;
        const auto status = readRoadModelSectionNode(
            filer,
            SubgradeSide::Right,
            node,
            totalPointCount);
        if (status != Acad::eOk) {
            return status;
        }
        section.rightNodes.push_back(std::move(node));
    }

    section.leftPavementLayerNodes.clear();
    section.rightPavementLayerNodes.clear();
    if (version >= 6) {
        Adesk::Int32 leftPavementLayerNodeCount = 0;
        if (!readCount(filer, kMaxRoadModelSectionNodes, leftPavementLayerNodeCount)) {
            return Acad::eInvalidInput;
        }
        section.leftPavementLayerNodes.reserve(static_cast<std::size_t>(leftPavementLayerNodeCount));
        for (Adesk::Int32 i = 0; i < leftPavementLayerNodeCount; ++i) {
            RoadModelSectionNode node;
            const auto status = readRoadModelSectionNode(
                filer,
                SubgradeSide::Left,
                node,
                totalPointCount);
            if (status != Acad::eOk) {
                return status;
            }
            section.leftPavementLayerNodes.push_back(std::move(node));
        }

        Adesk::Int32 rightPavementLayerNodeCount = 0;
        if (!readCount(filer, kMaxRoadModelSectionNodes, rightPavementLayerNodeCount)) {
            return Acad::eInvalidInput;
        }
        section.rightPavementLayerNodes.reserve(static_cast<std::size_t>(rightPavementLayerNodeCount));
        for (Adesk::Int32 i = 0; i < rightPavementLayerNodeCount; ++i) {
            RoadModelSectionNode node;
            const auto status = readRoadModelSectionNode(
                filer,
                SubgradeSide::Right,
                node,
                totalPointCount);
            if (status != Acad::eOk) {
                return status;
            }
            section.rightPavementLayerNodes.push_back(std::move(node));
        }
    }

    section.leftGroundProfile.clear();
    section.rightGroundProfile.clear();
    if (version >= 5) {
        auto status = readRoadModelGroundProfile(
            filer,
            section.leftGroundProfile,
            totalPointCount);
        if (status != Acad::eOk) {
            return status;
        }
        status = readRoadModelGroundProfile(
            filer,
            section.rightGroundProfile,
            totalPointCount);
        if (status != Acad::eOk) {
            return status;
        }
    }

    return Acad::eOk;
}

void writeRoadModelSection(AcDbDwgFiler* filer, const RoadModelSection& section)
{
    filer->writeDouble(section.station);
    filer->writeInt32(section.succeeded ? 1 : 0);
    writeWideString(filer, section.errorMessage);

    filer->writeInt32(static_cast<Adesk::Int32>(section.leftNodes.size()));
    for (const auto& node : section.leftNodes) {
        writeRoadModelSectionNode(filer, node);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(section.rightNodes.size()));
    for (const auto& node : section.rightNodes) {
        writeRoadModelSectionNode(filer, node);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(section.leftPavementLayerNodes.size()));
    for (const auto& node : section.leftPavementLayerNodes) {
        writeRoadModelSectionNode(filer, node);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(section.rightPavementLayerNodes.size()));
    for (const auto& node : section.rightPavementLayerNodes) {
        writeRoadModelSectionNode(filer, node);
    }

    writeRoadModelGroundProfile(filer, section.leftGroundProfile);
    writeRoadModelGroundProfile(filer, section.rightGroundProfile);
}

Acad::ErrorStatus readRoadModelWireLine(
    AcDbDwgFiler* filer,
    RoadModelWireLine& line,
    Adesk::Int32& totalPointCount)
{
    Adesk::Int32 kind = 0;
    Adesk::Int32 side = 0;
    filer->readInt32(&kind);
    filer->readInt32(&side);
    readRoadModelWireColor(filer, line.color);
    if (!isValidRoadModelWireLineKindValue(kind) || !isValidSubgradeSideValue(side)) {
        return Acad::eInvalidInput;
    }

    line.kind = static_cast<RoadModelWireLineKind>(kind);
    line.side = side == static_cast<Adesk::Int32>(SubgradeSide::Left)
        ? SubgradeSide::Left
        : SubgradeSide::Right;

    Adesk::Int32 pointCount = 0;
    if (!readCount(filer, kMaxLinePoints, pointCount)
        || !addToTotalPointCount(totalPointCount, pointCount)) {
        return Acad::eInvalidInput;
    }

    line.points.clear();
    line.points.reserve(static_cast<std::size_t>(pointCount));
    for (Adesk::Int32 i = 0; i < pointCount; ++i) {
        RoadModelPoint3d point;
        readPoint(filer, point);
        if (!isFiniteRoadModelPoint(point)) {
            return Acad::eInvalidInput;
        }
        line.points.push_back(point);
    }

    return Acad::eOk;
}

void writeRoadModelWireLine(AcDbDwgFiler* filer, const RoadModelWireLine& line)
{
    filer->writeInt32(static_cast<Adesk::Int32>(line.kind));
    filer->writeInt32(static_cast<Adesk::Int32>(line.side));
    writeRoadModelWireColor(filer, line.color);
    filer->writeInt32(static_cast<Adesk::Int32>(line.points.size()));
    for (const auto& point : line.points) {
        writePoint(filer, point);
    }
}

void drawRoadModelPolyline(AcGiWorldDraw* worldDraw, const std::vector<RoadModelPoint3d>& points)
{
    if (worldDraw == nullptr || points.size() < 2) {
        return;
    }

    for (std::size_t i = 1; i < points.size(); ++i) {
        if (!isFiniteRoadModelPoint(points[i - 1]) || !isFiniteRoadModelPoint(points[i])) {
            continue;
        }
        AcGePoint3d segment[2] = {
            AcGePoint3d(points[i - 1].x, points[i - 1].y, points[i - 1].z),
            AcGePoint3d(points[i].x, points[i].y, points[i].z)};
        worldDraw->geometry().polyline(2, segment);
    }
}

void drawRoadModelWireLines(
    AcGiWorldDraw* worldDraw,
    const std::vector<RoadModelWireLine>& wireLines)
{
    for (const auto& line : wireLines) {
        if (line.points.size() < 2) {
            continue;
        }
        worldDraw->subEntityTraits().setTrueColor(roadModelLineColor(line.color));
        drawRoadModelPolyline(worldDraw, line.points);
    }
}

void drawPavementLayerLines(
    AcGiWorldDraw* worldDraw,
    const std::vector<RoadModelPavementLayerLine>& pavementLayerLines)
{
    for (const auto& line : pavementLayerLines) {
        if (line.points.size() < 2) {
            continue;
        }
        worldDraw->subEntityTraits().setTrueColor(roadModelLineColor(line.color));
        drawRoadModelPolyline(worldDraw, line.points);
    }
}

void addPointToRoadModelExtents(
    AcDbExtents& extents,
    const RoadModelPoint3d& point,
    bool& hasPoint)
{
    if (!isFiniteRoadModelPoint(point)) {
        return;
    }

    extents.addPoint(AcGePoint3d(point.x, point.y, point.z));
    hasPoint = true;
}

Acad::ErrorStatus transformRoadModelPoint(
    RoadModelPoint3d& point,
    const AcGeMatrix3d& transform)
{
    AcGePoint3d cadPoint(point.x, point.y, point.z);
    cadPoint.transformBy(transform);
    if (!transformedPointIsFinite(cadPoint)) {
        return Acad::eInvalidInput;
    }

    point.x = cadPoint.x;
    point.y = cadPoint.y;
    point.z = cadPoint.z;
    return Acad::eOk;
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
    if (version < 0) {
        return Acad::eInvalidInput;
    }
    if (version > kEntityVersion) {
        return Acad::eMakeMeProxy;
    }

    RoadModelData readData;
    readData.version = version;
    readData.config.roadCenterlineHandle = readWideString(filer);
    readData.config.profileVerticalCurveHandle = readWideString(filer);
    filer->readDouble(&readData.config.sampleInterval);

    if (version >= 3) {
        Adesk::Int32 stationCount = 0;
        if (!readCount(filer, kMaxSampledStations, stationCount)) {
            return Acad::eInvalidInput;
        }
        readData.sampledStations.reserve(static_cast<std::size_t>(stationCount));
        for (Adesk::Int32 i = 0; i < stationCount; ++i) {
            double station = 0.0;
            filer->readDouble(&station);
            if (!std::isfinite(station)) {
                return Acad::eInvalidInput;
            }
            readData.sampledStations.push_back(station);
        }
    }

    Adesk::Int32 assignmentCount = 0;
    if (!readCount(filer, kMaxAssignments, assignmentCount)) {
        return Acad::eInvalidInput;
    }
    readData.config.assignments.reserve(static_cast<std::size_t>(assignmentCount));
    for (Adesk::Int32 i = 0; i < assignmentCount; ++i) {
        RoadModelTemplateAssignment row;
        readAssignment(filer, row);
        if (!std::isfinite(row.startStation) || !std::isfinite(row.endStation)) {
            return Acad::eInvalidInput;
        }
        readData.config.assignments.push_back(std::move(row));
    }

    if (version >= 2) {
        filer->readDouble(&readData.config.slopeConfig.leftSearchWidth);
        filer->readDouble(&readData.config.slopeConfig.rightSearchWidth);

        Adesk::Int32 leftGroupCount = 0;
        if (!readCount(filer, kMaxSlopeTemplateGroups, leftGroupCount)) {
            return Acad::eInvalidInput;
        }
        readData.config.slopeConfig.leftGroups.reserve(static_cast<std::size_t>(leftGroupCount));
        for (Adesk::Int32 i = 0; i < leftGroupCount; ++i) {
            RoadModelSlopeTemplateGroup group;
            status = readSlopeTemplateGroup(filer, group);
            if (status != Acad::eOk) {
                return status;
            }
            readData.config.slopeConfig.leftGroups.push_back(std::move(group));
        }

        Adesk::Int32 rightGroupCount = 0;
        if (!readCount(filer, kMaxSlopeTemplateGroups, rightGroupCount)) {
            return Acad::eInvalidInput;
        }
        readData.config.slopeConfig.rightGroups.reserve(static_cast<std::size_t>(rightGroupCount));
        for (Adesk::Int32 i = 0; i < rightGroupCount; ++i) {
            RoadModelSlopeTemplateGroup group;
            status = readSlopeTemplateGroup(filer, group);
            if (status != Acad::eOk) {
                return status;
            }
            readData.config.slopeConfig.rightGroups.push_back(std::move(group));
        }
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

    if (version >= 2) {
        Adesk::Int32 slopeLineCount = 0;
        if (!readCount(filer, kMaxComponentLines, slopeLineCount)) {
            return Acad::eInvalidInput;
        }
        readData.slopeLines.reserve(static_cast<std::size_t>(slopeLineCount));
        for (Adesk::Int32 i = 0; i < slopeLineCount; ++i) {
            RoadModelSlopeComponentLine line;
            status = readSlopeComponentLine(filer, line, totalPointCount);
            if (status != Acad::eOk) {
                return status;
            }
            readData.slopeLines.push_back(std::move(line));
        }
    }

    if (version >= 6) {
        Adesk::Int32 pavementLayerLineCount = 0;
        if (!readCount(filer, kMaxComponentLines, pavementLayerLineCount)) {
            return Acad::eInvalidInput;
        }
        readData.pavementLayerLines.reserve(static_cast<std::size_t>(pavementLayerLineCount));
        for (Adesk::Int32 i = 0; i < pavementLayerLineCount; ++i) {
            RoadModelPavementLayerLine line;
            status = readPavementLayerLine(filer, line, totalPointCount);
            if (status != Acad::eOk) {
                return status;
            }
            readData.pavementLayerLines.push_back(std::move(line));
        }
    }

    if (version >= 4) {
        Adesk::Int32 sectionCount = 0;
        if (!readCount(filer, kMaxRoadModelSections, sectionCount)) {
            return Acad::eInvalidInput;
        }
        readData.sections.reserve(static_cast<std::size_t>(sectionCount));
        for (Adesk::Int32 i = 0; i < sectionCount; ++i) {
            RoadModelSection section;
            status = readRoadModelSection(filer, section, version, totalPointCount);
            if (status != Acad::eOk) {
                return status;
            }
            readData.sections.push_back(std::move(section));
        }

        Adesk::Int32 wireLineCount = 0;
        if (!readCount(filer, kMaxRoadModelWireLines, wireLineCount)) {
            return Acad::eInvalidInput;
        }
        readData.wireLines.reserve(static_cast<std::size_t>(wireLineCount));
        for (Adesk::Int32 i = 0; i < wireLineCount; ++i) {
            RoadModelWireLine line;
            status = readRoadModelWireLine(filer, line, totalPointCount);
            if (status != Acad::eOk) {
                return status;
            }
            readData.wireLines.push_back(std::move(line));
        }
    }

    if (!validateRoadModelDataForPersistence(readData)) {
        return Acad::eInvalidInput;
    }

    const auto finalStatus = filer->filerStatus();
    if (finalStatus != Acad::eOk) {
        return finalStatus;
    }

    data_ = std::move(readData);
    return finalStatus;
}

Acad::ErrorStatus DnRoadModelEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }
    if (!validateRoadModelDataForPersistence(data_)) {
        return Acad::eInvalidInput;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, data_.config.roadCenterlineHandle);
    writeWideString(filer, data_.config.profileVerticalCurveHandle);
    filer->writeDouble(data_.config.sampleInterval);

    filer->writeInt32(static_cast<Adesk::Int32>(data_.sampledStations.size()));
    for (const auto station : data_.sampledStations) {
        filer->writeDouble(station);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(data_.config.assignments.size()));
    for (const auto& row : data_.config.assignments) {
        writeAssignment(filer, row);
    }

    filer->writeDouble(data_.config.slopeConfig.leftSearchWidth);
    filer->writeDouble(data_.config.slopeConfig.rightSearchWidth);
    filer->writeInt32(static_cast<Adesk::Int32>(data_.config.slopeConfig.leftGroups.size()));
    for (const auto& group : data_.config.slopeConfig.leftGroups) {
        writeSlopeTemplateGroup(filer, group);
    }
    filer->writeInt32(static_cast<Adesk::Int32>(data_.config.slopeConfig.rightGroups.size()));
    for (const auto& group : data_.config.slopeConfig.rightGroups) {
        writeSlopeTemplateGroup(filer, group);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(data_.componentLines.size()));
    for (const auto& line : data_.componentLines) {
        writeComponentLine(filer, line);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(data_.slopeLines.size()));
    for (const auto& line : data_.slopeLines) {
        writeSlopeComponentLine(filer, line);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(data_.pavementLayerLines.size()));
    for (const auto& line : data_.pavementLayerLines) {
        writePavementLayerLine(filer, line);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(data_.sections.size()));
    for (const auto& section : data_.sections) {
        writeRoadModelSection(filer, section);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(data_.wireLines.size()));
    for (const auto& line : data_.wireLines) {
        writeRoadModelWireLine(filer, line);
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
    if (!data_.wireLines.empty()) {
        drawRoadModelWireLines(worldDraw, data_.wireLines);
        return Adesk::kTrue;
    }

    for (const auto& line : data_.componentLines) {
        if (line.points.size() < 2) {
            continue;
        }

        worldDraw->subEntityTraits().setTrueColor(roadModelLineColor(line.color));
        drawRoadModelPolyline(worldDraw, line.points);
    }
    for (const auto& line : data_.slopeLines) {
        if (line.points.size() < 2) {
            continue;
        }

        worldDraw->subEntityTraits().setTrueColor(roadModelLineColor(line.color));
        drawRoadModelPolyline(worldDraw, line.points);
    }
    drawPavementLayerLines(worldDraw, data_.pavementLayerLines);

    return Adesk::kTrue;
}

Acad::ErrorStatus DnRoadModelEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();

    bool hasPoint = false;
    for (const auto& line : data_.componentLines) {
        for (const auto& point : line.points) {
            addPointToRoadModelExtents(extents, point, hasPoint);
        }
    }
    for (const auto& line : data_.slopeLines) {
        for (const auto& point : line.points) {
            addPointToRoadModelExtents(extents, point, hasPoint);
        }
    }
    for (const auto& line : data_.pavementLayerLines) {
        for (const auto& point : line.points) {
            addPointToRoadModelExtents(extents, point, hasPoint);
        }
    }
    for (const auto& section : data_.sections) {
        for (const auto& node : section.leftNodes) {
            addPointToRoadModelExtents(extents, node.point, hasPoint);
        }
        for (const auto& node : section.rightNodes) {
            addPointToRoadModelExtents(extents, node.point, hasPoint);
        }
        for (const auto& node : section.leftPavementLayerNodes) {
            addPointToRoadModelExtents(extents, node.point, hasPoint);
        }
        for (const auto& node : section.rightPavementLayerNodes) {
            addPointToRoadModelExtents(extents, node.point, hasPoint);
        }
    }
    for (const auto& line : data_.wireLines) {
        for (const auto& point : line.points) {
            addPointToRoadModelExtents(extents, point, hasPoint);
        }
    }

    return hasPoint ? Acad::eOk : Acad::eInvalidExtents;
}

Acad::ErrorStatus DnRoadModelEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    assertWriteEnabled();

    if (!validateAllRoadModelPointsFinite(data_)) {
        return Acad::eInvalidInput;
    }

    auto transformedData = data_;
    for (auto& line : transformedData.componentLines) {
        for (auto& point : line.points) {
            const auto status = transformRoadModelPoint(point, transform);
            if (status != Acad::eOk) {
                return status;
            }
        }
    }
    for (auto& line : transformedData.slopeLines) {
        for (auto& point : line.points) {
            const auto status = transformRoadModelPoint(point, transform);
            if (status != Acad::eOk) {
                return status;
            }
        }
    }
    for (auto& line : transformedData.pavementLayerLines) {
        for (auto& point : line.points) {
            const auto status = transformRoadModelPoint(point, transform);
            if (status != Acad::eOk) {
                return status;
            }
        }
    }
    for (auto& section : transformedData.sections) {
        for (auto& node : section.leftNodes) {
            const auto status = transformRoadModelPoint(node.point, transform);
            if (status != Acad::eOk) {
                return status;
            }
        }
        for (auto& node : section.rightNodes) {
            const auto status = transformRoadModelPoint(node.point, transform);
            if (status != Acad::eOk) {
                return status;
            }
        }
        for (auto& node : section.leftPavementLayerNodes) {
            const auto status = transformRoadModelPoint(node.point, transform);
            if (status != Acad::eOk) {
                return status;
            }
        }
        for (auto& node : section.rightPavementLayerNodes) {
            const auto status = transformRoadModelPoint(node.point, transform);
            if (status != Acad::eOk) {
                return status;
            }
        }
    }
    for (auto& line : transformedData.wireLines) {
        for (auto& point : line.points) {
            const auto status = transformRoadModelPoint(point, transform);
            if (status != Acad::eOk) {
                return status;
            }
        }
    }

    data_ = std::move(transformedData);
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
