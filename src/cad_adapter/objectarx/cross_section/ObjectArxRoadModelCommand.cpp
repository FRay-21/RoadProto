#include "cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/cross_section/RoadModelBuildService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"
#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h"
#include "cad_adapter/objectarx/cross_section/DnSlopeTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/RoadModelDialogBridge.h"
#include "cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.h"
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"
#include "cad_adapter/objectarx/terrain/DnTerrainTinEntity.h"

#include "domain/alignment/StationFormatter.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#endif

#ifndef ROADPROTO_TEST_BUILD
int acedSetStatusBarProgressMeter(const ACHAR* pszLabel, int nMinPos, int nMaxPos);
int acedSetStatusBarProgressMeterPos(int nPos);
void acedRestoreStatusBar();
#endif

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

#ifndef ROADPROTO_TEST_BUILD

using roadproto::application::cross_section::RoadModelBuildInput;
using roadproto::application::cross_section::RoadModelBuildService;
using roadproto::domain::alignment::AlignmentPoint2d;
using roadproto::domain::alignment::AlignmentSamplePoint;
using roadproto::domain::cross_section::RoadModelConfig;
using roadproto::domain::cross_section::RoadModelData;
using roadproto::domain::cross_section::RoadModelPavementLayerTemplateSource;
using roadproto::domain::cross_section::RoadModelSection;
using roadproto::domain::cross_section::RoadModelSectionPreviewBuilder;
using roadproto::domain::cross_section::RoadModelSectionPreviewRequest;
using roadproto::domain::cross_section::RoadModelSectionPreviewSegment;
using roadproto::domain::cross_section::RoadModelSectionPreviewSegmentKind;
using roadproto::domain::cross_section::RoadModelSlopeTemplateGroup;
using roadproto::domain::cross_section::RoadModelSlopeTemplateReference;
using roadproto::domain::cross_section::RoadModelSlopeTemplateSource;
using roadproto::domain::cross_section::RoadModelTemplateAssignment;
using roadproto::domain::cross_section::RoadModelTemplateSource;
using roadproto::domain::cross_section::RoadModelTerrainSurface;
using roadproto::domain::profile::ProfileVerticalCurveData;

constexpr double kStationEpsilon = 1e-8;
constexpr double kSectionDrawingPadding = 2.0;
constexpr double kSectionDrawingStationLabelBand = 4.0;
constexpr double kSectionDrawingSpacing = 6.0;

class StatusProgressMeter {
public:
    StatusProgressMeter(const ACHAR* label, int minPosition, int maxPosition)
        : active_(acedSetStatusBarProgressMeter(label, minPosition, maxPosition) == 0)
        , minPosition_(minPosition)
        , maxPosition_(maxPosition)
        , lastPosition_(minPosition)
    {
    }

    ~StatusProgressMeter()
    {
        if (active_) {
            acedRestoreStatusBar();
        }
    }

    void setPosition(int position)
    {
        if (!active_) {
            return;
        }
        const auto clamped = std::clamp(position, minPosition_, maxPosition_);
        if (clamped == lastPosition_) {
            return;
        }
        lastPosition_ = clamped;
        acedSetStatusBarProgressMeterPos(clamped);
    }

private:
    bool active_ = false;
    int minPosition_ = 0;
    int maxPosition_ = 100;
    int lastPosition_ = 0;
};

std::wstring trimDialogCommandPath(std::wstring path)
{
    while (!path.empty() && std::iswspace(path.front()) != 0) {
        path.erase(path.begin());
    }
    while (!path.empty() && std::iswspace(path.back()) != 0) {
        path.pop_back();
    }

    if (path.size() >= 2) {
        const auto first = path.front();
        const auto last = path.back();
        if ((first == L'"' && last == L'"') || (first == L'\'' && last == L'\'')) {
            path = path.substr(1, path.size() - 2);
        }
    }

    return path;
}

bool appendEntityToModelSpace(AcDbEntity* entity, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr || entity == nullptr) {
        return false;
    }

    AcDbBlockTable* blockTable = nullptr;
    if (database->getBlockTable(blockTable, AcDb::kForRead) != Acad::eOk || blockTable == nullptr) {
        return false;
    }

    AcDbBlockTableRecord* modelSpace = nullptr;
    const auto status = blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForWrite);
    blockTable->close();
    if (status != Acad::eOk || modelSpace == nullptr) {
        return false;
    }

    const auto appendStatus = modelSpace->appendAcDbEntity(entityId, entity);
    modelSpace->close();
    return appendStatus == Acad::eOk;
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

std::wstring entityHandleText(AcDbEntity* entity)
{
    if (entity == nullptr) {
        return L"";
    }

    AcDbHandle handle;
    entity->getAcDbHandle(handle);
    ACHAR handleText[32] = {};
    handle.getIntoAsciiBuffer(handleText);
    return handleText;
}

template <typename TEntity>
bool findEntityInSelectionSet(const ads_name selectionSet, AcDbObjectId& entityId)
{
    Adesk::Int32 length = 0;
    if (acedSSLength(selectionSet, &length) != RTNORM || length <= 0) {
        return false;
    }

    for (Adesk::Int32 i = 0; i < length; ++i) {
        ads_name entityName;
        if (acedSSName(selectionSet, i, entityName) != RTNORM) {
            continue;
        }

        AcDbObjectId candidateId;
        if (acdbGetObjectId(candidateId, entityName) != Acad::eOk) {
            continue;
        }

        TEntity* entity = nullptr;
        if (acdbOpenObject(entity, candidateId, AcDb::kForRead) == Acad::eOk && entity != nullptr) {
            entity->close();
            entityId = candidateId;
            return true;
        }
    }

    return false;
}

