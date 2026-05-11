#include "cad_adapter/objectarx/profile/ObjectArxProfileGradeGraphCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/profile/ProfileGradeGraphCreateService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/alignment/DnRoadCenterlineEntity.h"
#include "cad_adapter/objectarx/profile/DnProfileGradeGraphEntity.h"
#include "cad_adapter/objectarx/profile/ProfileGradeGraphDialogBridge.h"
#include "cad_adapter/objectarx/terrain/DnTerrainTinEntity.h"
#include "domain/profile/ProfileDmxFile.h"
#include "domain/profile/ProfileGradeGraphLayout.h"

#include "aced.h"
#include "adscodes.h"
#include "acutmem.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <cmath>
#endif

namespace roadproto::cad_adapter::objectarx::profile {
namespace {

#ifndef ROADPROTO_TEST_BUILD

using roadproto::application::profile::ProfileGradeGraphCreateInput;
using roadproto::application::profile::ProfileGradeGraphCreateService;
using roadproto::domain::alignment::AlignmentPoint2d;
using roadproto::domain::alignment::AlignmentSamplePoint;
using roadproto::domain::alignment::HorizontalAlignment;
using roadproto::domain::alignment::RoadCenterlineProperties;
using roadproto::domain::profile::ProfileDmxFile;
using roadproto::domain::profile::ProfileGradeGraphData;
using roadproto::domain::profile::ProfileGradeGraphLayout;
using roadproto::domain::profile::ProfileGradeGraphProperties;
using roadproto::domain::profile::ProfileGroundSample;
using roadproto::domain::profile::ProfileGroundSourceType;

constexpr double kDefaultGroundLinePrecision = 10.0;
constexpr double kStationEpsilon = 1e-8;
constexpr double kMaxGroundLineWidth = 2.11;
constexpr std::size_t kMaxTerrainProfileSamples = 50000;

bool appendEntityToModelSpace(AcDbEntity* entity, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr) {
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

bool findRoadCenterlineEntityInSelectionSet(const ads_name selectionSet, AcDbObjectId& entityId)
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

        DnRoadCenterlineEntity* entity = nullptr;
        if (acdbOpenObject(entity, candidateId, AcDb::kForRead) == Acad::eOk && entity != nullptr) {
            entity->close();
            entityId = candidateId;
            return true;
        }
    }

