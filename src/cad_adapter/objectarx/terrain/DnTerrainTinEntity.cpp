#include "cad_adapter/objectarx/terrain/DnTerrainTinEntity.h"

#include "cad_adapter/objectarx/ObjectArxSelectionSetGuard.h"
#include "cad_adapter/objectarx/terrain/TerrainTinCreateDialog.h"
#include "domain/terrain/TerrainTinBuilder.h"
#include "domain/terrain/TerrainSurfaceQuery.h"

#include "AcDblClkEdit.h"
#include "aced.h"
#include "adscodes.h"
#include "acgi.h"
#include "acutads.h"
#include "acutmem.h"
#include "dbproxy.h"
#include "geassign.h"
#include "rxregsvc.h"

#include <algorithm>
#include <array>
#include <unordered_map>

using roadproto::domain::terrain::TerrainSurfaceQuery;
using roadproto::domain::terrain::TerrainTinBuilder;
using roadproto::domain::terrain::TerrainTinDisplayMode;
using roadproto::domain::terrain::TinBuildOptions;
using roadproto::domain::terrain::TinBuildResult;
using roadproto::domain::terrain::TinExtractOptions;
using roadproto::domain::terrain::TinPoint;
using roadproto::domain::terrain::TinPointSourceType;
using roadproto::domain::terrain::TinTriangle;

ACRX_DXF_DEFINE_MEMBERS(
    DnTerrainTinEntity,
    AcDbEntity,
    AcDb::kDHL_CURRENT,
    AcDb::kMReleaseCurrent,
    AcDbProxyEntity::kAllAllowedBits,
    DNTERRAINTINENTITY,
    "RoadProto Terrain TIN");

namespace {

constexpr Adesk::Int16 kEntityVersion = 1;

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

template <typename T>
void writeBool(AcDbDwgFiler* filer, T value)
{
    filer->writeInt8(value ? 1 : 0);
}

bool readBool(AcDbDwgFiler* filer)
{
    Adesk::Int8 value = 0;
    filer->readInt8(&value);
    return value != 0;
}

struct EdgeKey {
    std::size_t a = 0;
    std::size_t b = 0;

    bool operator==(const EdgeKey& other) const
    {
        return a == other.a && b == other.b;
    }
};

struct EdgeKeyHash {
    std::size_t operator()(const EdgeKey& edge) const
    {
        const auto ha = std::hash<std::size_t>{}(edge.a);
        const auto hb = std::hash<std::size_t>{}(edge.b);
        return ha ^ (hb + 0x9e3779b97f4a7c15ULL + (ha << 6) + (ha >> 2));
    }
};

EdgeKey edgeKey(std::size_t a, std::size_t b)
{
    return a < b ? EdgeKey{a, b} : EdgeKey{b, a};
}

class TerrainTinDoubleClickEdit : public AcDbDoubleClickEdit {
public:
    ACRX_DECLARE_MEMBERS(TerrainTinDoubleClickEdit);

    void startEdit(AcDbEntity* entity, AcGePoint3d) override;
    void finishEdit() override {}
};

class TerrainTinEditorReactor : public AcEditorReactor {
public:
    void beginDoubleClick(const AcGePoint3d& clickPoint) override;
};

ACRX_CONS_DEFINE_MEMBERS(TerrainTinDoubleClickEdit, AcDbDoubleClickEdit, 1);

TerrainTinDoubleClickEdit* g_doubleClickEdit = nullptr;
TerrainTinEditorReactor* g_editorReactor = nullptr;
bool g_editDialogActive = false;

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

    roadproto::cad_adapter::objectarx::SelectionSetGuard guard(selectionSet);
    return findTerrainTinEntityInSelectionSet(selectionSet, entityId);
}

