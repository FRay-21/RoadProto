#include "domain/terrain/TerrainMeshFile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>

namespace roadproto::domain::terrain {
namespace {

constexpr std::array<char, 15> kMagic = {'R', 'O', 'A', 'D', 'P', 'R', 'O', 'T', 'O', '_', 'R', 'M', 'E', 'S', 'H'};
constexpr std::uint32_t kFileVersion = 1;
constexpr std::uint64_t kMaxReasonableItemCount = 20000000;

std::wstring widenAscii(const std::string& value)
{
    return std::wstring(value.begin(), value.end());
}

std::string toUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("string too long for UTF-8 conversion");
    }

    const auto sourceLength = static_cast<int>(value.size());
    const auto requiredSize = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        sourceLength,
        nullptr,
        0,
        nullptr,
        nullptr);
    if (requiredSize <= 0) {
        throw std::runtime_error("failed to convert string to UTF-8");
    }

    std::string utf8(static_cast<std::size_t>(requiredSize), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        value.data(),
        sourceLength,
        utf8.data(),
        requiredSize,
        nullptr,
        nullptr);
    return utf8;
}

std::wstring fromUtf8(const std::string& value)
{
    if (value.empty()) {
        return {};
    }
    if (value.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("string too long for UTF-8 conversion");
    }

    const auto sourceLength = static_cast<int>(value.size());
    const auto requiredSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), sourceLength, nullptr, 0);
    if (requiredSize <= 0) {
        throw std::runtime_error("invalid UTF-8 string in rmesh file");
    }

    std::wstring wide(static_cast<std::size_t>(requiredSize), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), sourceLength, wide.data(), requiredSize);
    return wide;
}

template <typename T>
void writePod(std::ostream& stream, const T& value)
{
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
    if (!stream) {
        throw std::runtime_error("failed to write rmesh file");
    }
}

template <typename T>
T readPod(std::istream& stream)
{
    T value{};
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    if (!stream) {
        throw std::runtime_error("unexpected end of rmesh file");
    }
    return value;
}

void writeBool(std::ostream& stream, bool value)
{
    writePod<std::uint8_t>(stream, value ? 1 : 0);
}

bool readBool(std::istream& stream)
{
    const auto value = readPod<std::uint8_t>(stream);
    if (value > 1) {
        throw std::runtime_error("invalid boolean value in rmesh file");
    }
    return value == 1;
}

void writeString(std::ostream& stream, const std::wstring& value)
{
    const auto utf8 = toUtf8(value);
    if (utf8.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("string too long for rmesh file");
    }
    writePod<std::uint32_t>(stream, static_cast<std::uint32_t>(utf8.size()));
    if (!utf8.empty()) {
        stream.write(utf8.data(), static_cast<std::streamsize>(utf8.size()));
        if (!stream) {
            throw std::runtime_error("failed to write rmesh string");
        }
    }
}

std::wstring readString(std::istream& stream)
{
    const auto size = readPod<std::uint32_t>(stream);
    std::string utf8(size, '\0');
    if (size != 0) {
        stream.read(utf8.data(), static_cast<std::streamsize>(size));
        if (!stream) {
            throw std::runtime_error("unexpected end of rmesh string");
        }
    }
    return fromUtf8(utf8);
}

void writeExtractOptions(std::ostream& stream, const TinExtractOptions& options)
{
    writePod(stream, options.xyMergeTolerance);
    writePod(stream, options.zEqualTolerance);
    writePod(stream, options.textPointSearchDistance);
    writeBool(stream, options.enableBlockExtraction);
    writeBool(stream, options.enableNestedBlockExtraction);
    writeBool(stream, options.useTextElevationForNearbyFlatPoint);
    writePod<std::int32_t>(stream, static_cast<std::int32_t>(options.maxNestedBlockDepth));
}

TinExtractOptions readExtractOptions(std::istream& stream)
{
    TinExtractOptions options;
    options.xyMergeTolerance = readPod<double>(stream);
    options.zEqualTolerance = readPod<double>(stream);
    options.textPointSearchDistance = readPod<double>(stream);
    options.enableBlockExtraction = readBool(stream);
    options.enableNestedBlockExtraction = readBool(stream);
    options.useTextElevationForNearbyFlatPoint = readBool(stream);
    options.maxNestedBlockDepth = readPod<std::int32_t>(stream);
    return options;
}

