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
#include <map>
#include <set>
#include <sstream>
#include <string>
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
using roadproto::domain::cross_section::RoadModelSection;
using roadproto::domain::cross_section::RoadModelSectionNode;
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

std::vector<SectionDrawingConfigComponentOption> collectComponentOptions(const RoadModelData& data)
{
    std::map<std::wstring, SectionDrawingConfigComponentOption> unique;
    for (const auto& line : data.componentLines) {
        SectionDrawingComponentTypeSelection selection;
        selection.side = line.key.side;
        selection.componentType = line.key.componentType;
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

    std::vector<SectionDrawingConfigComponentOption> options;
    options.reserve(unique.size());
    for (auto& pair : unique) {
        options.push_back(std::move(pair.second));
    }
    return options;
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

std::vector<RoadModelSectionNode> sortedFiniteNodesForSide(
    const RoadModelSection& section,
    SubgradeSide side)
{
    const auto& source = side == SubgradeSide::Left ? section.leftNodes : section.rightNodes;
    std::vector<RoadModelSectionNode> nodes;
    for (const auto& node : source) {
        if (std::isfinite(node.offset) && std::isfinite(node.elevation)) {
            nodes.push_back(node);
        }
    }
    std::sort(nodes.begin(), nodes.end(), [](const auto& left, const auto& right) {
        return std::fabs(left.offset) < std::fabs(right.offset);
    });
    return nodes;
}

bool isFiniteDrawingPoint(const RoadModelSectionDrawingPoint& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
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

double minSectionOffset(const RoadModelSection& section)
{
    bool initialized = false;
    double result = 0.0;
    const auto accumulate = [&](const std::vector<RoadModelSectionNode>& nodes) {
        for (const auto& node : nodes) {
            if (!std::isfinite(node.offset)) {
                continue;
            }
            if (!initialized) {
                result = node.offset;
                initialized = true;
            } else {
                result = std::min(result, node.offset);
            }
        }
    };
    accumulate(section.leftNodes);
    accumulate(section.rightNodes);
    return initialized ? result : 0.0;
}

double minSectionElevation(const RoadModelSection& section)
{
    bool initialized = false;
    double result = 0.0;
    const auto accumulate = [&](const std::vector<RoadModelSectionNode>& nodes) {
        for (const auto& node : nodes) {
            if (!std::isfinite(node.elevation)) {
                continue;
            }
            if (!initialized) {
                result = node.elevation;
                initialized = true;
            } else {
                result = std::min(result, node.elevation);
            }
        }
    };
    accumulate(section.leftNodes);
    accumulate(section.rightNodes);
    accumulate(section.leftPavementLayerNodes);
    accumulate(section.rightPavementLayerNodes);
    return initialized ? result : 0.0;
}

std::vector<RoadModelComponentLine> matchingComponentLines(
    const RoadModelData& data,
    const SectionPavementLayerConfigRow& row)
{
    std::vector<RoadModelComponentLine> result;
    std::set<std::tuple<SubgradeSide, SubgradeComponentType, std::size_t>> seen;
    for (const auto& line : data.componentLines) {
        if (!SectionDrawingConfigRules::matchesComponent(row, line.key.side, line.key.componentType)) {
            continue;
        }
        const auto key = std::make_tuple(line.key.side, line.key.componentType, line.key.componentIndex);
        if (seen.insert(key).second) {
            result.push_back(line);
        }
    }
    return result;
}

std::vector<RoadModelSectionDrawingFace> buildConfiguredPavementFaces(
    const RoadModelSectionDrawingData& drawing,
    const RoadModelData& roadModel,
    const std::unordered_map<std::wstring, PavementLayerTemplateData>& templates,
    std::size_t& preservedManualFaceCount)
{
    auto faces = preserveManualEditedFaces(drawing.faces, preservedManualFaceCount);
    const auto resolved = SectionDrawingConfigRules::resolvePavementRow(drawing.config, drawing.station);
    if (!resolved.has_value() || resolved->row.templateHandle.empty()) {
        return faces;
    }

    const auto templateIt = templates.find(resolved->row.templateHandle);
    if (templateIt == templates.end()) {
        return faces;
    }

    const auto* section = findRoadModelSectionAtStation(roadModel, drawing.station);
    if (section == nullptr) {
        return faces;
    }

    const auto minOffset = minSectionOffset(*section);
    const auto minElevation = minSectionElevation(*section);
    const auto components = matchingComponentLines(roadModel, resolved->row);
    for (const auto& component : components) {
        auto nodes = sortedFiniteNodesForSide(*section, component.key.side);
        if (nodes.size() < 2) {
            continue;
        }

        const auto index = std::min(component.key.componentIndex, nodes.size() - 2);
        auto first = nodes[index];
        auto second = nodes[index + 1];
        if (std::fabs(second.offset) < std::fabs(first.offset)) {
            std::swap(first, second);
        }

        const auto baseWidth = std::fabs(second.offset - first.offset);
        if (!std::isfinite(baseWidth) || baseWidth <= 1.0e-6) {
            continue;
        }

        const auto pavementSection = PavementLayerTemplateRules::buildSection(
            templateIt->second,
            baseWidth,
            component.key.side,
            first.elevation,
            second.elevation);
        if (!pavementSection.succeeded) {
            continue;
        }

        const auto direction = (second.offset >= first.offset ? 1.0 : -1.0);
        for (std::size_t layerIndex = 0; layerIndex < pavementSection.layers.size(); ++layerIndex) {
            const auto& layer = pavementSection.layers[layerIndex];
            const auto toDrawing = [&](double layerOffset, double layerElevation) {
                return mapSectionPointToDrawing(
                    first.offset + direction * layerOffset,
                    layerElevation,
                    minOffset,
                    minElevation);
            };

            RoadModelSectionDrawingFace face;
            face.layerName = layer.name.empty() ? L"\u8def\u9762\u7ed3\u6784\u5c42" : layer.name;
            face.componentName = first.componentName.empty()
                ? SectionDrawingConfigRules::componentSelectionDisplayName(
                    SectionDrawingComponentTypeSelection{component.key.side, component.key.componentType})
                : first.componentName;
            face.faceId = L"row"
                + std::to_wstring(resolved->rowIndex)
                + L":"
                + SectionDrawingConfigRules::componentSelectionCode(
                    SectionDrawingComponentTypeSelection{component.key.side, component.key.componentType})
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
        componentOptions = collectComponentOptions(roadModel);
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
            request.componentOptions = collectComponentOptions(roadModel);
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
    const SectionDrawingConfigData& config)
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
        }
    }
    return templates;
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

    const auto templates = collectPavementTemplatesForConfig(normalized);
    const auto drawingIds = collectSectionDrawingsForRoadModel(roadModelHandle);
    for (const auto& drawingId : drawingIds) {
        DnRoadModelSectionDrawingEntity* entity = nullptr;
        if (acdbOpenObject(entity, drawingId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
            continue;
        }
        if (!entity->isKindOf(DnRoadModelSectionDrawingEntity::desc())) {
            entity->close();
            continue;
        }

        auto drawing = entity->drawingData();
        drawing.config = normalized;
        std::size_t manualCountForDrawing = 0;
        auto faces = buildConfiguredPavementFaces(
            drawing,
            roadModel,
            templates,
            manualCountForDrawing);
        if (entity->setSectionDrawingConfig(normalized) == Acad::eOk
            && entity->replaceFaces(std::move(faces)) == Acad::eOk) {
            ++updatedDrawingCount;
            preservedManualFaceCount += manualCountForDrawing;
            entity->recordGraphicsModified(true);
        }
        entity->close();
    }

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
