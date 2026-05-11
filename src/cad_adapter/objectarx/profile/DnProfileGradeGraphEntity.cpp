#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"

#include "domain/alignment/StationFormatter.h"
#include "domain/profile/ProfileGradeGraphLayout.h"

#include "acgi.h"
#include "acutmem.h"
#include "dbproxy.h"
#include "rxregsvc.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <utility>

using roadproto::domain::profile::ProfileGradeGraphData;
using roadproto::domain::profile::ProfileGradeGraphLayout;
using roadproto::domain::profile::ProfileGradeGraphLayoutResult;
using roadproto::domain::profile::ProfileGradeGraphProperties;
using roadproto::domain::profile::ProfileGroundSample;
using roadproto::domain::profile::ProfileGroundSourceType;

ACRX_DXF_DEFINE_MEMBERS(
    DnProfileGradeGraphEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNPROFILEGRADEGRAPHENTITY,
    "RoadProto Profile Grade Graph");

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;
constexpr double kTextHeight = 2.5;
constexpr double kTitleOffset = 8.0;
constexpr double kTableHeaderHeight = 10.0;
constexpr double kLabelOffset = 2.0;
constexpr double kStationGridSpacing = 100.0;

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

std::wstring numberText(double value, int precision = 2)
{
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

AcGePoint3d graphPoint(const AcGePoint3d& origin, double x, double y)
{
    return AcGePoint3d(origin.x + x, origin.y + y, origin.z);
}

void drawLine(AcGiWorldDraw* worldDraw, const AcGePoint3d& origin, double x1, double y1, double x2, double y2)
{
    AcGePoint3d line[2] = {
        graphPoint(origin, x1, y1),
        graphPoint(origin, x2, y2)};
    worldDraw->geometry().polyline(2, line);
}

void drawText(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    double x,
    double y,
    const std::wstring& text,
    double height = kTextHeight)
{
    if (worldDraw == nullptr || text.empty()) {
        return;
    }

    worldDraw->geometry().text(
        graphPoint(origin, x, y),
        AcGeVector3d::kZAxis,
        AcGeVector3d::kXAxis,
        height,
        1.0,
        0.0,
        text.c_str());
}

void drawFrame(AcGiWorldDraw* worldDraw, const AcGePoint3d& origin, const ProfileGradeGraphLayoutResult& layout)
{
    worldDraw->subEntityTraits().setColor(7);
    drawLine(worldDraw, origin, 0.0, 0.0, layout.graphWidth, 0.0);
    drawLine(worldDraw, origin, layout.graphWidth, 0.0, layout.graphWidth, layout.graphHeight);
    drawLine(worldDraw, origin, layout.graphWidth, layout.graphHeight, 0.0, layout.graphHeight);
    drawLine(worldDraw, origin, 0.0, layout.graphHeight, 0.0, 0.0);
}

void drawGrid(AcGiWorldDraw* worldDraw, const AcGePoint3d& origin, const ProfileGradeGraphData& graph, const ProfileGradeGraphLayoutResult& layout)
{
    const auto gridSpacing = graph.properties.gridSpacing;
    const auto verticalScale = graph.properties.verticalScale;

    worldDraw->subEntityTraits().setColor(8);
    const auto topElevation = std::ceil(layout.maxElevation / gridSpacing) * gridSpacing;
    for (double elevation = layout.baseElevation; elevation <= topElevation + 1e-9; elevation += gridSpacing) {
        const auto y = (elevation - layout.baseElevation) * verticalScale;
        drawLine(worldDraw, origin, 0.0, y, layout.graphWidth, y);
    }

    const auto firstStation = std::ceil(layout.minStation / kStationGridSpacing) * kStationGridSpacing;
    for (double station = firstStation; station <= layout.maxStation + 1e-9; station += kStationGridSpacing) {
        const auto x = ProfileGradeGraphLayout::mapX(layout, station);
        drawLine(worldDraw, origin, x, 0.0, x, layout.graphHeight);
    }
}

void drawLabels(AcGiWorldDraw* worldDraw, const AcGePoint3d& origin, const ProfileGradeGraphData& graph, const ProfileGradeGraphLayoutResult& layout)
{
    const auto gridSpacing = graph.properties.gridSpacing;
    const auto verticalScale = graph.properties.verticalScale;

    worldDraw->subEntityTraits().setColor(3);
    const auto topElevation = std::ceil(layout.maxElevation / gridSpacing) * gridSpacing;
    for (double elevation = layout.baseElevation; elevation <= topElevation + 1e-9; elevation += gridSpacing) {
        const auto y = (elevation - layout.baseElevation) * verticalScale;
        drawText(worldDraw, origin, -18.0, y - kTextHeight * 0.5, numberText(elevation, 2));
    }

    const auto firstStation = std::ceil(layout.minStation / kStationGridSpacing) * kStationGridSpacing;
    for (double station = firstStation; station <= layout.maxStation + 1e-9; station += kStationGridSpacing) {
        const auto x = ProfileGradeGraphLayout::mapX(layout, station);
        drawText(worldDraw, origin, x - 8.0, -kTableHeaderHeight - kTextHeight, roadproto::domain::alignment::StationFormatter::format(station));
    }
}

void drawTitleAndHeader(AcGiWorldDraw* worldDraw, const AcGePoint3d& origin, const ProfileGradeGraphData& graph, const ProfileGradeGraphLayoutResult& layout)
{
    worldDraw->subEntityTraits().setColor(2);
    const auto title = graph.properties.graphName.empty() ? L"Profile Grade Graph" : graph.properties.graphName;
    drawText(worldDraw, origin, 0.0, layout.graphHeight + kTitleOffset, title, kTextHeight * 1.25);

    worldDraw->subEntityTraits().setColor(7);
    drawLine(worldDraw, origin, 0.0, -kTableHeaderHeight, layout.graphWidth, -kTableHeaderHeight);
    drawText(worldDraw, origin, 0.0, -kTableHeaderHeight + kLabelOffset, L"Station");
    drawText(worldDraw, origin, 42.0, -kTableHeaderHeight + kLabelOffset, L"Ground elevation");
}

void drawGroundLine(AcGiWorldDraw* worldDraw, const AcGePoint3d& origin, const ProfileGradeGraphData& graph, const ProfileGradeGraphLayoutResult& layout)
{
    if (layout.mappedPoints.size() < 2) {
        return;
    }

    const auto color = graph.properties.groundLineColorIndex > 0
        ? static_cast<Adesk::UInt16>(graph.properties.groundLineColorIndex)
        : static_cast<Adesk::UInt16>(4);
    worldDraw->subEntityTraits().setColor(color);

    for (std::size_t i = 1; i < layout.mappedPoints.size(); ++i) {
        const auto& previous = layout.mappedPoints[i - 1];
        const auto& current = layout.mappedPoints[i];
        drawLine(worldDraw, origin, previous.x, previous.y, current.x, current.y);
    }
}

} // namespace

