#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"

#include "domain/alignment/AlignmentGripEditService.h"
#include "domain/alignment/StationFormatter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "acgi.h"
#include "acutmem.h"
#include "dbgrip.h"
#include "dbgripoperations.h"
#include "dbproxy.h"
#include "geassign.h"
#include "rxregsvc.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <utility>

using roadproto::domain::alignment::AlignmentPoint2d;
using roadproto::domain::alignment::AlignmentSamplePoint;
using roadproto::domain::alignment::HorizontalAlignmentElement;
using roadproto::domain::alignment::HorizontalAlignmentFeaturePoint;
using roadproto::domain::alignment::AlignmentGripEditService;
using roadproto::domain::alignment::HorizontalAlignment;
using roadproto::domain::alignment::HorizontalAlignmentBuilder;
using roadproto::domain::alignment::HorizontalAlignmentInput;
using roadproto::domain::alignment::HorizontalCurveParameters;
using roadproto::domain::alignment::RoadCenterlineProperties;
using roadproto::domain::alignment::StationLabel;

ACRX_DXF_DEFINE_MEMBERS(
    DnRoadCenterlineEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNROADCENTERLINEENTITY,
    "RoadProto Road Centerline");

namespace {

constexpr Adesk::Int16 kEntityVersion = 2;
constexpr double kTextHeight = 2.5;

Adesk::UInt16 colorForElement(roadproto::domain::alignment::AlignmentElementType type)
{
    using roadproto::domain::alignment::AlignmentElementType;
    switch (type) {
    case AlignmentElementType::Line:
        return 4; // cyan
    case AlignmentElementType::SpiralIn:
        return 1; // red
    case AlignmentElementType::CircularArc:
        return 2; // yellow
    case AlignmentElementType::SpiralOut:
        return 6; // magenta
    case AlignmentElementType::PartialSpiral:
        return 30; // orange
    default:
        return 7;
    }
}

const roadproto::domain::alignment::HorizontalAlignmentElement* findArcElement(
    const HorizontalAlignment& alignment,
    std::size_t curveIndex)
{
    for (const auto& element : alignment.elements) {
        if (element.curveIndex == curveIndex
            && element.type == roadproto::domain::alignment::AlignmentElementType::CircularArc) {
            return &element;
        }
    }
    return nullptr;
}

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

void readPoint(AcDbDwgFiler* filer, AlignmentPoint2d& point)
{
    filer->readDouble(&point.x);
    filer->readDouble(&point.y);
}

void writePoint(AcDbDwgFiler* filer, const AlignmentPoint2d& point)
{
    filer->writeDouble(point.x);
    filer->writeDouble(point.y);
}

void readSamples(AcDbDwgFiler* filer, std::vector<AlignmentSamplePoint>& samples)
{
    Adesk::Int32 sampleCount = 0;
    filer->readInt32(&sampleCount);
    samples.clear();
    samples.reserve(sampleCount > 0 ? static_cast<std::size_t>(sampleCount) : 0);
    for (Adesk::Int32 i = 0; i < sampleCount; ++i) {
        AlignmentSamplePoint sample;
        readPoint(filer, sample.point);
        filer->readDouble(&sample.station);
        samples.push_back(sample);
    }
}

void writeSamples(AcDbDwgFiler* filer, const std::vector<AlignmentSamplePoint>& samples)
{
    filer->writeInt32(static_cast<Adesk::Int32>(samples.size()));
    for (const auto& sample : samples) {
        writePoint(filer, sample.point);
        filer->writeDouble(sample.station);
    }
}

void readElements(AcDbDwgFiler* filer, std::vector<HorizontalAlignmentElement>& elements)
{
    Adesk::Int32 elementCount = 0;
    filer->readInt32(&elementCount);
    elements.clear();
    elements.reserve(elementCount > 0 ? static_cast<std::size_t>(elementCount) : 0);
    for (Adesk::Int32 i = 0; i < elementCount; ++i) {
        HorizontalAlignmentElement element;
        Adesk::Int32 type = 0;
        Adesk::Int32 curveIndex = 0;
        filer->readInt32(&type);
        filer->readInt32(&curveIndex);
        element.type = static_cast<roadproto::domain::alignment::AlignmentElementType>(type);
        element.curveIndex = curveIndex < 0 ? 0 : static_cast<std::size_t>(curveIndex);
        readPoint(filer, element.start);
        readPoint(filer, element.end);
        filer->readDouble(&element.startStation);
        filer->readDouble(&element.length);
        filer->readDouble(&element.radius);
        filer->readDouble(&element.spiralLength);
        filer->readDouble(&element.startHeading);
        filer->readDouble(&element.endHeading);
        filer->readDouble(&element.startCurvature);
        filer->readDouble(&element.endCurvature);
        readSamples(filer, element.samples);
        elements.push_back(std::move(element));
    }
}

void writeElements(AcDbDwgFiler* filer, const std::vector<HorizontalAlignmentElement>& elements)
{
    filer->writeInt32(static_cast<Adesk::Int32>(elements.size()));
    for (const auto& element : elements) {
        filer->writeInt32(static_cast<Adesk::Int32>(element.type));
        filer->writeInt32(static_cast<Adesk::Int32>(element.curveIndex));
        writePoint(filer, element.start);
        writePoint(filer, element.end);
        filer->writeDouble(element.startStation);
        filer->writeDouble(element.length);
        filer->writeDouble(element.radius);
        filer->writeDouble(element.spiralLength);
        filer->writeDouble(element.startHeading);
        filer->writeDouble(element.endHeading);
        filer->writeDouble(element.startCurvature);
        filer->writeDouble(element.endCurvature);
        writeSamples(filer, element.samples);
    }
}

void readFeaturePoints(AcDbDwgFiler* filer, std::vector<HorizontalAlignmentFeaturePoint>& features)
{
    Adesk::Int32 featureCount = 0;
    filer->readInt32(&featureCount);
    features.clear();
    features.reserve(featureCount > 0 ? static_cast<std::size_t>(featureCount) : 0);
    for (Adesk::Int32 i = 0; i < featureCount; ++i) {
        HorizontalAlignmentFeaturePoint feature;
        Adesk::Int32 type = 0;
        Adesk::Int32 curveIndex = 0;
        filer->readInt32(&type);
        filer->readInt32(&curveIndex);
        feature.type = static_cast<roadproto::domain::alignment::AlignmentFeaturePointType>(type);
        feature.curveIndex = curveIndex < 0 ? 0 : static_cast<std::size_t>(curveIndex);
        readPoint(filer, feature.point);
        filer->readDouble(&feature.station);
        features.push_back(feature);
    }
}

void writeFeaturePoints(AcDbDwgFiler* filer, const std::vector<HorizontalAlignmentFeaturePoint>& features)
{
    filer->writeInt32(static_cast<Adesk::Int32>(features.size()));
    for (const auto& feature : features) {
        filer->writeInt32(static_cast<Adesk::Int32>(feature.type));
        filer->writeInt32(static_cast<Adesk::Int32>(feature.curveIndex));
        writePoint(filer, feature.point);
        filer->writeDouble(feature.station);
    }
}

void readStationLabels(AcDbDwgFiler* filer, std::vector<StationLabel>& labels)
{
    Adesk::Int32 labelCount = 0;
    filer->readInt32(&labelCount);
    labels.clear();
    labels.reserve(labelCount > 0 ? static_cast<std::size_t>(labelCount) : 0);
    for (Adesk::Int32 i = 0; i < labelCount; ++i) {
        StationLabel label;
        readPoint(filer, label.point);
        filer->readDouble(&label.station);
        label.text = readWideString(filer);
        labels.push_back(std::move(label));
    }
}

void writeStationLabels(AcDbDwgFiler* filer, const std::vector<StationLabel>& labels)
{
    filer->writeInt32(static_cast<Adesk::Int32>(labels.size()));
    for (const auto& label : labels) {
        writePoint(filer, label.point);
        filer->writeDouble(label.station);
        writeWideString(filer, label.text);
    }
}

double transformHeading(const AcGeMatrix3d& transform, double heading)
{
    AcGePoint3d origin(0.0, 0.0, 0.0);
    AcGePoint3d direction(std::cos(heading), std::sin(heading), 0.0);
    origin.transformBy(transform);
    direction.transformBy(transform);
    return std::atan2(direction.y - origin.y, direction.x - origin.x);
}

void transformPoint(AcGeMatrix3d transform, AlignmentPoint2d& point)
{
    AcGePoint3d transformed(point.x, point.y, 0.0);
    transformed.transformBy(transform);
    point.x = transformed.x;
    point.y = transformed.y;
}

void transformStoredGeometry(HorizontalAlignment& alignment, const AcGeMatrix3d& transform)
{
    for (auto& element : alignment.elements) {
        transformPoint(transform, element.start);
        transformPoint(transform, element.end);
        element.startHeading = transformHeading(transform, element.startHeading);
        element.endHeading = transformHeading(transform, element.endHeading);
        for (auto& sample : element.samples) {
            transformPoint(transform, sample.point);
        }
    }
    for (auto& feature : alignment.featurePoints) {
        transformPoint(transform, feature.point);
    }
    for (auto& label : alignment.stationLabels) {
        transformPoint(transform, label.point);
    }
}

std::wstring featureName(roadproto::domain::alignment::AlignmentFeaturePointType type)
{
    using roadproto::domain::alignment::AlignmentFeaturePointType;
    switch (type) {
    case AlignmentFeaturePointType::Start:
        return L"起点";
    case AlignmentFeaturePointType::End:
        return L"终点";
    case AlignmentFeaturePointType::PI:
        return L"交点";
    case AlignmentFeaturePointType::TS:
        return L"直缓点";
    case AlignmentFeaturePointType::SC:
        return L"缓圆点";
    case AlignmentFeaturePointType::CS:
        return L"圆缓点";
    case AlignmentFeaturePointType::ST:
        return L"缓直点";
    case AlignmentFeaturePointType::TC:
        return L"直圆点";
    case AlignmentFeaturePointType::CT:
        return L"圆直点";
    case AlignmentFeaturePointType::TangentIntersection:
        return L"直线交点";
    case AlignmentFeaturePointType::ArcMid:
        return L"圆曲线中点";
    default:
        return L"特征点";
    }
}

std::wstring numberText(double value)
{
    std::wostringstream stream;
    stream.precision(6);
    stream << value;
    return stream.str();
}

void* gripAppDataFromIndex(std::size_t index)
{
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(index + 1));
}

