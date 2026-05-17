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

constexpr Adesk::Int16 kEntityVersion = 2;
constexpr Adesk::Int32 kMaxPersistedSamples = 1000000;
constexpr int kMaxGridLines = 2000;
constexpr double kMinAxisLength = 1.0e-9;
constexpr double kMaxAxisLength = 1.0e9;
constexpr double kMinAxisSine = 1.0e-6;
constexpr double kTextHeight = 2.5;
constexpr double kTitleOffset = 8.0;
constexpr double kTableHeaderHeight = 10.0;
constexpr double kLabelOffset = 2.0;
constexpr double kStationGridSpacing = 100.0;
constexpr double kExtentsPadding = 24.0;

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

bool isValidSourceType(Adesk::Int32 value)
{
    return value == static_cast<Adesk::Int32>(ProfileGroundSourceType::TerrainTin)
        || value == static_cast<Adesk::Int32>(ProfileGroundSourceType::DmxFile);
}

bool isFiniteVector(const AcGeVector3d& vector)
{
    return std::isfinite(vector.x) && std::isfinite(vector.y) && std::isfinite(vector.z);
}

void resetDefaultAxes(AcGeVector3d& xAxis, AcGeVector3d& yAxis)
{
    xAxis = AcGeVector3d(1.0, 0.0, 0.0);
    yAxis = AcGeVector3d(0.0, 1.0, 0.0);
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
    if (!std::isfinite(crossLength)) {
        return false;
    }

    return crossLength / (xLength * yLength) >= kMinAxisSine;
}

AcGePoint3d graphPoint(const AcGePoint3d& origin, const AcGeVector3d& xAxis, const AcGeVector3d& yAxis, double x, double y)
{
    return origin + xAxis * x + yAxis * y;
}

