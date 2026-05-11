#include "cad_adapter/objectarx/terrain/ObjectArxTerrainTinCommand.h"

#include "app/startup/ApplicationContext.h"
#include "application/terrain/TerrainTinCreateService.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/terrain/DnTerrainTinEntity.h"
#include "cad_adapter/objectarx/terrain/TerrainTinCreateDialog.h"
#include "domain/terrain/TerrainPointNormalizer.h"
#include "domain/terrain/TerrainMeshFile.h"
#include "domain/terrain/TerrainTextElevationParser.h"
#include "domain/terrain/TerrainTinBuilder.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "Windows.h"
#include "aced.h"
#include "acutads.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbents.h"
#include "dbsymtb.h"
#include "geassign.h"
#include "gepnt2d.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <filesystem>
#include <memory>
#include <sstream>
#include <unordered_set>
#include <utility>
#include <vector>

int acedSetStatusBarProgressMeter(const ACHAR* pszLabel, int nMinPos, int nMaxPos);
int acedSetStatusBarProgressMeterPos(int nPos);
void acedRestoreStatusBar();

using roadproto::domain::terrain::TerrainTextElevationParser;
using roadproto::domain::terrain::TerrainMeshFile;
using roadproto::domain::terrain::TerrainPointNormalizer;
using roadproto::domain::terrain::TerrainTinBuilder;
using roadproto::domain::terrain::TerrainTinDisplayMode;
using roadproto::domain::terrain::TinBuildOptions;
using roadproto::domain::terrain::TinBuildResult;
using roadproto::domain::terrain::TinExtractOptions;
using roadproto::domain::terrain::TinExtractSummary;
using roadproto::domain::terrain::TinPoint;
using roadproto::domain::terrain::TinPointSourceType;

namespace roadproto::cad_adapter::objectarx {

namespace {

struct ExtractionContext {
    TinExtractOptions options;
    TinExtractSummary summary;
    std::vector<TinPoint> points;
    TerrainTextElevationParser parser;
};

class StatusProgressMeter {
public:
    StatusProgressMeter(const ACHAR* label, int minPosition, int maxPosition)
        : active_(acedSetStatusBarProgressMeter(label, minPosition, maxPosition) == 0)
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
        if (!active_ || position == lastPosition_) {
            return;
        }
        lastPosition_ = position;
        acedSetStatusBarProgressMeterPos(position);
    }

private:
    bool active_ = false;
    int lastPosition_ = 0;
};

std::wstring handleOf(const AcDbObject* object)
{
    if (object == nullptr) {
        return L"";
    }

    AcDbHandle handle;
    object->getAcDbHandle(handle);
    ACHAR buffer[32] = {};
    handle.getIntoAsciiBuffer(buffer);
    return buffer;
}

std::wstring toUpper(std::wstring value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towupper(ch));
    });
    return value;
}

bool isElevationTag(const std::wstring& tag)
{
    const auto upper = toUpper(tag);
    return upper == L"H"
        || upper == L"ELEV"
        || upper == L"EL"
        || upper == L"Z"
        || tag == L"高程"
        || tag == L"标高"
        || tag == L"设计高程";
}

std::wstring blockName(const AcDbObjectId& blockRecordId)
{
    AcDbBlockTableRecord* record = nullptr;
    if (acdbOpenObject(record, blockRecordId, AcDb::kForRead) != Acad::eOk || record == nullptr) {
        return L"";
    }

    AcString name;
    record->getName(name);
    record->close();
    return static_cast<const ACHAR*>(name);
}

std::wstring entityLayer(const AcDbEntity* entity)
{
    return entity == nullptr || entity->layer() == nullptr ? L"" : entity->layer();
}

std::wstring entityClassName(const AcDbEntity* entity)
{
    if (entity == nullptr || entity->isA() == nullptr || entity->isA()->name() == nullptr) {
        return L"Unknown";
    }
    return entity->isA()->name();
}

std::wstring entityDisplayType(const AcDbEntity* entity)
{
    if (AcDbPoint::cast(entity) != nullptr) {
        return L"AcDbPoint";
    }
    if (AcDbLine::cast(entity) != nullptr) {
        return L"AcDbLine";
    }
    if (AcDbPolyline::cast(entity) != nullptr) {
        return L"AcDbPolyline";
    }
    if (AcDb2dPolyline::cast(entity) != nullptr) {
        return L"AcDb2dPolyline";
    }
    if (AcDb3dPolyline::cast(entity) != nullptr) {
        return L"AcDb3dPolyline";
    }
    if (AcDbText::cast(entity) != nullptr) {
        return L"AcDbText";
    }
    if (AcDbMText::cast(entity) != nullptr) {
        return L"AcDbMText";
    }
    if (AcDbBlockReference::cast(entity) != nullptr) {
        return L"AcDbBlockReference";
    }
    return entityClassName(entity);
}

bool isCoreConsole()
{
    wchar_t path[MAX_PATH] = {};
    if (GetModuleFileNameW(nullptr, path, MAX_PATH) == 0) {
        return false;
    }

    auto value = toUpper(path);
    return value.find(L"ACCORECONSOLE.EXE") != std::wstring::npos;
}

void appendPoint(
    ExtractionContext& context,
    const AcGePoint3d& point,
    bool hasValidElevation,
    TinPointSourceType sourceType,
    const std::wstring& sourceHandle,
    const std::wstring& blockName = L"",
    const std::wstring& attributeTag = L"")
{
    TinPoint tinPoint;
    tinPoint.x = point.x;
    tinPoint.y = point.y;
    tinPoint.z = hasValidElevation ? point.z : 0.0;
    tinPoint.hasValidElevation = hasValidElevation;
    tinPoint.sourceType = sourceType;
    tinPoint.sourceObjectHandle = sourceHandle;
    tinPoint.blockName = blockName;
    tinPoint.attributeTag = attributeTag;
    context.points.push_back(std::move(tinPoint));
}

AcGePoint3d transformPoint(const AcGePoint3d& point, const AcGeMatrix3d& transform)
{
    auto transformed = point;
    transformed.transformBy(transform);
    return transformed;
}

bool appendTextPoint(
    ExtractionContext& context,
    const std::wstring& text,
    const AcGePoint3d& position,
    const AcGeMatrix3d& transform,
    const std::wstring& handle,
    TinPointSourceType sourceType,
    const std::wstring& blockName = L"")
{
    const auto elevation = context.parser.tryParse(text);
    if (!elevation.has_value()) {
        ++context.summary.invalidObjectCount;
        return false;
    }

    auto point = transformPoint(position, transform);
    point.z = *elevation;
    appendPoint(context, point, true, sourceType, handle, blockName);
    return true;
}

void extractEntity(
    AcDbEntity* entity,
    const AcGeMatrix3d& transform,
    int blockDepth,
    ExtractionContext& context,
    std::vector<TinPoint>* textBucket,
    std::vector<TinPoint>* geometryBucket);