bool findTerrainTinEntityAtPoint(const AcGePoint3d& point, AcDbObjectId& entityId)
{
    ads_name selectionSet;
    if (acedSSGet(nullptr, asDblArray(point), nullptr, nullptr, selectionSet) != RTNORM) {
        return false;
    }

    roadproto::cad_adapter::objectarx::SelectionSetGuard guard(selectionSet);
    return findTerrainTinEntityInSelectionSet(selectionSet, entityId);
}

class EditDialogGuard {
public:
    EditDialogGuard()
    {
        g_editDialogActive = true;
    }

    ~EditDialogGuard()
    {
        g_editDialogActive = false;
    }
};

void editTerrainTinEntity(DnTerrainTinEntity& entity)
{
    if (g_editDialogActive) {
        return;
    }
    EditDialogGuard dialogGuard;

    if (entity.isWriteEnabled() != Adesk::kTrue) {
        if (entity.upgradeOpen() != Acad::eOk) {
            return;
        }
    }

    roadproto::cad_adapter::objectarx::TerrainTinEditDialogInput input;
    input.pointCount = entity.getPoints().size();
    input.triangleCount = entity.getTriangles().size();
    input.boundaryEdgeCount = entity.getBoundaryEdges().size();
    input.minElevation = entity.minElevation();
    input.maxElevation = entity.maxElevation();
    input.buildOptions = entity.buildOptions();
    input.buildOptions.displayMode = entity.displayMode();

    roadproto::cad_adapter::objectarx::TerrainTinEditDialogResult dialogResult;
    if (!roadproto::cad_adapter::objectarx::showTerrainTinEditDialog(input, dialogResult)) {
        return;
    }

    if (!dialogResult.rebuildRequested) {
        entity.setDisplayMode(dialogResult.buildOptions.displayMode);
        entity.setBuildOptions(dialogResult.buildOptions);
        entity.recordGraphicsModified(true);
        return;
    }

    TerrainTinBuilder builder;
    auto buildResult = builder.build(entity.getPoints(), dialogResult.buildOptions);
    if (!buildResult.succeeded) {
        acutPrintf(L"\nTIN \u91cd\u65b0\u751f\u6210\u5931\u8d25: %ls", buildResult.errorMessage.c_str());
        return;
    }

    buildResult.extractOptions = entity.extractOptions();
    buildResult.extractSummary.validPointCount = buildResult.points.size();
    buildResult.extractSummary.rawPointCount = buildResult.points.size();
    entity.setTinResult(buildResult);
    entity.recordGraphicsModified(true);
}

void editTerrainTinEntityById(const AcDbObjectId& entityId)
{
    if (entityId.isNull()) {
        return;
    }

    DnTerrainTinEntity* entity = nullptr;
    if (acdbOpenObject(entity, entityId, AcDb::kForWrite) != Acad::eOk || entity == nullptr) {
        return;
    }

    editTerrainTinEntity(*entity);
    entity->close();
    acedUpdateDisplay();
}

void TerrainTinDoubleClickEdit::startEdit(AcDbEntity* entity, AcGePoint3d point)
{
    auto* tinEntity = DnTerrainTinEntity::cast(entity);
    if (tinEntity == nullptr) {
        return;
    }
    editTerrainTinEntity(*tinEntity);
}

void TerrainTinEditorReactor::beginDoubleClick(const AcGePoint3d& clickPoint)
{
    if (g_editDialogActive) {
        return;
    }

    AcDbObjectId entityId;
    if (findImpliedTerrainTinEntity(entityId) || findTerrainTinEntityAtPoint(clickPoint, entityId)) {
        editTerrainTinEntityById(entityId);
    }
}

} // namespace

DnTerrainTinEntity::DnTerrainTinEntity() = default;

void DnTerrainTinEntity::setTinResult(const TinBuildResult& result)
{
    points_ = result.points;
    triangles_ = result.triangles;
    boundaryEdges_ = result.boundaryEdges;
    rebuildTriangleEdges();
    buildOptions_ = result.buildOptions;
    extractOptions_ = result.extractOptions;
    displayMode_ = result.buildOptions.displayMode;
    minElevation_ = result.minElevation;
    maxElevation_ = result.maxElevation;
    isDirty_ = false;
}