bool gripIndexFromAppData(void* appData, std::size_t& index)
{
    const auto raw = reinterpret_cast<std::uintptr_t>(appData);
    if (raw == 0) {
        return false;
    }
    index = static_cast<std::size_t>(raw - 1);
    return true;
}

} // namespace

DnRoadCenterlineEntity::DnRoadCenterlineEntity() = default;

void DnRoadCenterlineEntity::setAlignment(const HorizontalAlignment& alignment)
{
    alignment_ = alignment;
}

const HorizontalAlignment& DnRoadCenterlineEntity::alignment() const
{
    return alignment_;
}

void DnRoadCenterlineEntity::setProperties(const RoadCenterlineProperties& properties)
{
    const auto oldStationInterval = alignment_.properties.stationLabelInterval;
    const auto needsRebuild =
        alignment_.elements.empty() || std::fabs(oldStationInterval - properties.stationLabelInterval) > 1e-9;
    alignment_.properties = properties;
    if (needsRebuild) {
        rebuildFromStoredData();
    }
}

RoadCenterlineProperties DnRoadCenterlineEntity::properties() const
{
    return alignment_.properties;
}

void DnRoadCenterlineEntity::dragStatus(const AcDb::DragStat status)
{
    if (status == AcDb::kDragEnd || status == AcDb::kDragAbort) {
        dragDataFlags_ = 0;
        dragAlignment_ = {};
    }
    AcDbEntity::dragStatus(status);
}

