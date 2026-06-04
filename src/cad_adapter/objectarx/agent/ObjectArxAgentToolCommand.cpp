#include "cad_adapter/objectarx/agent/ObjectArxAgentToolCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "application/agent/AgentToolRequest.h"
#include "application/agent/SubgradeTemplateToolMapper.h"
#include "cad_adapter/objectarx/cross_section/DnSubgradeTemplateEntity.h"

#include "aced.h"
#include "adscodes.h"
#include "dbapserv.h"
#include "dbsymtb.h"

#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#endif

namespace roadproto::cad_adapter::objectarx::agent {
namespace {

#ifndef ROADPROTO_TEST_BUILD

constexpr std::uintmax_t kMaxAgentToolRequestFileBytes = 1024 * 1024;

std::wstring trimCommandPath(std::wstring path)
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

std::filesystem::path trustedAgentToolDirectory()
{
    wchar_t tempPath[MAX_PATH + 1] = {};
    const auto length = GetTempPathW(static_cast<DWORD>(std::size(tempPath)), tempPath);
    if (length == 0 || length > MAX_PATH) {
        return {};
    }
    return std::filesystem::path(tempPath) / L"RoadProtoAgent";
}

std::filesystem::path weaklyCanonicalPath(const std::filesystem::path& path, std::wstring& errorMessage)
{
    std::error_code error;
    auto normalized = std::filesystem::weakly_canonical(path, error);
    if (error) {
        normalized = std::filesystem::absolute(path, error);
    }
    if (error) {
        errorMessage = L"Agent 工具路径规范化失败。";
        return {};
    }
    return normalized;
}

bool isPathInsideTrustedAgentDirectory(const std::filesystem::path& path, std::wstring& errorMessage)
{
    const auto rawTrustedDirectory = trustedAgentToolDirectory();
    if (rawTrustedDirectory.empty()) {
        errorMessage = L"无法定位 Agent 工具可信临时目录。";
        return false;
    }

    const auto trustedDirectory = weaklyCanonicalPath(rawTrustedDirectory, errorMessage);
    if (trustedDirectory.empty()) {
        errorMessage = L"无法定位 Agent 工具可信临时目录。";
        return false;
    }

    const auto normalizedPath = weaklyCanonicalPath(path, errorMessage);
    if (normalizedPath.empty()) {
        return false;
    }

    auto trustedIt = trustedDirectory.begin();
    auto pathIt = normalizedPath.begin();
    for (; trustedIt != trustedDirectory.end(); ++trustedIt, ++pathIt) {
        if (pathIt == normalizedPath.end() || *trustedIt != *pathIt) {
            errorMessage = L"Agent 工具路径不在可信临时目录 %TEMP%\\RoadProtoAgent\\ 下。";
            return false;
        }
    }
    return true;
}

bool normalizeTrustedAgentToolPath(const std::wstring& rawPath, std::filesystem::path& normalizedPath, std::wstring& errorMessage)
{
    const auto trimmedPath = trimCommandPath(rawPath);
    if (trimmedPath.empty()) {
        errorMessage = L"Agent 工具请求文件路径为空。";
        return false;
    }

    normalizedPath = weaklyCanonicalPath(trimmedPath, errorMessage);
    if (normalizedPath.empty()) {
        return false;
    }
    if (!isPathInsideTrustedAgentDirectory(normalizedPath, errorMessage)) {
        return false;
    }

    std::error_code error;
    if (!std::filesystem::exists(normalizedPath, error) || error) {
        errorMessage = L"Agent 工具请求文件不存在。";
        return false;
    }
    if (!std::filesystem::is_regular_file(normalizedPath, error) || error) {
        errorMessage = L"Agent 工具请求路径不是普通文件。";
        return false;
    }

    const auto fileSize = std::filesystem::file_size(normalizedPath, error);
    if (error) {
        errorMessage = L"无法读取 Agent 工具请求文件大小。";
        return false;
    }
    if (fileSize > kMaxAgentToolRequestFileBytes) {
        errorMessage = L"Agent 工具请求文件超过 1MB 限制。";
        return false;
    }
    return true;
}

bool validateTrustedResultPath(const std::wstring& resultPath, std::wstring& errorMessage)
{
    if (resultPath.empty()) {
        return true;
    }

    const auto normalizedResultPath = weaklyCanonicalPath(trimCommandPath(resultPath), errorMessage);
    if (normalizedResultPath.empty()) {
        return false;
    }
    if (!isPathInsideTrustedAgentDirectory(normalizedResultPath, errorMessage)) {
        errorMessage = L"Agent 工具 resultPath 不在可信临时目录 %TEMP%\\RoadProtoAgent\\ 下。";
        return false;
    }
    return true;
}

std::string unicodeEscape4(unsigned int value)
{
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string result = "\\u0000";
    result[2] = kHex[(value >> 12) & 0x0F];
    result[3] = kHex[(value >> 8) & 0x0F];
    result[4] = kHex[(value >> 4) & 0x0F];
    result[5] = kHex[value & 0x0F];
    return result;
}

std::string jsonEscape(const std::wstring& value)
{
    std::string result;
    result.reserve(value.size());
    for (const auto item : value) {
        switch (item) {
        case L'\\':
            result += "\\\\";
            break;
        case L'\"':
            result += "\\\"";
            break;
        case L'\b':
            result += "\\b";
            break;
        case L'\f':
            result += "\\f";
            break;
        case L'\n':
            result += "\\n";
            break;
        case L'\r':
            result += "\\r";
            break;
        case L'\t':
            result += "\\t";
            break;
        default: {
            const auto code = static_cast<unsigned int>(item);
            if (code < 0x20) {
                result += unicodeEscape4(code);
            } else if (code <= 0x7E) {
                result.push_back(static_cast<char>(code));
            } else if (code <= 0xFFFF) {
                result += unicodeEscape4(code);
            } else {
                const auto adjusted = code - 0x10000;
                result += unicodeEscape4(0xD800 + ((adjusted >> 10) & 0x3FF));
                result += unicodeEscape4(0xDC00 + (adjusted & 0x3FF));
            }
            break;
        }
        }
    }
    return result;
}

void writeResultFile(
    const std::wstring& resultPath,
    const std::wstring& requestId,
    const std::wstring& tool,
    bool succeeded,
    const std::wstring& message,
    const std::wstring& errorCode = L"",
    const std::wstring& handle = L"")
{
    if (resultPath.empty()) {
        return;
    }

    const auto outputPath = std::filesystem::path(resultPath);
    std::error_code directoryError;
    std::filesystem::create_directories(outputPath.parent_path(), directoryError);

    std::ofstream output(outputPath, std::ios::binary);
    if (!output) {
        return;
    }

    output << "{\n";
    output << R"(  "requestId": ")" << jsonEscape(requestId) << "\",\n";
    output << R"(  "tool": ")" << jsonEscape(tool) << "\",\n";
    if (succeeded) {
        output << R"(  "succeeded": true,)" << "\n";
        output << R"(  "entityHandle": ")" << jsonEscape(handle) << "\",\n";
        output << R"(  "entityType": "DnSubgradeTemplateEntity",)" << "\n";
        output << R"(  "message": ")" << jsonEscape(message) << "\"\n";
    } else {
        output << R"(  "succeeded": false,)" << "\n";
        output << R"(  "errorCode": ")" << jsonEscape(errorCode) << "\",\n";
        output << R"(  "message": ")" << jsonEscape(message) << "\"\n";
    }
    output << "}\n";
}