AcGeVector3d textDirection(const AcGeVector3d& xAxis)
{
    if (xAxis.isZeroLength()) {
        return AcGeVector3d::kXAxis;
    }

    auto direction = xAxis;
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

int boundedIterationCount(double start, double end, double step)
{
    if (!std::isfinite(start) || !std::isfinite(end) || !std::isfinite(step) || step <= 0.0) {
        return -1;
    }

    if (start > end + 1e-9) {
        return 0;
    }

    const auto rawCount = std::floor((end - start) / step) + 1.0;
    if (!std::isfinite(rawCount) || rawCount < 0.0 || rawCount > static_cast<double>(kMaxGridLines)) {
        return -1;
    }

    return static_cast<int>(rawCount);
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
    AcGePoint3d line[2] = {
        graphPoint(origin, xAxis, yAxis, x1, y1),
        graphPoint(origin, xAxis, yAxis, x2, y2)};
    worldDraw->geometry().polyline(2, line);
}

void drawText(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    double x,
    double y,
    const std::wstring& text,
    double height = kTextHeight)
{
    if (worldDraw == nullptr || text.empty()) {
        return;
    }

    worldDraw->geometry().text(
        graphPoint(origin, xAxis, yAxis, x, y),
        textNormal(xAxis, yAxis),
        textDirection(xAxis),
        height,
        1.0,
        0.0,
        text.c_str());
}

void drawFrame(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const ProfileGradeGraphLayoutResult& layout)
{
    worldDraw->subEntityTraits().setColor(7);
    drawLine(worldDraw, origin, xAxis, yAxis, 0.0, 0.0, layout.graphWidth, 0.0);
    drawLine(worldDraw, origin, xAxis, yAxis, layout.graphWidth, 0.0, layout.graphWidth, layout.graphHeight);
    drawLine(worldDraw, origin, xAxis, yAxis, layout.graphWidth, layout.graphHeight, 0.0, layout.graphHeight);
    drawLine(worldDraw, origin, xAxis, yAxis, 0.0, layout.graphHeight, 0.0, 0.0);
}

void drawGrid(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const ProfileGradeGraphData& graph,
    const ProfileGradeGraphLayoutResult& layout)
{
    const auto gridSpacing = graph.properties.gridSpacing;
    const auto verticalScale = graph.properties.verticalScale;

    worldDraw->subEntityTraits().setColor(8);
    const auto topElevation = std::ceil(layout.maxElevation / gridSpacing) * gridSpacing;
    const auto horizontalCount = boundedIterationCount(layout.baseElevation, topElevation, gridSpacing);
    for (int i = 0; i < horizontalCount; ++i) {
        const auto elevation = layout.baseElevation + static_cast<double>(i) * gridSpacing;
        const auto y = (elevation - layout.baseElevation) * verticalScale;
        drawLine(worldDraw, origin, xAxis, yAxis, 0.0, y, layout.graphWidth, y);
    }

    const auto firstStation = std::ceil(layout.minStation / kStationGridSpacing) * kStationGridSpacing;
    const auto verticalCount = boundedIterationCount(firstStation, layout.maxStation, kStationGridSpacing);
    for (int i = 0; i < verticalCount; ++i) {
        const auto station = firstStation + static_cast<double>(i) * kStationGridSpacing;
        const auto x = ProfileGradeGraphLayout::mapX(layout, station);
        drawLine(worldDraw, origin, xAxis, yAxis, x, 0.0, x, layout.graphHeight);
    }
}

void drawLabels(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const ProfileGradeGraphData& graph,
    const ProfileGradeGraphLayoutResult& layout)
{
    const auto gridSpacing = graph.properties.gridSpacing;
    const auto verticalScale = graph.properties.verticalScale;

    worldDraw->subEntityTraits().setColor(3);
    const auto topElevation = std::ceil(layout.maxElevation / gridSpacing) * gridSpacing;
    const auto elevationLabelCount = boundedIterationCount(layout.baseElevation, topElevation, gridSpacing);
    for (int i = 0; i < elevationLabelCount; ++i) {
        const auto elevation = layout.baseElevation + static_cast<double>(i) * gridSpacing;
        const auto y = (elevation - layout.baseElevation) * verticalScale;
        drawText(worldDraw, origin, xAxis, yAxis, -18.0, y - kTextHeight * 0.5, numberText(elevation, 2));
    }

    const auto firstStation = std::ceil(layout.minStation / kStationGridSpacing) * kStationGridSpacing;
    const auto stationLabelCount = boundedIterationCount(firstStation, layout.maxStation, kStationGridSpacing);
    for (int i = 0; i < stationLabelCount; ++i) {
        const auto station = firstStation + static_cast<double>(i) * kStationGridSpacing;
        const auto x = ProfileGradeGraphLayout::mapX(layout, station);
        drawText(worldDraw, origin, xAxis, yAxis, x - 8.0, -kTableHeaderHeight - kTextHeight, roadproto::domain::alignment::StationFormatter::format(station));
    }
}

void drawTitleAndHeader(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const ProfileGradeGraphData& graph,
    const ProfileGradeGraphLayoutResult& layout)
{
    worldDraw->subEntityTraits().setColor(2);
    const auto title = graph.properties.graphName.empty() ? L"Profile Grade Graph" : graph.properties.graphName;
    drawText(worldDraw, origin, xAxis, yAxis, 0.0, layout.graphHeight + kTitleOffset, title, kTextHeight * 1.25);

    worldDraw->subEntityTraits().setColor(7);
    drawLine(worldDraw, origin, xAxis, yAxis, 0.0, -kTableHeaderHeight, layout.graphWidth, -kTableHeaderHeight);
    drawText(worldDraw, origin, xAxis, yAxis, 0.0, -kTableHeaderHeight + kLabelOffset, L"Station");
    drawText(worldDraw, origin, xAxis, yAxis, 42.0, -kTableHeaderHeight + kLabelOffset, L"Ground elevation");
}

void drawGroundLine(
    AcGiWorldDraw* worldDraw,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    const ProfileGradeGraphData& graph,
    const ProfileGradeGraphLayoutResult& layout)
{
    if (layout.mappedPoints.size() < 2) {
        return;
    }

    const auto color = graph.properties.groundLineColorIndex > 0
        ? static_cast<Adesk::UInt16>(graph.properties.groundLineColorIndex)
        : static_cast<Adesk::UInt16>(4);
    worldDraw->subEntityTraits().setColor(color);
    worldDraw->subEntityTraits().setLineWeight(lineWeightFromMillimeters(graph.properties.groundLineWidth));

    for (std::size_t i = 1; i < layout.mappedPoints.size(); ++i) {
        const auto& previous = layout.mappedPoints[i - 1];
        const auto& current = layout.mappedPoints[i];
        drawLine(worldDraw, origin, xAxis, yAxis, previous.x, previous.y, current.x, current.y);
    }
    worldDraw->subEntityTraits().setLineWeight(AcDb::kLnWtByLayer);
}

void addExtentsPoint(
    AcDbExtents& extents,
    const AcGePoint3d& origin,
    const AcGeVector3d& xAxis,
    const AcGeVector3d& yAxis,
    double x,
    double y)
{
    extents.addPoint(graphPoint(origin, xAxis, yAxis, x, y));
}

} // namespace

DnProfileGradeGraphEntity::DnProfileGradeGraphEntity()
    : insertionPoint_(0.0, 0.0, 0.0)
    , xAxis_(1.0, 0.0, 0.0)
    , yAxis_(0.0, 1.0, 0.0)
{
}

void DnProfileGradeGraphEntity::setGraphData(const ProfileGradeGraphData& data)
{
    assertWriteEnabled();
    graphData_ = data;
    markGraphicsModifiedIfResident(*this);
}

const ProfileGradeGraphData& DnProfileGradeGraphEntity::graphData() const
{
    return graphData_;
}

void DnProfileGradeGraphEntity::setInsertionPoint(const AcGePoint3d& point)
{
    assertWriteEnabled();
    insertionPoint_ = point;
    markGraphicsModifiedIfResident(*this);
}

AcGePoint3d DnProfileGradeGraphEntity::insertionPoint() const
{
    return insertionPoint_;
}