template <typename TEntity>
bool findImpliedEntity(AcDbObjectId& entityId)
{
    ads_name selectionSet;
    if (acedSSGet(L"_I", nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }

    SelectionSetGuard guard(selectionSet);
    return findEntityInSelectionSet<TEntity>(selectionSet, entityId);
}

template <typename TEntity>
bool selectTypedEntity(AcDbObjectId& entityId)
{
    if (findImpliedEntity<TEntity>(entityId)) {
        return true;
    }

    ads_name selectionSet;
    if (acedSSGet(nullptr, nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }

    SelectionSetGuard guard(selectionSet);
    return findEntityInSelectionSet<TEntity>(selectionSet, entityId);
}

bool isFinitePoint(const AlignmentPoint2d& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

std::vector<AlignmentSamplePoint> collectAlignmentPathSamples(
    const roadproto::domain::alignment::HorizontalAlignment& alignment)
{
    std::vector<AlignmentSamplePoint> rawSamples;
    for (const auto& element : alignment.elements) {
        for (const auto& sample : element.samples) {
            if (std::isfinite(sample.station) && isFinitePoint(sample.point)) {
                rawSamples.push_back(sample);
            }
        }
    }

    std::sort(rawSamples.begin(), rawSamples.end(), [](const auto& left, const auto& right) {
        return left.station < right.station;
    });

    std::vector<AlignmentSamplePoint> samples;
    samples.reserve(rawSamples.size());
    for (const auto& sample : rawSamples) {
        if (samples.empty() || sample.station > samples.back().station + kStationEpsilon) {
            samples.push_back(sample);
        } else if (std::fabs(sample.station - samples.back().station) <= kStationEpsilon) {
            samples.back() = sample;
        }
    }
    return samples;
}

bool readRoadCenterline(
    AcDbObjectId id,
    std::wstring& handle,
    std::vector<AlignmentSamplePoint>& samples)
{
    DnRoadCenterlineEntity* entity = nullptr;
    if (acdbOpenObject(entity, id, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }

    handle = entityHandleText(entity);
    samples = collectAlignmentPathSamples(entity->alignment());
    entity->close();
    return !handle.empty() && samples.size() >= 2;
}

bool readVerticalCurve(
    AcDbObjectId id,
    std::wstring& handle,
    ProfileVerticalCurveData& data)
{
    DnProfileVerticalCurveEntity* entity = nullptr;
    if (acdbOpenObject(entity, id, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }

    handle = entityHandleText(entity);
    data = entity->curveData();
    entity->close();
    return !handle.empty();
}

bool readSubgradeTemplate(const std::wstring& handle, RoadModelTemplateSource& source)
{
    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handle, entityId)) {
        return false;
    }

    DnSubgradeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }
    if (!entity->isKindOf(DnSubgradeTemplateEntity::desc())) {
        entity->close();
        return false;
    }

    source.templateHandle = handle;
    source.data = entity->templateData();
    entity->close();
    return !source.templateHandle.empty();
}

bool readSlopeTemplate(const std::wstring& handle, RoadModelSlopeTemplateSource& source)
{
    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handle, entityId)) {
        return false;
    }

    DnSlopeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }
    if (!entity->isKindOf(DnSlopeTemplateEntity::desc())) {
        entity->close();
        return false;
    }

    source.templateHandle = handle;
    source.data = entity->templateData();
    entity->close();
    return !source.templateHandle.empty();
}

bool readPavementLayerTemplate(const std::wstring& handle, RoadModelPavementLayerTemplateSource& source)
{
    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handle, entityId)) {
        return false;
    }

    DnPavementLayerTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }
    if (!entity->isKindOf(DnPavementLayerTemplateEntity::desc())) {
        entity->close();
        return false;
    }

    source.templateHandle = handle;
    source.data = entity->templateData();
    entity->close();
    return !source.templateHandle.empty();
}

bool collectPavementLayerTemplates(
    roadproto::cad_adapter::IEditor& editor,
    const std::vector<RoadModelTemplateSource>& subgradeTemplateSources,
    std::vector<RoadModelPavementLayerTemplateSource>& pavementLayerTemplates)
{
    for (const auto& source : subgradeTemplateSources) {
        for (std::size_t componentIndex = 0; componentIndex < source.data.components.size(); ++componentIndex) {
            const auto& component = source.data.components[componentIndex];
            if (!component.pavementLayerLinked || component.pavementLayerHandle.empty()) {
                continue;
            }

            const auto alreadyAdded = std::any_of(
                pavementLayerTemplates.begin(),
                pavementLayerTemplates.end(),
                [&component](const auto& candidate) {
                    return candidate.templateHandle == component.pavementLayerHandle;
                });
            if (alreadyAdded) {
                continue;
            }

            RoadModelPavementLayerTemplateSource pavementSource;
            if (!readPavementLayerTemplate(component.pavementLayerHandle, pavementSource)) {
                std::wstringstream message;
                message << L"Cannot read pavement layer template entity for handle: "
                        << component.pavementLayerHandle
                        << L", subgrade template: " << source.templateHandle
                        << L", component index: " << componentIndex << L".";
                editor.writeError(message.str());
                return false;
            }

            pavementLayerTemplates.push_back(std::move(pavementSource));
        }
    }

    return true;
}

struct PavementLayerDrawingStyle {
    int colorR = 0;
    int colorG = 0;
    int colorB = 0;
    std::wstring hatchPattern = L"SOLID";
    double hatchAngle = 0.0;
    double hatchScale = 1.0;
};

std::wstring pavementLayerColorKey(int colorR, int colorG, int colorB)
{
    return std::to_wstring(std::clamp(colorR, 0, 255))
        + L"," + std::to_wstring(std::clamp(colorG, 0, 255))
        + L"," + std::to_wstring(std::clamp(colorB, 0, 255));
}

std::unordered_map<std::wstring, PavementLayerDrawingStyle> collectPavementLayerDrawingStyles(
    const RoadModelData& data)
{
    std::unordered_set<std::wstring> handles;
    for (const auto& line : data.pavementLayerLines) {
        if (!line.key.pavementLayerTemplateHandle.empty()) {
            handles.insert(line.key.pavementLayerTemplateHandle);
        }
    }

    std::unordered_map<std::wstring, PavementLayerDrawingStyle> styles;
    for (const auto& handle : handles) {
        RoadModelPavementLayerTemplateSource source;
        if (!readPavementLayerTemplate(handle, source)) {
            continue;
        }

        for (const auto& layer : source.data.layers) {
            const auto key = pavementLayerColorKey(layer.color.r, layer.color.g, layer.color.b);
            if (styles.find(key) != styles.end()) {
                continue;
            }
            styles[key] = PavementLayerDrawingStyle{
                layer.color.r,
                layer.color.g,
                layer.color.b,
                layer.hatchPattern,
                layer.hatchAngle,
                layer.hatchScale};
        }
    }
    return styles;
}

PavementLayerDrawingStyle styleForPavementSegment(
    const RoadModelSectionPreviewSegment& segment,
    const std::unordered_map<std::wstring, PavementLayerDrawingStyle>& styles)
{
    const auto found = styles.find(pavementLayerColorKey(segment.color.r, segment.color.g, segment.color.b));
    if (found != styles.end()) {
        return found->second;
    }
    return PavementLayerDrawingStyle{
        segment.color.r,
        segment.color.g,
        segment.color.b,
        L"SOLID",
        0.0,
        1.0};
}

int drawingKindForPreviewSegment(RoadModelSectionPreviewSegmentKind kind)
{
    switch (kind) {
    case RoadModelSectionPreviewSegmentKind::Slope:
        return 1;
    case RoadModelSectionPreviewSegmentKind::Ground:
        return 2;
    case RoadModelSectionPreviewSegmentKind::PavementLayer:
        return 3;
    case RoadModelSectionPreviewSegmentKind::Subgrade:
    default:
        return 0;
    }
}