bool DnRoadCenterlineEntity::rebuildWithCurveParameters(
    const HorizontalCurveParameters& parameters,
    std::size_t curveIndex)
{
    HorizontalAlignmentInput input;
    input.properties = alignment_.properties;
    input.controlPoints = alignment_.controlPoints;
    input.defaultParameters = parameters;
    input.curveParameters = alignment_.curveParameters;
    const auto requiredCount = alignment_.controlPoints.size() >= 3 ? alignment_.controlPoints.size() - 2 : 1;
    if (input.curveParameters.empty()) {
        input.curveParameters.resize(requiredCount, parameters);
    }
    if (curveIndex >= input.curveParameters.size()) {
        input.curveParameters.resize(curveIndex + 1, input.curveParameters.empty() ? parameters : input.curveParameters.back());
    }
    input.curveParameters[curveIndex] = parameters;
    HorizontalAlignmentBuilder builder;
    const auto rebuilt = builder.build(input);
    if (!rebuilt.succeeded) {
        return false;
    }
    alignment_ = rebuilt.alignment;
    return true;
}

bool DnRoadCenterlineEntity::rebuildWithCurveParameterList(
    const std::vector<HorizontalCurveParameters>& parameters)
{
    if (parameters.empty()) {
        return rebuildFromStoredData();
    }

    HorizontalAlignmentInput input;
    input.properties = alignment_.properties;
    input.controlPoints = alignment_.controlPoints;
    input.defaultParameters = parameters.front();
    input.curveParameters = parameters;
    const auto requiredCount = alignment_.controlPoints.size() >= 3 ? alignment_.controlPoints.size() - 2 : 1;
    if (input.curveParameters.size() < requiredCount) {
        input.curveParameters.resize(requiredCount, input.curveParameters.back());
    }

    HorizontalAlignmentBuilder builder;
    const auto rebuilt = builder.build(input);
    if (!rebuilt.succeeded) {
        return false;
    }
    alignment_ = rebuilt.alignment;
    return true;
}