void writeBuildOptions(std::ostream& stream, const TinBuildOptions& options)
{
    writePod(stream, options.maxEdgeLength);
    writePod(stream, options.minTriangleArea);
    writeBool(stream, options.removeDegenerateTriangles);
    writePod<std::int32_t>(stream, static_cast<std::int32_t>(options.displayMode));
}

TinBuildOptions readBuildOptions(std::istream& stream)
{
    TinBuildOptions options;
    options.maxEdgeLength = readPod<double>(stream);
    options.minTriangleArea = readPod<double>(stream);
    options.removeDegenerateTriangles = readBool(stream);
    options.displayMode = static_cast<TerrainTinDisplayMode>(readPod<std::int32_t>(stream));
    return options;
}

void writeExtractSummary(std::ostream& stream, const TinExtractSummary& summary)
{
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.selectedObjectCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.rawPointCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.validPointCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.duplicatePointCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.zConflictPointCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.invalidObjectCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.textElevationPointCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.textPointMergeCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.textAssignedElevationPointCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.ambiguousTextPointMatchCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.blockCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.recognizedElevationBlockCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.blockAttributeElevationCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.blockParseFailedCount));
    writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(summary.triangleCount));
    writeString(stream, summary.status);
}

TinExtractSummary readExtractSummary(std::istream& stream)
{
    TinExtractSummary summary;
    summary.selectedObjectCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.rawPointCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.validPointCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.duplicatePointCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.zConflictPointCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.invalidObjectCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.textElevationPointCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.textPointMergeCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.textAssignedElevationPointCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.ambiguousTextPointMatchCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.blockCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.recognizedElevationBlockCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.blockAttributeElevationCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.blockParseFailedCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.triangleCount = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
    summary.status = readString(stream);
    return summary;
}

void writePoint(std::ostream& stream, const TinPoint& point)
{
    writePod(stream, point.x);
    writePod(stream, point.y);
    writePod(stream, point.z);
    writeBool(stream, point.hasValidElevation);
    writePod<std::int32_t>(stream, static_cast<std::int32_t>(point.sourceType));
    writeString(stream, point.sourceObjectHandle);
    writeString(stream, point.associatedTextHandle);
    writeString(stream, point.associatedGeometryHandle);
    writeString(stream, point.blockName);
    writeString(stream, point.attributeTag);
    writeBool(stream, point.mergedFromText);
}

TinPoint readPoint(std::istream& stream)
{
    TinPoint point;
    point.x = readPod<double>(stream);
    point.y = readPod<double>(stream);
    point.z = readPod<double>(stream);
    point.hasValidElevation = readBool(stream);
    point.sourceType = static_cast<TinPointSourceType>(readPod<std::int32_t>(stream));
    point.sourceObjectHandle = readString(stream);
    point.associatedTextHandle = readString(stream);
    point.associatedGeometryHandle = readString(stream);
    point.blockName = readString(stream);
    point.attributeTag = readString(stream);
    point.mergedFromText = readBool(stream);
    return point;
}

void validateCount(std::uint64_t count, const char* label)
{
    if (count > kMaxReasonableItemCount) {
        throw std::runtime_error(std::string(label) + " count is too large");
    }
}

void validateIndex(std::size_t index, std::size_t pointCount, const char* label)
{
    if (index >= pointCount) {
        throw std::runtime_error(std::string(label) + " references an out-of-range point");
    }
}

} // namespace