const std::vector<TinPoint>& DnTerrainTinEntity::getPoints() const
{
    return points_;
}

const std::vector<TinTriangle>& DnTerrainTinEntity::getTriangles() const
{
    return triangles_;
}

const std::vector<std::pair<std::size_t, std::size_t>>& DnTerrainTinEntity::getBoundaryEdges() const
{
    return boundaryEdges_;
}

const TinBuildOptions& DnTerrainTinEntity::buildOptions() const
{
    return buildOptions_;
}

const TinExtractOptions& DnTerrainTinEntity::extractOptions() const
{
    return extractOptions_;
}

double DnTerrainTinEntity::minElevation() const
{
    return minElevation_;
}

double DnTerrainTinEntity::maxElevation() const
{
    return maxElevation_;
}

TerrainTinDisplayMode DnTerrainTinEntity::displayMode() const
{
    return displayMode_;
}

void DnTerrainTinEntity::setDisplayMode(TerrainTinDisplayMode mode)
{
    displayMode_ = mode;
    buildOptions_.displayMode = mode;
}

void DnTerrainTinEntity::setBuildOptions(const TinBuildOptions& options)
{
    buildOptions_ = options;
    displayMode_ = options.displayMode;
}

bool DnTerrainTinEntity::SampleElevation(double x, double y, double& z) const
{
    assertReadEnabled();
    return TerrainSurfaceQuery::sampleElevation(points_, triangles_, x, y, z);
}

bool DnTerrainTinEntity::FindTriangle(double x, double y, TinTriangle& triangle) const
{
    assertReadEnabled();
    return TerrainSurfaceQuery::findTriangle(points_, triangles_, x, y, triangle);
}

bool DnTerrainTinEntity::RebuildFromSourceObjects()
{
    assertWriteEnabled();
    isDirty_ = true;
    return false;
}

bool DnTerrainTinEntity::isDirty() const
{
    return isDirty_;
}

Acad::ErrorStatus DnTerrainTinEntity::dwgInFields(AcDbDwgFiler* filer)
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

    Adesk::Int32 displayModeValue = 0;
    filer->readInt32(&displayModeValue);
    displayMode_ = static_cast<TerrainTinDisplayMode>(displayModeValue);
    isDirty_ = readBool(filer);

    filer->readDouble(&buildOptions_.maxEdgeLength);
    filer->readDouble(&buildOptions_.minTriangleArea);
    buildOptions_.removeDegenerateTriangles = readBool(filer);
    buildOptions_.displayMode = displayMode_;

    filer->readDouble(&extractOptions_.xyMergeTolerance);
    filer->readDouble(&extractOptions_.zEqualTolerance);
    filer->readDouble(&extractOptions_.textPointSearchDistance);
    extractOptions_.enableBlockExtraction = readBool(filer);
    extractOptions_.enableNestedBlockExtraction = readBool(filer);
    extractOptions_.useTextElevationForNearbyFlatPoint = readBool(filer);
    Adesk::Int32 maxDepth = 0;
    filer->readInt32(&maxDepth);
    extractOptions_.maxNestedBlockDepth = maxDepth;

    filer->readDouble(&minElevation_);
    filer->readDouble(&maxElevation_);

    Adesk::Int32 pointCount = 0;
    filer->readInt32(&pointCount);
    points_.clear();
    points_.reserve(pointCount > 0 ? static_cast<std::size_t>(pointCount) : 0);
    for (Adesk::Int32 i = 0; i < pointCount; ++i) {
        TinPoint point;
        filer->readDouble(&point.x);
        filer->readDouble(&point.y);
        filer->readDouble(&point.z);
        point.hasValidElevation = readBool(filer);
        Adesk::Int32 sourceType = 0;
        filer->readInt32(&sourceType);
        point.sourceType = static_cast<TinPointSourceType>(sourceType);
        point.sourceObjectHandle = readWideString(filer);
        point.associatedTextHandle = readWideString(filer);
        point.associatedGeometryHandle = readWideString(filer);
        point.blockName = readWideString(filer);
        point.attributeTag = readWideString(filer);
        point.mergedFromText = readBool(filer);
        points_.push_back(std::move(point));
    }

    Adesk::Int32 triangleCount = 0;
    filer->readInt32(&triangleCount);
    triangles_.clear();
    triangles_.reserve(triangleCount > 0 ? static_cast<std::size_t>(triangleCount) : 0);
    for (Adesk::Int32 i = 0; i < triangleCount; ++i) {
        Adesk::Int32 a = 0;
        Adesk::Int32 b = 0;
        Adesk::Int32 c = 0;
        filer->readInt32(&a);
        filer->readInt32(&b);
        filer->readInt32(&c);
        triangles_.push_back(TinTriangle{
            static_cast<std::size_t>(a),
            static_cast<std::size_t>(b),
            static_cast<std::size_t>(c)});
    }

    Adesk::Int32 edgeCount = 0;
    filer->readInt32(&edgeCount);
    boundaryEdges_.clear();
    boundaryEdges_.reserve(edgeCount > 0 ? static_cast<std::size_t>(edgeCount) : 0);
    for (Adesk::Int32 i = 0; i < edgeCount; ++i) {
        Adesk::Int32 a = 0;
        Adesk::Int32 b = 0;
        filer->readInt32(&a);
        filer->readInt32(&b);
        boundaryEdges_.push_back({static_cast<std::size_t>(a), static_cast<std::size_t>(b)});
    }
    rebuildTriangleEdges();

    return filer->filerStatus();
}

