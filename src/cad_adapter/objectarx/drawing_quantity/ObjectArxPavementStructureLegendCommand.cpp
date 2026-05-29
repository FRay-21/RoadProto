#include "cad_adapter/objectarx/drawing_quantity/ObjectArxPavementStructureLegendCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/cross_section/DnPavementLayerTemplateEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelEntity.h"
#include "cad_adapter/objectarx/cross_section/DnRoadModelSectionDrawingEntity.h"
#include "domain/quantity/PavementStructureLegend.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbcolor.h"
#include "dbhatch.h"
#include "dbpl.h"
#include "dbsymtb.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#endif

namespace roadproto::cad_adapter::objectarx::drawing_quantity {
namespace {

#ifndef ROADPROTO_TEST_BUILD

using roadproto::cad_adapter::objectarx::SelectionSetGuard;
using roadproto::cad_adapter::objectarx::cross_section::RoadModelSectionDrawingData;
using roadproto::domain::cross_section::RoadModelData;
using roadproto::domain::quantity::PavementStructureLegendColumnPlan;
using roadproto::domain::quantity::PavementStructureLegendItemPlan;
using roadproto::domain::quantity::PavementStructureLegendLayerPlan;
using roadproto::domain::quantity::PavementStructureLegendPlan;
using roadproto::domain::quantity::PavementStructureLegendPlanner;
using roadproto::domain::quantity::PavementStructureLegendTemplateSource;

constexpr double kNumberTolerance = 1.0e-9;
constexpr double kTopRowHeight = 6.0;
constexpr double kTotalThicknessRowHeight = 7.0;
constexpr double kLegendRowHeight = 24.0;
constexpr double kLegendItemWidth = 56.0;
constexpr double kStructureVerticalMargin = 6.0;
constexpr double kMinimumStructureRowHeight = 42.0;
constexpr double kTextHeight = 2.5;
constexpr double kSmallTextHeight = 2.1;
constexpr double kLegendSampleHeight = 8.0;

struct LegendSourceSelection {
    bool fromSectionDrawing = false;
    bool hasRoadModelData = false;
    std::wstring roadModelHandle;
    RoadModelData roadModelData;
    std::vector<AcDbObjectId> sectionDrawingIds;
};

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

bool appendEntityOpenToModelSpace(AcDbEntity* entity, AcDbObjectId& entityId)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr || entity == nullptr) {
        return false;
    }