RoadModelSectionDrawingPoint drawingPoint(
    const roadproto::domain::cross_section::RoadModelSectionPreviewPoint& point,
    double minOffset,
    double minElevation)
{
    return RoadModelSectionDrawingPoint{
        point.offset - minOffset + kSectionDrawingPadding,
        point.elevation - minElevation + kSectionDrawingStationLabelBand + kSectionDrawingPadding};
}

bool samePreviewPoint(
    const roadproto::domain::cross_section::RoadModelSectionPreviewPoint& lhs,
    const roadproto::domain::cross_section::RoadModelSectionPreviewPoint& rhs)
{
    return std::fabs(lhs.offset - rhs.offset) <= 1.0e-6
        && std::fabs(lhs.elevation - rhs.elevation) <= 1.0e-6;
}

bool isPavementLayerFace(
    const RoadModelSectionPreviewSegment& first,
    const RoadModelSectionPreviewSegment& second,
    const RoadModelSectionPreviewSegment& third,
    const RoadModelSectionPreviewSegment& fourth)
{
    return first.kind == RoadModelSectionPreviewSegmentKind::PavementLayer
        && second.kind == RoadModelSectionPreviewSegmentKind::PavementLayer
        && third.kind == RoadModelSectionPreviewSegmentKind::PavementLayer
        && fourth.kind == RoadModelSectionPreviewSegmentKind::PavementLayer
        && first.points.size() >= 2
        && second.points.size() >= 2
        && third.points.size() >= 2
        && fourth.points.size() >= 2
        && samePreviewPoint(first.points.back(), second.points.front())
        && samePreviewPoint(second.points.back(), third.points.front())
        && samePreviewPoint(third.points.back(), fourth.points.front())
        && samePreviewPoint(fourth.points.back(), first.points.front());
}

bool buildRoadModelSectionDrawingData(
    const RoadModelSectionViewerPreview& previewWrapper,
    const std::wstring& roadModelHandle,
    const std::wstring& roadCenterlineHandle,
    const AcGePoint3d& insertionPoint,
    const std::unordered_map<std::wstring, PavementLayerDrawingStyle>& pavementStyles,
    RoadModelSectionDrawingData& drawingData)
{
    const auto& preview = previewWrapper.data;
    std::vector<roadproto::domain::cross_section::RoadModelSectionPreviewPoint> points;
    for (const auto& segment : preview.segments) {
        points.insert(points.end(), segment.points.begin(), segment.points.end());
    }
    if (points.empty()) {
        return false;
    }

    auto minOffset = points.front().offset;
    auto maxOffset = points.front().offset;
    auto minElevation = points.front().elevation;
    auto maxElevation = points.front().elevation;
    for (const auto& point : points) {
        if (!std::isfinite(point.offset) || !std::isfinite(point.elevation)) {
            return false;
        }
        minOffset = std::min(minOffset, point.offset);
        maxOffset = std::max(maxOffset, point.offset);
        minElevation = std::min(minElevation, point.elevation);
        maxElevation = std::max(maxElevation, point.elevation);
    }
    if (maxOffset - minOffset < 1.0e-6) {
        minOffset -= 1.0;
        maxOffset += 1.0;
    }
    if (maxElevation - minElevation < 1.0e-6) {
        minElevation -= 1.0;
        maxElevation += 1.0;
    }

    drawingData = RoadModelSectionDrawingData{};
    drawingData.roadModelHandle = roadModelHandle;
    drawingData.roadCenterlineHandle = roadCenterlineHandle;
    drawingData.station = preview.station;
    drawingData.stationLabel = roadproto::domain::alignment::StationFormatter::format(preview.station);
    drawingData.insertionPoint = insertionPoint;
    drawingData.width = (maxOffset - minOffset) + kSectionDrawingPadding * 2.0;
    drawingData.height = (maxElevation - minElevation) + kSectionDrawingStationLabelBand + kSectionDrawingPadding * 2.0;

    for (const auto& segment : preview.segments) {
        if (segment.points.size() < 2) {
            continue;
        }
        RoadModelSectionDrawingLine line;
        line.kind = drawingKindForPreviewSegment(segment.kind);
        line.colorR = segment.color.r;
        line.colorG = segment.color.g;
        line.colorB = segment.color.b;
        line.points.reserve(segment.points.size());
        for (const auto& point : segment.points) {
            line.points.push_back(drawingPoint(point, minOffset, minElevation));
        }
        drawingData.lines.push_back(std::move(line));
    }

    for (std::size_t i = 0; i + 3 < preview.segments.size(); ++i) {
        const auto& first = preview.segments[i];
        const auto& second = preview.segments[i + 1];
        const auto& third = preview.segments[i + 2];
        const auto& fourth = preview.segments[i + 3];
        if (!isPavementLayerFace(first, second, third, fourth)) {
            continue;
        }

        const auto style = styleForPavementSegment(first, pavementStyles);
        RoadModelSectionDrawingFace face;
        face.colorR = style.colorR;
        face.colorG = style.colorG;
        face.colorB = style.colorB;
        face.hatchPattern = style.hatchPattern;
        face.hatchAngle = style.hatchAngle;
        face.hatchScale = style.hatchScale;
        face.points = {
            drawingPoint(first.points.front(), minOffset, minElevation),
            drawingPoint(first.points.back(), minOffset, minElevation),
            drawingPoint(second.points.back(), minOffset, minElevation),
            drawingPoint(third.points.back(), minOffset, minElevation)};
        drawingData.faces.push_back(std::move(face));
        i += 3;
    }

    return !drawingData.lines.empty();
}

bool appendRoadModelSectionDrawingEntities(
    roadproto::cad_adapter::IEditor& editor,
    const std::vector<RoadModelSectionViewerPreview>& previews,
    const RoadModelData& data,
    const std::wstring& roadModelHandle,
    const std::wstring& roadCenterlineHandle,
    const AcGePoint3d& basePoint)
{
    const auto pavementStyles = collectPavementLayerDrawingStyles(data);
    auto insertionPoint = basePoint;
    int appendedCount = 0;
    for (const auto& preview : previews) {
        RoadModelSectionDrawingData drawingData;
        if (!buildRoadModelSectionDrawingData(
                preview,
                roadModelHandle,
                roadCenterlineHandle,
                insertionPoint,
                pavementStyles,
                drawingData)) {
            continue;
        }

        auto* entity = new DnRoadModelSectionDrawingEntity();
        if (entity->setDrawingData(drawingData) != Acad::eOk) {
            delete entity;
            continue;
        }

        AcDbObjectId entityId;
        if (!appendEntityToModelSpace(entity, entityId)) {
            delete entity;
            editor.writeError(L"Failed to append DnRoadModelSectionDrawingEntity to model space.");
            return false;
        }

        entity->recordGraphicsModified(true);
        entity->close();
        insertionPoint.y += drawingData.height + kSectionDrawingSpacing;
        ++appendedCount;
    }

    if (appendedCount == 0) {
        editor.writeWarning(L"No road model section drawing entities were created.");
        return false;
    }

    acedUpdateDisplay();
    editor.writeMessage(L"Road model section drawings created: " + std::to_wstring(appendedCount));
    return true;
}

