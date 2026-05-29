#include "cad_adapter/objectarx/cross_section/ObjectArxSectionDrawingConfigCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h"
#include "cad_adapter/objectarx/cross_section/SectionDrawingConfigDialogBridge.h"
#include "domain/cross_section/PavementLayerTemplateModel.h"
#include "domain/cross_section/RoadModel.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>
#endif

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

#ifndef ROADPROTO_TEST_BUILD

using roadproto::domain::cross_section::PavementLayerTemplateData;
using roadproto::domain::cross_section::PavementLayerTemplateRules;
using roadproto::domain::cross_section::RoadModelComponentLine;
using roadproto::domain::cross_section::RoadModelData;
using roadproto::domain::cross_section::RoadModelGroundProfilePoint;
using roadproto::domain::cross_section::RoadModelPoint3d;
using roadproto::domain::cross_section::RoadModelSection;
using roadproto::domain::cross_section::RoadModelSectionNode;
using roadproto::domain::cross_section::RoadModelSectionNodeKind;
using roadproto::domain::cross_section::SectionDrawingComponentTypeSelection;
using roadproto::domain::cross_section::SectionDrawingConfigData;
using roadproto::domain::cross_section::SectionDrawingConfigRules;
using roadproto::domain::cross_section::SectionPavementLayerConfigRow;
using roadproto::domain::cross_section::SubgradeComponentType;
using roadproto::domain::cross_section::SubgradeSide;

constexpr double kSectionDrawingPadding = 2.0;
constexpr double kSectionDrawingStationLabelBand = 4.0;
constexpr double kStationEpsilon = 1.0e-6;

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

bool promptSectionDrawingHandle(std::wstring& handle)
{
    ACHAR handleBuffer[128] = {};
    if (acedGetString(Adesk::kFalse, L"\nSection drawing handle: ", handleBuffer) != RTNORM) {
        return false;
    }

    handle = trimDialogCommandPath(handleBuffer);
    return !handle.empty();
}

bool promptResponsePath(std::wstring& responsePath)
{
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto section drawing config response file: ", pathBuffer) != RTNORM) {
        return false;
    }

    responsePath = trimDialogCommandPath(pathBuffer);
    return !responsePath.empty();
}

bool readRoadModelDataByHandle(const std::wstring& handle, RoadModelData& data)
{
    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handle, entityId)) {
        return false;
    }

    DnRoadModelEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }
    if (!entity->isKindOf(DnRoadModelEntity::desc())) {
        entity->close();
        return false;
    }

    data = entity->roadModelData();
    entity->close();
    return true;
}

bool readPavementLayerTemplateByHandle(
    const std::wstring& handle,
    PavementLayerTemplateData& data,
    std::wstring& templateName)
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

    data = entity->templateData();
    templateName = data.properties.name;
    entity->close();
    return true;
}

bool promptPavementLayerTemplate(std::wstring& templateHandle, std::wstring& templateName)
{
    AcDbObjectId templateId;
    if (!selectTypedEntity<DnPavementLayerTemplateEntity>(templateId)) {
        return false;
    }

    DnPavementLayerTemplateEntity* entity = nullptr;
    if (acdbOpenObject(entity, templateId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }
    if (!entity->isKindOf(DnPavementLayerTemplateEntity::desc())) {
        entity->close();
        return false;
    }

    templateHandle = entityHandleText(entity);
    templateName = entity->templateData().properties.name;
    entity->close();
    return !templateHandle.empty();
}

std::vector<AcDbObjectId> collectSectionDrawingsForRoadModel(const std::wstring& roadModelHandle)
{
    std::vector<AcDbObjectId> drawingIds;
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr || roadModelHandle.empty()) {
        return drawingIds;
    }

    AcDbBlockTable* blockTable = nullptr;
    if (database->getBlockTable(blockTable, AcDb::kForRead) != Acad::eOk || blockTable == nullptr) {
        return drawingIds;
    }

    AcDbBlockTableRecord* modelSpace = nullptr;
    const auto status = blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForRead);
    blockTable->close();
    if (status != Acad::eOk || modelSpace == nullptr) {
        return drawingIds;
    }

    AcDbBlockTableRecordIterator* iterator = nullptr;
    if (modelSpace->newIterator(iterator) != Acad::eOk || iterator == nullptr) {
        modelSpace->close();
        return drawingIds;
    }

    for (; !iterator->done(); iterator->step()) {
        AcDbObjectId entityId;
        if (iterator->getEntityId(entityId) != Acad::eOk) {
            continue;
        }

        DnRoadModelSectionDrawingEntity* entity = nullptr;
        if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
            continue;
        }
        const auto matches = entity->isKindOf(DnRoadModelSectionDrawingEntity::desc())
            && entity->drawingData().roadModelHandle == roadModelHandle;
        entity->close();
        if (matches) {
            drawingIds.push_back(entityId);
        }
    }

    delete iterator;
    modelSpace->close();
    return drawingIds;
}