Acad::ErrorStatus DnRoadCenterlineEntity::dwgInFields(AcDbDwgFiler* filer)
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

    alignment_.properties.roadName = readWideString(filer);
    Adesk::Int32 grade = 0;
    filer->readInt32(&grade);
    alignment_.properties.roadGrade = static_cast<roadproto::domain::alignment::RoadGrade>(grade);
    Adesk::Int8 linked = 0;
    filer->readInt8(&linked);
    alignment_.properties.linkedTerrainEnabled = linked != 0;
    alignment_.properties.linkedTerrainHandle = readWideString(filer);
    filer->readDouble(&alignment_.properties.stationLabelInterval);

    Adesk::Int32 pointCount = 0;
    filer->readInt32(&pointCount);
    alignment_.controlPoints.clear();
    for (Adesk::Int32 i = 0; i < pointCount; ++i) {
        AlignmentPoint2d point;
        filer->readDouble(&point.x);
        filer->readDouble(&point.y);
        alignment_.controlPoints.push_back(point);
    }

    Adesk::Int32 parameterCount = 0;
    filer->readInt32(&parameterCount);
    alignment_.curveParameters.clear();
    for (Adesk::Int32 i = 0; i < parameterCount; ++i) {
        HorizontalCurveParameters parameters;
        filer->readDouble(&parameters.tangentIn);
        filer->readDouble(&parameters.tangentOut);
        filer->readDouble(&parameters.ls1);
        filer->readDouble(&parameters.radius);
        filer->readDouble(&parameters.ls2);
        alignment_.curveParameters.push_back(parameters);
    }

    if (version >= 2) {
        Adesk::Int32 combinationType = 0;
        filer->readInt32(&combinationType);
        alignment_.combinationType =
            static_cast<roadproto::domain::alignment::AlignmentCurveCombinationType>(combinationType);
        filer->readDouble(&alignment_.totalLength);
        readElements(filer, alignment_.elements);
        readFeaturePoints(filer, alignment_.featurePoints);
        readStationLabels(filer, alignment_.stationLabels);
    }

    if (!alignment_.controlPoints.empty()) {
        rebuildFromStoredData();
    }
    return filer->filerStatus();
}