bool readTerrainSurface(const std::wstring& handle, RoadModelTerrainSurface& surface)
{
    if (handle.empty()) {
        return false;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handle, entityId)) {
        return false;
    }

    DnTerrainTinEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }
    if (!entity->isKindOf(DnTerrainTinEntity::desc())) {
        entity->close();
        return false;
    }

    surface.points = entity->getPoints();
    surface.triangles = entity->getTriangles();
    entity->close();
    return !surface.points.empty() && !surface.triangles.empty();
}

bool collectProfileGraphHandlesForCenterline(
    const std::wstring& centerlineHandle,
    std::vector<std::wstring>& graphHandles)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr || centerlineHandle.empty()) {
        return false;
    }

    AcDbBlockTable* blockTable = nullptr;
    if (database->getBlockTable(blockTable, AcDb::kForRead) != Acad::eOk || blockTable == nullptr) {
        return false;
    }

    AcDbBlockTableRecord* modelSpace = nullptr;
    const auto status = blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForRead);
    blockTable->close();
    if (status != Acad::eOk || modelSpace == nullptr) {
        return false;
    }

    AcDbBlockTableRecordIterator* iterator = nullptr;
    if (modelSpace->newIterator(iterator) != Acad::eOk || iterator == nullptr) {
        modelSpace->close();
        return false;
    }

    for (; !iterator->done(); iterator->step()) {
        AcDbEntity* entity = nullptr;
        if (iterator->getEntity(entity, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
            continue;
        }

        if (entity->isKindOf(DnProfileGradeGraphEntity::desc())) {
            auto* graph = static_cast<DnProfileGradeGraphEntity*>(entity);
            if (graph->graphData().roadCenterlineHandle == centerlineHandle) {
                graphHandles.push_back(entityHandleText(graph));
            }
        }
        entity->close();
    }

    delete iterator;
    modelSpace->close();
    return true;
}

bool collectVerticalCurveForGraphs(
    const std::vector<std::wstring>& graphHandles,
    std::vector<AcDbObjectId>& curveIds)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr || graphHandles.empty()) {
        return false;
    }

    AcDbBlockTable* blockTable = nullptr;
    if (database->getBlockTable(blockTable, AcDb::kForRead) != Acad::eOk || blockTable == nullptr) {
        return false;
    }

    AcDbBlockTableRecord* modelSpace = nullptr;
    const auto status = blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForRead);
    blockTable->close();
    if (status != Acad::eOk || modelSpace == nullptr) {
        return false;
    }

    AcDbBlockTableRecordIterator* iterator = nullptr;
    if (modelSpace->newIterator(iterator) != Acad::eOk || iterator == nullptr) {
        modelSpace->close();
        return false;
    }

    for (; !iterator->done(); iterator->step()) {
        AcDbEntity* entity = nullptr;
        if (iterator->getEntity(entity, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
            continue;
        }

        if (entity->isKindOf(DnProfileVerticalCurveEntity::desc())) {
            auto* curve = static_cast<DnProfileVerticalCurveEntity*>(entity);
            const auto& profileGraphHandle = curve->curveData().profileGraphHandle;
            if (std::find(graphHandles.begin(), graphHandles.end(), profileGraphHandle) != graphHandles.end()) {
                curveIds.push_back(curve->objectId());
            }
        }
        entity->close();
    }

    delete iterator;
    modelSpace->close();
    return true;
}

bool findUniqueVerticalCurveForCenterline(
    const std::wstring& centerlineHandle,
    AcDbObjectId& verticalCurveId)
{
    std::vector<std::wstring> graphHandles;
    if (!collectProfileGraphHandlesForCenterline(centerlineHandle, graphHandles) || graphHandles.empty()) {
        return false;
    }

    std::vector<AcDbObjectId> curveIds;
    if (!collectVerticalCurveForGraphs(graphHandles, curveIds) || curveIds.size() != 1) {
        return false;
    }

    verticalCurveId = curveIds.front();
    return true;
}

bool profileGraphBelongsToCenterline(
    const std::wstring& profileGraphHandle,
    const std::wstring& centerlineHandle)
{
    if (profileGraphHandle.empty() || centerlineHandle.empty()) {
        return false;
    }

    AcDbObjectId graphId;
    if (!resolveObjectIdFromHandle(profileGraphHandle, graphId)) {
        return false;
    }

    DnProfileGradeGraphEntity* graph = nullptr;
    if (acdbOpenObject(graph, graphId, AcDb::kForRead) != Acad::eOk || graph == nullptr) {
        return false;
    }

    const auto belongs = graph->isKindOf(DnProfileGradeGraphEntity::desc())
        && graph->graphData().roadCenterlineHandle == centerlineHandle;
    graph->close();
    return belongs;
}

std::wstring terrainTinHandleForProfileGraph(const std::wstring& profileGraphHandle)
{
    if (profileGraphHandle.empty()) {
        return L"";
    }

    AcDbObjectId graphId;
    if (!resolveObjectIdFromHandle(profileGraphHandle, graphId)) {
        return L"";
    }

    DnProfileGradeGraphEntity* graph = nullptr;
    if (acdbOpenObject(graph, graphId, AcDb::kForRead) != Acad::eOk || graph == nullptr) {
        return L"";
    }

    std::wstring terrainHandle;
    if (graph->isKindOf(DnProfileGradeGraphEntity::desc())) {
        terrainHandle = graph->graphData().terrainTinHandle;
    }
    graph->close();
    return terrainHandle;
}

const RoadModelSection* findRoadModelSectionAtStation(
    const std::vector<RoadModelSection>& sections,
    double station)
{
    const RoadModelSection* best = nullptr;
    double bestDistance = std::numeric_limits<double>::infinity();
    for (const auto& section : sections) {
        const double distance = std::fabs(section.station - station);
        if (distance < bestDistance) {
            best = &section;
            bestDistance = distance;
        }
    }
    return bestDistance <= kStationEpsilon ? best : nullptr;
}

bool hasUsableGroundSnapshot(const RoadModelSection& section)
{
    return section.leftGroundProfile.size() + section.rightGroundProfile.size() >= 2;
}

bool needsTerrainSurfaceForSectionPreview(
    const RoadModelData& data,
    const std::vector<double>& stations)
{
    for (const auto station : stations) {
        const auto* section = findRoadModelSectionAtStation(data.sections, station);
        if (section == nullptr || !hasUsableGroundSnapshot(*section)) {
            return true;
        }
    }
    return false;
}