bool TerrainMeshFile::write(const std::wstring& path, const TinBuildResult& mesh, std::wstring& errorMessage) const
{
    errorMessage.clear();

    try {
        std::ofstream stream(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
        if (!stream) {
            errorMessage = L"无法创建 RMesh 文件";
            return false;
        }

        stream.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
        writePod<std::uint32_t>(stream, kFileVersion);
        writeExtractOptions(stream, mesh.extractOptions);
        writeBuildOptions(stream, mesh.buildOptions);
        writeExtractSummary(stream, mesh.extractSummary);
        writePod(stream, mesh.minElevation);
        writePod(stream, mesh.maxElevation);

        writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(mesh.points.size()));
        for (const auto& point : mesh.points) {
            writePoint(stream, point);
        }

        writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(mesh.triangles.size()));
        for (const auto& triangle : mesh.triangles) {
            writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(triangle.a));
            writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(triangle.b));
            writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(triangle.c));
        }

        writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(mesh.boundaryEdges.size()));
        for (const auto& edge : mesh.boundaryEdges) {
            writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(edge.first));
            writePod<std::uint64_t>(stream, static_cast<std::uint64_t>(edge.second));
        }

        return true;
    } catch (const std::exception& error) {
        errorMessage = L"写入 RMesh 文件失败: " + widenAscii(error.what());
    } catch (...) {
        errorMessage = L"写入 RMesh 文件失败";
    }

    return false;
}

TerrainMeshFileReadResult TerrainMeshFile::read(const std::wstring& path) const
{
    TerrainMeshFileReadResult result;

    try {
        std::ifstream stream(std::filesystem::path(path), std::ios::binary);
        if (!stream) {
            result.errorMessage = L"无法打开 RMesh 文件";
            return result;
        }

        std::array<char, kMagic.size()> magic{};
        stream.read(magic.data(), static_cast<std::streamsize>(magic.size()));
        if (!stream || magic != kMagic) {
            result.errorMessage = L"不是有效的 RoadProto RMesh 文件";
            return result;
        }

        const auto version = readPod<std::uint32_t>(stream);
        if (version != kFileVersion) {
            result.errorMessage = L"RMesh 文件版本不受支持";
            return result;
        }

        TinBuildResult mesh;
        mesh.extractOptions = readExtractOptions(stream);
        mesh.buildOptions = readBuildOptions(stream);
        mesh.extractSummary = readExtractSummary(stream);
        mesh.minElevation = readPod<double>(stream);
        mesh.maxElevation = readPod<double>(stream);

        const auto pointCount = readPod<std::uint64_t>(stream);
        validateCount(pointCount, "point");
        mesh.points.reserve(static_cast<std::size_t>(pointCount));
        for (std::uint64_t i = 0; i < pointCount; ++i) {
            mesh.points.push_back(readPoint(stream));
        }

        const auto triangleCount = readPod<std::uint64_t>(stream);
        validateCount(triangleCount, "triangle");
        mesh.triangles.reserve(static_cast<std::size_t>(triangleCount));
        for (std::uint64_t i = 0; i < triangleCount; ++i) {
            TinTriangle triangle;
            triangle.a = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
            triangle.b = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
            triangle.c = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
            validateIndex(triangle.a, mesh.points.size(), "triangle");
            validateIndex(triangle.b, mesh.points.size(), "triangle");
            validateIndex(triangle.c, mesh.points.size(), "triangle");
            mesh.triangles.push_back(triangle);
        }

        const auto boundaryEdgeCount = readPod<std::uint64_t>(stream);
        validateCount(boundaryEdgeCount, "boundary edge");
        mesh.boundaryEdges.reserve(static_cast<std::size_t>(boundaryEdgeCount));
        for (std::uint64_t i = 0; i < boundaryEdgeCount; ++i) {
            const auto first = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
            const auto second = static_cast<std::size_t>(readPod<std::uint64_t>(stream));
            validateIndex(first, mesh.points.size(), "boundary edge");
            validateIndex(second, mesh.points.size(), "boundary edge");
            mesh.boundaryEdges.emplace_back(first, second);
        }

        mesh.succeeded = true;
        mesh.errorMessage.clear();
        mesh.extractSummary.validPointCount = mesh.points.size();
        mesh.extractSummary.triangleCount = mesh.triangles.size();

        result.succeeded = true;
        result.mesh = std::move(mesh);
        return result;
    } catch (const std::exception& error) {
        result.errorMessage = L"读取 RMesh 文件失败: " + widenAscii(error.what());
    } catch (...) {
        result.errorMessage = L"读取 RMesh 文件失败";
    }

    return result;
}

} // namespace roadproto::domain::terrain