void appendToPreferredBucket(
    ExtractionContext& context,
    const std::vector<TinPoint>& extracted,
    std::vector<TinPoint>* bucket)
{
    if (bucket == nullptr) {
        context.points.insert(context.points.end(), extracted.begin(), extracted.end());
        return;
    }
    bucket->insert(bucket->end(), extracted.begin(), extracted.end());
}

bool extractBlockAttributeElevation(
    AcDbBlockReference* blockReference,
    const AcGeMatrix3d& transform,
    const std::wstring& sourceHandle,
    const std::wstring& name,
    ExtractionContext& context)
{
    auto recognized = false;
    auto hasConflict = false;
    auto chosenElevation = 0.0;
    std::wstring chosenTag;

    std::unique_ptr<AcDbObjectIterator> attributeIterator(blockReference->attributeIterator());
    if (!attributeIterator) {
        return false;
    }

    for (; !attributeIterator->done(); attributeIterator->step()) {
        AcDbAttribute* attribute = nullptr;
        if (acdbOpenObject(attribute, attributeIterator->objectId(), AcDb::kForRead) != Acad::eOk || attribute == nullptr) {
            continue;
        }

        const std::wstring tag = attribute->tagConst() == nullptr ? L"" : attribute->tagConst();
        const std::wstring text = attribute->textStringConst() == nullptr ? L"" : attribute->textStringConst();
        const auto parsed = isElevationTag(tag) ? context.parser.tryParse(text) : std::nullopt;
        if (parsed.has_value()) {
            if (recognized && std::fabs(chosenElevation - *parsed) > context.options.zEqualTolerance) {
                hasConflict = true;
            }
            recognized = true;
            chosenElevation = *parsed;
            chosenTag = tag;
        }

        attribute->close();
    }

    if (!recognized) {
        return false;
    }

    auto point = transformPoint(blockReference->position(), transform);
    point.z = chosenElevation;
    appendPoint(context, point, true, TinPointSourceType::BlockAttribute, sourceHandle, name, chosenTag);
    ++context.summary.recognizedElevationBlockCount;
    ++context.summary.blockAttributeElevationCount;
    if (hasConflict) {
        ++context.summary.zConflictPointCount;
    }
    return true;
}

void extractNestedBlockContent(
    AcDbBlockReference* blockReference,
    const AcGeMatrix3d& transform,
    int blockDepth,
    ExtractionContext& context)
{
    if (!context.options.enableNestedBlockExtraction || blockDepth >= context.options.maxNestedBlockDepth) {
        ++context.summary.blockParseFailedCount;
        return;
    }

    AcDbBlockTableRecord* record = nullptr;
    if (acdbOpenObject(record, blockReference->blockTableRecord(), AcDb::kForRead) != Acad::eOk || record == nullptr) {
        ++context.summary.blockParseFailedCount;
        return;
    }

    AcDbBlockTableRecordIterator* iterator = nullptr;
    if (record->newIterator(iterator) != Acad::eOk || iterator == nullptr) {
        record->close();
        ++context.summary.blockParseFailedCount;
        return;
    }

    std::vector<TinPoint> textPoints;
    std::vector<TinPoint> geometryPoints;
    const auto childTransform = transform * blockReference->blockTransform();
    for (; !iterator->done(); iterator->step()) {
        AcDbEntity* child = nullptr;
        if (iterator->getEntity(child, AcDb::kForRead) == Acad::eOk && child != nullptr) {
            extractEntity(child, childTransform, blockDepth + 1, context, &textPoints, &geometryPoints);
            child->close();
        }
    }

    delete iterator;
    record->close();

    if (!textPoints.empty()) {
        context.points.insert(context.points.end(), textPoints.begin(), textPoints.end());
    } else if (!geometryPoints.empty()) {
        context.points.insert(context.points.end(), geometryPoints.begin(), geometryPoints.end());
    } else {
        ++context.summary.blockParseFailedCount;
    }
}

void extractPolyline2dVertices(
    AcDb2dPolyline* polyline,
    const AcGeMatrix3d& transform,
    ExtractionContext& context,
    std::vector<TinPoint>* geometryBucket)
{
    std::vector<TinPoint> extracted;
    const auto elevation = polyline->elevation();
    const auto hasElevation = std::fabs(elevation) > context.options.zEqualTolerance;
    std::unique_ptr<AcDbObjectIterator> vertexIterator(polyline->vertexIterator());
    for (; vertexIterator && !vertexIterator->done(); vertexIterator->step()) {
        AcDb2dVertex* vertex = nullptr;
        if (polyline->openVertex(vertex, vertexIterator->objectId(), AcDb::kForRead) != Acad::eOk || vertex == nullptr) {
            continue;
        }

        auto position = vertex->position();
        position.z = elevation;
        TinPoint point;
        const auto transformed = transformPoint(position, transform);
        point.x = transformed.x;
        point.y = transformed.y;
        point.z = hasElevation ? transformed.z : 0.0;
        point.hasValidElevation = hasElevation;
        point.sourceType = TinPointSourceType::PolylineVertex;
        point.sourceObjectHandle = handleOf(polyline);
        extracted.push_back(std::move(point));
        vertex->close();
    }
    appendToPreferredBucket(context, extracted, geometryBucket);
}

void extractPolyline3dVertices(
    AcDb3dPolyline* polyline,
    const AcGeMatrix3d& transform,
    ExtractionContext& context,
    std::vector<TinPoint>* geometryBucket)
{
    std::vector<TinPoint> extracted;
    std::unique_ptr<AcDbObjectIterator> vertexIterator(polyline->vertexIterator());
    for (; vertexIterator && !vertexIterator->done(); vertexIterator->step()) {
        AcDb3dPolylineVertex* vertex = nullptr;
        if (polyline->openVertex(vertex, vertexIterator->objectId(), AcDb::kForRead) != Acad::eOk || vertex == nullptr) {
            continue;
        }

        const auto transformed = transformPoint(vertex->position(), transform);
        TinPoint point;
        point.x = transformed.x;
        point.y = transformed.y;
        point.z = transformed.z;
        point.hasValidElevation = true;
        point.sourceType = TinPointSourceType::PolylineVertex;
        point.sourceObjectHandle = handleOf(polyline);
        extracted.push_back(std::move(point));
        vertex->close();
    }
    appendToPreferredBucket(context, extracted, geometryBucket);
}