bool queueDialogForRoadModelEdit(AcDbObjectId roadModelId)
{
    auto& editor = app::ApplicationContext::instance().editor();

    DnRoadModelEntity* entity = nullptr;
    if (acdbOpenObject(entity, roadModelId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"Cannot open road model entity.");
        return false;
    }
    if (!entity->isKindOf(DnRoadModelEntity::desc())) {
        entity->close();
        editor.writeWarning(L"The selected object is not a road model entity.");
        return false;
    }

    RoadModelDialogRequest request;
    request.handle = entityHandleText(entity);
    const auto config = entity->roadModelData().config;
    request.roadCenterlineHandle = config.roadCenterlineHandle;
    request.profileVerticalCurveHandle = config.profileVerticalCurveHandle;
    request.sampleInterval = config.sampleInterval;
    request.assignments = config.assignments;
    request.structures = config.structures;
    request.leftSlopeSearchWidth = config.slopeConfig.leftSearchWidth;
    request.rightSlopeSearchWidth = config.slopeConfig.rightSearchWidth;
    request.leftSlopeGroups = config.slopeConfig.leftGroups;
    request.rightSlopeGroups = config.slopeConfig.rightGroups;
    entity->close();

    std::wstring errorMessage;
    if (!queueRoadModelWpfDialog(request, errorMessage)) {
        editor.writeError(L"Failed to open road model WPF dialog: " + errorMessage);
        return false;
    }

    return true;
}

bool promptSubgradeTemplateForRoadModel(std::wstring& templateHandle, std::wstring& templateName)
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"请选择要用于该行的路基模板实体。");

    AcDbObjectId templateId;
    if (!selectTypedEntity<DnSubgradeTemplateEntity>(templateId)) {
        editor.writeWarning(L"未选择路基模板实体。");
        return false;
    }

    DnSubgradeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, templateId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开路基模板实体。");
        return false;
    }
    if (!entity->isKindOf(DnSubgradeTemplateEntity::desc())) {
        entity->close();
        editor.writeWarning(L"选择对象不是 RoadProto 路基模板实体。");
        return false;
    }

    templateHandle = entityHandleText(entity);
    templateName = entity->templateData().properties.name.empty()
        ? templateHandle
        : entity->templateData().properties.name;
    entity->close();
    return !templateHandle.empty();
}

bool promptSlopeTemplateForRoadModel(std::wstring& templateHandle, std::wstring& templateName)
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"请选择要加入该模板组的边坡模板实体。");

    AcDbObjectId templateId;
    if (!selectTypedEntity<DnSlopeTemplateEntity>(templateId)) {
        editor.writeWarning(L"未选择边坡模板实体。");
        return false;
    }

    DnSlopeTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, templateId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开边坡模板实体。");
        return false;
    }
    if (!entity->isKindOf(DnSlopeTemplateEntity::desc())) {
        entity->close();
        editor.writeWarning(L"选择对象不是 RoadProto 边坡模板实体。");
        return false;
    }

    templateHandle = entityHandleText(entity);
    templateName = entity->templateData().properties.name.empty()
        ? templateHandle
        : entity->templateData().properties.name;
    entity->close();
    return !templateHandle.empty();
}

bool handlePickTemplateAction(const RoadModelDialogResponse& response)
{
    auto& editor = app::ApplicationContext::instance().editor();

    RoadModelDialogRequest request;
    request.handle = response.handle;
    request.roadCenterlineHandle = response.roadCenterlineHandle;
    request.profileVerticalCurveHandle = response.profileVerticalCurveHandle;
    request.sampleInterval = response.sampleInterval;
    request.selectedAssignmentIndex = response.pickAssignmentIndex;
    request.assignments = response.assignments;
    request.structures = response.structures;
    request.leftSlopeSearchWidth = response.leftSlopeSearchWidth;
    request.rightSlopeSearchWidth = response.rightSlopeSearchWidth;
    request.leftSlopeGroups = response.leftSlopeGroups;
    request.rightSlopeGroups = response.rightSlopeGroups;

    if (response.pickAssignmentIndex < 0
        || response.pickAssignmentIndex >= static_cast<int>(request.assignments.size())) {
        editor.writeWarning(L"路基模板点选行索引无效，已重新打开横断面戴帽窗口。");
    } else {
        std::wstring templateHandle;
        std::wstring templateName;
        if (promptSubgradeTemplateForRoadModel(templateHandle, templateName)) {
            auto& assignment = request.assignments[static_cast<std::size_t>(response.pickAssignmentIndex)];
            assignment.templateHandle = templateHandle;
            assignment.templateName = templateName;
        }
    }

    std::wstring errorMessage;
    if (!queueRoadModelWpfDialog(request, errorMessage)) {
        editor.writeError(L"重新打开道路模型 WPF 对话框失败: " + errorMessage);
        return false;
    }
    return true;
}

bool handlePickSlopeTemplateAction(const RoadModelDialogResponse& response, bool leftSide)
{
    auto& editor = app::ApplicationContext::instance().editor();

    RoadModelDialogRequest request;
    request.handle = response.handle;
    request.roadCenterlineHandle = response.roadCenterlineHandle;
    request.profileVerticalCurveHandle = response.profileVerticalCurveHandle;
    request.sampleInterval = response.sampleInterval;
    request.leftSlopeSearchWidth = response.leftSlopeSearchWidth;
    request.rightSlopeSearchWidth = response.rightSlopeSearchWidth;
    request.assignments = response.assignments;
    request.structures = response.structures;
    request.leftSlopeGroups = response.leftSlopeGroups;
    request.rightSlopeGroups = response.rightSlopeGroups;
    request.selectedLeftSlopeGroupIndex = leftSide ? response.pickSlopeGroupIndex : -1;
    request.selectedRightSlopeGroupIndex = leftSide ? -1 : response.pickSlopeGroupIndex;

    auto& groups = leftSide ? request.leftSlopeGroups : request.rightSlopeGroups;
    if (response.pickSlopeGroupIndex < 0
        || response.pickSlopeGroupIndex >= static_cast<int>(groups.size())) {
        editor.writeWarning(L"边坡模板组点选行索引无效，已重新打开横断面戴帽窗口。");
    } else {
        std::wstring templateHandle;
        std::wstring templateName;
        if (promptSlopeTemplateForRoadModel(templateHandle, templateName)) {
            auto& group = groups[static_cast<std::size_t>(response.pickSlopeGroupIndex)];
            group.templates.push_back(RoadModelSlopeTemplateReference{templateHandle, templateName});
        }
    }

    std::wstring errorMessage;
    if (!queueRoadModelWpfDialog(request, errorMessage)) {
        editor.writeError(L"重新打开道路模型 WPF 对话框失败: " + errorMessage);
        return false;
    }
    return true;
}