ProfileGradeGraphDrawingFrame DnProfileGradeGraphEntity::drawingFrame() const
{
    assertReadEnabled();
    ProfileGradeGraphDrawingFrame frame;
    const auto layout = ProfileGradeGraphLayout::calculate(graphData_);
    if (!layout.succeeded || !areAxesUsable(xAxis_, yAxis_)) {
        return frame;
    }
    frame.valid = true;
    frame.minStation = layout.minStation;
    frame.baseElevation = layout.baseElevation;
    frame.verticalScale = graphData_.properties.verticalScale;
    frame.insertionPoint = insertionPoint_;
    frame.xAxis = xAxis_;
    frame.yAxis = yAxis_;
    return frame;
}

void DnProfileGradeGraphEntity::setProperties(const ProfileGradeGraphProperties& properties)
{
    assertWriteEnabled();
    graphData_.properties = properties;
    markGraphicsModifiedIfResident(*this);
}

bool DnProfileGradeGraphEntity::replaceGroundSamples(const std::vector<ProfileGroundSample>& samples)
{
    assertWriteEnabled();
    auto graph = graphData_;
    graph.groundSamples = samples;
    const auto layout = ProfileGradeGraphLayout::calculate(graph);
    if (!layout.succeeded) {
        return false;
    }

    graphData_.groundSamples = samples;
    markGraphicsModifiedIfResident(*this);
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
    xAxis_ = AcGeVector3d(1.0, 0.0, 0.0);
    yAxis_ = AcGeVector3d(0.0, 1.0, 0.0);
    if (version >= 2) {
        filer->readDouble(&xAxis_.x);
        filer->readDouble(&xAxis_.y);
        filer->readDouble(&xAxis_.z);
        filer->readDouble(&yAxis_.x);
        filer->readDouble(&yAxis_.y);
        filer->readDouble(&yAxis_.z);
        if (!areAxesUsable(xAxis_, yAxis_)) {
            resetDefaultAxes(xAxis_, yAxis_);
        }
    }

    Adesk::Int32 sourceType = 0;
    filer->readInt32(&sourceType);
    if (!isValidSourceType(sourceType)) {
        return Acad::eInvalidInput;
    }
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
    if (sampleCount < 0 || sampleCount > kMaxPersistedSamples) {
        return Acad::eInvalidInput;
    }
    graphData_.groundSamples.clear();
    graphData_.groundSamples.reserve(sampleCount > 0 ? static_cast<std::size_t>(sampleCount) : 0);
    for (Adesk::Int32 i = 0; i < sampleCount; ++i) {
        ProfileGroundSample sample;
        filer->readDouble(&sample.station);
        filer->readDouble(&sample.elevation);
        sample.rawStationText = readWideString(filer);
        sample.breakChainIndex.reset();
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
    if (!areAxesUsable(xAxis_, yAxis_)) {
        return Adesk::kTrue;
    }

    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    drawFrame(worldDraw, insertionPoint_, xAxis_, yAxis_, layout);
    drawGrid(worldDraw, insertionPoint_, xAxis_, yAxis_, graphData_, layout);
    drawLabels(worldDraw, insertionPoint_, xAxis_, yAxis_, graphData_, layout);
    drawTitleAndHeader(worldDraw, insertionPoint_, xAxis_, yAxis_, graphData_, layout);
    drawGroundLine(worldDraw, insertionPoint_, xAxis_, yAxis_, graphData_, layout);

    return Adesk::kTrue;
}

Acad::ErrorStatus DnProfileGradeGraphEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    const auto layout = ProfileGradeGraphLayout::calculate(graphData_);
    if (!layout.succeeded) {
        return Acad::eInvalidExtents;
    }
    if (!areAxesUsable(xAxis_, yAxis_)) {
        return Acad::eInvalidExtents;
    }

    const auto minX = -kExtentsPadding;
    const auto maxX = std::max(layout.graphWidth + kExtentsPadding, 42.0 + 120.0);
    const auto minY = -kTableHeaderHeight - kTextHeight - kExtentsPadding;
    const auto maxY = layout.graphHeight + kTitleOffset + kTextHeight * 2.0 + kExtentsPadding;

    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, minX, minY);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, minX, 0.0);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, minX, layout.graphHeight);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, minX, maxY);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, 0.0, minY);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, 0.0, 0.0);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, 0.0, layout.graphHeight);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, 0.0, maxY);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, layout.graphWidth, minY);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, layout.graphWidth, 0.0);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, layout.graphWidth, layout.graphHeight);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, layout.graphWidth, maxY);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, maxX, minY);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, maxX, 0.0);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, maxX, layout.graphHeight);
    addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, maxX, maxY);
    for (const auto& point : layout.mappedPoints) {
        addExtentsPoint(extents, insertionPoint_, xAxis_, yAxis_, point.x, point.y);
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnProfileGradeGraphEntity::subTransformBy(const AcGeMatrix3d& transform)
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
