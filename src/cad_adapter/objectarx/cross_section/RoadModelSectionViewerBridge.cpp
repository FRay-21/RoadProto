#include "cad_adapter/objectarx/cross_section/RoadModelSectionViewerBridge.h"

#include "domain/alignment/StationFormatter.h"

#include "acdocman.h"

#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

std::string wideToUtf8(const std::wstring& value)
{
    if (value.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (size <= 0) {
        return {};
    }

    std::string output(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        output.data(),
        size,
        nullptr,
        nullptr);
    return output;
}

std::string escapeValue(const std::wstring& value)
{
    const auto utf8 = wideToUtf8(value);
    std::ostringstream escaped;
    escaped << std::uppercase << std::hex;
    for (const unsigned char ch : utf8) {
        if (ch == '%' || ch == '\r' || ch == '\n') {
            escaped << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        } else {
            escaped << static_cast<char>(ch);
        }
    }
    return escaped.str();
}

std::wstring tempFilePath(const wchar_t* suffix)
{
    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);

    const auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::wstringstream name;
    name << L"RoadProtoRoadModelSectionViewer_" << GetCurrentProcessId() << L"_" << now << suffix;
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

std::wstring pendingRequestPath()
{
    wchar_t tempPath[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, tempPath);

    std::wstringstream name;
    name << L"RoadProtoRoadModelSectionViewer_" << GetCurrentProcessId() << L".pending";
    return (std::filesystem::path(tempPath) / name.str()).wstring();
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, const std::wstring& value)
{
    stream << wideToUtf8(key) << '=' << escapeValue(value) << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, bool value)
{
    stream << wideToUtf8(key) << '=' << (value ? 1 : 0) << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, int value)
{
    stream << wideToUtf8(key) << '=' << value << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, double value)
{
    stream << wideToUtf8(key) << '=' << std::setprecision(17) << value << '\n';
}

void writeKeyValue(std::ostream& stream, const std::wstring& key, std::size_t value)
{
    stream << wideToUtf8(key) << '=' << value << '\n';
}

std::wstring segmentKindText(roadproto::domain::cross_section::RoadModelSectionPreviewSegmentKind kind)
{
    using roadproto::domain::cross_section::RoadModelSectionPreviewSegmentKind;
    switch (kind) {
    case RoadModelSectionPreviewSegmentKind::Slope:
        return L"Slope";
    case RoadModelSectionPreviewSegmentKind::Ground:
        return L"Ground";
    case RoadModelSectionPreviewSegmentKind::PavementLayer:
        return L"PavementLayer";
    case RoadModelSectionPreviewSegmentKind::Subgrade:
    default:
        return L"Subgrade";
    }
}

bool writeRequestFile(
    const RoadModelSectionViewerRequest& request,
    const std::wstring& requestPath,
    std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(requestPath), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write road model section viewer request file.";
        return false;
    }

    writeKeyValue(stream, L"handle", request.handle);
    writeKeyValue(stream, L"roadCenterlineHandle", request.roadCenterlineHandle);
    writeKeyValue(stream, L"previewCount", request.previews.size());
    for (std::size_t i = 0; i < request.previews.size(); ++i) {
        const auto& preview = request.previews[i].data;
        const auto previewPrefix = L"preview." + std::to_wstring(i);
        writeKeyValue(stream, previewPrefix + L".station", preview.station);
        writeKeyValue(stream, previewPrefix + L".stationLabel", roadproto::domain::alignment::StationFormatter::format(preview.station));
        writeKeyValue(stream, previewPrefix + L".statusMessage", preview.statusMessage);
        writeKeyValue(stream, previewPrefix + L".hasGroundLine", preview.hasGroundLine);
        writeKeyValue(stream, previewPrefix + L".segmentCount", preview.segments.size());

        for (std::size_t j = 0; j < preview.segments.size(); ++j) {
            const auto& segment = preview.segments[j];
            const auto segmentPrefix = previewPrefix + L".segment." + std::to_wstring(j);
            writeKeyValue(stream, segmentPrefix + L".kind", segmentKindText(segment.kind));
            writeKeyValue(stream, segmentPrefix + L".label", segment.label);
            writeKeyValue(stream, segmentPrefix + L".colorR", segment.color.r);
            writeKeyValue(stream, segmentPrefix + L".colorG", segment.color.g);
            writeKeyValue(stream, segmentPrefix + L".colorB", segment.color.b);
            writeKeyValue(stream, segmentPrefix + L".pointCount", segment.points.size());
            for (std::size_t k = 0; k < segment.points.size(); ++k) {
                const auto pointPrefix = segmentPrefix + L".point." + std::to_wstring(k);
                writeKeyValue(stream, pointPrefix + L".offset", segment.points[k].offset);
                writeKeyValue(stream, pointPrefix + L".elevation", segment.points[k].elevation);
            }
        }
    }

    return true;
}

bool writePendingRequestPath(const std::wstring& requestPath, std::wstring& errorMessage)
{
    std::ofstream stream(std::filesystem::path(pendingRequestPath()), std::ios::binary);
    if (!stream) {
        errorMessage = L"Cannot write road model section viewer pending request path.";
        return false;
    }

    stream << wideToUtf8(requestPath);
    return true;
}

void removeFileIfExists(const std::wstring& path)
{
    if (path.empty()) {
        return;
    }

    std::error_code error;
    std::filesystem::remove(std::filesystem::path(path), error);
}

} // namespace

bool queueRoadModelSectionViewerWpfDialog(
    const RoadModelSectionViewerRequest& request,
    std::wstring& errorMessage)
{
    const auto requestPath = tempFilePath(L".request");
    if (!writeRequestFile(request, requestPath, errorMessage)) {
        return false;
    }
    if (!writePendingRequestPath(requestPath, errorMessage)) {
        removeFileIfExists(requestPath);
        return false;
    }

    auto* document = acDocManager == nullptr ? nullptr : acDocManager->curDocument();
    if (document == nullptr) {
        removeFileIfExists(pendingRequestPath());
        removeFileIfExists(requestPath);
        errorMessage = L"No active AutoCAD document is available.";
        return false;
    }

    const std::wstring command = L"RD_SECTION_ROAD_MODEL_VIEW_SECTION_SHOW_WPF_DIALOG\n";
    acDocManager->sendStringToExecute(document, command.c_str(), true, false, false);
    return true;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
