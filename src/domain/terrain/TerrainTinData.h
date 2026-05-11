#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace roadproto::domain::terrain {

enum class TinPointSourceType {
    Unknown,
    CadPoint,
    LineVertex,
    PolylineVertex,
    TextElevation,
    BlockAttribute,
    BlockText,
    BlockGeometry,
};

enum class TerrainTinDisplayMode {
    ColoredTriangles,
    BoundaryOnly,
};

struct TinPoint {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    bool hasValidElevation = true;
    TinPointSourceType sourceType = TinPointSourceType::Unknown;
    std::wstring sourceObjectHandle;
    std::wstring associatedTextHandle;
    std::wstring associatedGeometryHandle;
    std::wstring blockName;
    std::wstring attributeTag;
    bool mergedFromText = false;
};

struct TinTriangle {
    std::size_t a = 0;
    std::size_t b = 0;
    std::size_t c = 0;
};

struct TinExtractOptions {
    double xyMergeTolerance = 0.001;
    double zEqualTolerance = 0.01;
    double textPointSearchDistance = 0.5;
    bool enableBlockExtraction = true;
    bool enableNestedBlockExtraction = true;
    bool useTextElevationForNearbyFlatPoint = true;
    int maxNestedBlockDepth = 3;
};

struct TinBuildOptions {
    double maxEdgeLength = 0.0;
    double minTriangleArea = 1e-8;
    bool removeDegenerateTriangles = true;
    TerrainTinDisplayMode displayMode = TerrainTinDisplayMode::ColoredTriangles;
};

struct TinExtractSummary {
    std::size_t selectedObjectCount = 0;
    std::size_t rawPointCount = 0;
    std::size_t validPointCount = 0;
    std::size_t duplicatePointCount = 0;
    std::size_t zConflictPointCount = 0;
    std::size_t invalidObjectCount = 0;
    std::size_t textElevationPointCount = 0;
    std::size_t textPointMergeCount = 0;
    std::size_t textAssignedElevationPointCount = 0;
    std::size_t ambiguousTextPointMatchCount = 0;
    std::size_t blockCount = 0;
    std::size_t recognizedElevationBlockCount = 0;
    std::size_t blockAttributeElevationCount = 0;
    std::size_t blockParseFailedCount = 0;
    std::size_t triangleCount = 0;
    std::wstring status;
};

struct TinBuildResult {
    bool succeeded = false;
    std::wstring errorMessage;
    std::vector<TinPoint> points;
    std::vector<TinTriangle> triangles;
    std::vector<std::pair<std::size_t, std::size_t>> boundaryEdges;
    TinExtractSummary extractSummary;
    TinBuildOptions buildOptions;
    TinExtractOptions extractOptions;
    double minElevation = 0.0;
    double maxElevation = 0.0;
};

struct TinNormalizeResult {
    std::vector<TinPoint> points;
    TinExtractSummary summary;
};

} // namespace roadproto::domain::terrain