Acad::ErrorStatus DnTerrainTinEntity::dwgOutFields(AcDbDwgFiler* filer) const
{
    assertReadEnabled();
    auto status = AcDbEntity::dwgOutFields(filer);
    if (status != Acad::eOk) {
        return status;
    }

    filer->writeInt16(kEntityVersion);
    filer->writeInt32(static_cast<Adesk::Int32>(displayMode_));
    writeBool(filer, isDirty_);

    filer->writeDouble(buildOptions_.maxEdgeLength);
    filer->writeDouble(buildOptions_.minTriangleArea);
    writeBool(filer, buildOptions_.removeDegenerateTriangles);

    filer->writeDouble(extractOptions_.xyMergeTolerance);
    filer->writeDouble(extractOptions_.zEqualTolerance);
    filer->writeDouble(extractOptions_.textPointSearchDistance);
    writeBool(filer, extractOptions_.enableBlockExtraction);
    writeBool(filer, extractOptions_.enableNestedBlockExtraction);
    writeBool(filer, extractOptions_.useTextElevationForNearbyFlatPoint);
    filer->writeInt32(extractOptions_.maxNestedBlockDepth);

    filer->writeDouble(minElevation_);
    filer->writeDouble(maxElevation_);

    filer->writeInt32(static_cast<Adesk::Int32>(points_.size()));
    for (const auto& point : points_) {
        filer->writeDouble(point.x);
        filer->writeDouble(point.y);
        filer->writeDouble(point.z);
        writeBool(filer, point.hasValidElevation);
        filer->writeInt32(static_cast<Adesk::Int32>(point.sourceType));
        writeWideString(filer, point.sourceObjectHandle);
        writeWideString(filer, point.associatedTextHandle);
        writeWideString(filer, point.associatedGeometryHandle);
        writeWideString(filer, point.blockName);
        writeWideString(filer, point.attributeTag);
        writeBool(filer, point.mergedFromText);
    }

    filer->writeInt32(static_cast<Adesk::Int32>(triangles_.size()));
    for (const auto& triangle : triangles_) {
        filer->writeInt32(static_cast<Adesk::Int32>(triangle.a));
        filer->writeInt32(static_cast<Adesk::Int32>(triangle.b));
        filer->writeInt32(static_cast<Adesk::Int32>(triangle.c));
    }

    const auto edges = boundaryEdges();
    filer->writeInt32(static_cast<Adesk::Int32>(edges.size()));
    for (const auto& edge : edges) {
        filer->writeInt32(static_cast<Adesk::Int32>(edge.first));
        filer->writeInt32(static_cast<Adesk::Int32>(edge.second));
    }

    return filer->filerStatus();
}