Acad::ErrorStatus DnRoadCenterlineEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    filer->writeInt16(kEntityVersion);
    writeWideString(filer, alignment_.properties.roadName);
    filer->writeInt32(static_cast<Adesk::Int32>(alignment_.properties.roadGrade));
    filer->writeInt8(alignment_.properties.linkedTerrainEnabled ? 1 : 0);
    writeWideString(filer, alignment_.properties.linkedTerrainHandle);
    filer->writeDouble(alignment_.properties.stationLabelInterval);

    filer->writeInt32(static_cast<Adesk::Int32>(alignment_.controlPoints.size()));
    for (const auto& point : alignment_.controlPoints) {
        filer->writeDouble(point.x);
        filer->writeDouble(point.y);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(alignment_.curveParameters.size()));
    for (const auto& parameters : alignment_.curveParameters) {
        filer->writeDouble(parameters.tangentIn);
        filer->writeDouble(parameters.tangentOut);
        filer->writeDouble(parameters.ls1);
        filer->writeDouble(parameters.radius);
        filer->writeDouble(parameters.ls2);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(alignment_.combinationType));
    filer->writeDouble(alignment_.totalLength);
    writeElements(filer, alignment_.elements);
    writeFeaturePoints(filer, alignment_.featurePoints);
    writeStationLabels(filer, alignment_.stationLabels);

    return filer->filerStatus();
}

const HorizontalAlignment& DnRoadCenterlineEntity::drawAlignment() const
{
    return (dragDataFlags_ & kUseDragCache) != 0 ? dragAlignment_ : alignment_;
}

Adesk::Boolean DnRoadCenterlineEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr) {
        return Adesk::kTrue;
    }

    const auto& alignment = drawAlignment();
    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    for (const auto& element : alignment.elements) {
        worldDraw->subEntityTraits().setColor(colorForElement(element.type));
        for (std::size_t i = 1; i < element.samples.size(); ++i) {
            AcGePoint3d line[2] = {
                AcGePoint3d(element.samples[i - 1].point.x, element.samples[i - 1].point.y, 0.0),
                AcGePoint3d(element.samples[i].point.x, element.samples[i].point.y, 0.0)};
            worldDraw->geometry().polyline(2, line);
        }
    }

    worldDraw->subEntityTraits().setColor(2);
    for (const auto& feature : alignment.featurePoints) {
        drawText(worldDraw, AlignmentPoint2d{feature.point.x + 1.2, feature.point.y + 1.2}, featureName(feature.type) + L" " + roadproto::domain::alignment::StationFormatter::format(feature.station), kTextHeight);
    }

    worldDraw->subEntityTraits().setColor(3);
    for (const auto& label : alignment.stationLabels) {
        const AlignmentPoint2d textPoint{label.point.x + 3.0, label.point.y - 3.0};
        AcGePoint3d leader[2] = {
            AcGePoint3d(label.point.x, label.point.y, 0.0),
            AcGePoint3d(textPoint.x - 0.4, textPoint.y + 0.4, 0.0)};
        worldDraw->geometry().polyline(2, leader);
        drawText(worldDraw, textPoint, label.text, kTextHeight);
    }

    for (std::size_t curveIndex = 0; curveIndex < alignment.curveParameters.size(); ++curveIndex) {
        const auto* arcElement = findArcElement(alignment, curveIndex);
        if (arcElement == nullptr) {
            continue;
        }

        const auto& parameters = alignment.curveParameters[curveIndex];
        AlignmentPoint2d labelPoint = arcElement->samples.empty()
            ? arcElement->start
            : arcElement->samples[arcElement->samples.size() / 2].point;
        std::wstring text = L"R=" + numberText(parameters.radius)
            + L" Lc=" + numberText(arcElement->length)
            + L" LS1=" + numberText(parameters.ls1)
            + L" LS2=" + numberText(parameters.ls2);
        drawText(worldDraw, AlignmentPoint2d{labelPoint.x + 2.0, labelPoint.y + 2.0}, text, kTextHeight);
    }

    return Adesk::kTrue;
}

