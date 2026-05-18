#include "cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/cross_section/RoadModelBuildService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
#include "cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/RoadModelDialogBridge.h"
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#endif

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

#ifndef ROADPROTO_TEST_BUILD

using roadproto::application::cross_section::RoadModelBuildInput;
using roadproto::application::cross_section::RoadModelBuildService;
using roadproto::domain::alignment::AlignmentPoint2d;
using roadproto::domain::alignment::AlignmentSamplePoint;
using roadproto::domain::cross_section::RoadModelConfig;
using roadproto::domain::cross_section::RoadModelTemplateAssignment;
using roadproto::domain::cross_section::RoadModelTemplateSource;
using roadproto::domain::profile::ProfileVerticalCurveData;

constexpr double kStationEpsilon = 1e-8;

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
    entity->close();

    std::wstring errorMessage;
    if (!queueRoadModelWpfDialog(request, errorMessage)) {
        editor.writeError(L"Failed to open road model WPF dialog: " + errorMessage);
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
           << L", component line count: " << data.componentLines.size() << L".";
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

    RoadModelBuildInput input;
    input.config = std::move(config);
    input.alignmentSamples = std::move(alignmentSamples);
    input.verticalCurve = std::move(verticalCurve);
    input.templates = std::move(templateSources);

    RoadModelBuildService service;
    const auto result = service.build(input);
    if (!result.succeeded) {
        editor.writeError(L"Road model build failed: " + resultErrorText(L"invalid road model input.", result.errorMessage));
        return;
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

} // namespace roadproto::cad_adapter::objectarx::cross_section