void extractEntity(
    AcDbEntity* entity,
    const AcGeMatrix3d& transform,
    int blockDepth,
    ExtractionContext& context,
    std::vector<TinPoint>* textBucket,
    std::vector<TinPoint>* geometryBucket)
{
    const auto handle = handleOf(entity);
    if (auto point = AcDbPoint::cast(entity)) {
        std::vector<TinPoint> extracted;
        TinPoint tinPoint;
        const auto transformed = transformPoint(point->position(), transform);
        tinPoint.x = transformed.x;
        tinPoint.y = transformed.y;
        tinPoint.z = transformed.z;
        tinPoint.hasValidElevation = true;
        tinPoint.sourceType = TinPointSourceType::CadPoint;
        tinPoint.sourceObjectHandle = handle;
        extracted.push_back(std::move(tinPoint));
        appendToPreferredBucket(context, extracted, geometryBucket);
        return;
    }

    if (auto line = AcDbLine::cast(entity)) {
        std::vector<TinPoint> extracted;
        for (const auto& position : {line->startPoint(), line->endPoint()}) {
            TinPoint tinPoint;
            const auto transformed = transformPoint(position, transform);
            tinPoint.x = transformed.x;
            tinPoint.y = transformed.y;
            tinPoint.z = transformed.z;
            tinPoint.hasValidElevation = true;
            tinPoint.sourceType = TinPointSourceType::LineVertex;
            tinPoint.sourceObjectHandle = handle;
            extracted.push_back(std::move(tinPoint));
        }
        appendToPreferredBucket(context, extracted, geometryBucket);
        return;
    }

    if (auto polyline = AcDbPolyline::cast(entity)) {
        std::vector<TinPoint> extracted;
        const auto elevation = polyline->elevation();
        const auto hasElevation = std::fabs(elevation) > context.options.zEqualTolerance;
        for (unsigned int i = 0; i < polyline->numVerts(); ++i) {
            AcGePoint2d point2d;
            if (polyline->getPointAt(i, point2d) != Acad::eOk) {
                continue;
            }
            const auto transformed = transformPoint(AcGePoint3d(point2d.x, point2d.y, elevation), transform);
            TinPoint tinPoint;
            tinPoint.x = transformed.x;
            tinPoint.y = transformed.y;
            tinPoint.z = hasElevation ? transformed.z : 0.0;
            tinPoint.hasValidElevation = hasElevation;
            tinPoint.sourceType = TinPointSourceType::PolylineVertex;
            tinPoint.sourceObjectHandle = handle;
            extracted.push_back(std::move(tinPoint));
        }
        appendToPreferredBucket(context, extracted, geometryBucket);
        return;
    }

    if (auto polyline2d = AcDb2dPolyline::cast(entity)) {
        extractPolyline2dVertices(polyline2d, transform, context, geometryBucket);
        return;
    }

    if (auto polyline3d = AcDb3dPolyline::cast(entity)) {
        extractPolyline3dVertices(polyline3d, transform, context, geometryBucket);
        return;
    }

    if (auto text = AcDbText::cast(entity)) {
        const auto before = context.points.size();
        appendTextPoint(
            context,
            text->textStringConst() == nullptr ? L"" : text->textStringConst(),
            text->position(),
            transform,
            handle,
            blockDepth > 0 ? TinPointSourceType::BlockText : TinPointSourceType::TextElevation);
        if (textBucket != nullptr && context.points.size() > before) {
            textBucket->push_back(context.points.back());
            context.points.pop_back();
        }
        return;
    }

    if (auto mtext = AcDbMText::cast(entity)) {
        const auto before = context.points.size();
        appendTextPoint(
            context,
            mtext->contents() == nullptr ? L"" : mtext->contents(),
            mtext->location(),
            transform,
            handle,
            blockDepth > 0 ? TinPointSourceType::BlockText : TinPointSourceType::TextElevation);
        if (textBucket != nullptr && context.points.size() > before) {
            textBucket->push_back(context.points.back());
            context.points.pop_back();
        }
        return;
    }

    if (auto blockReference = AcDbBlockReference::cast(entity)) {
        if (!context.options.enableBlockExtraction) {
            ++context.summary.blockParseFailedCount;
            return;
        }

        ++context.summary.blockCount;
        const auto name = blockName(blockReference->blockTableRecord());
        if (extractBlockAttributeElevation(blockReference, transform, handle, name, context)) {
            return;
        }

        extractNestedBlockContent(blockReference, transform, blockDepth, context);
        return;
    }

    ++context.summary.invalidObjectCount;
}

struct SampleFilter {
    AcDbObjectId sampleObjectId;
    AcRxClass* sampleClass = nullptr;
    std::wstring layer;
    std::wstring typeName;
};

enum class SampleSelectionResult {
    Selected,
    Finish,
    Cancel,
};

std::wstring sampleSignature(const SampleFilter& filter)
{
    const auto className = filter.sampleClass != nullptr && filter.sampleClass->name() != nullptr
        ? std::wstring(filter.sampleClass->name())
        : filter.typeName;
    return filter.layer + L"\n" + className;
}

SampleSelectionResult chooseSampleObject(SampleFilter& filter, IEditor& editor, bool hasExtractedAnyClass)
{
    editor.writeMessage(
        L"DN_TERRAIN_TIN_CREATE: "
        L"\u8bf7\u70b9\u9009\u4e00\u4e2a\u6837\u672c\u5bf9\u8c61\u63d0\u53d6\u540c\u56fe\u5c42\u3001\u540c\u7c7b\u578b\u6570\u636e\uff1b"
        L"\u53ef\u8fde\u7eed\u9009\u62e9\u591a\u7c7b\uff0c\u6309 Enter \u540e\u6253\u5f00\u53c2\u6570\u7a97\u53e3\u3002");

    ads_name selectionSet;
    const auto selectionStatus = acedSSGet(nullptr, nullptr, nullptr, nullptr, selectionSet);
    if (selectionStatus != RTNORM) {
        if (hasExtractedAnyClass) {
            editor.writeMessage(L"\u5df2\u7ed3\u675f\u6837\u672c\u9009\u62e9\uff0c\u6b63\u5728\u51c6\u5907\u53c2\u6570\u7a97\u53e3\u3002");
            return SampleSelectionResult::Finish;
        }
        editor.writeWarning(L"\u672a\u9009\u62e9\u6837\u672c\u5bf9\u8c61\uff0c\u5730\u5f62\u6784\u7f51\u5df2\u53d6\u6d88\u3002");
        return SampleSelectionResult::Cancel;
    }

    SelectionSetGuard guard(selectionSet);
    Adesk::Int32 selectionLength = 0;
    if (acedSSLength(selectionSet, &selectionLength) != RTNORM || selectionLength <= 0) {
        if (hasExtractedAnyClass) {
            return SampleSelectionResult::Finish;
        }
        editor.writeWarning(L"\u672a\u9009\u62e9\u6837\u672c\u5bf9\u8c61\uff0c\u5730\u5f62\u6784\u7f51\u5df2\u53d6\u6d88\u3002");
        return SampleSelectionResult::Cancel;
    }

    if (selectionLength > 1) {
        editor.writeWarning(L"\u5df2\u9009\u62e9\u591a\u4e2a\u5bf9\u8c61\uff0c\u5c06\u4f7f\u7528\u7b2c\u4e00\u4e2a\u4f5c\u4e3a\u6837\u672c\u3002");
    }

    ads_name entityName;
    if (acedSSName(selectionSet, 0, entityName) != RTNORM
        || acdbGetObjectId(filter.sampleObjectId, entityName) != Acad::eOk) {
        editor.writeError(L"\u65e0\u6cd5\u83b7\u53d6\u6837\u672c\u5bf9\u8c61\u3002");
        return SampleSelectionResult::Cancel;
    }

    AcDbEntity* sample = nullptr;
    if (acdbOpenObject(sample, filter.sampleObjectId, AcDb::kForRead) != Acad::eOk || sample == nullptr) {
        editor.writeError(L"\u65e0\u6cd5\u6253\u5f00\u6837\u672c\u5bf9\u8c61\u3002");
        return SampleSelectionResult::Cancel;
    }

    filter.sampleClass = sample->isA();
    filter.layer = entityLayer(sample);
    filter.typeName = entityDisplayType(sample);
    sample->close();

    if (filter.sampleClass == nullptr || filter.layer.empty()) {
        editor.writeError(L"\u6837\u672c\u5bf9\u8c61\u7c7b\u578b\u6216\u56fe\u5c42\u65e0\u6548\u3002");
        return SampleSelectionResult::Cancel;
    }

    return SampleSelectionResult::Selected;
}