void writeFailureResult(
    const application::agent::AgentToolRequest& request,
    const std::wstring& errorCode,
    const std::wstring& message)
{
    writeResultFile(request.resultPath, request.requestId, request.tool, false, message, errorCode);
}

void writeSuccessResult(const application::agent::AgentToolRequest& request, const std::wstring& handle)
{
    writeResultFile(
        request.resultPath,
        request.requestId,
        request.tool,
        true,
        L"已创建路基模板实体。",
        L"",
        handle);
}

std::string readBinaryFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

bool promptInsertionPoint(AcGePoint3d& insertionPoint)
{
    ads_point raw;
    if (acedGetPoint(nullptr, L"\n请选择 Agent 路基模板插入位置: ", raw) != RTNORM) {
        return false;
    }

    insertionPoint = AcGePoint3d(raw[0], raw[1], raw[2]);
    return true;
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
    const auto status = blockTable->getAt(ACDB_MODEL_SPACE, modelSpace, AcDb::kForWrite);
    blockTable->close();
    if (status != Acad::eOk || modelSpace == nullptr) {
        return false;
    }

    const auto appendStatus = modelSpace->appendAcDbEntity(entityId, entity);
    modelSpace->close();
    return appendStatus == Acad::eOk;
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

bool validateInsertionPointRequest(
    const application::agent::AgentToolPoint& requestPoint,
    AcGePoint3d& insertionPoint,
    bool& shouldPrompt,
    std::wstring& errorMessage)
{
    shouldPrompt = false;
    if (requestPoint.mode == L"Explicit") {
        if (!requestPoint.hasX || !requestPoint.hasY) {
            errorMessage = L"Explicit insertionPoint requires x and y.";
            return false;
        }
        const auto z = requestPoint.hasZ ? requestPoint.z : 0.0;
        if (!std::isfinite(requestPoint.x) || !std::isfinite(requestPoint.y) || !std::isfinite(z)) {
            errorMessage = L"Explicit insertionPoint coordinates must be finite.";
            return false;
        }
        insertionPoint = AcGePoint3d(requestPoint.x, requestPoint.y, z);
        return true;
    }

    if (requestPoint.mode != L"PickInCad") {
        errorMessage = L"Agent insertionPoint.mode must be Explicit or PickInCad.";
        return false;
    }

    shouldPrompt = true;
    return true;
}

void runAgentExecuteToolFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto Agent tool request file: ", pathBuffer) != RTNORM) {
        return;
    }

    std::wstring error;
    std::filesystem::path requestPath;
    if (!normalizeTrustedAgentToolPath(pathBuffer, requestPath, error)) {
        editor.writeError(error.empty() ? L"Agent 工具请求文件路径不可信。" : error);
        return;
    }

    const auto request = application::agent::parseAgentToolRequestJson(readBinaryFile(requestPath), error);
    if (!request.succeeded) {
        editor.writeError(error.empty() ? L"Agent 工具请求解析失败。" : error);
        return;
    }

    if (!validateTrustedResultPath(request.resultPath, error)) {
        editor.writeError(error.empty() ? L"Agent 工具 resultPath 不可信。" : error);
        return;
    }

    const auto dataResult = application::agent::buildSubgradeTemplateToolData(request.arguments, error);
    if (!dataResult.succeeded) {
        const auto message = error.empty() ? L"Agent 路基模板参数无效。" : error;
        writeFailureResult(request, L"InvalidArguments", message);
        editor.writeError(message);
        return;
    }

    AcGePoint3d insertionPoint(0.0, 0.0, 0.0);
    bool shouldPrompt = false;
    if (!validateInsertionPointRequest(request.arguments.insertionPoint, insertionPoint, shouldPrompt, error)) {
        const auto message = error.empty() ? L"Agent 插入点参数无效。" : error;
        writeFailureResult(request, L"InvalidInsertionPoint", message);
        editor.writeError(message);
        return;
    }
    if (shouldPrompt && !promptInsertionPoint(insertionPoint)) {
        const auto message = L"Agent 路基模板创建已取消。";
        writeFailureResult(request, L"Cancelled", message);
        editor.writeWarning(message);
        return;
    }

    auto* entity = new DnSubgradeTemplateEntity();
    entity->setTemplateData(dataResult.data);
    entity->setInsertionPoint(insertionPoint);

    AcDbObjectId entityId;
    if (!appendEntityToModelSpace(entity, entityId)) {
        delete entity;
        const auto message = L"Agent 插入 DnSubgradeTemplateEntity 失败。";
        writeFailureResult(request, L"CreateEntityFailed", message);
        editor.writeError(message);
        return;
    }

    const auto handle = entityHandleText(entity);
    writeSuccessResult(request, handle);
    entity->close();
    acedUpdateDisplay();
    editor.writeMessage(L"Agent 已创建路基模板实体，handle: " + handle);
}

#else

void runAgentExecuteToolFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure agentExecuteToolFileCommandProcedure()
{
    return &runAgentExecuteToolFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::agent
