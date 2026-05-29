#include "cad_adapter/objectarx/drawing_quantity/ObjectArxPavementQuantityTableCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h"
#include "domain/quantity/ClearTableQuantityDrawingFaceSampler.h"
#include "domain/quantity/PavementQuantityDrawingFaceSampler.h"
#include "domain/quantity/PavementQuantityTable.h"
#include "domain/quantity/RoadModelPavementQuantitySampler.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <objbase.h>
#include <shobjidl.h>
#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#endif

namespace roadproto::cad_adapter::objectarx::drawing_quantity {
namespace {

#ifndef ROADPROTO_TEST_BUILD

using roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingData;
using roadproto::domain::cross_section::RoadModelData;
using roadproto::domain::cross_section::RoadModelStructureType;
using roadproto::domain::quantity::ClearTableQuantityDrawingFace;
using roadproto::domain::quantity::ClearTableQuantityDrawingFaceSampler;
using roadproto::domain::quantity::ClearTableQuantityDrawingPoint;
using roadproto::domain::quantity::ClearTableQuantitySectionSample;
using roadproto::domain::quantity::PavementQuantityAggregationMode;
using roadproto::domain::quantity::PavementQuantityDrawingFace;
using roadproto::domain::quantity::PavementQuantityDrawingFaceSampler;
using roadproto::domain::quantity::PavementQuantityDrawingPoint;
using roadproto::domain::quantity::PavementQuantitySectionSample;
using roadproto::domain::quantity::PavementQuantitySegmentType;
using roadproto::domain::quantity::PavementQuantityStructureRange;
using roadproto::domain::quantity::PavementQuantityTableCalculator;
using roadproto::domain::quantity::PavementQuantityTableXlsWriter;
using roadproto::domain::quantity::RoadModelPavementQuantitySampler;

constexpr double kStationTolerance = 1.0e-7;
constexpr DWORD kAggregationModeControlId = 101;
constexpr DWORD kAggregationModeByComponentItemId = 1;
constexpr DWORD kAggregationModeByLayerTypeItemId = 2;

std::wstring toUpper(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towupper(ch));
    });
    return value;
}

std::wstring ensureXlsExtension(const std::wstring& path)
{
    std::filesystem::path filePath(path);
    if (toUpper(filePath.extension().wstring()) != L".XLS") {
        filePath.replace_extension(L".xls");
    }
    return filePath.wstring();
}