std::vector<double> collectDrawnSectionStationsForRoadModel(const std::wstring& roadModelHandle)
{
    std::vector<double> stations;
    for (const auto& drawingId : collectSectionDrawingsForRoadModel(roadModelHandle)) {
        DnRoadModelSectionDrawingEntity* entity = nullptr;
        if (acdbOpenObject(entity, drawingId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
            continue;
        }
        if (entity->isKindOf(DnRoadModelSectionDrawingEntity::desc())
            && std::isfinite(entity->drawingData().station)) {
            stations.push_back(entity->drawingData().station);
        }
        entity->close();
    }

    std::sort(stations.begin(), stations.end());
    stations.erase(
        std::unique(stations.begin(), stations.end(), [](double left, double right) {
            return std::fabs(left - right) <= kStationEpsilon;
        }),
        stations.end());
    return stations;
}

const RoadModelSection* findRoadModelSectionAtStation(const RoadModelData& data, double station)
{
    const RoadModelSection* best = nullptr;
    double bestDistance = 0.0;
    for (const auto& section : data.sections) {
        const auto distance = std::fabs(section.station - station);
        if (best == nullptr || distance < bestDistance) {
            best = &section;
            bestDistance = distance;
        }
    }
    return bestDistance <= kStationEpsilon ? best : nullptr;
}

std::vector<RoadModelSectionDrawingFace> preserveManualEditedFaces(
    const std::vector<RoadModelSectionDrawingFace>& faces,
    std::size_t& preservedCount)
{
    std::vector<RoadModelSectionDrawingFace> preserved;
    for (const auto& face : faces) {
        if (face.manualEdited) {
            preserved.push_back(face);
        }
    }
    preservedCount = preserved.size();
    return preserved;
}

bool sameRoadModelPoint(const RoadModelPoint3d& left, const RoadModelPoint3d& right)
{
    constexpr double kPointTolerance = 1.0e-5;
    return std::fabs(left.x - right.x) <= kPointTolerance
        && std::fabs(left.y - right.y) <= kPointTolerance
        && std::fabs(left.z - right.z) <= kPointTolerance;
}

bool linePointMatchesSectionNode(const RoadModelComponentLine& line, const RoadModelSectionNode& node)
{
    return std::any_of(line.points.begin(), line.points.end(), [&node](const auto& point) {
        return sameRoadModelPoint(point, node.point);
    });
}

bool findBoundaryNodeAtStation(
    const RoadModelComponentLine& line,
    const RoadModelSection& section,
    double station,
    RoadModelSectionNode& boundaryNode)
{
    if (!std::isfinite(station)) {
        return false;
    }

    const auto& nodes = line.key.side == SubgradeSide::Left ? section.leftNodes : section.rightNodes;
    for (const auto& node : nodes) {
        if (node.kind == RoadModelSectionNodeKind::Subgrade
            && std::isfinite(node.offset)
            && std::isfinite(node.elevation)
            && linePointMatchesSectionNode(line, node)) {
            boundaryNode = node;
            return true;
        }
    }
    return false;
}

struct SectionComponentSpan {
    SubgradeSide side = SubgradeSide::Right;
    SubgradeComponentType componentType = SubgradeComponentType::TravelLane;
    std::size_t componentIndex = 0;
    RoadModelSectionNode inner;
    RoadModelSectionNode outer;
};

struct ComponentBoundaryNodes {
    bool hasInner = false;
    bool hasOuter = false;
    RoadModelSectionNode inner;
    RoadModelSectionNode outer;
};

std::vector<SectionComponentSpan> collectSectionComponentSpans(
    const RoadModelData& roadModel,
    const RoadModelSection& section,
    double drawingStation,
    const SectionPavementLayerConfigRow* row)
{
    std::map<std::tuple<SubgradeSide, SubgradeComponentType, std::size_t>, ComponentBoundaryNodes> boundaries;
    for (const auto& line : roadModel.componentLines) {
        if (row != nullptr && !SectionDrawingConfigRules::matchesComponent(*row, line.key.side, line.key.componentType)) {
            continue;
        }
        RoadModelSectionNode boundaryNode;
        if (!findBoundaryNodeAtStation(line, section, drawingStation, boundaryNode)) {
            continue;
        }
        auto& boundary = boundaries[std::make_tuple(
            line.key.side,
            line.key.componentType,
            line.key.componentIndex)];
        if (line.key.boundaryIndex == 0) {
            boundary.hasInner = true;
            boundary.inner = std::move(boundaryNode);
        } else if (line.key.boundaryIndex == 1) {
            boundary.hasOuter = true;
            boundary.outer = std::move(boundaryNode);
        }
    }

    std::vector<SectionComponentSpan> spans;
    for (const auto& entry : boundaries) {
        const auto side = std::get<0>(entry.first);
        const auto componentType = std::get<1>(entry.first);
        const auto componentIndex = std::get<2>(entry.first);
        const auto& boundary = entry.second;
        if (!boundary.hasInner || !boundary.hasOuter) {
            continue;
        }

        auto inner = boundary.inner;
        auto outer = boundary.outer;
        if (std::fabs(outer.offset) < std::fabs(inner.offset)) {
            std::swap(inner, outer);
        }

        const auto width = std::fabs(outer.offset - inner.offset);
        if (!std::isfinite(width) || width <= 1.0e-6) {
            continue;
        }

        SectionComponentSpan span;
        span.side = side;
        span.componentType = componentType;
        span.componentIndex = componentIndex;
        span.inner = std::move(inner);
        span.outer = std::move(outer);
        spans.push_back(std::move(span));
    }
    return spans;
}

std::vector<SectionDrawingConfigComponentOption> collectComponentOptions(
    const RoadModelData& roadModel,
    const std::vector<double>& drawnStations)
{
    std::map<std::wstring, SectionDrawingConfigComponentOption> unique;
    for (const auto station : drawnStations) {
        const auto* section = findRoadModelSectionAtStation(roadModel, station);
        if (section == nullptr) {
            continue;
        }

        for (const auto& span : collectSectionComponentSpans(roadModel, *section, station, nullptr)) {
            SectionDrawingComponentTypeSelection selection;
            selection.side = span.side;
            selection.componentType = span.componentType;
            const auto code = SectionDrawingConfigRules::componentSelectionCode(selection);
            if (code.empty() || unique.find(code) != unique.end()) {
                continue;
            }

            SectionDrawingConfigComponentOption option;
            option.code = code;
            option.displayName = SectionDrawingConfigRules::componentSelectionDisplayName(selection);
            option.selection = selection;
            unique.emplace(code, std::move(option));
        }
    }

    std::vector<SectionDrawingConfigComponentOption> options;
    options.reserve(unique.size());
    for (auto& pair : unique) {
        options.push_back(std::move(pair.second));
    }
    return options;
}

bool isFiniteDrawingPoint(const RoadModelSectionDrawingPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool isClearTableFace(const RoadModelSectionDrawingFace& face)
{
    return face.faceId.rfind(L"clearTable:", 0) == 0;
}

RoadModelSectionDrawingPoint mapSectionPointToDrawing(
    double offset,
    double elevation,
    double minOffset,
    double minElevation)
{
    return RoadModelSectionDrawingPoint{
        offset - minOffset + kSectionDrawingPadding,
        elevation - minElevation + kSectionDrawingStationLabelBand + kSectionDrawingPadding};
}

double signedSideOffset(SubgradeSide side, const RoadModelSectionNode& node)
{
    return side == SubgradeSide::Left ? std::fabs(node.offset) : -std::fabs(node.offset);
}

double clearTableGroundDistance(const RoadModelGroundProfilePoint& point)
{
    return std::fabs(point.offset);
}

double sideDistance(const RoadModelSectionNode& node)
{
    return std::fabs(node.offset);
}

const std::vector<RoadModelGroundProfilePoint>& groundProfileForSide(
    const RoadModelSection& section,
    SubgradeSide side)
{
    return side == SubgradeSide::Left ? section.leftGroundProfile : section.rightGroundProfile;
}

const std::vector<RoadModelSectionNode>& sectionNodesForSide(
    const RoadModelSection& section,
    SubgradeSide side)
{
    return side == SubgradeSide::Left ? section.leftNodes : section.rightNodes;
}

double sideClearTableCoverageDistance(const RoadModelSection& section, SubgradeSide side)
{
    double coverage = 0.0;
    for (const auto& node : sectionNodesForSide(section, side)) {
        if (node.kind == RoadModelSectionNodeKind::Subgrade || node.kind == RoadModelSectionNodeKind::Slope) {
            coverage = std::max(coverage, sideDistance(node));
        }
    }
    return coverage;
}

std::optional<double> groundElevationAtDistance(
    const std::vector<RoadModelGroundProfilePoint>& profile,
    double distance)
{
    if (profile.empty() || !std::isfinite(distance)) {
        return std::nullopt;
    }

    struct GroundSample {
        double distance = 0.0;
        double elevation = 0.0;
    };

    std::vector<GroundSample> points;
    points.reserve(profile.size());
    for (const auto& point : profile) {
        if (std::isfinite(point.offset) && std::isfinite(point.elevation)) {
            points.push_back(GroundSample{clearTableGroundDistance(point), point.elevation});
        }
    }
    if (points.empty()) {
        return std::nullopt;
    }

    std::sort(points.begin(), points.end(), [](const auto& left, const auto& right) {
        return left.distance < right.distance;
    });

    for (std::size_t i = 1; i < points.size(); ++i) {
        const auto& first = points[i - 1];
        const auto& second = points[i];
        if (distance < first.distance - kStationEpsilon || distance > second.distance + kStationEpsilon) {
            continue;
        }
        const auto span = second.distance - first.distance;
        if (std::fabs(span) <= 1.0e-9) {
            return first.elevation;
        }
        const auto t = std::clamp((distance - first.distance) / span, 0.0, 1.0);
        return first.elevation + (second.elevation - first.elevation) * t;
    }

    const auto closest = std::min_element(points.begin(), points.end(), [&](const auto& left, const auto& right) {
        return std::fabs(left.distance - distance) < std::fabs(right.distance - distance);
    });
    return closest == points.end() ? std::nullopt : std::optional<double>(closest->elevation);
}

void ensureClearTableGroundPoint(
    std::vector<RoadModelGroundProfilePoint>& points,
    const std::vector<RoadModelGroundProfilePoint>& profile,
    double distance)
{
    if (!std::isfinite(distance)) {
        return;
    }
    const auto elevation = groundElevationAtDistance(profile, distance);
    if (!elevation.has_value()) {
        return;
    }
    points.push_back(RoadModelGroundProfilePoint{distance, *elevation});
}

std::vector<RoadModelGroundProfilePoint> sampleClearTableGroundPoints(
    const std::vector<RoadModelGroundProfilePoint>& profile,
    double coverage)
{
    std::vector<RoadModelGroundProfilePoint> points;
    if (!std::isfinite(coverage) || coverage <= 1.0e-6) {
        return points;
    }

    points.reserve(profile.size() + 2);
    for (const auto& point : profile) {
        if (!std::isfinite(point.offset) || !std::isfinite(point.elevation)) {
            continue;
        }
        const auto distance = std::fabs(point.offset);
        if (distance < -kStationEpsilon || distance > coverage + kStationEpsilon) {
            continue;
        }
        points.push_back(RoadModelGroundProfilePoint{std::clamp(distance, 0.0, coverage), point.elevation});
    }
    ensureClearTableGroundPoint(points, profile, 0.0);
    ensureClearTableGroundPoint(points, profile, coverage);

    std::sort(points.begin(), points.end(), [](const auto& left, const auto& right) {
        return left.offset < right.offset;
    });

    std::vector<RoadModelGroundProfilePoint> unique;
    unique.reserve(points.size());
    for (const auto& point : points) {
        if (unique.empty() || std::fabs(unique.back().offset - point.offset) > kStationEpsilon) {
            unique.push_back(point);
        } else {
            if (std::fabs(point.offset) <= kStationEpsilon
                || std::fabs(point.offset - coverage) <= kStationEpsilon) {
                unique.back().offset = point.offset;
            }
            unique.back().elevation = (unique.back().elevation + point.elevation) * 0.5;
        }
    }
    return unique;
}

bool isCutSide(const RoadModelSection& section, SubgradeSide side)
{
    const auto& nodes = sectionNodesForSide(section, side);
    const RoadModelSectionNode* outerSubgrade = nullptr;
    for (const auto& node : nodes) {
        if (node.kind != RoadModelSectionNodeKind::Subgrade || !std::isfinite(node.elevation)) {
            continue;
        }
        if (outerSubgrade == nullptr || sideDistance(node) > sideDistance(*outerSubgrade)) {
            outerSubgrade = &node;
        }
    }
    if (outerSubgrade == nullptr) {
        return false;
    }

    const auto groundElevation = groundElevationAtDistance(
        groundProfileForSide(section, side),
        sideDistance(*outerSubgrade));
    return groundElevation.has_value() && *groundElevation > outerSubgrade->elevation + 1.0e-5;
}

struct DrawingBasis {
    double minOffset = 0.0;
    double minElevation = 0.0;
    bool valid = false;
};

void includeSectionBasisPoint(DrawingBasis& basis, double offset, double elevation)
{
    if (!std::isfinite(offset) || !std::isfinite(elevation)) {
        return;
    }
    if (!basis.valid) {
        basis.minOffset = offset;
        basis.minElevation = elevation;
        basis.valid = true;
        return;
    }
    basis.minOffset = std::min(basis.minOffset, offset);
    basis.minElevation = std::min(basis.minElevation, elevation);
}

void includeSectionBasisNodes(DrawingBasis& basis, const std::vector<RoadModelSectionNode>& nodes)
{
    for (const auto& node : nodes) {
        includeSectionBasisPoint(basis, node.offset, node.elevation);
    }
}

void includeSectionGroundBasisPoints(
    DrawingBasis& basis,
    const std::vector<RoadModelGroundProfilePoint>& points,
    double sign)
{
    for (const auto& point : points) {
        includeSectionBasisPoint(basis, sign * std::fabs(point.offset), point.elevation);
    }
}

DrawingBasis drawingBasisForSection(
    const RoadModelSectionDrawingData& drawing,
    const RoadModelSection& section)
{
    DrawingBasis basis;
    includeSectionBasisNodes(basis, section.leftNodes);
    includeSectionBasisNodes(basis, section.rightNodes);
    includeSectionBasisNodes(basis, section.leftPavementLayerNodes);
    includeSectionBasisNodes(basis, section.rightPavementLayerNodes);
    includeSectionGroundBasisPoints(basis, section.leftGroundProfile, 1.0);
    includeSectionGroundBasisPoints(basis, section.rightGroundProfile, -1.0);
    if (basis.valid) {
        return basis;
    }

    double minLocalX = std::numeric_limits<double>::infinity();
    double minLocalY = std::numeric_limits<double>::infinity();
    const auto includeDrawingPoint = [&](const RoadModelSectionDrawingPoint& point) {
        if (!isFiniteDrawingPoint(point)) {
            return;
        }
        minLocalX = std::min(minLocalX, point.x);
        minLocalY = std::min(minLocalY, point.y);
    };
    for (const auto& line : drawing.lines) {
        for (const auto& point : line.points) {
            includeDrawingPoint(point);
        }
    }
    for (const auto& face : drawing.faces) {
        for (const auto& point : face.points) {
            includeDrawingPoint(point);
        }
    }
    if (std::isfinite(minLocalX) && std::isfinite(minLocalY)) {
        basis.minOffset = kSectionDrawingPadding - minLocalX;
        basis.minElevation = kSectionDrawingStationLabelBand + kSectionDrawingPadding - minLocalY;
        basis.valid = true;
    }
    return basis;
}

std::vector<RoadModelSectionDrawingPoint> clearTableFacePointsForSide(
    const RoadModelSection& section,
    SubgradeSide side,
    double innerSlopeRatio,
    double outerSlopeRatio,
    double thickness,
    const DrawingBasis& basis)
{
    const auto coverage = sideClearTableCoverageDistance(section, side);
    if (!std::isfinite(coverage)
        || coverage <= 1.0e-6
        || !std::isfinite(innerSlopeRatio)
        || !std::isfinite(outerSlopeRatio)
        || !std::isfinite(thickness)
        || thickness <= 0.0) {
        return {};
    }

    const auto& profile = groundProfileForSide(section, side);
    const auto groundPoints = sampleClearTableGroundPoints(profile, coverage);
    if (groundPoints.size() < 2) {
        return {};
    }

    const auto sign = side == SubgradeSide::Left ? 1.0 : -1.0;
    std::vector<RoadModelSectionDrawingPoint> points;
    points.reserve(groundPoints.size() * 2);
    for (const auto& point : groundPoints) {
        points.push_back(mapSectionPointToDrawing(
            sign * point.offset,
            point.elevation,
            basis.minOffset,
            basis.minElevation));
    }

    const auto bottomInnerDistance = std::min(std::max(0.0, thickness * innerSlopeRatio), coverage);
    const auto bottomOuterDistance = std::max(0.0, coverage - std::max(0.0, thickness * outerSlopeRatio));
    const auto bottomSpan = std::max(0.0, bottomOuterDistance - bottomInnerDistance);

    for (auto it = groundPoints.rbegin(); it != groundPoints.rend(); ++it) {
        const auto ratio = coverage <= 1.0e-9 ? 0.0 : std::clamp(it->offset / coverage, 0.0, 1.0);
        const auto bottomDistance = bottomInnerDistance + bottomSpan * ratio;
        const auto bottomOffset = sign * bottomDistance;
        points.push_back(mapSectionPointToDrawing(
            bottomOffset,
            it->elevation - thickness,
            basis.minOffset,
            basis.minElevation));
    }

    return points;
}

std::vector<RoadModelSectionDrawingFace> buildConfiguredClearTableFaces(
    const RoadModelSectionDrawingData& drawing,
    const RoadModelData& roadModel,
    std::size_t& preservedManualFaceCount,
    std::vector<std::wstring>& warnings)
{
    auto faces = preserveManualEditedFaces(drawing.faces, preservedManualFaceCount);
    const auto* section = findRoadModelSectionAtStation(roadModel, drawing.station);
    if (section == nullptr) {
        warnings.push_back(L"No road model section exists for clear table at station " + std::to_wstring(drawing.station));
        return faces;
    }

    const auto basis = drawingBasisForSection(drawing, *section);
    if (!basis.valid) {
        warnings.push_back(L"Cannot derive section drawing basis for clear table at station " + std::to_wstring(drawing.station));
        return faces;
    }

    const SubgradeSide sides[] = {SubgradeSide::Left, SubgradeSide::Right};
    for (const auto side : sides) {
        const auto cutSide = isCutSide(*section, side);
        const auto resolved = SectionDrawingConfigRules::resolveClearTableRow(
            drawing.config,
            drawing.station,
            side,
            cutSide);
        if (!resolved.has_value()) {
            continue;
        }

        const auto slopeRatios = SectionDrawingConfigRules::clearTableEdgeSlopeRatios(
            resolved->row,
            side);
        auto points = clearTableFacePointsForSide(
            *section,
            side,
            slopeRatios.innerSlopeRatio,
            slopeRatios.outerSlopeRatio,
            resolved->row.thickness,
            basis);
        if (points.size() < 3 || !std::all_of(points.begin(), points.end(), isFiniteDrawingPoint)) {
            warnings.push_back(L"Cannot build clear table face at station " + std::to_wstring(drawing.station));
            continue;
        }

        RoadModelSectionDrawingFace face;
        face.layerName = L"\u6e05\u8868";
        face.componentName = SectionDrawingConfigRules::clearTableScopeDisplayName(
            side == SubgradeSide::Left
                ? roadproto::domain::cross_section::SectionClearTableScope::Left
                : roadproto::domain::cross_section::SectionClearTableScope::Right);
        face.faceId = L"clearTable:"
            + (side == SubgradeSide::Left ? std::wstring(L"Left") : std::wstring(L"Right"))
            + L":"
            + std::to_wstring(resolved->rowIndex);
        face.sourceConfigRowIndex = static_cast<int>(resolved->rowIndex);
        face.manualEdited = false;
        face.colorR = 126;
        face.colorG = 84;
        face.colorB = 45;
        face.hatchPattern = L"EARTH";
        face.hatchAngle = 45.0;
        face.hatchScale = 1.0;
        face.points = std::move(points);
        faces.push_back(std::move(face));
    }

    return faces;
}

std::vector<RoadModelSectionDrawingFace> buildConfiguredPavementFaces(
    const RoadModelSectionDrawingData& drawing,
    const RoadModelData& roadModel,
    const std::unordered_map<std::wstring, PavementLayerTemplateData>& templates,
    std::vector<RoadModelSectionDrawingFace> faces,
    std::vector<std::wstring>& warnings)
{
    const auto* section = findRoadModelSectionAtStation(roadModel, drawing.station);
    if (section == nullptr) {
        warnings.push_back(L"No road model section exists at station " + std::to_wstring(drawing.station));
        return faces;
    }

    const auto basis = drawingBasisForSection(drawing, *section);
    if (!basis.valid) {
        warnings.push_back(L"Cannot derive section drawing basis at station " + std::to_wstring(drawing.station));
        return faces;
    }

    const auto spans = collectSectionComponentSpans(roadModel, *section, drawing.station, nullptr);
    if (spans.empty()) {
        warnings.push_back(L"No matching component span exists at station " + std::to_wstring(drawing.station));
    }
    for (const auto& span : spans) {
        const auto resolved = SectionDrawingConfigRules::resolvePavementRow(drawing.config, drawing.station, span.side, span.componentType);
        if (!resolved.has_value() || resolved->row.templateHandle.empty()) {
            continue;
        }

        const auto templateIt = templates.find(resolved->row.templateHandle);
        if (templateIt == templates.end()) {
            warnings.push_back(L"Missing pavement layer template: " + resolved->row.templateHandle);
            continue;
        }

        const auto baseWidth = std::fabs(span.outer.offset - span.inner.offset);
        if (!std::isfinite(baseWidth) || baseWidth <= 1.0e-6) {
            continue;
        }

        const auto pavementSection = PavementLayerTemplateRules::buildSection(
            templateIt->second,
            baseWidth,
            span.side,
            span.inner.elevation,
            span.outer.elevation);
        if (!pavementSection.succeeded) {
            warnings.push_back(
                L"Pavement layer section failed at station "
                + std::to_wstring(drawing.station)
                + L", componentIndex "
                + std::to_wstring(span.componentIndex)
                + L": "
                + pavementSection.errorMessage);
            continue;
        }

        const auto direction = (span.outer.offset >= span.inner.offset ? 1.0 : -1.0);
        for (std::size_t layerIndex = 0; layerIndex < pavementSection.layers.size(); ++layerIndex) {
            const auto& layer = pavementSection.layers[layerIndex];
            const auto toDrawing = [&](double layerOffset, double layerElevation) {
                return mapSectionPointToDrawing(
                    span.inner.offset + direction * layerOffset,
                    layerElevation,
                    basis.minOffset,
                    basis.minElevation);
            };

            RoadModelSectionDrawingFace face;
            face.layerName = layer.name.empty() ? L"\u8def\u9762\u7ed3\u6784\u5c42" : layer.name;
            face.componentName = span.inner.componentName.empty()
                ? SectionDrawingConfigRules::componentSelectionDisplayName(
                    SectionDrawingComponentTypeSelection{span.side, span.componentType})
                : span.inner.componentName;
            face.faceId = L"row"
                + std::to_wstring(resolved->rowIndex)
                + L":"
                + SectionDrawingConfigRules::componentSelectionCode(
                    SectionDrawingComponentTypeSelection{span.side, span.componentType})
                + L":"
                + std::to_wstring(span.componentIndex)
                + L":"
                + std::to_wstring(layerIndex);
            face.sourceTemplateHandle = resolved->row.templateHandle;
            face.sourceConfigRowIndex = static_cast<int>(resolved->rowIndex);
            face.manualEdited = false;
            face.colorR = layer.color.r;
            face.colorG = layer.color.g;
            face.colorB = layer.color.b;
            if (layerIndex < templateIt->second.layers.size()) {
                const auto& templateLayer = templateIt->second.layers[layerIndex];
                face.hatchPattern = templateLayer.hatchPattern;
                face.hatchAngle = templateLayer.hatchAngle;
                face.hatchScale = templateLayer.hatchScale;
            }
            face.points = {
                toDrawing(layer.topInner.offset, layer.topInner.elevation),
                toDrawing(layer.topOuter.offset, layer.topOuter.elevation),
                toDrawing(layer.bottomOuter.offset, layer.bottomOuter.elevation),
                toDrawing(layer.bottomInner.offset, layer.bottomInner.elevation)};
            if (std::all_of(face.points.begin(), face.points.end(), isFiniteDrawingPoint)) {
                faces.push_back(std::move(face));
            }
        }
    }

    return faces;
}

bool queueDialogForSectionDrawing(AcDbObjectId drawingId, const SectionDrawingConfigData* overrideConfig)
{
    auto& editor = app::ApplicationContext::instance().editor();

    DnRoadModelSectionDrawingEntity* entity = nullptr;
    if (acdbOpenObject(entity, drawingId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"Cannot open section drawing entity.");
        return false;
    }
    if (!entity->isKindOf(DnRoadModelSectionDrawingEntity::desc())) {
        entity->close();
        editor.writeWarning(L"The selected object is not a section drawing entity.");
        return false;
    }

    const auto drawingHandle = entityHandleText(entity);
    auto drawingData = entity->drawingData();
    entity->close();

    if (overrideConfig != nullptr) {
        drawingData.config = *overrideConfig;
    }

    RoadModelData roadModel;
    std::vector<SectionDrawingConfigComponentOption> componentOptions;
    if (readRoadModelDataByHandle(drawingData.roadModelHandle, roadModel)) {
        const auto drawnStations = collectDrawnSectionStationsForRoadModel(drawingData.roadModelHandle);
        componentOptions = collectComponentOptions(roadModel, drawnStations);
    }

    SectionDrawingConfigDialogRequest request;
    request.drawingHandle = drawingHandle;
    request.roadModelHandle = drawingData.roadModelHandle;
    request.config = drawingData.config;
    request.componentOptions = std::move(componentOptions);

    std::wstring errorMessage;
    if (!queueSectionDrawingConfigWpfDialog(request, errorMessage)) {
        editor.writeError(L"Failed to open section drawing config dialog: " + errorMessage);
        return false;
    }
    return true;
}

bool queueDialogFromResponse(const SectionDrawingConfigDialogResponse& response)
{
    auto& editor = app::ApplicationContext::instance().editor();

    AcDbObjectId drawingId;
    if (!resolveObjectIdFromHandle(response.drawingHandle, drawingId)) {
        editor.writeWarning(L"No section drawing was found for the dialog response.");
        return false;
    }

    DnRoadModelSectionDrawingEntity* entity = nullptr;
    if (acdbOpenObject(entity, drawingId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"Cannot reopen section drawing entity.");
        return false;
    }
    auto drawingData = entity->drawingData();
    entity->close();

    SectionDrawingConfigDialogRequest request;
    request.drawingHandle = response.drawingHandle;
    request.roadModelHandle = response.roadModelHandle.empty() ? drawingData.roadModelHandle : response.roadModelHandle;
    request.responsePath = response.responsePath;
    request.config = response.config;
    request.componentOptions = response.componentOptions;
    if (request.componentOptions.empty()) {
        RoadModelData roadModel;
        if (readRoadModelDataByHandle(request.roadModelHandle, roadModel)) {
            const auto drawnStations = collectDrawnSectionStationsForRoadModel(request.roadModelHandle);
            request.componentOptions = collectComponentOptions(roadModel, drawnStations);
        }
    }

    std::wstring errorMessage;
    if (!queueSectionDrawingConfigWpfDialog(request, errorMessage)) {
        editor.writeError(L"Failed to reopen section drawing config dialog: " + errorMessage);
        return false;
    }
    return true;
}

std::unordered_map<std::wstring, PavementLayerTemplateData> collectPavementTemplatesForConfig(
    const SectionDrawingConfigData& config,
    std::vector<std::wstring>& warnings)
{
    std::unordered_map<std::wstring, PavementLayerTemplateData> templates;
    for (const auto& row : config.pavementRows) {
        if (row.templateHandle.empty() || templates.find(row.templateHandle) != templates.end()) {
            continue;
        }

        PavementLayerTemplateData data;
        std::wstring templateName;
        if (readPavementLayerTemplateByHandle(row.templateHandle, data, templateName)) {
            templates.emplace(row.templateHandle, std::move(data));
        } else {
            warnings.push_back(L"Cannot read pavement layer template: " + row.templateHandle);
        }
    }
    return templates;
}

void writeWarnings(roadproto::cad_adapter::IEditor& editor, const std::vector<std::wstring>& warnings)
{
    constexpr std::size_t kMaxWarningsToPrint = 12;
    for (std::size_t i = 0; i < warnings.size() && i < kMaxWarningsToPrint; ++i) {
        editor.writeWarning(warnings[i]);
    }
    if (warnings.size() > kMaxWarningsToPrint) {
        editor.writeWarning(
            L"Additional section drawing config warning count: "
            + std::to_wstring(warnings.size() - kMaxWarningsToPrint));
    }
}

bool applySectionDrawingConfigToAllDrawings(
    const std::wstring& roadModelHandle,
    const SectionDrawingConfigData& config,
    int& updatedDrawingCount,
    std::size_t& preservedManualFaceCount)
{
    auto& editor = app::ApplicationContext::instance().editor();
    updatedDrawingCount = 0;
    preservedManualFaceCount = 0;

    RoadModelData roadModel;
    if (!readRoadModelDataByHandle(roadModelHandle, roadModel)) {
        editor.writeWarning(L"No road model entity was found for the section drawing config.");
        return false;
    }

    auto normalized = config;
    std::wstring normalizeError;
    if (!SectionDrawingConfigRules::normalize(normalized, normalizeError)) {
        editor.writeError(L"Invalid section drawing config: " + normalizeError);
        return false;
    }

    std::vector<std::wstring> warnings;
    const auto templates = collectPavementTemplatesForConfig(normalized, warnings);
    const auto drawingIds = collectSectionDrawingsForRoadModel(roadModelHandle);
    for (const auto& drawingId : drawingIds) {
        DnRoadModelSectionDrawingEntity* entity = nullptr;
        if (acdbOpenObject(entity, drawingId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
            continue;
        }
        if (!entity->isKindOf(DnRoadModelSectionDrawingEntity::desc())) {
            entity->close();
            continue;
        }

        auto drawing = entity->drawingData();
        entity->close();
        entity = nullptr;
        drawing.config = normalized;
        std::size_t manualCountForDrawing = 0;
        auto faces = buildConfiguredClearTableFaces(
            drawing,
            roadModel,
            manualCountForDrawing,
            warnings);
        faces = buildConfiguredPavementFaces(
            drawing,
            roadModel,
            templates,
            std::move(faces),
            warnings);

        if (acdbOpenObject(entity, drawingId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
            continue;
        }
        if (entity->setSectionDrawingConfigAndFaces(normalized, std::move(faces)) == Acad::eOk) {
            ++updatedDrawingCount;
            preservedManualFaceCount += manualCountForDrawing;
            entity->recordGraphicsModified(true);
        } else {
            warnings.push_back(L"Failed to update section drawing entity at station " + std::to_wstring(drawing.station));
        }
        entity->close();
    }

    writeWarnings(editor, warnings);
    return updatedDrawingCount > 0;
}

bool handlePickTemplateAction(SectionDrawingConfigDialogResponse& response)
{
    auto& editor = app::ApplicationContext::instance().editor();
    if (response.pickRowIndex < 0
        || response.pickRowIndex >= static_cast<int>(response.config.pavementRows.size())) {
        editor.writeWarning(L"Section drawing config template row index is invalid.");
        return queueDialogFromResponse(response);
    }

    std::wstring templateHandle;
    std::wstring templateName;
    if (promptPavementLayerTemplate(templateHandle, templateName)) {
        auto& row = response.config.pavementRows[static_cast<std::size_t>(response.pickRowIndex)];
        row.templateHandle = std::move(templateHandle);
        row.templateName = std::move(templateName);
    }
    return queueDialogFromResponse(response);
}

void runSectionDrawingConfigCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_SECTION_DRAWING_CONFIG: select a section drawing entity.");

    AcDbObjectId drawingId;
    if (!selectTypedEntity<DnRoadModelSectionDrawingEntity>(drawingId)) {
        editor.writeWarning(L"No section drawing entity was selected.");
        return;
    }

    queueDialogForSectionDrawing(drawingId, nullptr);
}

void runSectionDrawingConfigEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    std::wstring handle;
    if (!promptSectionDrawingHandle(handle)) {
        return;
    }

    AcDbObjectId drawingId;
    if (!resolveObjectIdFromHandle(handle, drawingId)) {
        editor.writeWarning(L"No section drawing entity was found for the handle.");
        return;
    }

    queueDialogForSectionDrawing(drawingId, nullptr);
}

void runSectionDrawingConfigApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    std::wstring responsePath;
    if (!promptResponsePath(responsePath)) {
        return;
    }

    SectionDrawingConfigDialogResponse response;
    std::wstring errorMessage;
    if (!readSectionDrawingConfigDialogResponse(responsePath, response, errorMessage)) {
        editor.writeError(L"Failed to read section drawing config response: " + errorMessage);
        return;
    }

    if (response.action == SectionDrawingConfigDialogAction::PickTemplate) {
        handlePickTemplateAction(response);
        return;
    }

    if (!response.accepted || response.action != SectionDrawingConfigDialogAction::Draw) {
        return;
    }

    auto roadModelHandle = response.roadModelHandle;
    if (roadModelHandle.empty()) {
        AcDbObjectId drawingId;
        if (resolveObjectIdFromHandle(response.drawingHandle, drawingId)) {
            DnRoadModelSectionDrawingEntity* entity = nullptr;
            if (acdbOpenObject(entity, drawingId, AcDb::kForRead) == Acad::eOk && entity != nullptr) {
                roadModelHandle = entity->drawingData().roadModelHandle;
                entity->close();
            }
        }
    }
    if (roadModelHandle.empty()) {
        editor.writeWarning(L"The section drawing config response does not include a road model handle.");
        return;
    }

    int updatedDrawingCount = 0;
    std::size_t preservedManualFaceCount = 0;
    if (!applySectionDrawingConfigToAllDrawings(
            roadModelHandle,
            response.config,
            updatedDrawingCount,
            preservedManualFaceCount)) {
        editor.writeWarning(L"No section drawing entities were updated.");
        return;
    }

    std::wstringstream message;
    message << L"Updated " << updatedDrawingCount
            << L" section drawing(s); preserved "
            << preservedManualFaceCount
            << L" manual edited pavement face(s).";
    editor.writeMessage(message.str());
}

#endif

} // namespace

#ifndef ROADPROTO_TEST_BUILD
roadproto::core::CommandProcedure sectionDrawingConfigCommandProcedure()
{
    return &runSectionDrawingConfigCommand;
}

roadproto::core::CommandProcedure sectionDrawingConfigEditHandleCommandProcedure()
{
    return &runSectionDrawingConfigEditHandleCommand;
}

roadproto::core::CommandProcedure sectionDrawingConfigApplyDialogFileCommandProcedure()
{
    return &runSectionDrawingConfigApplyDialogFileCommand;
}
#endif

} // namespace roadproto::cad_adapter::objectarx::cross_section
