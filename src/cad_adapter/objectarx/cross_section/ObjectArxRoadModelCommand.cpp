#include "cad_adapter/objectarx/cross_section/ObjectArxRoadModelCommand.h"

#ifndef ROADPROTO_TEST_BUILD
#include "app/startup/ApplicationContext.h"
#include "cad_adapter/common/IEditor.h"
#include "cad_adapter/objectarx/cross_section/RoadModelDialogBridge.h"

#include "aced.h"
#include "adscodes.h"

#include <cwctype>
#include <sstream>
#include <string>
#endif

namespace roadproto::cad_adapter::objectarx::cross_section {
namespace {

#ifndef ROADPROTO_TEST_BUILD

std::wstring trimDialogCommandPath(std::wstring path)
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

bool promptRoadModelHandle(std::wstring& handle)
{
    ACHAR handleBuffer[128] = {};
    if (acedGetString(Adesk::kFalse, L"\nRoad model handle: ", handleBuffer) != RTNORM) {
        return false;
    }

    handle = trimDialogCommandPath(handleBuffer);
    return !handle.empty();
}

bool queueRoadModelDialog(const std::wstring& handle)
{
    auto& editor = app::ApplicationContext::instance().editor();

    RoadModelDialogRequest request;
    request.handle = handle;

    std::wstring errorMessage;
    if (!queueRoadModelWpfDialog(request, errorMessage)) {
        editor.writeError(L"Failed to open road model WPF dialog: " + errorMessage);
        return false;
    }

    return true;
}

void writeReceivedRoadModelMessage(
    roadproto::cad_adapter::IEditor& editor,
    const RoadModelDialogResponse& response)
{
    std::wstringstream stream;
    stream << L"Received road model dialog configuration";
    if (!response.handle.empty()) {
        stream << L" for handle " << response.handle;
    }
    stream << L": centerline=" << response.roadCenterlineHandle
           << L", verticalCurve=" << response.profileVerticalCurveHandle
           << L", sampleInterval=" << response.sampleInterval
           << L", assignmentCount=" << response.assignments.size() << L".";
    editor.writeMessage(stream.str());
}

void runRoadModelCreateCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_SECTION_ROAD_MODEL_CREATE: opening road model WPF dialog.");
    queueRoadModelDialog(L"");
}

void runRoadModelEditCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    editor.writeMessage(L"RD_SECTION_ROAD_MODEL_EDIT: enter a road model handle.");

    std::wstring handle;
    if (!promptRoadModelHandle(handle)) {
        editor.writeWarning(L"Road model edit was cancelled.");
        return;
    }

    queueRoadModelDialog(handle);
}

void runRoadModelEditHandleCommand()
{
    std::wstring handle;
    if (!promptRoadModelHandle(handle)) {
        return;
    }

    queueRoadModelDialog(handle);
}

void runRoadModelApplyDialogFileCommand()
{
    auto& editor = app::ApplicationContext::instance().editor();
    ACHAR pathBuffer[1024] = {};
    if (acedGetString(Adesk::kTrue, L"\nRoadProto road model dialog response file: ", pathBuffer) != RTNORM) {
        return;
    }

    RoadModelDialogResponse response;
    std::wstring errorMessage;
    const auto responsePath = trimDialogCommandPath(pathBuffer);
    if (!readRoadModelDialogResponse(responsePath, response, errorMessage)) {
        editor.writeError(L"Failed to read road model dialog response: " + errorMessage);
        return;
    }

    if (!response.accepted) {
        return;
    }

    writeReceivedRoadModelMessage(editor, response);
}

#else

void runRoadModelCreateCommand()
{
}

void runRoadModelEditCommand()
{
}

void runRoadModelEditHandleCommand()
{
}

void runRoadModelApplyDialogFileCommand()
{
}

#endif

} // namespace

core::CommandProcedure roadModelCreateCommandProcedure()
{
    return &runRoadModelCreateCommand;
}

core::CommandProcedure roadModelEditCommandProcedure()
{
    return &runRoadModelEditCommand;
}

core::CommandProcedure roadModelEditHandleCommandProcedure()
{
    return &runRoadModelEditHandleCommand;
}

core::CommandProcedure roadModelApplyDialogFileCommandProcedure()
{
    return &runRoadModelApplyDialogFileCommand;
}

} // namespace roadproto::cad_adapter::objectarx::cross_section