DnProfileGradeGraphEntity::DnProfileGradeGraphEntity()
    : insertionPoint_(0.0, 0.0, 0.0)
{
}

void DnProfileGradeGraphEntity::setGraphData(const ProfileGradeGraphData& data)
{
    graphData_ = data;
}

const ProfileGradeGraphData& DnProfileGradeGraphEntity::graphData() const
{
    return graphData_;
}

void DnProfileGradeGraphEntity::setInsertionPoint(const AcGePoint3d& point)
{
    insertionPoint_ = point;
}

AcGePoint3d DnProfileGradeGraphEntity::insertionPoint() const
{
    return insertionPoint_;
}

void DnProfileGradeGraphEntity::setProperties(const ProfileGradeGraphProperties& properties)
{
    graphData_.properties = properties;
}

bool DnProfileGradeGraphEntity::replaceGroundSamples(const std::vector<ProfileGroundSample>& samples)
{
    auto graph = graphData_;
    graph.groundSamples = samples;
    const auto layout = ProfileGradeGraphLayout::calculate(graph);
    if (!layout.succeeded) {
        return false;
    }

    graphData_.groundSamples = samples;
    return true;
}

Acad::ErrorStatus DnProfileGradeGraphEntity::dwgInFields(AcDbDwgFiler* filer)
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

    Adesk::Int32 sourceType = 0;
    filer->readInt32(&sourceType);
    graphData_.sourceType = static_cast<ProfileGroundSourceType>(sourceType);
    graphData_.roadCenterlineHandle = readWideString(filer);
    graphData_.terrainTinHandle = readWideString(filer);
    graphData_.dmxFilePath = readWideString(filer);

    graphData_.properties.graphName = readWideString(filer);
    filer->readInt32(&graphData_.properties.groundLineColorIndex);
    filer->readDouble(&graphData_.properties.groundLineWidth);
    filer->readDouble(&graphData_.properties.groundLinePrecision);
    filer->readDouble(&graphData_.properties.verticalScale);
    filer->readDouble(&graphData_.properties.gridSpacing);

    Adesk::Int32 sampleCount = 0;
    filer->readInt32(&sampleCount);
    graphData_.groundSamples.clear();
    graphData_.groundSamples.reserve(sampleCount > 0 ? static_cast<std::size_t>(sampleCount) : 0);
    for (Adesk::Int32 i = 0; i < sampleCount; ++i) {
        ProfileGroundSample sample;
        filer->readDouble(&sample.station);
        filer->readDouble(&sample.elevation);
        sample.rawStationText = readWideString(filer);
        const bool hasBreakChain = readBool(filer);
        Adesk::Int32 breakChainIndex = 0;
        filer->readInt32(&breakChainIndex);
        if (hasBreakChain) {
            sample.breakChainIndex = static_cast<int>(breakChainIndex);
        }
        graphData_.groundSamples.push_back(std::move(sample));
    }

    return filer->filerStatus();
}