bool isSameLayerAndType(const AcDbEntity* entity, const SampleFilter& filter)
{
    return entity != nullptr
        && entity->isA() == filter.sampleClass
        && entityLayer(entity) == filter.layer;
}

bool extractMatchingModelSpaceEntities(
    const SampleFilter& filter,
    ExtractionContext& extraction,
    IEditor& editor,
    std::vector<AcDbObjectId>& matchedObjectIds)
{
    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr) {
        editor.writeError(L"\u65e0\u6cd5\u83b7\u53d6\u5f53\u524d\u6570\u636e\u5e93\u3002");
        return false;
    }

    AcDbBlockTable* blockTable = nullptr;
    if (database->getBlockTable(blockTable, AcDb::kForRead) != Acad::eOk || blockTable == nullptr) {
        editor.writeError(L"\u65e0\u6cd5\u6253\u5f00\u5757\u8868\u3002");
        return false;
    }

    AcDbBlockTableRecord* modelSpace = nullptr;
    if (blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForRead) != Acad::eOk || modelSpace == nullptr) {
        blockTable->close();
        editor.writeError(L"\u65e0\u6cd5\u6253\u5f00\u6a21\u578b\u7a7a\u95f4\u3002");
        return false;
    }

    std::size_t totalEntities = 0;
    AcDbBlockTableRecordIterator* iterator = nullptr;
    if (modelSpace->newIterator(iterator) != Acad::eOk || iterator == nullptr) {
        modelSpace->close();
        blockTable->close();
        editor.writeError(L"\u65e0\u6cd5\u904d\u5386\u6a21\u578b\u7a7a\u95f4\u3002");
        return false;
    }
    for (; !iterator->done(); iterator->step()) {
        ++totalEntities;
    }
    delete iterator;
    iterator = nullptr;

    if (modelSpace->newIterator(iterator) != Acad::eOk || iterator == nullptr) {
        modelSpace->close();
        blockTable->close();
        editor.writeError(L"\u65e0\u6cd5\u904d\u5386\u6a21\u578b\u7a7a\u95f4\u3002");
        return false;
    }

    StatusProgressMeter progress(L"\u5730\u5f62\u6570\u636e\u63d0\u53d6", 0, 100);
    std::size_t visited = 0;
    matchedObjectIds.clear();
    for (; !iterator->done(); iterator->step()) {
        ++visited;
        if (visited == 1 || visited % 200 == 0 || visited == totalEntities) {
            const auto percent = totalEntities == 0
                ? 100
                : static_cast<int>((visited * 100) / totalEntities);
            progress.setPosition(percent);
        }

        AcDbEntity* entity = nullptr;
        if (iterator->getEntity(entity, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
            ++extraction.summary.invalidObjectCount;
            continue;
        }

        if (entity->visibility() == AcDb::kInvisible) {
            entity->close();
            continue;
        }

        if (isSameLayerAndType(entity, filter)) {
            ++extraction.summary.selectedObjectCount;
            AcDbObjectId entityId;
            if (iterator->getEntityId(entityId) == Acad::eOk) {
                matchedObjectIds.push_back(entityId);
            }
            extractEntity(entity, AcGeMatrix3d::kIdentity, 0, extraction, nullptr, nullptr);
        }
        entity->close();
    }

    delete iterator;
    modelSpace->close();
    blockTable->close();

    extraction.summary.rawPointCount = extraction.points.size();
    if (extraction.summary.selectedObjectCount == 0) {
        editor.writeWarning(L"\u672a\u627e\u5230\u540c\u56fe\u5c42\u540c\u7c7b\u578b\u7684\u5bf9\u8c61\u3002");
        return false;
    }
    return true;
}

void setEntitiesVisibility(
    const std::vector<AcDbObjectId>& objectIds,
    AcDb::Visibility visibility,
    IEditor& editor)
{
    StatusProgressMeter progress(
        visibility == AcDb::kInvisible
            ? L"\u9690\u85cf\u5df2\u63d0\u53d6\u7684\u6e90\u5bf9\u8c61"
            : L"\u6062\u590d\u6e90\u5bf9\u8c61\u663e\u793a",
        0,
        100);

    for (std::size_t i = 0; i < objectIds.size(); ++i) {
        if (i == 0 || i % 200 == 0 || i + 1 == objectIds.size()) {
            const auto percent = objectIds.empty()
                ? 100
                : static_cast<int>(((i + 1) * 100) / objectIds.size());
            progress.setPosition(percent);
        }

        AcDbEntity* entity = nullptr;
        if (acdbOpenObject(entity, objectIds[i], AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
            continue;
        }
        const auto status = entity->setVisibility(visibility);
        if (status != Acad::eOk) {
            editor.writeWarning(L"\u90e8\u5206\u6e90\u5bf9\u8c61\u53ef\u89c1\u6027\u8bbe\u7f6e\u5931\u8d25\u3002");
        }
        entity->close();
    }
    acedUpdateDisplay();
}

void mergeExtractionCounters(TinExtractSummary& target, const TinExtractSummary& extracted)
{
    target.selectedObjectCount = extracted.selectedObjectCount;
    target.rawPointCount = extracted.rawPointCount;
    target.invalidObjectCount += extracted.invalidObjectCount;
    target.blockCount = extracted.blockCount;
    target.recognizedElevationBlockCount = extracted.recognizedElevationBlockCount;
    target.blockAttributeElevationCount = extracted.blockAttributeElevationCount;
    target.blockParseFailedCount = extracted.blockParseFailedCount;
}

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
    if (blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForWrite) != Acad::eOk || modelSpace == nullptr) {
        blockTable->close();
        return false;
    }

    const auto status = modelSpace->appendAcDbEntity(entityId, entity);
    modelSpace->close();
    blockTable->close();
    return status == Acad::eOk;
}