bool promptRoadModelHandle(std::wstring& handle)
{
    ACHAR handleBuffer[128] = {};
    if (acedGetString(Adesk::kFalse, L"\nRoad model handle: ", handleBuffer) != RTNORM) {
        return false;
    }

    handle = trimDialogCommandPath(handleBuffer);
    return !handle.empty();
}

std::wstring resultErrorText(const std::wstring& fallback, const std::wstring& errorMessage)
{
    return errorMessage.empty() ? fallback : errorMessage;
}

void writeCreatedRoadModelMessage(
    roadproto::cad_adapter::IEditor& editor,
    const std::wstring& handle,
    const roadproto::domain::cross_section::RoadModelData& data)
{
    std::wstringstream stream;
    stream << L"Created road model entity, handle: " << handle
           << L", component line count: " << data.componentLines.size()
           << L", slope line count: " << data.slopeLines.size() << L".";
    editor.writeMessage(stream.str());
}

void runRoadModelCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_SECTION_ROAD_MODEL_CREATE: select a road centerline.");

    AcDbObjectId centerlineId;
    if (!selectTypedEntity<DnRoadCenterlineEntity>(centerlineId)) {
        editor.writeWarning(L"No road centerline entity was selected.");
        return;
    }

    std::wstring centerlineHandle;
    std::vector<AlignmentSamplePoint> alignmentSamples;
    if (!readRoadCenterline(centerlineId, centerlineHandle, alignmentSamples)) {
        editor.writeError(L"Cannot read road centerline data.");
        return;
    }

    AcDbObjectId verticalCurveId;
    if (!findUniqueVerticalCurveForCenterline(centerlineHandle, verticalCurveId)) {
        editor.writeMessage(L"No unique related profile vertical curve was found. Select a vertical curve.");
        if (!selectTypedEntity<DnProfileVerticalCurveEntity>(verticalCurveId)) {
            editor.writeWarning(L"Road model creation was cancelled.");
            return;
        }
    }

    std::wstring verticalCurveHandle;
    ProfileVerticalCurveData verticalCurve;
    if (!readVerticalCurve(verticalCurveId, verticalCurveHandle, verticalCurve)) {
        editor.writeError(L"Cannot read profile vertical curve data.");
        return;
    }
    if (!profileGraphBelongsToCenterline(verticalCurve.profileGraphHandle, centerlineHandle)) {
        editor.writeWarning(L"The selected profile vertical curve does not belong to the selected road centerline.");
        return;
    }

    RoadModelDialogRequest request;
    request.roadCenterlineHandle = centerlineHandle;
    request.profileVerticalCurveHandle = verticalCurveHandle;
    request.sampleInterval = 10.0;

    std::wstring errorMessage;
    if (!queueRoadModelWpfDialog(request, errorMessage)) {
        editor.writeError(L"Failed to open road model WPF dialog: " + errorMessage);
    }
}

void runRoadModelEditCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    AcDbObjectId roadModelId;
    if (!selectTypedEntity<DnRoadModelEntity>(roadModelId)) {
        editor.writeWarning(L"No road model entity was selected.");
        return;
    }

    queueDialogForRoadModelEdit(roadModelId);
}

void runRoadModelEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    std::wstring handle;
    if (!promptRoadModelHandle(handle)) {
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handle, entityId)) {
        editor.writeWarning(L"No road model entity was found for the handle.");
        return;
    }

    DnRoadModelEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"Cannot open road model entity.");
        return;
    }
    const auto isRoadModel = entity->isKindOf(DnRoadModelEntity::desc());
    entity->close();
    if (!isRoadModel) {
        editor.writeWarning(L"The handle does not reference a road model entity.");
        return;
    }

    queueDialogForRoadModelEdit(entityId);
}

void runRoadModelViewSectionCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_SECTION_ROAD_MODEL_VIEW_SECTION: select a road model entity.");

    AcDbObjectId roadModelId;
    if (!selectTypedEntity<DnRoadModelEntity>(roadModelId)) {
        editor.writeWarning(L"No road model entity was selected.");
        return;
    }

    DnRoadModelEntity* entity = nullptr;
    if (acdbOpenObject(entity, roadModelId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"Cannot open road model entity.");
        return;
    }
    if (!entity->isKindOf(DnRoadModelEntity::desc())) {
        entity->close();
        editor.writeWarning(L"The selected object is not a road model entity.");
        return;
    }

    const auto roadModelHandle = entityHandleText(entity);
    RoadModelData data = entity->roadModelData();
    entity->close();

    AcDbObjectId centerlineId;
    if (!resolveObjectIdFromHandle(data.config.roadCenterlineHandle, centerlineId)) {
        editor.writeWarning(L"No road centerline entity was found for the selected road model.");
        return;
    }

    std::wstring centerlineHandle;
    std::vector<AlignmentSamplePoint> alignmentSamples;
    if (!readRoadCenterline(centerlineId, centerlineHandle, alignmentSamples)) {
        editor.writeError(L"Cannot read road centerline data for section preview.");
        return;
    }

    ProfileVerticalCurveData verticalCurve;
    if (!data.config.profileVerticalCurveHandle.empty()) {
        AcDbObjectId verticalCurveId;
        std::wstring verticalCurveHandle;
        if (resolveObjectIdFromHandle(data.config.profileVerticalCurveHandle, verticalCurveId)) {
            readVerticalCurve(verticalCurveId, verticalCurveHandle, verticalCurve);
        }
    }

    auto stations = data.sampledStations;
    if (stations.empty() && !alignmentSamples.empty()) {
        stations = roadproto::domain::cross_section::RoadModelStationSampler::collectStations(
            alignmentSamples.front().station,
            alignmentSamples.back().station,
            verticalCurve,
            data.config.assignments,
            data.config.sampleInterval);
    }
    if (stations.empty()) {
        editor.writeWarning(L"The selected road model has no sampled station data for section preview.");
        return;
    }

    RoadModelTerrainSurface terrainSurface;
    if (needsTerrainSurfaceForSectionPreview(data, stations)) {
        const auto terrainHandle = terrainTinHandleForProfileGraph(verticalCurve.profileGraphHandle);
        if (!terrainHandle.empty()) {
            readTerrainSurface(terrainHandle, terrainSurface);
        }
    }

    RoadModelSectionViewerRequest request;
    request.handle = roadModelHandle;
    request.roadCenterlineHandle = centerlineHandle;
    request.previews.reserve(stations.size());
    RoadModelSectionPreviewRequest previewRequest;
    previewRequest.data = data;
    previewRequest.alignmentSamples = alignmentSamples;
    previewRequest.terrainSurface = terrainSurface;
    for (const auto station : stations) {
        previewRequest.station = station;

        auto preview = RoadModelSectionPreviewBuilder::build(previewRequest);
        if (preview.succeeded) {
            request.previews.push_back(RoadModelSectionViewerPreview{std::move(preview)});
        }
    }

    if (request.previews.empty()) {
        editor.writeWarning(L"No section preview could be generated for the selected road model.");
        return;
    }

    std::wstring errorMessage;
    if (!queueRoadModelSectionViewerWpfDialog(request, errorMessage)) {
        editor.writeError(L"Failed to open road model section viewer: " + errorMessage);
    }
}

void runRoadModelViewSectionApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto road model section viewer response file: ", pathBuffer) != RTNORM) {
        return;
    }

    RoadModelSectionViewerResponse response;
    std::wstring errorMessage;
    const auto responsePath = trimDialogCommandPath(pathBuffer);
    if (!readRoadModelSectionViewerResponse(responsePath, response, errorMessage)) {
        editor.writeError(L"Failed to read road model section viewer response: " + errorMessage);
        return;
    }

    if (!response.accepted || response.action != RoadModelSectionViewerAction::DrawSections) {
        return;
    }

    AcDbObjectId roadModelId;
    if (!resolveObjectIdFromHandle(response.handle, roadModelId)) {
        editor.writeWarning(L"No road model entity was found for the section viewer response.");
        return;
    }

    DnRoadModelEntity* entity = nullptr;
    if (acdbOpenObject(entity, roadModelId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"Cannot open road model entity for section drawing.");
        return;
    }
    if (!entity->isKindOf(DnRoadModelEntity::desc())) {
        entity->close();
        editor.writeWarning(L"The section viewer response handle does not reference a road model entity.");
        return;
    }

    const auto roadModelHandle = entityHandleText(entity);
    RoadModelData data = entity->roadModelData();
    entity->close();

    AcDbObjectId centerlineId;
    if (!resolveObjectIdFromHandle(data.config.roadCenterlineHandle, centerlineId)) {
        editor.writeWarning(L"No road centerline entity was found for the selected road model.");
        return;
    }

    std::wstring centerlineHandle;
    std::vector<AlignmentSamplePoint> alignmentSamples;
    if (!readRoadCenterline(centerlineId, centerlineHandle, alignmentSamples)) {
        editor.writeError(L"Cannot read road centerline data for section drawing.");
        return;
    }

    ProfileVerticalCurveData verticalCurve;
    if (!data.config.profileVerticalCurveHandle.empty()) {
        AcDbObjectId verticalCurveId;
        std::wstring verticalCurveHandle;
        if (resolveObjectIdFromHandle(data.config.profileVerticalCurveHandle, verticalCurveId)) {
            readVerticalCurve(verticalCurveId, verticalCurveHandle, verticalCurve);
        }
    }

    auto stations = data.sampledStations;
    if (stations.empty() && !alignmentSamples.empty()) {
        stations = roadproto::domain::cross_section::RoadModelStationSampler::collectStations(
            alignmentSamples.front().station,
            alignmentSamples.back().station,
            verticalCurve,
            data.config.assignments,
            data.config.sampleInterval);
    }
    if (stations.empty()) {
        editor.writeWarning(L"The selected road model has no sampled station data for section drawing.");
        return;
    }

    RoadModelTerrainSurface terrainSurface;
    if (needsTerrainSurfaceForSectionPreview(data, stations)) {
        const auto terrainHandle = terrainTinHandleForProfileGraph(verticalCurve.profileGraphHandle);
        if (!terrainHandle.empty()) {
            readTerrainSurface(terrainHandle, terrainSurface);
        }
    }

    std::vector<RoadModelSectionViewerPreview> previews;
    previews.reserve(stations.size());
    RoadModelSectionPreviewRequest previewRequest;
    previewRequest.data = data;
    previewRequest.alignmentSamples = alignmentSamples;
    previewRequest.terrainSurface = terrainSurface;
    for (const auto station : stations) {
        previewRequest.station = station;
        auto preview = RoadModelSectionPreviewBuilder::build(previewRequest);
        if (preview.succeeded) {
            previews.push_back(RoadModelSectionViewerPreview{std::move(preview)});
        }
    }

    if (previews.empty()) {
        editor.writeWarning(L"No section preview could be generated for drawing.");
        return;
    }

    ads_point point;
    if (acedGetPoint(nullptr, L"\n选择横断面绘制基点: ", point) != RTNORM) {
        return;
    }

    const AcGePoint3d basePoint(point[0], point[1], point[2]);
    appendRoadModelSectionDrawingEntities(
        editor,
        previews,
        data,
        roadModelHandle,
        centerlineHandle,
        basePoint);
}

void runRoadModelApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto road model dialog response file: ", pathBuffer) != RTNORM) {
        return;
    }

    RoadModelDialogResponse response;
    std::wstring errorMessage;
    const auto responsePath = trimDialogCommandPath(pathBuffer);
    if (!readRoadModelDialogResponse(responsePath, response, errorMessage)) {
        editor.writeError(L"Failed to read road model dialog response: " + errorMessage);
        return;
    }

    if (response.action == RoadModelDialogAction::PickTemplate) {
        handlePickTemplateAction(response);
        return;
    }
    if (response.action == RoadModelDialogAction::PickLeftSlopeTemplate) {
        handlePickSlopeTemplateAction(response, true);
        return;
    }
    if (response.action == RoadModelDialogAction::PickRightSlopeTemplate) {
        handlePickSlopeTemplateAction(response, false);
        return;
    }

    if (!response.accepted) {
        return;
    }

    AcDbObjectId centerlineId;
    if (!resolveObjectIdFromHandle(response.roadCenterlineHandle, centerlineId)) {
        editor.writeWarning(L"No road centerline entity was found for the dialog response.");
        return;
    }

    std::wstring centerlineHandle;
    std::vector<AlignmentSamplePoint> alignmentSamples;
    if (!readRoadCenterline(centerlineId, centerlineHandle, alignmentSamples)) {
        editor.writeError(L"Cannot read road centerline data.");
        return;
    }

    AcDbObjectId verticalCurveId;
    if (!resolveObjectIdFromHandle(response.profileVerticalCurveHandle, verticalCurveId)) {
        editor.writeWarning(L"No profile vertical curve entity was found for the dialog response.");
        return;
    }

    std::wstring verticalCurveHandle;
    ProfileVerticalCurveData verticalCurve;
    if (!readVerticalCurve(verticalCurveId, verticalCurveHandle, verticalCurve)) {
        editor.writeError(L"Cannot read profile vertical curve data.");
        return;
    }
    if (!profileGraphBelongsToCenterline(verticalCurve.profileGraphHandle, centerlineHandle)) {
        editor.writeError(L"The profile vertical curve does not belong to the road centerline in the dialog response.");
        return;
    }

    RoadModelConfig config;
    config.roadCenterlineHandle = centerlineHandle;
    config.profileVerticalCurveHandle = verticalCurveHandle;
    config.sampleInterval = response.sampleInterval;
    config.assignments = response.assignments;
    config.structures = response.structures;
    config.slopeConfig.leftSearchWidth = response.leftSlopeSearchWidth;
    config.slopeConfig.rightSearchWidth = response.rightSlopeSearchWidth;
    config.slopeConfig.leftGroups = response.leftSlopeGroups;
    config.slopeConfig.rightGroups = response.rightSlopeGroups;

    std::vector<RoadModelTemplateSource> templateSources;
    templateSources.reserve(config.assignments.size());
    for (auto& assignment : config.assignments) {
        RoadModelTemplateSource source;
        if (!readSubgradeTemplate(assignment.templateHandle, source)) {
            editor.writeError(L"Cannot read subgrade template entity for handle: " + assignment.templateHandle);
            return;
        }
        if (assignment.templateName.empty()) {
            assignment.templateName = source.data.properties.name.empty()
                ? assignment.templateHandle
                : source.data.properties.name;
        }
        templateSources.push_back(std::move(source));
    }

    std::vector<RoadModelSlopeTemplateSource> slopeTemplateSources;
    const auto collectSlopeTemplates = [&editor, &slopeTemplateSources](const std::vector<RoadModelSlopeTemplateGroup>& groups) {
        for (const auto& group : groups) {
            for (const auto& reference : group.templates) {
                if (reference.templateHandle.empty()) {
                    continue;
                }
                const auto alreadyAdded = std::any_of(
                    slopeTemplateSources.begin(),
                    slopeTemplateSources.end(),
                    [&reference](const auto& source) {
                        return source.templateHandle == reference.templateHandle;
                    });
                if (alreadyAdded) {
                    continue;
                }
                RoadModelSlopeTemplateSource source;
                if (!readSlopeTemplate(reference.templateHandle, source)) {
                    editor.writeError(L"Cannot read slope template entity for handle: " + reference.templateHandle);
                    return false;
                }
                slopeTemplateSources.push_back(std::move(source));
            }
        }
        return true;
    };
    if (!collectSlopeTemplates(config.slopeConfig.leftGroups)
        || !collectSlopeTemplates(config.slopeConfig.rightGroups)) {
        return;
    }

    std::vector<RoadModelPavementLayerTemplateSource> pavementLayerTemplateSources;
    if (!collectPavementLayerTemplates(editor, templateSources, pavementLayerTemplateSources)) {
        return;
    }

    RoadModelTerrainSurface terrainSurface;
    const auto terrainHandle = terrainTinHandleForProfileGraph(verticalCurve.profileGraphHandle);
    if (!terrainHandle.empty()) {
        readTerrainSurface(terrainHandle, terrainSurface);
    }

    RoadModelBuildInput input;
    input.config = std::move(config);
    input.alignmentSamples = std::move(alignmentSamples);
    input.verticalCurve = std::move(verticalCurve);
    input.templates = std::move(templateSources);
    input.slopeTemplates = std::move(slopeTemplateSources);
    input.pavementLayerTemplates = std::move(pavementLayerTemplateSources);
    input.terrainSurface = std::move(terrainSurface);

    StatusProgressMeter progressMeter(L"横断面戴帽生成模型", 0, 100);
    input.progressCallback = [&progressMeter](int percent, const std::wstring&) {
        progressMeter.setPosition(percent);
    };

    RoadModelBuildService service;
    const auto result = service.build(input);
    if (!result.succeeded) {
        editor.writeError(L"Road model build failed: " + resultErrorText(L"invalid road model input.", result.errorMessage));
        return;
    }
    if (result.diagnostics.terrainTriangleCount > 0) {
        const auto averageCandidates = result.diagnostics.groundProfileQueryCount == 0
            ? 0
            : result.diagnostics.groundProfileCandidateTotal / result.diagnostics.groundProfileQueryCount;
        editor.writeMessage(
            L"Road model terrain index: enabled=" + std::to_wstring(result.diagnostics.terrainIndexEnabled ? 1 : 0) +
            L", grid=" + std::to_wstring(result.diagnostics.terrainIndexColumns) +
            L"x" + std::to_wstring(result.diagnostics.terrainIndexRows) +
            L", triangles=" + std::to_wstring(result.diagnostics.terrainTriangleCount) +
            L", refs=" + std::to_wstring(result.diagnostics.terrainIndexTriangleReferences) +
            L", global=" + std::to_wstring(result.diagnostics.terrainIndexGlobalTriangles) +
            L", groundQueries=" + std::to_wstring(result.diagnostics.groundProfileQueryCount) +
            L", avgCandidates=" + std::to_wstring(averageCandidates) +
            L", maxCandidates=" + std::to_wstring(result.diagnostics.groundProfileCandidateMax));
    }

    if (response.handle.empty()) {
        auto* entity = new DnRoadModelEntity();
        entity->setRoadModelData(result.data);

        AcDbObjectId entityId;
        if (!appendEntityToModelSpace(entity, entityId)) {
            delete entity;
            editor.writeError(L"Failed to append DnRoadModelEntity to model space.");
            return;
        }

        const auto handle = entityHandleText(entity);
        entity->recordGraphicsModified(true);
        entity->close();
        acedUpdateDisplay();
        writeCreatedRoadModelMessage(editor, handle, result.data);
        return;
    }

    AcDbObjectId roadModelId;
    if (!resolveObjectIdFromHandle(response.handle, roadModelId)) {
        editor.writeWarning(L"No road model entity was found for the dialog response.");
        return;
    }

    DnRoadModelEntity* entity = nullptr;
    if (acdbOpenObject(entity, roadModelId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"Cannot open road model entity for write.");
        return;
    }
    if (!entity->isKindOf(DnRoadModelEntity::desc())) {
        entity->close();
        editor.writeWarning(L"The dialog response handle does not reference a road model entity.");
        return;
    }

    entity->setRoadModelData(result.data);
    entity->recordGraphicsModified(true);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"Road model entity has been updated.");
}

#else

void runRoadModelCreateCommand()
{
}

void runRoadModelEditCommand()
{
}

void runRoadModelEditHandleCommand()
{
}

void runRoadModelApplyDialogFileCommand()
{
}

void runRoadModelViewSectionCommand()
{
}

void runRoadModelViewSectionApplyDialogFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure roadModelCreateCommandProcedure()
{
    return &runRoadModelCreateCommand;
}

core::CommandProcedure roadModelEditCommandProcedure()
{
    return &runRoadModelEditCommand;
}

core::CommandProcedure roadModelEditHandleCommandProcedure()
{
    return &runRoadModelEditHandleCommand;
}

core::CommandProcedure roadModelApplyDialogFileCommandProcedure()
{
    return &runRoadModelApplyDialogFileCommand;
}

core::CommandProcedure roadModelViewSectionCommandProcedure()
{
    return &runRoadModelViewSectionCommand;
}

core::CommandProcedure roadModelViewSectionApplyDialogFileCommandProcedure()
{
    return &runRoadModelViewSectionApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