bool promptXlsFilePath(
    const ACHAR* title,
    const ACHAR* defaultFileName,
    std::wstring& path,
    PavementQuantityAggregationMode& aggregationMode,
    roadproto::cad_adapter::IEditor& editor)
{
    path.clear();
    aggregationMode = PavementQuantityAggregationMode::ByComponentAndLayer;

    HRESULT initializeResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = initializeResult == S_OK || initializeResult == S_FALSE;

    IFileSaveDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_FileSaveDialog,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(hr) && dialog != nullptr) {
        dialog->SetTitle(title);
        dialog->SetFileName(defaultFileName);
        dialog->SetDefaultExtension(L"xls");
        const COMDLG_FILTERSPEC filters[] = {
            {L"Excel 97-2003 工作簿 (*.xls)", L"*.xls"},
            {L"所有文件 (*.*)", L"*.*"},
        };
        dialog->SetFileTypes(2, filters);
        dialog->SetFileTypeIndex(1);

        IFileDialogCustomize* customize = nullptr;
        if (SUCCEEDED(dialog->QueryInterface(IID_PPV_ARGS(&customize))) && customize != nullptr) {
            customize->StartVisualGroup(kAggregationModeControlId + 10, L"统计方式");
            customize->AddRadioButtonList(kAggregationModeControlId);
            customize->AddControlItem(
                kAggregationModeControlId,
                kAggregationModeByComponentItemId,
                L"按部件和结构层");
            customize->AddControlItem(
                kAggregationModeControlId,
                kAggregationModeByLayerTypeItemId,
                L"按结构层类型");
            customize->SetSelectedControlItem(
                kAggregationModeControlId,
                kAggregationModeByComponentItemId);
            customize->EndVisualGroup();
        }

        hr = dialog->Show(nullptr);
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED)) {
            if (customize != nullptr) {
                customize->Release();
            }
            dialog->Release();
            if (shouldUninitialize) {
                CoUninitialize();
            }
            editor.writeWarning(L"已取消路面工程量统计表输出路径选择。");
            return false;
        }
        if (SUCCEEDED(hr)) {
            if (customize != nullptr) {
                DWORD selectedMode = kAggregationModeByComponentItemId;
                if (SUCCEEDED(customize->GetSelectedControlItem(kAggregationModeControlId, &selectedMode))
                    && selectedMode == kAggregationModeByLayerTypeItemId) {
                    aggregationMode = PavementQuantityAggregationMode::ByLayerType;
                }
            }

            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr) {
                PWSTR selectedPath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &selectedPath)) && selectedPath != nullptr) {
                    path = ensureXlsExtension(selectedPath);
                    CoTaskMemFree(selectedPath);
                }
                item->Release();
            }
        }

        if (customize != nullptr) {
            customize->Release();
        }
        dialog->Release();
        if (shouldUninitialize) {
            CoUninitialize();
        }

        if (!path.empty()) {
            return true;
        }
    } else if (shouldUninitialize) {
        CoUninitialize();
    }

    editor.writeWarning(L"系统保存对话框不可用，使用默认按部件和结构层统计模式。");
    resbuf* result = acutNewRb(RTSTR);
    if (result == nullptr) {
        editor.writeError(L"无法创建 XLS 文件选择缓冲。");
        return false;
    }
    result->resval.rstring = nullptr;

    const auto status = acedGetFileD(title, defaultFileName, L"xls", 3, result);
    if (status != RTNORM || result->resval.rstring == nullptr || result->resval.rstring[0] == L'\0') {
        acutRelRb(result);
        editor.writeWarning(L"已取消路面工程量统计表输出路径选择。");
        return false;
    }

    path = ensureXlsExtension(result->resval.rstring);
    acutRelRb(result);
    return true;
}

bool appendSectionDrawingIdsFromSelection(
    const ads_name selectionSet,
    std::vector<AcDbObjectId>& entityIds)
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
        if (acdbGetObjectId(candidateId, entityName) != Acad::eOk || candidateId.isNull()) {
            continue;
        }

        DnRoadModelSectionDrawingEntity* entity = nullptr;
        if (acdbOpenObject(entity, candidateId, AcDb::kForRead) == Acad::eOk && entity != nullptr) {
            if (entity->isKindOf(DnRoadModelSectionDrawingEntity::desc())) {
                entityIds.push_back(candidateId);
            }
            entity->close();
        }
    }

    return !entityIds.empty();
}