bool eraseEntity(const AcDbObjectId& entityId)
{
    if (entityId.isNull()) {
        return true;
    }

    AcDbEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        return false;
    }
    const auto status = entity->erase();
    entity->close();
    return status == Acad::eOk;
}

bool findTerrainTinEntityInSelectionSet(const ads_name selectionSet, AcDbObjectId& entityId)
{
    Adesk::Int32 selectionLength = 0;
    if (acedSSLength(selectionSet, &selectionLength) != RTNORM || selectionLength <= 0) {
        return false;
    }

    for (Adesk::Int32 i = 0; i < selectionLength; ++i) {
        ads_name entityName;
        AcDbObjectId candidateId;
        if (acedSSName(selectionSet, i, entityName) != RTNORM
            || acdbGetObjectId(candidateId, entityName) != Acad::eOk) {
            continue;
        }

        DnTerrainTinEntity* entity = nullptr;
        if (acdbOpenObject(entity, candidateId, AcDb::kForRead) == Acad::eOk && entity != nullptr) {
            entity->close();
            entityId = candidateId;
            return true;
        }
    }

    return false;
}

bool findImpliedTerrainTinEntity(AcDbObjectId& entityId)
{
    ads_name selectionSet;
    if (acedSSGet(L"_I", nullptr, nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }

    SelectionSetGuard guard(selectionSet);
    return findTerrainTinEntityInSelectionSet(selectionSet, entityId);
}

std::wstring ensureRmeshExtension(const std::wstring& path)
{
    std::filesystem::path filePath(path);
    const auto extension = toUpper(filePath.extension().wstring());
    if (extension != L".RMESH") {
        filePath.replace_extension(L".rmesh");
    }
    return filePath.wstring();
}

bool promptRmeshFilePath(bool forWrite, const ACHAR* title, const ACHAR* defaultFileName, std::wstring& path, IEditor& editor)
{
    if (isCoreConsole()) {
        AcString input;
        const auto status = acedGetString(
            0,
            forWrite
                ? L"\nRMesh \u5bfc\u51fa\u6587\u4ef6\u8def\u5f84: "
                : L"\nRMesh \u5bfc\u5165\u6587\u4ef6\u8def\u5f84: ",
            input);
        if (status != RTNORM || input.isEmpty()) {
            editor.writeWarning(L"\u5df2\u53d6\u6d88 RMesh \u6587\u4ef6\u9009\u62e9\u3002");
            return false;
        }
        path = ensureRmeshExtension(input.constPtr());
        return true;
    }

    resbuf* result = acutNewRb(RTSTR);
    if (result == nullptr) {
        editor.writeError(L"\u65e0\u6cd5\u521b\u5efa RMesh \u6587\u4ef6\u9009\u62e9\u7ed3\u679c\u7f13\u51b2\u3002");
        return false;
    }
    result->resval.rstring = nullptr;

    const int flags = 2 + (forWrite ? 1 : 0);
    const auto status = acedGetFileD(title, defaultFileName, L"rmesh", flags, result);
    if (status != RTNORM || result->resval.rstring == nullptr || result->resval.rstring[0] == L'\0') {
        acutRelRb(result);
        editor.writeWarning(L"\u5df2\u53d6\u6d88 RMesh \u6587\u4ef6\u9009\u62e9\u3002");
        return false;
    }

    path = ensureRmeshExtension(result->resval.rstring);
    acutRelRb(result);
    return true;
}

TinBuildResult meshResultFromEntity(const DnTerrainTinEntity& entity)
{
    TinBuildResult mesh;
    mesh.succeeded = true;
    mesh.points = entity.getPoints();
    mesh.triangles = entity.getTriangles();
    mesh.boundaryEdges = entity.getBoundaryEdges();
    mesh.buildOptions = entity.buildOptions();
    mesh.buildOptions.displayMode = entity.displayMode();
    mesh.extractOptions = entity.extractOptions();
    mesh.minElevation = entity.minElevation();
    mesh.maxElevation = entity.maxElevation();
    mesh.extractSummary.rawPointCount = mesh.points.size();
    mesh.extractSummary.validPointCount = mesh.points.size();
    mesh.extractSummary.triangleCount = mesh.triangles.size();
    mesh.extractSummary.status = L"RMesh";
    return mesh;
}

bool insertTerrainTinMesh(const TinBuildResult& mesh, AcDbObjectId& entityId, std::wstring& entityHandle)
{
    auto* tinEntity = new DnTerrainTinEntity();
    tinEntity->setTinResult(mesh);

    if (!appendEntityToModelSpace(tinEntity, entityId)) {
        delete tinEntity;
        return false;
    }

    AcDbHandle handle;
    tinEntity->getAcDbHandle(handle);
    ACHAR handleText[32] = {};
    handle.getIntoAsciiBuffer(handleText);
    entityHandle = handleText;
    tinEntity->close();
    return true;
}

void writeCommandSummary(
    const TinExtractSummary& summary,
    const TinBuildResult& result,
    const std::wstring& entityHandle)
{
    auto& editor = app::ApplicationContext::instance().editor();
    std::wstringstream stream;
    stream << L"DN_TERRAIN_TIN_CREATE completed. "
           << L"Raw points: " << summary.rawPointCount
           << L", valid points: " << result.points.size()
           << L", duplicates: " << summary.duplicatePointCount
           << L", text elevations: " << summary.textElevationPointCount
           << L", text merges: " << summary.textPointMergeCount
           << L", text assigned: " << summary.textAssignedElevationPointCount
           << L", blocks: " << summary.blockCount
           << L", triangles: " << result.triangles.size()
           << L", entity handle: " << entityHandle;
    editor.writeMessage(stream.str());
}

void runTerrainTinCreateCommandLegacy()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"DN_TERRAIN_TIN_CREATE: 选择点、线、多段线、文字、块等地形相关对象。");

    ads_name selectionSet;
    const auto selectionStatus = acedSSGet(nullptr, nullptr, nullptr, nullptr, selectionSet);
    if (selectionStatus != RTNORM) {
        editor.writeWarning(L"用户取消选择或未选择对象，地形构网已结束。");
        return;
    }

    SelectionSetGuard guard(selectionSet);
    Adesk::Int32 selectionLength = 0;
    if (acedSSLength(selectionSet, &selectionLength) != RTNORM || selectionLength <= 0) {
        editor.writeWarning(L"未选择对象，地形构网已结束。");
        return;
    }

    ExtractionContext extraction;
    extraction.points.reserve(static_cast<std::size_t>(selectionLength) * 2);
    extraction.summary.selectedObjectCount = static_cast<std::size_t>(selectionLength);

    for (Adesk::Int32 i = 0; i < selectionLength; ++i) {
        ads_name entityName;
        if (acedSSName(selectionSet, i, entityName) != RTNORM) {
            ++extraction.summary.invalidObjectCount;
            continue;
        }

        AcDbObjectId objectId;
        if (acdbGetObjectId(objectId, entityName) != Acad::eOk) {
            ++extraction.summary.invalidObjectCount;
            continue;
        }

        AcDbEntity* entity = nullptr;
        if (acdbOpenObject(entity, objectId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
            ++extraction.summary.invalidObjectCount;
            continue;
        }

        extractEntity(entity, AcGeMatrix3d::kIdentity, 0, extraction, nullptr, nullptr);
        entity->close();
    }

    extraction.summary.rawPointCount = extraction.points.size();

    application::TerrainTinCreateService service;
    TinBuildOptions buildOptions;
    auto result = service.buildFromExtractedPoints(extraction.points, extraction.options, buildOptions);
    result.extractSummary.selectedObjectCount = extraction.summary.selectedObjectCount;
    result.extractSummary.invalidObjectCount += extraction.summary.invalidObjectCount;
    result.extractSummary.blockCount = extraction.summary.blockCount;
    result.extractSummary.recognizedElevationBlockCount = extraction.summary.recognizedElevationBlockCount;
    result.extractSummary.blockAttributeElevationCount = extraction.summary.blockAttributeElevationCount;
    result.extractSummary.blockParseFailedCount = extraction.summary.blockParseFailedCount;
    result.extractSummary.rawPointCount = extraction.summary.rawPointCount;

    if (!result.succeeded) {
        editor.writeError(L"TIN 构建失败: " + result.errorMessage);
        return;
    }

    auto* tinEntity = new DnTerrainTinEntity();
    tinEntity->setTinResult(result);

    AcDbObjectId entityId;
    if (!appendEntityToModelSpace(tinEntity, entityId)) {
        delete tinEntity;
        editor.writeError(L"插入 DnTerrainTinEntity 失败。");
        return;
    }

    AcDbHandle handle;
    tinEntity->getAcDbHandle(handle);
    ACHAR handleText[32] = {};
    handle.getIntoAsciiBuffer(handleText);
    tinEntity->close();

    writeCommandSummary(result.extractSummary, result, handleText);
}