    entity->setDatabaseDefaults(database);

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

bool appendAndCloseEntity(AcDbEntity* entity, AcDbObjectId* appendedId = nullptr)
{
    AcDbObjectId entityId;
    if (!appendEntityOpenToModelSpace(entity, entityId)) {
        delete entity;
        return false;
    }

    entity->close();
    if (appendedId != nullptr) {
        *appendedId = entityId;
    }
    return true;
}

AcCmColor indexedColor(Adesk::UInt16 colorIndex)
{
    AcCmColor color;
    color.setColorIndex(colorIndex);
    return color;
}

AcCmColor rgbColor(int r, int g, int b, Adesk::UInt16 fallbackColorIndex = 7)
{
    if (r < 0 || g < 0 || b < 0) {
        return indexedColor(fallbackColorIndex);
    }

    AcCmColor color;
    color.setRGB(
        static_cast<Adesk::UInt8>(std::clamp(r, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(g, 0, 255)),
        static_cast<Adesk::UInt8>(std::clamp(b, 0, 255)));
    return color;
}

std::wstring formatCentimeters(double value)
{
    if (!std::isfinite(value)) {
        return L"";
    }

    const auto rounded = std::round(value);
    std::wostringstream stream;
    stream.imbue(std::locale::classic());
    if (std::fabs(value - rounded) <= kNumberTolerance) {
        stream << static_cast<long long>(rounded);
    } else {
        stream << std::fixed << std::setprecision(1) << value;
    }
    return stream.str();
}

double maxStructureHeight(const PavementStructureLegendPlan& plan)
{
    double result = 0.0;
    for (const auto& column : plan.columns) {
        result = std::max(result, column.totalThicknessCm);
    }
    return result;
}

void appendUniqueHandle(std::vector<std::wstring>& handles, std::set<std::wstring>& seen, const std::wstring& handle)
{
    if (handle.empty() || seen.find(handle) != seen.end()) {
        return;
    }
    seen.insert(handle);
    handles.push_back(handle);
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

bool readPavementLayerTemplate(
    const std::wstring& handle,
    PavementStructureLegendTemplateSource& source)
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

    source.handle = handle;
    source.data = entity->templateData();
    entity->close();
    return true;
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

std::vector<std::wstring> collectTemplateHandlesFromSectionDrawings(const std::vector<AcDbObjectId>& sectionDrawingIds)
{
    std::vector<std::wstring> handles;
    std::set<std::wstring> seen;
    for (const auto& drawingId : sectionDrawingIds) {
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

        for (const auto& face : data.faces) {
            appendUniqueHandle(handles, seen, face.sourceTemplateHandle);
        }
        for (const auto& row : data.config.pavementRows) {
            appendUniqueHandle(handles, seen, row.templateHandle);
        }
    }
    return handles;
}

std::vector<std::wstring> collectTemplateHandlesFromRoadModel(
    const roadproto::domain::cross_section::RoadModelData& roadModelData)
{
    std::vector<std::wstring> handles;
    std::set<std::wstring> seen;
    for (const auto& line : roadModelData.pavementLayerLines) {
        appendUniqueHandle(handles, seen, line.key.pavementLayerTemplateHandle);
    }
    return handles;
}

bool fillRoadModelSelection(AcDbObjectId entityId, LegendSourceSelection& selection)
{
    DnRoadModelEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }
    if (!entity->isKindOf(DnRoadModelEntity::desc())) {
        entity->close();
        return false;
    }

    selection.fromSectionDrawing = false;
    selection.roadModelHandle = entityHandleText(entity);
    selection.roadModelData = entity->roadModelData();
    selection.hasRoadModelData = true;
    entity->close();

    selection.sectionDrawingIds = collectSectionDrawingsForRoadModel(selection.roadModelHandle);
    return !selection.roadModelHandle.empty();
}

bool fillSectionDrawingSelection(AcDbObjectId entityId, LegendSourceSelection& selection)
{
    DnRoadModelSectionDrawingEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        return false;
    }
    if (!entity->isKindOf(DnRoadModelSectionDrawingEntity::desc())) {
        entity->close();
        return false;
    }

    const auto drawingData = entity->drawingData();
    entity->close();
    if (drawingData.roadModelHandle.empty()) {
        return false;
    }

    selection.fromSectionDrawing = true;
    selection.roadModelHandle = drawingData.roadModelHandle;
    selection.hasRoadModelData = readRoadModelDataByHandle(selection.roadModelHandle, selection.roadModelData);
    selection.sectionDrawingIds = collectSectionDrawingsForRoadModel(selection.roadModelHandle);
    if (selection.sectionDrawingIds.empty()) {
        selection.sectionDrawingIds.push_back(entityId);
    }
    return true;
}

bool findLegendSourceInSelectionSet(const ads_name selectionSet, LegendSourceSelection& selection)
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

        if (fillRoadModelSelection(candidateId, selection) || fillSectionDrawingSelection(candidateId, selection)) {
            return true;
        }
    }

    return false;
}