bool selectSectionDrawingEntities(std::vector<AcDbObjectId>& entityIds)
{
    entityIds.clear();

    ads_name impliedSelection;
    if (acedSSGet(L"_I", nullptr, nullptr, nullptr, impliedSelection) == RTNORM) {
        roadproto::cad_adapter::objectarx::SelectionSetGuard guard(impliedSelection);
        if (appendSectionDrawingIdsFromSelection(impliedSelection, entityIds)) {
            return true;
        }
    }

    ads_name selectionSet;
    if (acedSSGet(nullptr, nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }

    roadproto::cad_adapter::objectarx::SelectionSetGuard guard(selectionSet);
    return appendSectionDrawingIdsFromSelection(selectionSet, entityIds);
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

bool resolveObjectIdFromHandle(const std::wstring& handleText, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr || handleText.empty()) {
        return false;
    }

    const AcDbHandle handle(handleText.c_str());
    return database->getAcDbObjectId(entityId, false, handle) == Acad::eOk && !entityId.isNull();
}

bool readRoadModelData(
    const std::wstring& roadModelHandle,
    RoadModelData& data,
    std::wstring& actualHandle)
{
    AcDbObjectId roadModelId;
    if (!resolveObjectIdFromHandle(roadModelHandle, roadModelId)) {
        return false;
    }

    DnRoadModelEntity* entity = nullptr;
    if (acdbOpenObject(entity, roadModelId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }

    if (!entity->isKindOf(DnRoadModelEntity::desc())) {
        entity->close();
        return false;
    }

    data = entity->roadModelData();
    actualHandle = entityHandleText(entity);
    entity->close();
    return !actualHandle.empty();
}

bool sameStation(double lhs, double rhs)
{
    return std::fabs(lhs - rhs) <= kStationTolerance;
}

bool isClearTableFaceId(const std::wstring& faceId)
{
    return faceId.rfind(L"clearTable:", 0) == 0;
}

std::wstring clearTableSideNameFromFaceId(const std::wstring& faceId)
{
    if (faceId.find(L"clearTable:Left:") == 0) {
        return L"左侧";
    }
    if (faceId.find(L"clearTable:Right:") == 0) {
        return L"右侧";
    }
    return L"";
}

std::optional<PavementQuantitySectionSample> sampleFromRoadModelSection(
    double station,
    const RoadModelData& data)
{
    return RoadModelPavementQuantitySampler::sampleAtStation(data, station);
}

std::vector<PavementQuantityDrawingFace> drawingFacesFromSectionDrawing(
    const RoadModelSectionDrawingData& drawingData)
{
    std::vector<PavementQuantityDrawingFace> faces;
    faces.reserve(drawingData.faces.size());
    for (const auto& face : drawingData.faces) {
        if (isClearTableFaceId(face.faceId)) {
            continue;
        }
        PavementQuantityDrawingFace mapped;
        mapped.layerName = face.layerName;
        mapped.componentName = face.componentName;
        mapped.points.reserve(face.points.size());
        for (const auto& point : face.points) {
            mapped.points.push_back(PavementQuantityDrawingPoint{point.x, point.y});
        }
        faces.push_back(std::move(mapped));
    }
    return faces;
}

double clearTableThicknessFromConfig(
    const RoadModelSectionDrawingData& drawingData,
    int sourceConfigRowIndex)
{
    if (sourceConfigRowIndex < 0) {
        return 0.0;
    }

    const auto index = static_cast<std::size_t>(sourceConfigRowIndex);
    if (index >= drawingData.config.clearTableRows.size()) {
        return 0.0;
    }

    const auto thickness = drawingData.config.clearTableRows[index].thickness;
    return std::isfinite(thickness) && thickness > 0.0 ? thickness : 0.0;
}

std::vector<ClearTableQuantityDrawingFace> clearTableQuantityFacesFromSectionDrawing(
    const RoadModelSectionDrawingData& drawingData)
{
    std::vector<ClearTableQuantityDrawingFace> faces;
    faces.reserve(drawingData.faces.size());
    for (const auto& face : drawingData.faces) {
        if (!isClearTableFaceId(face.faceId)) {
            continue;
        }

        ClearTableQuantityDrawingFace mapped;
        mapped.layerName = face.layerName.empty() ? L"清表" : face.layerName;
        mapped.sideName = clearTableSideNameFromFaceId(face.faceId);
        mapped.sourceConfigRowIndex = face.sourceConfigRowIndex;
        mapped.thickness = clearTableThicknessFromConfig(drawingData, face.sourceConfigRowIndex);
        mapped.points.reserve(face.points.size());
        for (const auto& point : face.points) {
            mapped.points.push_back(ClearTableQuantityDrawingPoint{point.x, point.y});
        }
        faces.push_back(std::move(mapped));
    }
    return faces;
}

std::optional<ClearTableQuantitySectionSample> clearTableQuantitySampleFromSectionDrawing(
    const RoadModelSectionDrawingData& drawingData)
{
    return ClearTableQuantityDrawingFaceSampler::sampleAtStation(
        drawingData.station,
        clearTableQuantityFacesFromSectionDrawing(drawingData));
}

PavementQuantitySegmentType quantityTypeFromStructure(RoadModelStructureType type)
{
    return type == RoadModelStructureType::Tunnel
        ? PavementQuantitySegmentType::Tunnel
        : PavementQuantitySegmentType::Bridge;
}

std::vector<PavementQuantityStructureRange> quantityStructuresFromRoadModel(const RoadModelData& data)
{
    std::vector<PavementQuantityStructureRange> structures;
    structures.reserve(data.config.structures.size());
    for (const auto& structure : data.config.structures) {
        structures.push_back(
            PavementQuantityStructureRange{
                structure.startStation,
                structure.endStation,
                quantityTypeFromStructure(structure.type)});
    }
    return structures;
}

bool readSelectedSectionDrawings(
    const std::vector<AcDbObjectId>& drawingIds,
    std::vector<RoadModelSectionDrawingData>& drawings,
    std::wstring& roadModelHandle)
{
    drawings.clear();
    roadModelHandle.clear();
    for (const auto& drawingId : drawingIds) {
        DnRoadModelSectionDrawingEntity* entity = nullptr;
        if (acdbOpenObject(entity, drawingId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
            continue;
        }
        if (!entity->isKindOf(DnRoadModelSectionDrawingEntity::desc())) {
            entity->close();
            continue;
        }

        const auto data = entity->drawingData();
        entity->close();

        if (data.roadModelHandle.empty() || !std::isfinite(data.station)) {
            continue;
        }
        if (roadModelHandle.empty()) {
            roadModelHandle = data.roadModelHandle;
        } else if (roadModelHandle != data.roadModelHandle) {
            return false;
        }
        drawings.push_back(data);
    }

    return !drawings.empty() && !roadModelHandle.empty();
}

void runPavementQuantityTableCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_DRAWING_PAVEMENT_QUANTITY_TABLE: 请选择查看横断面绘制的横断面图。");

    std::vector<AcDbObjectId> drawingIds;
    if (!selectSectionDrawingEntities(drawingIds)) {
        editor.writeWarning(L"未选择可统计的横断面图实体。");
        return;
    }

    std::vector<RoadModelSectionDrawingData> drawings;
    std::wstring roadModelHandle;
    if (!readSelectedSectionDrawings(drawingIds, drawings, roadModelHandle)) {
        editor.writeWarning(L"请选择同一个道路模型生成的横断面图实体。");
        return;
    }

    RoadModelData roadModelData;
    std::wstring actualRoadModelHandle;
    const bool hasRoadModelData = readRoadModelData(roadModelHandle, roadModelData, actualRoadModelHandle);

    std::vector<PavementQuantitySectionSample> samples;
    samples.reserve(drawings.size());
    for (const auto& drawing : drawings) {
        auto sample = PavementQuantityDrawingFaceSampler::sampleAtStation(
            drawing.station,
            drawingFacesFromSectionDrawing(drawing));
        if (!sample.has_value() && hasRoadModelData) {
            sample = sampleFromRoadModelSection(drawing.station, roadModelData);
        }
        if (sample.has_value()) {
            samples.push_back(*sample);
        }
    }

    std::sort(
        samples.begin(),
        samples.end(),
        [](const auto& lhs, const auto& rhs) {
            return lhs.station < rhs.station;
        });
    samples.erase(
        std::unique(
            samples.begin(),
            samples.end(),
            [](const auto& lhs, const auto& rhs) {
                return sameStation(lhs.station, rhs.station);
            }),
        samples.end());

    std::wstring errorMessage;
    const auto structures = hasRoadModelData
        ? quantityStructuresFromRoadModel(roadModelData)
        : std::vector<PavementQuantityStructureRange>{};

    std::wstring outputPath;
    PavementQuantityAggregationMode aggregationMode = PavementQuantityAggregationMode::ByComponentAndLayer;
    if (!promptXlsFilePath(
            L"输出路面工程量统计表",
            L"pavement_quantity.xls",
            outputPath,
            aggregationMode,
            editor)) {
        return;
    }

    const auto table = PavementQuantityTableCalculator::build(
        samples,
        structures,
        aggregationMode,
        errorMessage);
    if (!errorMessage.empty()) {
        editor.writeError(errorMessage);
        return;
    }

    if (!PavementQuantityTableXlsWriter::write(outputPath, table, errorMessage)) {
        editor.writeError(errorMessage.empty() ? L"路面工程量统计表生成失败。" : errorMessage);
        return;
    }

    std::wostringstream message;
    message << L"路面工程量统计表已生成: " << outputPath
            << L"，分段 " << table.rows.size()
            << L"，统计列 " << table.layerNames.size() << L"。";
    editor.writeMessage(message.str());
}

#else

void runPavementQuantityTableCommand()
{
}

#endif

} // namespace

core::CommandProcedure pavementQuantityTableCommandProcedure()
{
    return &runPavementQuantityTableCommand;
}

} // namespace roadproto::cad_adapter::objectarx::drawing_quantity