bool selectTerrainTinEntity(AcDbObjectId& entityId, IEditor& editor, const std::wstring& commandName)
{
    if (findImpliedTerrainTinEntity(entityId)) {
        editor.writeMessage(commandName + L": \u5df2\u4f7f\u7528\u5f53\u524d\u9009\u4e2d\u7684\u5730\u5f62\u6570\u6a21\u5b9e\u4f53\u3002");
        return true;
    }

    editor.writeMessage(commandName + L": \u8bf7\u9009\u62e9\u5730\u5f62\u6570\u6a21\u5b9e\u4f53\u3002");

    ads_name selectionSet;
    const auto selectionStatus = acedSSGet(nullptr, nullptr, nullptr, nullptr, selectionSet);
    if (selectionStatus != RTNORM) {
        editor.writeWarning(L"\u7528\u6237\u53d6\u6d88\u9009\u62e9\u6216\u672a\u9009\u62e9\u6570\u6a21\u5b9e\u4f53\u3002");
        return false;
    }

    SelectionSetGuard guard(selectionSet);
    if (findTerrainTinEntityInSelectionSet(selectionSet, entityId)) {
        return true;
    }

    editor.writeWarning(L"\u9009\u62e9\u96c6\u4e2d\u672a\u627e\u5230 DnTerrainTinEntity\u3002");
    return false;
}

bool resolveTerrainTinEntityHandle(const std::wstring& handleText, AcDbObjectId& entityId, IEditor& editor)
{
    if (handleText.empty()) {
        editor.writeWarning(L"\u6570\u6a21\u5b9e\u4f53 handle \u4e3a\u7a7a\u3002");
        return false;
    }

    AcDbDatabase* database = acdbHostApplicationServices()->workingDatabase();
    if (database == nullptr) {
        editor.writeError(L"\u65e0\u6cd5\u83b7\u53d6\u5f53\u524d DWG \u6570\u636e\u5e93\u3002");
        return false;
    }

    const AcDbHandle handle(handleText.c_str());
    if (handle.isNull()) {
        editor.writeWarning(L"\u6570\u6a21\u5b9e\u4f53 handle \u65e0\u6548\u3002");
        return false;
    }

    if (database->getAcDbObjectId(entityId, false, handle) != Acad::eOk || entityId.isNull()) {
        editor.writeWarning(L"\u672a\u627e\u5230 handle \u5bf9\u5e94\u7684\u6570\u6a21\u5b9e\u4f53\u3002");
        return false;
    }

    DnTerrainTinEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeWarning(L"handle \u5bf9\u5e94\u7684\u5bf9\u8c61\u4e0d\u662f DnTerrainTinEntity\u3002");
        return false;
    }
    entity->close();
    return true;
}

void editTerrainTinEntityByIdForCommand(const AcDbObjectId& entityId, IEditor& editor)
{
    DnTerrainTinEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"\u65e0\u6cd5\u6253\u5f00 DnTerrainTinEntity\u3002");
        return;
    }

    if (isCoreConsole()) {
        auto options = entity->buildOptions();
        options.displayMode = TerrainTinDisplayMode::BoundaryOnly;
        entity->setBuildOptions(options);
        entity->recordGraphicsModified(true);
        entity->close();
        editor.writeMessage(L"Core Console \u73af\u5883\u4e0b\u5df2\u5c06\u6570\u6a21\u663e\u793a\u6a21\u5f0f\u5207\u6362\u4e3a\u8fb9\u754c\u663e\u793a\u3002");
        return;
    }

    TerrainTinEditDialogInput dialogInput;
    dialogInput.pointCount = entity->getPoints().size();
    dialogInput.triangleCount = entity->getTriangles().size();
    dialogInput.boundaryEdgeCount = entity->getBoundaryEdges().size();
    dialogInput.minElevation = entity->minElevation();
    dialogInput.maxElevation = entity->maxElevation();
    dialogInput.buildOptions = entity->buildOptions();
    dialogInput.buildOptions.displayMode = entity->displayMode();

    TerrainTinEditDialogResult dialogResult;
    if (!showTerrainTinEditDialog(dialogInput, dialogResult)) {
        entity->close();
        editor.writeWarning(L"\u6570\u6a21\u7f16\u8f91\u5df2\u53d6\u6d88\u3002");
        return;
    }

    if (!dialogResult.rebuildRequested) {
        entity->setBuildOptions(dialogResult.buildOptions);
        entity->recordGraphicsModified(true);
        entity->close();
        editor.writeMessage(L"\u6570\u6a21\u663e\u793a\u53c2\u6570\u5df2\u66f4\u65b0\u3002");
        return;
    }

    TerrainTinBuilder builder;
    TinBuildResult rebuildResult;
    {
        StatusProgressMeter progress(L"TIN \u91cd\u65b0\u6784\u5efa", 0, 100);
        progress.setPosition(10);
        rebuildResult = builder.build(entity->getPoints(), dialogResult.buildOptions);
        progress.setPosition(100);
    }

    if (!rebuildResult.succeeded) {
        const auto error = rebuildResult.errorMessage;
        entity->close();
        editor.writeError(L"TIN \u91cd\u65b0\u751f\u6210\u5931\u8d25: " + error);
        return;
    }

    rebuildResult.extractOptions = entity->extractOptions();
    rebuildResult.extractSummary.rawPointCount = rebuildResult.points.size();
    rebuildResult.extractSummary.validPointCount = rebuildResult.points.size();
    entity->setTinResult(rebuildResult);
    entity->recordGraphicsModified(true);
    entity->close();
    editor.writeMessage(L"\u6570\u6a21\u5df2\u91cd\u65b0\u751f\u6210\u3002");
}

void runTerrainTinEditCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    AcDbObjectId entityId;
    if (!selectTerrainTinEntity(entityId, editor, L"DN_TERRAIN_TIN_EDIT")) {
        return;
    }

    editTerrainTinEntityByIdForCommand(entityId, editor);
}

void runTerrainTinEditHandleCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    AcString handleText;
    const auto status = acedGetString(
        0,
        L"\n\u6570\u6a21\u5b9e\u4f53 handle: ",
        handleText);
    if (status != RTNORM || handleText.isEmpty()) {
        editor.writeWarning(L"\u5df2\u53d6\u6d88\u6570\u6a21\u53cc\u51fb\u7f16\u8f91\u3002");
        return;
    }

    AcDbObjectId entityId;
    if (!resolveTerrainTinEntityHandle(handleText.constPtr(), entityId, editor)) {
        return;
    }

    editTerrainTinEntityByIdForCommand(entityId, editor);
}

void runTerrainTinExportCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    AcDbObjectId entityId;
    if (!selectTerrainTinEntity(entityId, editor, L"DN_TERRAIN_TIN_EXPORT")) {
        return;
    }

    DnTerrainTinEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForRead) != Acad::eOk || entity == nullptr) {
        editor.writeError(L"\u65e0\u6cd5\u6253\u5f00 DnTerrainTinEntity\u3002");
        return;
    }

    auto mesh = meshResultFromEntity(*entity);
    entity->close();

    if (mesh.points.empty() || mesh.triangles.empty()) {
        editor.writeError(L"\u5f53\u524d\u6570\u6a21\u6ca1\u6709\u53ef\u5bfc\u51fa\u7684\u4e09\u89d2\u7f51\u6570\u636e\u3002");
        return;
    }

    std::wstring path;
    if (!promptRmeshFilePath(
            true,
            L"\u5bfc\u51fa\u5730\u5f62\u6570\u6a21",
            L"terrain.rmesh",
            path,
            editor)) {
        return;
    }

    StatusProgressMeter progress(L"\u5bfc\u51fa RMesh \u5730\u5f62\u6570\u6a21", 0, 100);
    progress.setPosition(20);

    TerrainMeshFile meshFile;
    std::wstring errorMessage;
    if (!meshFile.write(path, mesh, errorMessage)) {
        editor.writeError(errorMessage.empty() ? L"RMesh \u5bfc\u51fa\u5931\u8d25\u3002" : errorMessage);
        return;
    }

    progress.setPosition(100);

    std::wstringstream stream;
    stream << L"RMesh \u5df2\u5bfc\u51fa: "
           << path
           << L"\uff0c\u70b9 "
           << mesh.points.size()
           << L"\uff0c\u4e09\u89d2\u5f62 "
           << mesh.triangles.size()
           << L"\u3002";
    editor.writeMessage(stream.str());
}

void runTerrainTinImportCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    std::wstring path;
    if (!promptRmeshFilePath(
            false,
            L"\u5bfc\u5165\u5730\u5f62\u6570\u6a21",
            L"",
            path,
            editor)) {
        return;
    }

    StatusProgressMeter progress(L"\u5bfc\u5165 RMesh \u5730\u5f62\u6570\u6a21", 0, 100);
    progress.setPosition(20);

    TerrainMeshFile meshFile;
    auto loaded = meshFile.read(path);
    if (!loaded.succeeded) {
        editor.writeError(loaded.errorMessage.empty() ? L"RMesh \u5bfc\u5165\u5931\u8d25\u3002" : loaded.errorMessage);
        return;
    }

    progress.setPosition(70);
    if (loaded.mesh.points.size() < 3 || loaded.mesh.triangles.empty()) {
        editor.writeError(L"RMesh \u6587\u4ef6\u4e2d\u6ca1\u6709\u53ef\u7528\u7684\u4e09\u89d2\u7f51\u6570\u636e\u3002");
        return;
    }

    AcDbObjectId entityId;
    std::wstring entityHandle;
    if (!insertTerrainTinMesh(loaded.mesh, entityId, entityHandle)) {
        editor.writeError(L"\u63d2\u5165 DnTerrainTinEntity \u5931\u8d25\u3002");
        return;
    }

    progress.setPosition(100);
    acedUpdateDisplay();

    std::wstringstream stream;
    stream << L"RMesh \u5df2\u5bfc\u5165: "
           << path
           << L"\uff0c\u70b9 "
           << loaded.mesh.points.size()
           << L"\uff0c\u4e09\u89d2\u5f62 "
           << loaded.mesh.triangles.size()
           << L"\uff0c\u5b9e\u4f53\u53e5\u67c4 "
           << entityHandle
           << L"\u3002";
    editor.writeMessage(stream.str());
}

void runTerrainTinCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();

    ExtractionContext extraction;
    extraction.points.reserve(8192);
    std::vector<AcDbObjectId> hiddenSourceObjectIds;
    std::unordered_set<std::wstring> extractedSignatures;
    std::vector<std::wstring> extractedLayers;
    std::vector<std::wstring> extractedTypes;

    while (true) {
        SampleFilter filter;
        const auto selectionResult = chooseSampleObject(filter, editor, !hiddenSourceObjectIds.empty());
        if (selectionResult == SampleSelectionResult::Finish) {
            break;
        }
        if (selectionResult == SampleSelectionResult::Cancel) {
            if (!hiddenSourceObjectIds.empty()) {
                setEntitiesVisibility(hiddenSourceObjectIds, AcDb::kVisible, editor);
            }
            return;
        }

        const auto signature = sampleSignature(filter);
        if (extractedSignatures.find(signature) != extractedSignatures.end()) {
            editor.writeWarning(L"\u8be5\u56fe\u5c42\u548c\u7c7b\u578b\u5df2\u63d0\u53d6\uff0c\u8bf7\u7ee7\u7eed\u9009\u62e9\u5176\u4ed6\u7c7b\u578b\u6216\u6309 Enter\u3002");
            continue;
        }

        std::vector<AcDbObjectId> matchedObjectIds;
        const auto pointsBefore = extraction.points.size();
        const auto objectsBefore = extraction.summary.selectedObjectCount;
        if (!extractMatchingModelSpaceEntities(filter, extraction, editor, matchedObjectIds) || matchedObjectIds.empty()) {
            extraction.points.resize(pointsBefore);
            extraction.summary.selectedObjectCount = objectsBefore;
            continue;
        }

        setEntitiesVisibility(matchedObjectIds, AcDb::kInvisible, editor);
        hiddenSourceObjectIds.insert(hiddenSourceObjectIds.end(), matchedObjectIds.begin(), matchedObjectIds.end());
        extractedSignatures.insert(signature);
        extractedLayers.push_back(filter.layer);
        extractedTypes.push_back(filter.typeName);

        std::wstringstream stream;
        stream << L"\u5df2\u63d0\u53d6\u5e76\u9690\u85cf: \u56fe\u5c42 "
               << filter.layer
               << L" / \u7c7b\u578b "
               << filter.typeName
               << L"\uff0c\u672c\u7c7b\u5bf9\u8c61 "
               << matchedObjectIds.size()
               << L"\uff0c\u7d2f\u8ba1\u539f\u59cb\u70b9 "
               << extraction.points.size()
               << L"\u3002";
        editor.writeMessage(stream.str());

        if (isCoreConsole()) {
            break;
        }
    }

    if (hiddenSourceObjectIds.empty() || extraction.points.empty()) {
        editor.writeWarning(L"\u672a\u63d0\u53d6\u5230\u5730\u5f62\u70b9\uff0c\u5730\u5f62\u6784\u7f51\u5df2\u7ed3\u675f\u3002");
        return;
    }

    bool restoreHiddenSources = true;

    TerrainPointNormalizer normalizer;
    auto preview = [&]() {
        StatusProgressMeter previewProgress(L"\u5730\u5f62\u70b9\u6e05\u6d17", 0, 100);
        auto normalizeResult = normalizer.normalize(extraction.points, extraction.options);
        previewProgress.setPosition(100);
        return normalizeResult;
    }();
    mergeExtractionCounters(preview.summary, extraction.summary);

    TinBuildOptions buildOptions;
    TinBuildResult result;
    AcDbObjectId previewTinEntityId;
    std::wstring latestEntityHandle;

    auto buildAndInsertPreview = [&](const TinExtractOptions& extractOptions,
                                     const TinBuildOptions& requestedBuildOptions,
                                     const TerrainTinProgressCallback& progress,
                                     std::wstring& statusMessage) -> bool {
        progress(10, L"\u6b63\u5728\u6e05\u6d17\u5730\u5f62\u70b9...");
        application::TerrainTinCreateService service;
        auto currentResult = service.buildFromExtractedPoints(extraction.points, extractOptions, requestedBuildOptions);
        mergeExtractionCounters(currentResult.extractSummary, extraction.summary);
        if (!currentResult.succeeded) {
            statusMessage = L"TIN \u6784\u5efa\u5931\u8d25: " + currentResult.errorMessage;
            return false;
        }

        progress(88, L"\u6b63\u5728\u5199\u5165 DnTerrainTinEntity...");
        auto* tinEntity = new DnTerrainTinEntity();
        tinEntity->setTinResult(currentResult);

        AcDbObjectId newEntityId;
        if (!appendEntityToModelSpace(tinEntity, newEntityId)) {
            delete tinEntity;
            statusMessage = L"\u63d2\u5165 DnTerrainTinEntity \u5931\u8d25\u3002";
            return false;
        }

        AcDbHandle handle;
        tinEntity->getAcDbHandle(handle);
        ACHAR handleText[32] = {};
        handle.getIntoAsciiBuffer(handleText);
        tinEntity->close();

        if (!previewTinEntityId.isNull()) {
            eraseEntity(previewTinEntityId);
        }
        previewTinEntityId = newEntityId;
        result = std::move(currentResult);
        latestEntityHandle = handleText;
        acedUpdateDisplay();

        std::wstringstream stream;
        stream << L"\u5df2\u751f\u6210\u6570\u6a21: "
               << result.points.size()
               << L" \u70b9 / "
               << result.triangles.size()
               << L" \u4e09\u89d2\u5f62\u3002";
        statusMessage = stream.str();
        progress(100, statusMessage);
        return true;
    };

    if (!isCoreConsole()) {
        TerrainTinCreateDialogInput dialogInput;
        dialogInput.summary = preview.summary;
        dialogInput.extractOptions = extraction.options;
        dialogInput.buildOptions = buildOptions;
        for (std::size_t i = 0; i < extractedLayers.size(); ++i) {
            if (i > 0) {
                dialogInput.sampleLayer += L"; ";
                dialogInput.sampleType += L"; ";
            }
            dialogInput.sampleLayer += extractedLayers[i];
            dialogInput.sampleType += extractedTypes[i];
        }
        dialogInput.hiddenSourceObjectCount = hiddenSourceObjectIds.size();

        TerrainTinCreateDialogResult dialogResult;
        if (!showTerrainTinCreateDialog(dialogInput, dialogResult, buildAndInsertPreview)) {
            if (!previewTinEntityId.isNull()) {
                eraseEntity(previewTinEntityId);
            }
            if (restoreHiddenSources) {
                setEntitiesVisibility(hiddenSourceObjectIds, AcDb::kVisible, editor);
            }
            editor.writeWarning(L"\u5730\u5f62\u6784\u7f51\u5df2\u53d6\u6d88\u3002");
            return;
        }

        extraction.options = dialogResult.extractOptions;
        buildOptions = dialogResult.buildOptions;
    } else {
        editor.writeMessage(L"Core Console \u73af\u5883\u4e0b\u8df3\u8fc7\u53c2\u6570 UI\uff0c\u4f7f\u7528\u9ed8\u8ba4\u6784\u7f51\u53c2\u6570\u3002");
        StatusProgressMeter buildProgress(L"TIN \u4e09\u89d2\u7f51\u6784\u5efa", 0, 100);
        std::wstring statusMessage;
        if (!buildAndInsertPreview(
                extraction.options,
                buildOptions,
                [&](int percent, const std::wstring&) {
                    buildProgress.setPosition(percent);
                },
                statusMessage)) {
            if (restoreHiddenSources) {
                setEntitiesVisibility(hiddenSourceObjectIds, AcDb::kVisible, editor);
            }
            editor.writeError(statusMessage);
            return;
        }
    }
    restoreHiddenSources = false;

    writeCommandSummary(result.extractSummary, result, latestEntityHandle);
}

} // namespace

core::CommandProcedure terrainTinCreateCommandProcedure()
{
    return &runTerrainTinCreateCommand;
}

core::CommandProcedure terrainTinEditCommandProcedure()
{
    return &runTerrainTinEditCommand;
}

core::CommandProcedure terrainTinEditHandleCommandProcedure()
{
    return &runTerrainTinEditHandleCommand;
}

core::CommandProcedure terrainTinExportCommandProcedure()
{
    return &runTerrainTinExportCommand;
}

core::CommandProcedure terrainTinImportCommandProcedure()
{
    return &runTerrainTinImportCommand;
}

} // namespace roadproto::cad_adapter::objectarx