bool selectRoadModelOrSectionDrawing(
    LegendSourceSelection& selection,
    roadproto::cad_adapter::IEditor& editor)
{
    ads_name impliedSelection;
    if (acedSSGet(L"_I", nullptr, nullptr, nullptr, impliedSelection) == RTNORM) {
        SelectionSetGuard guard(impliedSelection);
        if (findLegendSourceInSelectionSet(impliedSelection, selection)) {
            return true;
        }
    }

    editor.writeMessage(L"请选择道路模型或一个横断面图。");
    ads_name selectionSet;
    if (acedSSGet(nullptr, nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }

    SelectionSetGuard guard(selectionSet);
    return findLegendSourceInSelectionSet(selectionSet, selection);
}

std::vector<std::wstring> collectTemplateHandlesForSelection(const LegendSourceSelection& selection)
{
    std::vector<std::wstring> handles;
    if (selection.fromSectionDrawing) {
        handles = collectTemplateHandlesFromSectionDrawings(selection.sectionDrawingIds);
        if (handles.empty() && selection.hasRoadModelData) {
            handles = collectTemplateHandlesFromRoadModel(selection.roadModelData);
        }
    } else if (selection.hasRoadModelData) {
        handles = collectTemplateHandlesFromRoadModel(selection.roadModelData);
        if (handles.empty()) {
            handles = collectTemplateHandlesFromSectionDrawings(selection.sectionDrawingIds);
        }
    }
    return handles;
}

bool appendLine(
    const AcGePoint3d& startPoint,
    const AcGePoint3d& endPoint,
    const AcCmColor& color)
{
    auto* line = new AcDbLine(startPoint, endPoint);
    line->setColor(color);
    return appendAndCloseEntity(line);
}

bool appendText(
    const AcGePoint3d& center,
    const std::wstring& text,
    double height,
    const AcCmColor& color)
{
    if (text.empty() || height <= 0.0 || !std::isfinite(height)) {
        return true;
    }

    auto* entity = new AcDbText(center, text.c_str(), AcDbObjectId::kNull, height, 0.0);
    entity->setColor(color);
    entity->setHorizontalMode(AcDb::kTextCenter);
    entity->setVerticalMode(AcDb::kTextVertMid);
    entity->setAlignmentPoint(center);
    return appendAndCloseEntity(entity);
}

AcDbPolyline* makeRectanglePolyline(
    double left,
    double bottom,
    double right,
    double top,
    const AcCmColor& color)
{
    auto* polyline = new AcDbPolyline(4);
    polyline->addVertexAt(0, AcGePoint2d(left, bottom));
    polyline->addVertexAt(1, AcGePoint2d(right, bottom));
    polyline->addVertexAt(2, AcGePoint2d(right, top));
    polyline->addVertexAt(3, AcGePoint2d(left, top));
    polyline->setClosed(Adesk::kTrue);
    polyline->setColor(color);
    return polyline;
}

bool appendRectangle(
    double left,
    double bottom,
    double right,
    double top,
    const AcCmColor& color,
    AcDbObjectId* boundaryId = nullptr)
{
    if (right <= left || top <= bottom) {
        return true;
    }

    return appendAndCloseEntity(makeRectanglePolyline(left, bottom, right, top, color), boundaryId);
}

bool appendHatchForBoundary(
    AcDbObjectId boundaryId,
    const PavementStructureLegendItemPlan& style)
{
    auto* hatch = new AcDbHatch();
    hatch->setColor(rgbColor(style.colorR, style.colorG, style.colorB));
    hatch->setPatternAngle(style.hatchAngle);
    hatch->setPatternScale(std::max(style.hatchScale, 0.1));
    const auto pattern = style.hatchPattern.empty() ? L"SOLID" : style.hatchPattern;
    hatch->setPattern(AcDbHatch::kPreDefined, pattern.c_str());
    hatch->setAssociative(Adesk::kFalse);

    AcDbObjectId hatchId;
    if (!appendEntityOpenToModelSpace(hatch, hatchId)) {
        delete hatch;
        return false;
    }

    AcDbObjectIdArray boundaryIds;
    boundaryIds.append(boundaryId);
    if (hatch->appendLoop(AcDbHatch::kExternal, boundaryIds) == Acad::eOk) {
        hatch->evaluateHatch();
    }
    hatch->close();
    return true;
}

PavementStructureLegendItemPlan styleFromLayer(const PavementStructureLegendLayerPlan& layer)
{
    PavementStructureLegendItemPlan item;
    item.colorR = layer.colorR;
    item.colorG = layer.colorG;
    item.colorB = layer.colorB;
    item.hatchPattern = layer.hatchPattern;
    item.hatchAngle = layer.hatchAngle;
    item.hatchScale = layer.hatchScale;
    return item;
}

bool appendFilledRectangle(
    double left,
    double bottom,
    double right,
    double top,
    const PavementStructureLegendItemPlan& style)
{
    AcDbObjectId boundaryId;
    if (!appendRectangle(left, bottom, right, top, indexedColor(7), &boundaryId)) {
        return false;
    }
    const auto hatchOk = appendHatchForBoundary(boundaryId, style);
    const auto outlineOk = appendRectangle(left, bottom, right, top, indexedColor(7), nullptr);
    return hatchOk && outlineOk;
}

void appendGrid(
    const AcGePoint3d& insertionPoint,
    const PavementStructureLegendPlan& plan,
    double tableWidth,
    double structureRowHeight)
{
    const auto lineColor = indexedColor(7);
    const auto headerWidth = plan.layout.headerColumnWidth;
    const auto columnWidth = plan.layout.columnWidth;
    const auto top = insertionPoint.y;
    const auto left = insertionPoint.x;
    const auto right = left + tableWidth;

    std::vector<double> rowHeights = {
        kTopRowHeight,
        kTopRowHeight,
        kTopRowHeight,
        kTopRowHeight,
        kTopRowHeight,
        structureRowHeight,
        kTotalThicknessRowHeight,
        kLegendRowHeight,
    };

    double y = top;
    appendLine(AcGePoint3d(left, y, insertionPoint.z), AcGePoint3d(right, y, insertionPoint.z), lineColor);
    for (const auto height : rowHeights) {
        y -= height;
        appendLine(AcGePoint3d(left, y, insertionPoint.z), AcGePoint3d(right, y, insertionPoint.z), lineColor);
    }

    const auto legendTop = top
        - kTopRowHeight * 5.0
        - structureRowHeight
        - kTotalThicknessRowHeight;
    appendLine(AcGePoint3d(left, top, insertionPoint.z), AcGePoint3d(left, y, insertionPoint.z), lineColor);
    appendLine(AcGePoint3d(right, top, insertionPoint.z), AcGePoint3d(right, y, insertionPoint.z), lineColor);
    appendLine(
        AcGePoint3d(left + headerWidth, top, insertionPoint.z),
        AcGePoint3d(left + headerWidth, y, insertionPoint.z),
        lineColor);

    const auto topTableRight = left + headerWidth + columnWidth * static_cast<double>(plan.columns.size());
    for (std::size_t i = 1; i <= plan.columns.size(); ++i) {
        const auto x = left + headerWidth + columnWidth * static_cast<double>(i);
        appendLine(AcGePoint3d(x, top, insertionPoint.z), AcGePoint3d(x, legendTop, insertionPoint.z), lineColor);
    }
    if (topTableRight < right - kNumberTolerance) {
        appendLine(AcGePoint3d(topTableRight, top, insertionPoint.z), AcGePoint3d(topTableRight, legendTop, insertionPoint.z), lineColor);
    }

    for (std::size_t i = 1; i <= plan.legendItems.size(); ++i) {
        const auto x = left + headerWidth + kLegendItemWidth * static_cast<double>(i);
        if (x < right - kNumberTolerance) {
            appendLine(AcGePoint3d(x, legendTop, insertionPoint.z), AcGePoint3d(x, y, insertionPoint.z), lineColor);
        }
    }
}

void appendColumnTexts(
    const AcGePoint3d& insertionPoint,
    const PavementStructureLegendPlan& plan,
    double structureRowHeight)
{
    const auto textColor = indexedColor(2);
    const auto headerWidth = plan.layout.headerColumnWidth;
    const auto columnWidth = plan.layout.columnWidth;
    const auto left = insertionPoint.x;
    const auto top = insertionPoint.y;
    const auto z = insertionPoint.z;

    const std::vector<std::wstring> rowLabels = {
        L"路基土组",
        L"路基干湿类型",
        L"设计弯沉",
        L"累计当量轴次",
        L"结构代号",
    };
    for (std::size_t row = 0; row < rowLabels.size(); ++row) {
        const auto centerY = top - kTopRowHeight * (static_cast<double>(row) + 0.5);
        appendText(AcGePoint3d(left + headerWidth * 0.5, centerY, z), rowLabels[row], kSmallTextHeight, textColor);
    }

    const auto structureCenterY = top - kTopRowHeight * 5.0 - structureRowHeight * 0.5;
    appendText(AcGePoint3d(left + headerWidth * 0.5, structureCenterY, z), L"结构图示", kTextHeight, textColor);

    const auto totalCenterY = top - kTopRowHeight * 5.0 - structureRowHeight - kTotalThicknessRowHeight * 0.5;
    appendText(AcGePoint3d(left + headerWidth * 0.5, totalCenterY, z), L"路面总厚度(cm)", kSmallTextHeight, textColor);

    const auto legendCenterY = top
        - kTopRowHeight * 5.0
        - structureRowHeight
        - kTotalThicknessRowHeight
        - kLegendRowHeight * 0.5;
    appendText(AcGePoint3d(left + headerWidth * 0.5, legendCenterY, z), L"图例", kTextHeight, textColor);

    for (std::size_t index = 0; index < plan.columns.size(); ++index) {
        const auto& column = plan.columns[index];
        const auto centerX = left + headerWidth + columnWidth * (static_cast<double>(index) + 0.5);
        const std::vector<std::wstring> values = {
            column.subgradeSoilGroupText,
            column.subgradeMoistureText,
            column.designDeflection,
            column.cumulativeAxleLoads,
            column.structureCode,
        };
        for (std::size_t row = 0; row < values.size(); ++row) {
            const auto centerY = top - kTopRowHeight * (static_cast<double>(row) + 0.5);
            appendText(AcGePoint3d(centerX, centerY, z), values[row], kTextHeight, textColor);
        }
        appendText(AcGePoint3d(centerX, totalCenterY, z), formatCentimeters(column.totalThicknessCm), kTextHeight, textColor);
    }
}

void appendStructureGraphics(
    const AcGePoint3d& insertionPoint,
    const PavementStructureLegendPlan& plan,
    double structureRowHeight)
{
    const auto textColor = indexedColor(2);
    const auto headerWidth = plan.layout.headerColumnWidth;
    const auto columnWidth = plan.layout.columnWidth;
    const auto graphicWidth = plan.layout.structureGraphicWidthCm;
    const auto left = insertionPoint.x;
    const auto top = insertionPoint.y;
    const auto z = insertionPoint.z;
    const auto structureTop = top - kTopRowHeight * 5.0;

    for (std::size_t index = 0; index < plan.columns.size(); ++index) {
        const auto& column = plan.columns[index];
        const auto columnLeft = left + headerWidth + columnWidth * static_cast<double>(index);
        const auto graphicLeft = columnLeft + (columnWidth - graphicWidth) * 0.5;
        const auto graphicRight = graphicLeft + graphicWidth;
        const auto graphicTop = structureTop - (structureRowHeight - column.totalThicknessCm) * 0.5;

        double cursorY = graphicTop;
        for (const auto& layer : column.layers) {
            const auto layerHeight = std::max(layer.displayThicknessCm, 0.0);
            if (layerHeight <= kNumberTolerance) {
                continue;
            }

            const auto layerBottom = cursorY - layerHeight;
            appendFilledRectangle(graphicLeft, layerBottom, graphicRight, cursorY, styleFromLayer(layer));

            const auto thicknessX = std::min(graphicRight + 4.0, columnLeft + columnWidth - 2.0);
            appendText(
                AcGePoint3d(thicknessX, (cursorY + layerBottom) * 0.5, z),
                layer.thicknessText,
                kSmallTextHeight,
                textColor);
            cursorY = layerBottom;
        }
    }
}

void appendLegendRow(
    const AcGePoint3d& insertionPoint,
    const PavementStructureLegendPlan& plan,
    double structureRowHeight)
{
    const auto textColor = indexedColor(2);
    const auto headerWidth = plan.layout.headerColumnWidth;
    const auto graphicWidth = plan.layout.structureGraphicWidthCm;
    const auto left = insertionPoint.x;
    const auto top = insertionPoint.y
        - kTopRowHeight * 5.0
        - structureRowHeight
        - kTotalThicknessRowHeight;
    const auto z = insertionPoint.z;

    for (std::size_t index = 0; index < plan.legendItems.size(); ++index) {
        const auto& item = plan.legendItems[index];
        const auto cellLeft = left + headerWidth + kLegendItemWidth * static_cast<double>(index);
        const auto cellCenterX = cellLeft + kLegendItemWidth * 0.5;
        const auto sampleLeft = cellCenterX - graphicWidth * 0.5;
        const auto sampleRight = cellCenterX + graphicWidth * 0.5;
        const auto sampleTop = top - 4.0;
        const auto sampleBottom = sampleTop - kLegendSampleHeight;

        appendFilledRectangle(sampleLeft, sampleBottom, sampleRight, sampleTop, item);
        appendText(
            AcGePoint3d(cellCenterX, top - kLegendRowHeight + 5.0, z),
            item.layerName,
            kSmallTextHeight,
            textColor);
    }
}

bool appendOrdinaryLegendEntities(
    const PavementStructureLegendPlan& plan,
    const AcGePoint3d& insertionPoint,
    roadproto::cad_adapter::IEditor& editor)
{
    if (plan.columns.empty()) {
        editor.writeWarning(L"未找到可绘制的路面结构层模板。");
        return false;
    }

    const auto structureHeight = maxStructureHeight(plan);
    const auto structureRowHeight = std::max(kMinimumStructureRowHeight, structureHeight + kStructureVerticalMargin * 2.0);
    const auto templateTableWidth = plan.layout.headerColumnWidth
        + plan.layout.columnWidth * static_cast<double>(plan.columns.size());
    const auto legendTableWidth = plan.layout.headerColumnWidth
        + kLegendItemWidth * static_cast<double>(plan.legendItems.size());
    const auto tableWidth = std::max(templateTableWidth, legendTableWidth);

    appendGrid(insertionPoint, plan, tableWidth, structureRowHeight);
    appendColumnTexts(insertionPoint, plan, structureRowHeight);
    appendStructureGraphics(insertionPoint, plan, structureRowHeight);
    appendLegendRow(insertionPoint, plan, structureRowHeight);
    acedUpdateDisplay();
    return true;
}

void runPavementStructureLegendCommand()
{
    auto& editor = roadproto::app::ApplicationContext::instance().editor();

    LegendSourceSelection selection;
    if (!selectRoadModelOrSectionDrawing(selection, editor)) {
        editor.writeWarning(L"未选择道路模型或横断面图。");
        return;
    }

    const auto handles = collectTemplateHandlesForSelection(selection);
    if (handles.empty()) {
        editor.writeWarning(L"未在选中道路中找到路面结构层模板。");
        return;
    }

    std::vector<PavementStructureLegendTemplateSource> sources;
    for (const auto& handle : handles) {
        PavementStructureLegendTemplateSource source;
        if (readPavementLayerTemplate(handle, source)) {
            sources.push_back(std::move(source));
        }
    }
    if (sources.empty()) {
        editor.writeWarning(L"路面结构层模板实体读取失败。");
        return;
    }

    ads_point rawPoint;
    if (acedGetPoint(nullptr, L"\n请选择路面结构图例插入位置: ", rawPoint) != RTNORM) {
        editor.writeWarning(L"已取消路面结构图例插入位置选择。");
        return;
    }

    const AcGePoint3d insertionPoint(rawPoint[0], rawPoint[1], rawPoint[2]);
    const auto plan = PavementStructureLegendPlanner::build(sources);
    if (!appendOrdinaryLegendEntities(plan, insertionPoint, editor)) {
        return;
    }

    std::wostringstream message;
    message << L"路面结构图例已绘制，结构模板 " << plan.columns.size()
            << L" 列，图例项 " << plan.legendItems.size() << L" 项。";
    editor.writeMessage(message.str());
}

#else

void runPavementStructureLegendCommand()
{
}

#endif

} // namespace

core::CommandProcedure pavementStructureLegendCommandProcedure()
{
    return &runPavementStructureLegendCommand;
}

} // namespace roadproto::cad_adapter::objectarx::drawing_quantity