Acad::ErrorStatus DnRoadCenterlineEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    const auto& alignment = drawAlignment();
    bool hasPoint = false;
    for (const auto& element : alignment.elements) {
        for (const auto& sample : element.samples) {
            extents.addPoint(AcGePoint3d(sample.point.x, sample.point.y, 0.0));
            hasPoint = true;
        }
    }
    return hasPoint ? Acad::eOk : Acad::eInvalidExtents;
}

Acad::ErrorStatus DnRoadCenterlineEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    auto transformedAlignment = alignment_;
    if (transformedAlignment.controlPoints.empty()) {
        transformStoredGeometry(transformedAlignment, transform);
        if ((dragDataFlags_ & kCloneMeForDraggingCalled) != 0) {
            dragAlignment_ = transformedAlignment;
            dragDataFlags_ |= kUseDragCache;
            return Acad::eOk;
        }

        assertWriteEnabled();
        alignment_ = transformedAlignment;
        return Acad::eOk;
    }

    for (auto& point : transformedAlignment.controlPoints) {
        AcGePoint3d transformed(point.x, point.y, 0.0);
        transformed.transformBy(transform);
        point.x = transformed.x;
        point.y = transformed.y;
    }

    HorizontalAlignmentInput input;
    input.properties = transformedAlignment.properties;
    input.controlPoints = transformedAlignment.controlPoints;
    if (!transformedAlignment.curveParameters.empty()) {
        input.defaultParameters = transformedAlignment.curveParameters.front();
        input.curveParameters = transformedAlignment.curveParameters;
    }

    HorizontalAlignmentBuilder builder;
    const auto rebuilt = builder.build(input);
    if (!rebuilt.succeeded) {
        return Acad::eInvalidInput;
    }

    if ((dragDataFlags_ & kCloneMeForDraggingCalled) != 0) {
        dragAlignment_ = rebuilt.alignment;
        dragDataFlags_ |= kUseDragCache;
        return Acad::eOk;
    }

    assertWriteEnabled();
    alignment_ = rebuilt.alignment;
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadCenterlineEntity::subGetGripPoints(
    AcGePoint3dArray& gripPoints,
    AcDbIntArray&,
    AcDbIntArray&) const
{
    assertReadEnabled();
    for (const auto& point : drawAlignment().controlPoints) {
        gripPoints.append(AcGePoint3d(point.x, point.y, 0.0));
    }
    if (drawAlignment().controlPoints.empty()) {
        return Acad::eOk;
    }
    for (const auto& feature : drawAlignment().featurePoints) {
        gripPoints.append(AcGePoint3d(feature.point.x, feature.point.y, 0.0));
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadCenterlineEntity::subGetGripPoints(
    AcDbGripDataPtrArray& grips,
    const double,
    const int,
    const AcGeVector3d&,
    const int) const
{
    assertReadEnabled();
    const auto& alignment = drawAlignment();
    std::size_t index = 0;
    for (const auto& point : alignment.controlPoints) {
        auto* grip = new AcDbGripData(AcGePoint3d(point.x, point.y, 0.0), gripAppDataFromIndex(index));
        grip->disableRubberBandLine(true);
        grip->disableModeKeywords(true);
        grip->setDrawAtDragImageGripPoint(true);
        grips.append(grip);
        ++index;
    }
    if (alignment.controlPoints.empty()) {
        return Acad::eOk;
    }
    for (const auto& feature : alignment.featurePoints) {
        auto* grip = new AcDbGripData(AcGePoint3d(feature.point.x, feature.point.y, 0.0), gripAppDataFromIndex(index));
        grip->disableRubberBandLine(true);
        grip->disableModeKeywords(true);
        grip->setDrawAtDragImageGripPoint(true);
        grips.append(grip);
        ++index;
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadCenterlineEntity::moveGripPointIndices(
    const std::vector<std::size_t>& gripIndices,
    const AcGeVector3d& offset,
    bool previewOnly)
{
    if (gripIndices.empty() || offset.isZeroLength()) {
        return Acad::eOk;
    }

    auto editedAlignment = alignment_;
    AlignmentGripEditService service;
    const auto result = service.applyGripOffsets(editedAlignment, gripIndices, offset.x, offset.y);
    if (!result.succeeded) {
        return Acad::eInvalidInput;
    }
    if (!result.changed) {
        return Acad::eOk;
    }

    if (previewOnly) {
        dragAlignment_ = editedAlignment;
        dragDataFlags_ |= kUseDragCache;
        return Acad::eOk;
    }

    assertWriteEnabled();
    alignment_ = editedAlignment;
    dragDataFlags_ = 0;
    dragAlignment_ = {};
    recordGraphicsModified(true);
    return Acad::eOk;
}

Acad::ErrorStatus DnRoadCenterlineEntity::subMoveGripPointsAt(const AcDbIntArray& indices, const AcGeVector3d& offset)
{
    std::vector<std::size_t> gripIndices;
    gripIndices.reserve(static_cast<std::size_t>(indices.length()));
    for (int i = 0; i < indices.length(); ++i) {
        const auto index = indices.at(i);
        if (index >= 0) {
            gripIndices.push_back(static_cast<std::size_t>(index));
        }
    }

    const bool previewOnly = (dragDataFlags_ & kCloneMeForDraggingCalled) != 0;
    return moveGripPointIndices(gripIndices, offset, previewOnly);
}

Acad::ErrorStatus DnRoadCenterlineEntity::subMoveGripPointsAt(
    const AcDbVoidPtrArray& gripAppData,
    const AcGeVector3d& offset,
    const int bitflags)
{
    std::vector<std::size_t> gripIndices;
    gripIndices.reserve(static_cast<std::size_t>(gripAppData.length()));
    for (int i = 0; i < gripAppData.length(); ++i) {
        std::size_t index = 0;
        if (gripIndexFromAppData(gripAppData.at(i), index)) {
            gripIndices.push_back(index);
        }
    }

    const bool previewOnly = (bitflags & AcDbGripOperations::kDragging) != 0;
    return moveGripPointIndices(gripIndices, offset, previewOnly);
}

Adesk::Boolean DnRoadCenterlineEntity::subCloneMeForDragging()
{
    dragDataFlags_ |= kCloneMeForDraggingCalled;
    return Adesk::kFalse;
}

bool DnRoadCenterlineEntity::rebuildFromStoredData()
{
    if (alignment_.controlPoints.empty()) {
        return !alignment_.elements.empty();
    }

    HorizontalAlignmentInput input;
    input.properties = alignment_.properties;
    input.controlPoints = alignment_.controlPoints;
    if (!alignment_.curveParameters.empty()) {
        input.defaultParameters = alignment_.curveParameters.front();
        input.curveParameters = alignment_.curveParameters;
    }

    HorizontalAlignmentBuilder builder;
    const auto rebuilt = builder.build(input);
    if (!rebuilt.succeeded) {
        return false;
    }
    alignment_ = rebuilt.alignment;
    return true;
}

void DnRoadCenterlineEntity::drawText(
    AcGiWorldDraw* worldDraw,
    const AlignmentPoint2d& point,
    const std::wstring& text,
    double height) const
{
    if (worldDraw == nullptr || text.empty()) {
        return;
    }

    worldDraw->geometry().text(
        AcGePoint3d(point.x, point.y, 0.0),
        AcGeVector3d::kZAxis,
        AcGeVector3d::kXAxis,
        height,
        1.0,
        0.0,
        text.c_str());
}

namespace roadproto::cad_adapter::objectarx {

void initializeRoadCenterlineEntityClass()
{
    DnRoadCenterlineEntity::rxInit();
    acrxBuildClassHierarchy();
}

void uninitializeRoadCenterlineEntityClass()
{
    deleteAcRxClass(DnRoadCenterlineEntity::desc());
}

} // namespace roadproto::cad_adapter::objectarx
