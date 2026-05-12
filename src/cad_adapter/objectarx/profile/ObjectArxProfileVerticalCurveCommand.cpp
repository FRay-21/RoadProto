#include "cad_adapter/objectarx/profile/ObjectArxProfileVerticalCurveCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/profile/ProfileVerticalCurveCreateService.h"
#include "application/profile/ProfileVerticalCurveEditService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "cad_adapter/objectarx/profile/DnProfileVerticalCurveEntity.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <string>
#endif

namespace roadproto::cad_adapter::objectarx::profile {
namespace {

#ifndef ROADPROTO_TEST_BUILD

using roadproto::application::profile::ProfileVerticalCurveCreateInput;
using roadproto::application::profile::ProfileVerticalCurveCreateService;
using roadproto::application::profile::ProfileVerticalCurveEditService;
using roadproto::domain::profile::ProfileVerticalCurveData;

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

bool selectProfileGradeGraphEntity(AcDbObjectId& entityId)
{
    return selectTypedEntity<DnProfileGradeGraphEntity>(entityId);
}

bool selectProfileVerticalCurveEntity(AcDbObjectId& entityId)
{
    return selectTypedEntity<DnProfileVerticalCurveEntity>(entityId);
}

bool readProfileGraph(AcDbObjectId entityId, ProfileVerticalCurveCreateInput& input)
{
    DnProfileGradeGraphEntity* graphEntity = nullptr;
    if (acdbOpenObject(graphEntity, entityId, AcDb::kForRead) != Acad::eOk || graphEntity == nullptr) {
        return false;
    }

    input.profileGraphHandle = entityHandleText(graphEntity);
    input.groundSamples = graphEntity->graphData().groundSamples;
    graphEntity->close();
    return !input.profileGraphHandle.empty();
}

bool promptGraphPointAsStationElevation(
    DnProfileVerticalCurveEntity& entity,
    double& station,
    double& elevation)
{
    ads_point raw;
    if (acedGetPoint(nullptr, L"\n请选择纵断面拉坡图上的点: ", raw) != RTNORM) {
        return false;
    }

    return entity.mapCadPointToDesign(AcGePoint3d(raw[0], raw[1], raw[2]), station, elevation);
}

std::wstring editErrorText(const std::wstring& fallback, const std::wstring& errorMessage)
{
    return errorMessage.empty() ? fallback : errorMessage;
}

void runProfileVerticalCurveCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_PROFILE_VERTICAL_CURVE_CREATE: 请选择纵断面拉坡图实体。");

    AcDbObjectId graphId;
    if (!selectProfileGradeGraphEntity(graphId)) {
        editor.writeWarning(L"未选择纵断面拉坡图实体。");
        return;
    }

    ProfileVerticalCurveCreateInput input;
    if (!readProfileGraph(graphId, input)) {
        editor.writeError(L"无法读取纵断面拉坡图数据。");
        return;
    }

    const ProfileVerticalCurveCreateService service;
    const auto buildResult = service.buildDefaultFromGraph(input);
    if (!buildResult.succeeded) {
        editor.writeError(editErrorText(L"纵曲线默认数据生成失败。", buildResult.errorMessage));
        return;
    }

    auto* entity = new DnProfileVerticalCurveEntity();
    entity->setCurveData(buildResult.data);

    AcDbObjectId curveId;
    if (!appendEntityToModelSpace(entity, curveId)) {
        delete entity;
        editor.writeError(L"插入纵曲线实体失败。");
        return;
    }

    const auto curveHandle = entityHandleText(entity);
    entity->close();
    acedUpdateDisplay();

    std::wstringstream stream;
    stream << L"已创建纵曲线实体，handle: " << curveHandle
           << L"，关联拉坡图 handle: " << input.profileGraphHandle << L"。";
    editor.writeMessage(stream.str());
}

void runProfileVerticalCurveEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"纵曲线 WPF 编辑窗口将在 Task 7 接入。");
}

void runProfileVerticalCurveApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"纵曲线对话框回写将在 Task 7 接入。");
}

void runProfileVerticalCurveAddPviCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"请选择纵曲线实体，并在拉坡图上点取新增 PVI。");

    AcDbObjectId curveId;
    if (!selectProfileVerticalCurveEntity(curveId)) {
        editor.writeWarning(L"未选择纵曲线实体。");
        return;
    }

    DnProfileVerticalCurveEntity* entity = nullptr;
    if (acdbOpenObject(entity, curveId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开纵曲线实体。");
        return;
    }

    double station = 0.0;
    double elevation = 0.0;
    if (!promptGraphPointAsStationElevation(*entity, station, elevation)) {
        entity->close();
        editor.writeWarning(L"PVI 添加已取消或点位无法映射到拉坡图。");
        return;
    }

    auto data = entity->curveData();
    const ProfileVerticalCurveEditService service;
    const auto result = service.addPvi(data, station, elevation, 1000.0);
    if (!result.succeeded) {
        entity->close();
        editor.writeError(L"添加 PVI 失败: " + editErrorText(L"纵曲线数据无效。", result.errorMessage));
        return;
    }

    entity->setCurveData(data);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"已添加纵曲线 PVI。");
}

void runProfileVerticalCurveDeletePviCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"请选择纵曲线实体，并在拉坡图上点取要删除的 PVI 附近位置。");

    AcDbObjectId curveId;
    if (!selectProfileVerticalCurveEntity(curveId)) {
        editor.writeWarning(L"未选择纵曲线实体。");
        return;
    }

    DnProfileVerticalCurveEntity* entity = nullptr;
    if (acdbOpenObject(entity, curveId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开纵曲线实体。");
        return;
    }

    auto data = entity->curveData();
    if (data.pvis.empty()) {
        entity->close();
        editor.writeWarning(L"当前纵曲线没有可删除的 PVI。");
        return;
    }

    double station = 0.0;
    double elevation = 0.0;
    if (!promptGraphPointAsStationElevation(*entity, station, elevation)) {
        entity->close();
        editor.writeWarning(L"PVI 删除已取消或点位无法映射到拉坡图。");
        return;
    }

    std::size_t nearestIndex = 0;
    double nearestDistance = std::numeric_limits<double>::max();
    for (std::size_t i = 0; i < data.pvis.size(); ++i) {
        const auto stationDelta = data.pvis[i].station - station;
        const auto elevationDelta = data.pvis[i].elevation - elevation;
        const auto distance = stationDelta * stationDelta + elevationDelta * elevationDelta;
        if (distance < nearestDistance) {
            nearestDistance = distance;
            nearestIndex = i;
        }
    }

    const ProfileVerticalCurveEditService service;
    const auto result = service.deletePvi(data, nearestIndex);
    if (!result.succeeded) {
        entity->close();
        editor.writeError(L"删除 PVI 失败: " + editErrorText(L"纵曲线数据无效。", result.errorMessage));
        return;
    }

    entity->setCurveData(data);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"已删除最近的纵曲线 PVI。");
}

#else

void runProfileVerticalCurveCreateCommand()
{
}

void runProfileVerticalCurveEditHandleCommand()
{
}

void runProfileVerticalCurveApplyDialogFileCommand()
{
}

void runProfileVerticalCurveAddPviCommand()
{
}

void runProfileVerticalCurveDeletePviCommand()
{
}

#endif

} // namespace

core::CommandProcedure profileVerticalCurveCreateCommandProcedure()
{
    return &runProfileVerticalCurveCreateCommand;
}

core::CommandProcedure profileVerticalCurveEditHandleCommandProcedure()
{
    return &runProfileVerticalCurveEditHandleCommand;
}

core::CommandProcedure profileVerticalCurveApplyDialogFileCommandProcedure()
{
    return &runProfileVerticalCurveApplyDialogFileCommand;
}

core::CommandProcedure profileVerticalCurveAddPviCommandProcedure()
{
    return &runProfileVerticalCurveAddPviCommand;
}

core::CommandProcedure profileVerticalCurveDeletePviCommandProcedure()
{
    return &runProfileVerticalCurveDeletePviCommand;
}

} // namespace roadproto::cad_adapter::objectarx::profile