Adesk::Boolean DnTerrainTinEntity::subWorldDraw(AcGiWorldDraw* worldDraw)
{
    assertReadEnabled();
    if (worldDraw == nullptr || points_.empty() || triangles_.empty()) {
        return Adesk::kTrue;
    }

    if (displayMode_ == TerrainTinDisplayMode::BoundaryOnly) {
        worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
        worldDraw->subEntityTraits().setColor(4);
        for (const auto& edge : boundaryEdges()) {
            if (edge.first >= points_.size() || edge.second >= points_.size()) {
                continue;
            }
            AcGePoint3d line[2] = {
                AcGePoint3d(points_[edge.first].x, points_[edge.first].y, points_[edge.first].z),
                AcGePoint3d(points_[edge.second].x, points_[edge.second].y, points_[edge.second].z)};
            worldDraw->geometry().polyline(2, line);
        }
        return Adesk::kTrue;
    }

    if (triangleEdges_.empty()) {
        rebuildTriangleEdges();
    }

    worldDraw->subEntityTraits().setFillType(kAcGiFillNever);
    for (const auto& edge : triangleEdges_) {
        if (edge.first >= points_.size() || edge.second >= points_.size()) {
            continue;
        }

        const auto avgElevation =
            (points_[edge.first].z + points_[edge.second].z) / 2.0;
        worldDraw->subEntityTraits().setTrueColor(colorForElevation(avgElevation));

        AcGePoint3d line[2] = {
            AcGePoint3d(points_[edge.first].x, points_[edge.first].y, points_[edge.first].z),
            AcGePoint3d(points_[edge.second].x, points_[edge.second].y, points_[edge.second].z)};
        worldDraw->geometry().polyline(2, line);
    }

    return Adesk::kTrue;
}

Acad::ErrorStatus DnTerrainTinEntity::subGetGeomExtents(AcDbExtents& extents) const
{
    assertReadEnabled();
    if (points_.empty()) {
        return Acad::eInvalidExtents;
    }

    for (const auto& point : points_) {
        extents.addPoint(AcGePoint3d(point.x, point.y, point.z));
    }
    return Acad::eOk;
}

Acad::ErrorStatus DnTerrainTinEntity::subTransformBy(const AcGeMatrix3d& transform)
{
    assertWriteEnabled();
    for (auto& point : points_) {
        AcGePoint3d transformed(point.x, point.y, point.z);
        transformed.transformBy(transform);
        point.x = transformed.x;
        point.y = transformed.y;
        point.z = transformed.z;
    }
    isDirty_ = true;
    return Acad::eOk;
}

std::vector<std::pair<std::size_t, std::size_t>> DnTerrainTinEntity::boundaryEdges() const
{
    if (!boundaryEdges_.empty()) {
        return boundaryEdges_;
    }

    std::unordered_map<EdgeKey, int, EdgeKeyHash> edgeCounts;
    for (const auto& triangle : triangles_) {
        ++edgeCounts[edgeKey(triangle.a, triangle.b)];
        ++edgeCounts[edgeKey(triangle.b, triangle.c)];
        ++edgeCounts[edgeKey(triangle.c, triangle.a)];
    }

    std::vector<std::pair<std::size_t, std::size_t>> edges;
    for (const auto& entry : edgeCounts) {
        if (entry.second == 1) {
            edges.push_back({entry.first.a, entry.first.b});
        }
    }
    return edges;
}