    return false;
}

bool findImpliedRoadCenterlineEntity(AcDbObjectId& entityId)
{
    ads_name selectionSet;
    if (acedSSGet(L"_I", nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }

    SelectionSetGuard guard(selectionSet);
    return findRoadCenterlineEntityInSelectionSet(selectionSet, entityId);
}

bool selectRoadCenterlineEntity(AcDbObjectId& entityId)
{
    if (findImpliedRoadCenterlineEntity(entityId)) {
        return true;
    }

    ads_name selectionSet;
    if (acedSSGet(nullptr, nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }

    SelectionSetGuard guard(selectionSet);
    return findRoadCenterlineEntityInSelectionSet(selectionSet, entityId);
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

bool readRoadCenterline(
    AcDbObjectId entityId,
    HorizontalAlignment& alignment,
    RoadCenterlineProperties& properties,
    std::wstring& centerlineHandle)
{
    DnRoadCenterlineEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }

    alignment = entity->alignment();
    properties = entity->properties();
    centerlineHandle = entityHandleText(entity);
    entity->close();
    return true;
}

bool isFinitePoint(const AlignmentPoint2d& point)
{
    return std::isfinite(point.x) && std::isfinite(point.y);
}

std::vector<AlignmentSamplePoint> collectAlignmentPathSamples(const HorizontalAlignment& alignment)
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

AlignmentPoint2d interpolateAlignmentPoint(
    const std::vector<AlignmentSamplePoint>& samples,
    double station)
{
    if (samples.empty()) {
        return {};
    }
    if (station <= samples.front().station) {
        return samples.front().point;
    }
    if (station >= samples.back().station) {
        return samples.back().point;
    }

    for (std::size_t i = 1; i < samples.size(); ++i) {
        const auto& previous = samples[i - 1];
        const auto& next = samples[i];
        if (station <= next.station + kStationEpsilon) {
            const auto stationSpan = next.station - previous.station;
            if (stationSpan <= kStationEpsilon) {
                return next.point;
            }

            const auto t = std::clamp((station - previous.station) / stationSpan, 0.0, 1.0);
            return AlignmentPoint2d{
                previous.point.x + (next.point.x - previous.point.x) * t,
                previous.point.y + (next.point.y - previous.point.y) * t};
        }
    }

    return samples.back().point;
}

bool sampleTerrainAlongAlignment(
    const HorizontalAlignment& alignment,
    DnTerrainTinEntity* terrain,
    double precision,
    std::vector<ProfileGroundSample>& samples,
    std::wstring& errorMessage)
{
    samples.clear();
    if (terrain == nullptr) {
        errorMessage = L"地形数模对象无效。";
        return false;
    }
    if (!std::isfinite(precision) || precision <= 0.0) {
        errorMessage = L"地面线精度必须大于 0。";
        return false;
    }

    const auto pathSamples = collectAlignmentPathSamples(alignment);
    if (pathSamples.size() < 2) {
        errorMessage = L"道路中线没有足够的采样点。";
        return false;
    }

    const auto startStation = pathSamples.front().station;
    const auto endStation = pathSamples.back().station;
    const auto stationSpan = endStation - startStation;
    if (!std::isfinite(stationSpan) || stationSpan <= kStationEpsilon) {
        errorMessage = L"道路中线桩号范围无效。";
        return false;
    }

    const auto estimatedIntervals = stationSpan / precision;
    if (!std::isfinite(estimatedIntervals)
        || estimatedIntervals + 2.0 > static_cast<double>(kMaxTerrainProfileSamples)) {
        errorMessage = L"地面线精度过小，采样点数量过多。";
        return false;
    }
    const auto intervalCount = static_cast<std::size_t>(std::floor(estimatedIntervals));

    auto appendSampleAt = [&](double station) {
        const auto point = interpolateAlignmentPoint(pathSamples, station);
        double elevation = 0.0;
        if (!terrain->SampleElevation(point.x, point.y, elevation)) {
            return;
        }

        ProfileGroundSample sample;
        sample.station = station;
        sample.elevation = elevation;
        samples.push_back(std::move(sample));
    };

    double lastAttemptedStation = startStation;
    for (std::size_t i = 0; i <= intervalCount; ++i) {
        const auto station = startStation + static_cast<double>(i) * precision;
        if (station > endStation - kStationEpsilon) {
            break;
        }
        appendSampleAt(station);
        lastAttemptedStation = station;
    }
    if (endStation - lastAttemptedStation > kStationEpsilon) {
        appendSampleAt(endStation);
    }

    if (samples.size() < 2) {
        samples.clear();
        errorMessage = L"地形数模在道路中线经过位置未取得足够高程点。";
        return false;
    }
    return true;
}

bool collectTerrainTinSamples(
    const HorizontalAlignment& alignment,
    const RoadCenterlineProperties& properties,
    double precision,
    std::vector<ProfileGroundSample>& samples,
    std::wstring& terrainTinHandle)
{
    if (!properties.linkedTerrainEnabled || properties.linkedTerrainHandle.empty()) {
        return false;
    }

    AcDbObjectId terrainId;
    if (!resolveObjectIdFromHandle(properties.linkedTerrainHandle, terrainId)) {
        return false;
    }

    DnTerrainTinEntity* terrain = nullptr;
    if (acdbOpenObject(terrain, terrainId, AcDb::kForRead) != Acad::eOk || terrain == nullptr) {
        return false;
    }

    std::wstring errorMessage;
    const auto sampled = sampleTerrainAlongAlignment(alignment, terrain, precision, samples, errorMessage);
    if (sampled) {
        terrainTinHandle = entityHandleText(terrain);
    }
    terrain->close();
    if (!sampled) {
        samples.clear();
        terrainTinHandle.clear();
        return false;
    }

    return true;
}

bool collectTerrainTinSamplesFromHandles(
    const std::wstring& roadCenterlineHandle,
    const std::wstring& terrainTinHandle,
    double precision,
    std::vector<ProfileGroundSample>& samples,
    std::wstring& errorMessage)
{
    AcDbObjectId centerlineId;
    if (!resolveObjectIdFromHandle(roadCenterlineHandle, centerlineId)) {
        errorMessage = L"未找到道路中线 handle 对应对象。";
        return false;
    }

    HorizontalAlignment alignment;
    RoadCenterlineProperties properties;
    std::wstring ignoredCenterlineHandle;
    if (!readRoadCenterline(centerlineId, alignment, properties, ignoredCenterlineHandle)) {
        errorMessage = L"无法打开道路中线实体。";
        return false;
    }

    AcDbObjectId terrainId;
    if (!resolveObjectIdFromHandle(terrainTinHandle, terrainId)) {
        errorMessage = L"未找到地形数模 handle 对应对象。";
        return false;
    }

    DnTerrainTinEntity* terrain = nullptr;
    if (acdbOpenObject(terrain, terrainId, AcDb::kForRead) != Acad::eOk || terrain == nullptr) {
        errorMessage = L"无法打开地形数模实体。";
        return false;
    }

    const auto sampled = sampleTerrainAlongAlignment(alignment, terrain, precision, samples, errorMessage);
    terrain->close();
    return sampled;
}

bool promptDmxFilePath(std::wstring& path, roadproto::cad_adapter::IEditor& editor)
{
    resbuf* result = acutNewRb(RTSTR);
    if (result == nullptr) {
        editor.writeError(L"无法创建 DMX 文件选择结果缓冲。");
        return false;
    }
    result->resval.rstring = nullptr;

    const auto status = acedGetFileD(L"选择纵断面地面线 DMX 文件", L"", L"dmx", 2, result);
    if (status != RTNORM || result->resval.rstring == nullptr || result->resval.rstring[0] == L'\0') {
        acutRelRb(result);
        editor.writeWarning(L"已取消 DMX 文件选择。");
        return false;
    }

    path = result->resval.rstring;
    acutRelRb(result);
    return true;
}

bool promptInsertionPoint(AcGePoint3d& insertionPoint)
{
    ads_point raw;
    if (acedGetPoint(nullptr, L"\n请选择纵断面拉坡图插入位置: ", raw) != RTNORM) {
        return false;
    }

    insertionPoint = AcGePoint3d(raw[0], raw[1], raw[2]);
    return true;
}

bool loadDmxSamples(
    std::wstring& dmxFilePath,
    std::vector<ProfileGroundSample>& samples,
    roadproto::cad_adapter::IEditor& editor)
{
    if (!promptDmxFilePath(dmxFilePath, editor)) {
        return false;
    }

    const auto parsed = ProfileDmxFile::read(dmxFilePath);
    if (!parsed.succeeded) {
        editor.writeError(parsed.errorMessage.empty() ? L"DMX 文件解析失败。" : parsed.errorMessage);
        return false;
    }

    samples = parsed.samples;
    return true;
}

std::wstring sourceTypeText(ProfileGroundSourceType sourceType)
{
    return sourceType == ProfileGroundSourceType::TerrainTin
        ? L"关联地形 TIN"
        : L"DMX 文件";
}

bool isSupportedVerticalScale(double value)
{
    return value == 1.0 || value == 10.0 || value == 100.0;
}

bool validateGraphProperties(const ProfileGradeGraphProperties& properties, std::wstring& errorMessage)
{
    if (properties.groundLineColorIndex < 1 || properties.groundLineColorIndex > 255) {
        errorMessage = L"地面线颜色 ACI index 必须在 1 到 255 之间。";
        return false;
    }
    if (!std::isfinite(properties.groundLineWidth)
        || properties.groundLineWidth <= 0.0
        || properties.groundLineWidth > kMaxGroundLineWidth) {
        errorMessage = L"地面线宽度必须大于 0 且不超过 2.11mm。";
        return false;
    }
    if (!std::isfinite(properties.groundLinePrecision) || properties.groundLinePrecision <= 0.0) {
        errorMessage = L"地面线精度必须大于 0。";
        return false;
    }
    if (!std::isfinite(properties.gridSpacing) || properties.gridSpacing <= 0.0) {
        errorMessage = L"网格线间距必须大于 0。";
        return false;
    }
    if (!std::isfinite(properties.verticalScale) || !isSupportedVerticalScale(properties.verticalScale)) {
        errorMessage = L"纵向比例只支持 1:1、1:10 或 1:100。";
        return false;
    }
    return true;
}

bool validateGraphData(const ProfileGradeGraphData& graph, std::wstring& errorMessage)
{
    if (!validateGraphProperties(graph.properties, errorMessage)) {
        return false;
    }

    const auto layout = ProfileGradeGraphLayout::calculate(graph);
    if (!layout.succeeded) {
        errorMessage = layout.errorMessage.empty() ? L"纵断面拉坡图布局无效。" : layout.errorMessage;
        return false;
    }
    return true;
}

bool queueDialogForProfileGradeGraph(AcDbObjectId entityId)
{
    auto& editor = app::ApplicationContext::instance().editor();

    DnProfileGradeGraphEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开纵断面拉坡图实体。");
        return false;
    }

    const auto graph = entity->graphData();
    ProfileGradeGraphDialogRequest request;
    request.handle = entityHandleText(entity);
    request.roadCenterlineHandle = graph.roadCenterlineHandle;
    request.terrainTinHandle = graph.terrainTinHandle;
    request.graphName = graph.properties.graphName.empty() ? L"拉坡图" : graph.properties.graphName;
    request.sourceType = graph.sourceType;
    request.dmxFilePath = graph.dmxFilePath;
    request.groundLineColorIndex = graph.properties.groundLineColorIndex;
    request.groundLineWidth = graph.properties.groundLineWidth;
    request.groundLinePrecision = graph.properties.groundLinePrecision;
    request.verticalScale = graph.properties.verticalScale;
    request.gridSpacing = graph.properties.gridSpacing;
    request.sampleCount = graph.groundSamples.size();
    entity->close();

    std::wstring errorMessage;
    if (!queueProfileGradeGraphWpfDialog(request, errorMessage)) {
        editor.writeError(L"打开纵断面拉坡图 WPF 属性窗口失败: " + errorMessage);
        return false;
    }
    return true;
}

void runProfileGradeGraphCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_PROFILE_GRADE_GRAPH_CREATE: 请选择道路中线自定义实体。");

    AcDbObjectId centerlineId;
    if (!selectRoadCenterlineEntity(centerlineId)) {
        editor.writeWarning(L"未选择道路中线自定义实体。");
        return;
    }

    HorizontalAlignment alignment;
    RoadCenterlineProperties centerlineProperties;
    std::wstring centerlineHandle;
    if (!readRoadCenterline(centerlineId, alignment, centerlineProperties, centerlineHandle)) {
        editor.writeError(L"无法打开道路中线实体。");
        return;
    }

    ProfileGroundSourceType sourceType = ProfileGroundSourceType::DmxFile;
    std::wstring terrainTinHandle;
    std::wstring dmxFilePath;
    std::vector<ProfileGroundSample> groundSamples;
    if (collectTerrainTinSamples(
            alignment,
            centerlineProperties,
            kDefaultGroundLinePrecision,
            groundSamples,
            terrainTinHandle)) {
        sourceType = ProfileGroundSourceType::TerrainTin;
    } else if (!loadDmxSamples(dmxFilePath, groundSamples, editor)) {
        return;
    }

    AcGePoint3d insertionPoint;
    if (!promptInsertionPoint(insertionPoint)) {
        editor.writeWarning(L"纵断面拉坡图创建已取消。");
        return;
    }

    ProfileGradeGraphCreateInput input;
    input.sourceType = sourceType;
    input.roadName = centerlineProperties.roadName;
    input.roadCenterlineHandle = centerlineHandle;
    input.terrainTinHandle = sourceType == ProfileGroundSourceType::TerrainTin ? terrainTinHandle : L"";
    input.dmxFilePath = sourceType == ProfileGroundSourceType::DmxFile ? dmxFilePath : L"";
    input.groundSamples = std::move(groundSamples);

    const ProfileGradeGraphCreateService service;
    const auto buildResult = service.build(input);
    if (!buildResult.succeeded) {
        editor.writeError(buildResult.errorMessage.empty() ? L"纵断面拉坡图数据生成失败。" : buildResult.errorMessage);
        return;
    }

    auto* entity = new DnProfileGradeGraphEntity();
    entity->setGraphData(buildResult.graph);
    entity->setInsertionPoint(insertionPoint);

    AcDbObjectId graphId;
    if (!appendEntityToModelSpace(entity, graphId)) {
        delete entity;
        editor.writeError(L"插入 DnProfileGradeGraphEntity 失败。");
        return;
    }

    const auto graphHandle = entityHandleText(entity);
    entity->close();
    acedUpdateDisplay();

    std::wstringstream stream;
    stream << L"已创建纵断面拉坡图，handle: " << graphHandle
           << L"，样本数量: " << buildResult.graph.groundSamples.size()
           << L"，数据来源: " << sourceTypeText(sourceType);
    if (sourceType == ProfileGroundSourceType::TerrainTin) {
        stream << L"，地形 handle: " << terrainTinHandle;
    } else {
        stream << L"，DMX: " << dmxFilePath;
    }
    stream << L"。";
    editor.writeMessage(stream.str());
}

void runProfileGradeGraphEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR handleBuffer[64] = {};
    if (acedGetString(Adesk::kFalse, L"\n纵断面拉坡图 handle: ", handleBuffer) != RTNORM) {
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(handleBuffer, entityId)) {
        editor.writeWarning(L"未找到 handle 对应的纵断面拉坡图实体。");
        return;
    }

    DnProfileGradeGraphEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开纵断面拉坡图实体。");
        return;
    }
    const auto isProfileGraph = entity->isKindOf(DnProfileGradeGraphEntity::desc());
    entity->close();
    if (!isProfileGraph) {
        editor.writeWarning(L"handle 对应对象不是纵断面拉坡图实体。");
        return;
    }

    queueDialogForProfileGradeGraph(entityId);
}

void runProfileApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto profile grade graph dialog response file: ", pathBuffer) != RTNORM) {
        return;
    }

    ProfileGradeGraphDialogResponse response;
    std::wstring errorMessage;
    if (!readProfileGradeGraphDialogResponse(pathBuffer, response, errorMessage)) {
        editor.writeError(L"读取纵断面拉坡图对话框结果失败: " + errorMessage);
        return;
    }

    if (!response.accepted) {
        return;
    }

    AcDbObjectId entityId;
    if (!resolveObjectIdFromHandle(response.handle, entityId)) {
        editor.writeWarning(L"未找到对话框结果对应的纵断面拉坡图实体。");
        return;
    }

    DnProfileGradeGraphEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"无法打开纵断面拉坡图实体。");
        return;
    }

    auto candidate = entity->graphData();
    const auto previousPrecision = candidate.properties.groundLinePrecision;
    candidate.properties.graphName = response.graphName.empty()
        ? (candidate.properties.graphName.empty() ? L"拉坡图" : candidate.properties.graphName)
        : response.graphName;
    candidate.properties.groundLineColorIndex = response.groundLineColorIndex;
    candidate.properties.groundLineWidth = response.groundLineWidth;
    candidate.properties.groundLinePrecision = response.groundLinePrecision;
    candidate.properties.verticalScale = response.verticalScale;
    candidate.properties.gridSpacing = response.gridSpacing;

    if (response.updateGroundLineRequested) {
        if (candidate.sourceType == ProfileGroundSourceType::DmxFile) {
            if (candidate.dmxFilePath.empty()) {
                entity->close();
                editor.writeError(L"当前拉坡图没有保存 DMX 文件路径，无法更新地面线。");
                return;
            }

            const auto parsed = ProfileDmxFile::read(candidate.dmxFilePath);
            if (!parsed.succeeded) {
                entity->close();
                editor.writeError(parsed.errorMessage.empty() ? L"DMX 文件解析失败，地面线未更新。" : parsed.errorMessage);
                return;
            }
            candidate.groundSamples = parsed.samples;
        } else {
            editor.writeWarning(L"数模来源的拉坡图暂不支持从属性窗口更新地面线。");
        }
    }

    if (candidate.sourceType == ProfileGroundSourceType::TerrainTin
        && std::fabs(candidate.properties.groundLinePrecision - previousPrecision) > kStationEpsilon) {
        std::vector<ProfileGroundSample> resampledSamples;
        if (!collectTerrainTinSamplesFromHandles(
                candidate.roadCenterlineHandle,
                candidate.terrainTinHandle,
                candidate.properties.groundLinePrecision,
                resampledSamples,
                errorMessage)) {
            entity->close();
            editor.writeError(L"按新精度重新采样地面线失败: " + errorMessage);
            return;
        }
        candidate.groundSamples = std::move(resampledSamples);
    }

    if (!validateGraphData(candidate, errorMessage)) {
        entity->close();
        editor.writeError(L"纵断面拉坡图参数无效: " + errorMessage);
        return;
    }

    entity->setGraphData(candidate);
    entity->recordGraphicsModified(true);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(response.updateGroundLineRequested
        ? L"纵断面拉坡图属性和地面线已更新。"
        : L"纵断面拉坡图属性已更新。");
}

#else

void runProfileGradeGraphCreateCommand()
{
}

void runProfileGradeGraphEditHandleCommand()
{
}

void runProfileApplyDialogFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure profileGradeGraphCreateCommandProcedure()
{
    return &runProfileGradeGraphCreateCommand;
}

core::CommandProcedure profileGradeGraphEditHandleCommandProcedure()
{
    return &runProfileGradeGraphEditHandleCommand;
}

core::CommandProcedure profileApplyDialogFileCommandProcedure()
{
    return &runProfileApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::profile