Acad::ErrorStatus DnProfileGradeGraphEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    filer->writeInt16(kEntityVersion);
    filer->writeDouble(insertionPoint_.x);
    filer->writeDouble(insertionPoint_.y);
    filer->writeDouble(insertionPoint_.z);

    filer->writeInt32(static_cast<Adesk::Int32>(graphData_.sourceType));
    writeWideString(filer, graphData_.roadCenterlineHandle);
    writeWideString(filer, graphData_.terrainTinHandle);
    writeWideString(filer, graphData_.dmxFilePath);

    writeWideString(filer, graphData_.properties.graphName);
    filer->writeInt32(static_cast<Adesk::Int32>(graphData_.properties.groundLineColorIndex));
    filer->writeDouble(graphData_.properties.groundLineWidth);
    filer->writeDouble(graphData_.properties.groundLinePrecision);
    filer->writeDouble(graphData_.properties.verticalScale);
    filer->writeDouble(graphData_.properties.gridSpacing);

    filer->writeInt32(static_cast<Adesk::Int32>(graphData_.groundSamples.size()));
    for (const auto& sample : graphData_.groundSamples) {
        filer->writeDouble(sample.station);
        filer->writeDouble(sample.elevation);
        writeWideString(filer, sample.rawStationText);
        writeBool(filer, sample.breakChainIndex.has_value());
        filer->writeInt32(sample.breakChainIndex.value_or(0));
    }

    return filer->filerStatus();
}

Adesk::Boolean DnProfileGradeGraphEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr) {
        return Adesk::kTrue;
    }

    const auto layout = ProfileGradeGraphLayout::calculate(graphData_);
    if (!layout.succeeded) {
        return Adesk::kTrue;
    }

    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    drawFrame(worldDraw, insertionPoint_, layout);
    drawGrid(worldDraw, insertionPoint_, graphData_, layout);
    drawLabels(worldDraw, insertionPoint_, graphData_, layout);
    drawTitleAndHeader(worldDraw, insertionPoint_, graphData_, layout);
    drawGroundLine(worldDraw, insertionPoint_, graphData_, layout);

    return Adesk::kTrue;
}

Acad::ErrorStatus DnProfileGradeGraphEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    const auto layout = ProfileGradeGraphLayout::calculate(graphData_);
    if (!layout.succeeded) {
        return Acad::eInvalidExtents;
    }

    extents.addPoint(graphPoint(insertionPoint_, -20.0, -kTableHeaderHeight - kTextHeight));
    extents.addPoint(graphPoint(insertionPoint_, layout.graphWidth, layout.graphHeight + kTitleOffset + kTextHeight * 1.25));
    for (const auto& point : layout.mappedPoints) {
        extents.addPoint(graphPoint(insertionPoint_, point.x, point.y));
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnProfileGradeGraphEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    assertWriteEnabled();
    insertionPoint_.transformBy(transform);
    return Acad::eOk;
}

namespace roadproto::cad_adapter::objectarx {

void initializeProfileGradeGraphEntityClass()
{
    DnProfileGradeGraphEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeProfileGradeGraphEntityClass()
{
    deleteAcRxClass(DnProfileGradeGraphEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