void DnTerrainTinEntity::rebuildTriangleEdges()
{
    std::unordered_map<EdgeKey, int, EdgeKeyHash> edgeCounts;
    edgeCounts.reserve(triangles_.size() * 3);
    for (const auto& triangle : triangles_) {
        ++edgeCounts[edgeKey(triangle.a, triangle.b)];
        ++edgeCounts[edgeKey(triangle.b, triangle.c)];
        ++edgeCounts[edgeKey(triangle.c, triangle.a)];
    }

    triangleEdges_.clear();
    triangleEdges_.reserve(edgeCounts.size());
    const bool shouldRebuildBoundaryEdges = boundaryEdges_.empty();
    if (shouldRebuildBoundaryEdges) {
        boundaryEdges_.reserve(edgeCounts.size());
    }

    for (const auto& entry : edgeCounts) {
        triangleEdges_.push_back({entry.first.a, entry.first.b});
        if (shouldRebuildBoundaryEdges && entry.second == 1) {
            boundaryEdges_.push_back({entry.first.a, entry.first.b});
        }
    }
}

AcCmEntityColor DnTerrainTinEntity::colorForElevation(double elevation) const
{
    if (maxElevation_ <= minElevation_) {
        return AcCmEntityColor(70, 180, 90);
    }

    struct ColorStop {
        double position;
        Adesk::UInt8 red;
        Adesk::UInt8 green;
        Adesk::UInt8 blue;
    };

    constexpr std::array<ColorStop, 6> stops = {{
        {0.00, 42, 93, 230},
        {0.22, 26, 191, 226},
        {0.44, 55, 168, 71},
        {0.66, 228, 220, 48},
        {0.82, 236, 137, 42},
        {1.00, 230, 55, 42},
    }};

    const auto ratio = std::clamp(
        (elevation - minElevation_) / (maxElevation_ - minElevation_),
        0.0,
        1.0);

    for (std::size_t i = 1; i < stops.size(); ++i) {
        if (ratio > stops[i].position) {
            continue;
        }

        const auto& previous = stops[i - 1];
        const auto& next = stops[i];
        const auto span = next.position - previous.position;
        const auto local = span <= 0.0 ? 0.0 : (ratio - previous.position) / span;
        const auto mix = [local](Adesk::UInt8 a, Adesk::UInt8 b) {
            return static_cast<Adesk::UInt8>(
                static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * local);
        };
        return AcCmEntityColor(
            mix(previous.red, next.red),
            mix(previous.green, next.green),
            mix(previous.blue, next.blue));
    }

    const auto& last = stops.back();
    return AcCmEntityColor(last.red, last.green, last.blue);
}

namespace roadproto::cad_adapter::objectarx {

void initializeTerrainTinEntityClass()
{
    TerrainTinDoubleClickEdit::rxInit();
    DnTerrainTinEntity::rxInit();
    acrxBuildClassHierarchy();
    if (g_doubleClickEdit == nullptr) {
        g_doubleClickEdit = new TerrainTinDoubleClickEdit();
        DnTerrainTinEntity::desc()->addX(AcDbDoubleClickEdit::desc(), g_doubleClickEdit);
    }
    if (g_editorReactor == nullptr && acedEditor != nullptr) {
        g_editorReactor = new TerrainTinEditorReactor();
        acedEditor->addReactor(g_editorReactor);
    }
}

void uninitializeTerrainTinEntityClass()
{
    if (g_editorReactor != nullptr && acedEditor != nullptr) {
        acedEditor->removeReactor(g_editorReactor);
    }
    delete g_editorReactor;
    g_editorReactor = nullptr;

    if (DnTerrainTinEntity::desc() != nullptr) {
        DnTerrainTinEntity::desc()->delX(AcDbDoubleClickEdit::desc());
    }
    delete g_doubleClickEdit;
    g_doubleClickEdit = nullptr;
    deleteAcRxClass(DnTerrainTinEntity::desc());
    deleteAcRxClass(TerrainTinDoubleClickEdit::desc());
}

} // namespace roadproto::cad_adapter::objectarx
