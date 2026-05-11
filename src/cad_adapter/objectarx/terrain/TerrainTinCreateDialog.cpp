#include "cad_adapter/objectarx/terrain/TerrainTinCreateDialog.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <CommCtrl.h>

#pragma comment(lib, "comctl32.lib")

#include <cstdlib>
#include <cwctype>
#include <sstream>

using roadproto::domain::terrain::TerrainTinDisplayMode;
using roadproto::domain::terrain::TinBuildOptions;

namespace roadproto::cad_adapter::objectarx {

namespace {

constexpr int kDialogWidth = 640;
constexpr int kDialogHeight = 560;
constexpr int kMargin = 22;

constexpr int kIdStats = 100;
constexpr int kIdXyTolerance = 101;
constexpr int kIdMaxEdge = 102;
constexpr int kIdRemoveDegenerate = 103;
constexpr int kIdDisplayMode = 104;
constexpr int kIdProgress = 105;
constexpr int kIdStatus = 106;
constexpr int kIdGenerate = 201;
constexpr int kIdApply = 202;
constexpr int kIdCancel = 203;
constexpr int kIdConfirm = 204;
constexpr int kIdPreview = 205;

enum class DialogMode {
    Create,
    Edit,
};

struct DialogState {
    DialogMode mode = DialogMode::Create;
    TerrainTinCreateDialogInput createInput;
    TerrainTinCreateDialogResult* createResult = nullptr;
    TerrainTinGenerateCallback createGenerateCallback;
    TerrainTinEditDialogInput editInput;
    TerrainTinEditDialogResult* editResult = nullptr;
    HWND owner = nullptr;
    HWND progress = nullptr;
    HWND statusText = nullptr;
    HWND generateButton = nullptr;
    HWND confirmButton = nullptr;
    HWND cancelButton = nullptr;
    HWND previewButton = nullptr;
    HFONT titleFont = nullptr;
    HFONT bodyFont = nullptr;
    HBRUSH windowBrush = nullptr;
    HBRUSH panelBrush = nullptr;
    HBRUSH editBrush = nullptr;
    bool hasGenerated = false;
    bool done = false;
};

std::wstring numberText(double value)
{
    std::wostringstream stream;
    stream.precision(12);
    stream << value;
    return stream.str();
}

std::wstring createStatsText(const TerrainTinCreateDialogInput& input)
{
    const auto& summary = input.summary;
    std::wostringstream stream;
    stream << L"\u6837\u672c\u56fe\u5c42: " << input.sampleLayer << L"\r\n"
           << L"\u6837\u672c\u7c7b\u578b: " << input.sampleType << L"\r\n"
           << L"\u5df2\u9690\u85cf\u6e90\u5bf9\u8c61: " << input.hiddenSourceObjectCount << L"\r\n"
           << L"\u540c\u56fe\u5c42\u540c\u7c7b\u578b\u5bf9\u8c61: " << summary.selectedObjectCount << L"\r\n"
           << L"\u539f\u59cb\u70b9\u6570: " << summary.rawPointCount << L"\r\n"
           << L"\u6709\u6548\u70b9\u6570: " << summary.validPointCount << L"\r\n"
           << L"\u91cd\u590d\u70b9: " << summary.duplicatePointCount << L"\r\n"
           << L"Z \u51b2\u7a81\u70b9: " << summary.zConflictPointCount << L"\r\n"
           << L"\u6587\u5b57\u9ad8\u7a0b\u70b9: " << summary.textElevationPointCount << L"\r\n"
           << L"\u6587\u5b57-\u70b9\u5408\u5e76: " << summary.textPointMergeCount << L"\r\n"
           << L"\u6587\u5b57\u8d4b\u9ad8\u7a0b: " << summary.textAssignedElevationPointCount << L"\r\n"
           << L"\u5757\u6570\u91cf: " << summary.blockCount << L"\r\n"
           << L"\u5757\u5c5e\u6027\u9ad8\u7a0b: " << summary.blockAttributeElevationCount << L"\r\n"
           << L"\u5757\u89e3\u6790\u5931\u8d25: " << summary.blockParseFailedCount << L"\r\n"
           << L"\u65e0\u6548\u5bf9\u8c61: " << summary.invalidObjectCount;
    return stream.str();
}

std::wstring editStatsText(const TerrainTinEditDialogInput& input)
{
    std::wostringstream stream;
    stream << L"\u6570\u6a21\u70b9\u6570: " << input.pointCount << L"\r\n"
           << L"\u4e09\u89d2\u5f62\u6570: " << input.triangleCount << L"\r\n"
           << L"\u8fb9\u754c\u8fb9\u6570: " << input.boundaryEdgeCount << L"\r\n"
           << L"\u6700\u4f4e\u9ad8\u7a0b: " << input.minElevation << L"\r\n"
           << L"\u6700\u9ad8\u9ad8\u7a0b: " << input.maxElevation << L"\r\n"
           << L"\r\n"
           << L"\u53ef\u4fee\u6539\u663e\u793a\u6a21\u5f0f\u548c\u6784\u7f51\u53c2\u6570\u3002\r\n"
           << L"\u201c\u91cd\u65b0\u751f\u6210\u201d\u4f1a\u4f7f\u7528\u5df2\u4fdd\u5b58\u7684\u5730\u5f62\u70b9\u91cd\u6784\u4e09\u89d2\u7f51\u3002";
    return stream.str();
}

std::wstring getText(HWND dialog, int controlId)
{
    wchar_t buffer[128] = {};
    GetDlgItemTextW(dialog, controlId, buffer, static_cast<int>(sizeof(buffer) / sizeof(buffer[0])));
    return buffer;
}

bool parseDouble(HWND dialog, int controlId, double& value)
{
    const auto text = getText(dialog, controlId);
    wchar_t* end = nullptr;
    const auto parsed = std::wcstod(text.c_str(), &end);
    if (end == text.c_str()) {
        return false;
    }

    while (end != nullptr && *end != L'\0') {
        if (!iswspace(*end)) {
            return false;
        }
        ++end;
    }

    value = parsed;
    return true;
}

void setFont(HWND control, HFONT font)
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
}

HWND createStatic(HWND parent, const wchar_t* text, int x, int y, int width, int height, HFONT font)
{
    HWND control = CreateWindowExW(
        0,
        L"STATIC",
        text,
        WS_CHILD | WS_VISIBLE,
        x,
        y,
        width,
        height,
        parent,
        nullptr,
        nullptr,
        nullptr);
    setFont(control, font);
    return control;
}

HWND createGroup(HWND parent, const wchar_t* text, int x, int y, int width, int height, HFONT font)
{
    HWND control = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        x,
        y,
        width,
        height,
        parent,
        nullptr,
        nullptr,
        nullptr);
    setFont(control, font);
    return control;
}

HWND createEdit(HWND parent, int id, const std::wstring& text, int x, int y, int width, HFONT font)
{
    HWND control = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        text.c_str(),
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
        x,
        y,
        width,
        26,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        nullptr,
        nullptr);
    setFont(control, font);
    return control;
}

HWND createButton(HWND parent, int id, const wchar_t* text, int x, int y, int width, int height, DWORD extraStyle, HFONT font)
{
    HWND control = CreateWindowExW(
        0,
        L"BUTTON",
        text,
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | extraStyle,
        x,
        y,
        width,
        height,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        nullptr,
        nullptr);
    setFont(control, font);
    return control;
}

void pumpDialogMessages(HWND dialog)
{
    MSG message = {};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
}

void setDialogProgress(HWND dialog, DialogState& state, int percent, const std::wstring& message)
{
    const auto clamped = percent < 0 ? 0 : (percent > 100 ? 100 : percent);
    if (state.progress != nullptr) {
        SendMessageW(state.progress, PBM_SETPOS, clamped, 0);
    }
    if (state.statusText != nullptr && !message.empty()) {
        SetWindowTextW(state.statusText, message.c_str());
    }
    UpdateWindow(dialog);
    pumpDialogMessages(dialog);
}

void setActionButtonsEnabled(DialogState& state, bool enabled)
{
    const auto flag = enabled ? TRUE : FALSE;
    if (state.generateButton != nullptr) {
        EnableWindow(state.generateButton, flag);
    }
    if (state.cancelButton != nullptr) {
        EnableWindow(state.cancelButton, flag);
    }
    if (state.previewButton != nullptr) {
        EnableWindow(state.previewButton, flag);
    }
    if (state.confirmButton != nullptr) {
        EnableWindow(state.confirmButton, enabled && (state.mode == DialogMode::Edit || state.hasGenerated) ? TRUE : FALSE);
    }
}

TinBuildOptions currentBuildOptions(const DialogState& state)
{
    return state.mode == DialogMode::Create
        ? state.createInput.buildOptions
        : state.editInput.buildOptions;
}

std::size_t validPointCount(const DialogState& state)
{
    return state.mode == DialogMode::Create
        ? state.createInput.summary.validPointCount
        : state.editInput.pointCount;
}

void buildControls(HWND dialog, DialogState& state)
{
    const auto title = state.mode == DialogMode::Create
        ? L"\u5730\u5f62\u6784\u7f51"
        : L"\u7f16\u8f91\u5730\u5f62\u6570\u6a21";
    const auto subtitle = state.mode == DialogMode::Create
        ? L"\u5df2\u6309\u6837\u672c\u5bf9\u8c61\u7684\u56fe\u5c42\u548c\u7c7b\u578b\u63d0\u53d6\u5730\u5f62\u6570\u636e\u3002"
        : L"\u4fee\u6539\u6570\u6a21\u663e\u793a\u6a21\u5f0f\u6216\u4f7f\u7528\u73b0\u6709\u70b9\u91cd\u65b0\u6784\u7f51\u3002";

    createStatic(dialog, title, kMargin, 18, 260, 30, state.titleFont);
    createStatic(dialog, subtitle, kMargin, 52, 560, 24, state.bodyFont);

    createGroup(dialog, L"\u63d0\u53d6\u7edf\u8ba1", kMargin, 90, 290, 310, state.bodyFont);
    HWND stats = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        state.mode == DialogMode::Create
            ? createStatsText(state.createInput).c_str()
            : editStatsText(state.editInput).c_str(),
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        kMargin + 16,
        118,
        258,
        262,
        dialog,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdStats)),
        nullptr,
        nullptr);
    setFont(stats, state.bodyFont);

    createGroup(dialog, L"\u6784\u7f51\u548c\u663e\u793a", 340, 90, 260, 310, state.bodyFont);
    createStatic(dialog, L"XY \u5408\u5e76\u5bb9\u5dee", 360, 126, 190, 22, state.bodyFont);
    const auto options = currentBuildOptions(state);
    const auto xy = state.mode == DialogMode::Create
        ? state.createInput.extractOptions.xyMergeTolerance
        : 0.001;
    HWND xyEdit = createEdit(dialog, kIdXyTolerance, numberText(xy), 360, 150, 170, state.bodyFont);
    if (state.mode == DialogMode::Edit) {
        EnableWindow(xyEdit, FALSE);
    }

    createStatic(dialog, L"\u6700\u5927\u4e09\u89d2\u8fb9\u957f", 360, 190, 190, 22, state.bodyFont);
    createEdit(dialog, kIdMaxEdge, numberText(options.maxEdgeLength), 360, 214, 170, state.bodyFont);
    createStatic(dialog, L"0 \u8868\u793a\u4e0d\u9650\u5236", 360, 244, 190, 20, state.bodyFont);

    HWND removeDegenerate = createButton(
        dialog,
        kIdRemoveDegenerate,
        L"\u8fc7\u6ee4\u9000\u5316\u4e09\u89d2\u5f62",
        360,
        276,
        200,
        24,
        BS_AUTOCHECKBOX,
        state.bodyFont);
    SendMessageW(
        removeDegenerate,
        BM_SETCHECK,
        options.removeDegenerateTriangles ? BST_CHECKED : BST_UNCHECKED,
        0);

    createStatic(dialog, L"\u663e\u793a\u6a21\u5f0f", 360, 318, 190, 22, state.bodyFont);
    HWND combo = CreateWindowExW(
        0,
        L"COMBOBOX",
        L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
        360,
        342,
        205,
        120,
        dialog,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdDisplayMode)),
        nullptr,
        nullptr);
    setFont(combo, state.bodyFont);
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u4e09\u89d2\u7f51\u8fb9\u6309\u9ad8\u7a0b\u7740\u8272"));
    SendMessageW(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"\u4ec5\u663e\u793a\u8fb9\u754c"));
    SendMessageW(
        combo,
        CB_SETCURSEL,
        options.displayMode == TerrainTinDisplayMode::BoundaryOnly ? 1 : 0,
        0);

    const bool canGenerate = validPointCount(state) >= 3;
    if (!canGenerate) {
        createStatic(dialog, L"\u6709\u6548\u70b9\u5c11\u4e8e 3 \u4e2a\uff0c\u65e0\u6cd5\u6784\u7f51\u3002", kMargin, 404, 500, 22, state.bodyFont);
    } else if (state.mode == DialogMode::Create) {
        createStatic(dialog, L"\u6e90\u5bf9\u8c61\u5df2\u6682\u65f6\u9690\u85cf\uff1b\u53d6\u6d88\u6216\u5931\u8d25\u65f6\u4f1a\u81ea\u52a8\u6062\u590d\u3002", kMargin, 404, 560, 22, state.bodyFont);
    } else {
        createStatic(dialog, L"\u53cc\u51fb\u6570\u6a21\u5b9e\u4f53\u53ef\u91cd\u590d\u6253\u5f00\u6b64\u7a97\u53e3\u3002", kMargin, 404, 560, 22, state.bodyFont);
    }

    state.progress = CreateWindowExW(
        0,
        PROGRESS_CLASSW,
        L"",
        WS_CHILD | WS_VISIBLE,
        kMargin,
        458,
        560,
        14,
        dialog,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(kIdProgress)),
        nullptr,
        nullptr);
    SendMessageW(state.progress, PBM_SETRANGE, 0, MAKELPARAM(0, 100));
    SendMessageW(state.progress, PBM_SETPOS, 0, 0);
    state.statusText = createStatic(
        dialog,
        state.mode == DialogMode::Create
            ? L"调整参数后点击“生成地形数模”，确认后写入当前图形。"
            : L"调整显示参数后可确认，或重新生成当前数模。",
        kMargin,
        428,
        560,
        24,
        state.bodyFont);

    if (state.mode == DialogMode::Create) {
        state.generateButton = createButton(
            dialog,
            kIdGenerate,
            L"\u751f\u6210\u5730\u5f62\u6570\u6a21",
            228,
            484,
            140,
            34,
            BS_DEFPUSHBUTTON,
            state.bodyFont);
        EnableWindow(state.generateButton, canGenerate ? TRUE : FALSE);
        state.previewButton = createButton(dialog, kIdPreview, L"\u9884\u89c8", 382, 484, 70, 34, 0, state.bodyFont);
        state.confirmButton = createButton(dialog, kIdConfirm, L"\u786e\u8ba4", 464, 484, 70, 34, 0, state.bodyFont);
        EnableWindow(state.confirmButton, FALSE);
    } else {
        state.generateButton = createButton(dialog, kIdGenerate, L"\u91cd\u65b0\u751f\u6210", 228, 484, 140, 34, 0, state.bodyFont);
        state.previewButton = createButton(dialog, kIdPreview, L"\u9884\u89c8", 382, 484, 70, 34, 0, state.bodyFont);
        state.confirmButton = createButton(dialog, kIdConfirm, L"\u786e\u8ba4", 464, 484, 70, 34, BS_DEFPUSHBUTTON, state.bodyFont);
        EnableWindow(state.generateButton, canGenerate ? TRUE : FALSE);
    }

    state.cancelButton = createButton(dialog, kIdCancel, L"\u53d6\u6d88", 546, 484, 70, 34, 0, state.bodyFont);
}

bool collectBuildOptions(HWND dialog, DialogState& state, TinBuildOptions& buildOptions)
{
    buildOptions = currentBuildOptions(state);

    if (!parseDouble(dialog, kIdMaxEdge, buildOptions.maxEdgeLength)
        || buildOptions.maxEdgeLength < 0.0) {
        MessageBoxW(dialog, L"\u6700\u5927\u4e09\u89d2\u8fb9\u957f\u5fc5\u987b\u5927\u4e8e\u7b49\u4e8e 0\u3002", L"RoadProto", MB_ICONWARNING);
        return false;
    }

    buildOptions.removeDegenerateTriangles =
        SendMessageW(GetDlgItem(dialog, kIdRemoveDegenerate), BM_GETCHECK, 0, 0) == BST_CHECKED;
    const auto displayModeIndex = SendMessageW(GetDlgItem(dialog, kIdDisplayMode), CB_GETCURSEL, 0, 0);
    buildOptions.displayMode = displayModeIndex == 1
        ? TerrainTinDisplayMode::BoundaryOnly
        : TerrainTinDisplayMode::ColoredTriangles;
    return true;
}

bool collectExtractOptions(HWND dialog, DialogState& state, roadproto::domain::terrain::TinExtractOptions& extractOptions)
{
    extractOptions = state.createInput.extractOptions;
    if (!parseDouble(dialog, kIdXyTolerance, extractOptions.xyMergeTolerance)
        || extractOptions.xyMergeTolerance <= 0.0) {
        MessageBoxW(dialog, L"XY \u5408\u5e76\u5bb9\u5dee\u5fc5\u987b\u5927\u4e8e 0\u3002", L"RoadProto", MB_ICONWARNING);
        return false;
    }
    return true;
}

bool collectCreateOptions(
    HWND dialog,
    DialogState& state,
    roadproto::domain::terrain::TinExtractOptions& extractOptions,
    TinBuildOptions& buildOptions)
{
    if (!collectExtractOptions(dialog, state, extractOptions) || !collectBuildOptions(dialog, state, buildOptions)) {
        return false;
    }
    return true;
}

void generateCreate(HWND dialog, DialogState& state)
{
    roadproto::domain::terrain::TinExtractOptions extractOptions;
    TinBuildOptions buildOptions;
    if (!collectCreateOptions(dialog, state, extractOptions, buildOptions)) {
        return;
    }

    if (!state.createGenerateCallback) {
        MessageBoxW(dialog, L"\u672a\u8fde\u63a5\u5730\u5f62\u6784\u7f51 Bridge\u3002", L"RoadProto", MB_ICONERROR);
        return;
    }

    setActionButtonsEnabled(state, false);
    setDialogProgress(dialog, state, 5, L"\u6b63\u5728\u51c6\u5907\u6784\u5efa TIN...");

    std::wstring statusMessage;
    const auto succeeded = state.createGenerateCallback(
        extractOptions,
        buildOptions,
        [&](int percent, const std::wstring& message) {
            setDialogProgress(dialog, state, percent, message);
        },
        statusMessage);

    if (succeeded) {
        state.hasGenerated = true;
        state.createResult->generated = true;
        state.createResult->extractOptions = extractOptions;
        state.createResult->buildOptions = buildOptions;
        setDialogProgress(
            dialog,
            state,
            100,
            statusMessage.empty() ? L"\u5df2\u751f\u6210\u9884\u89c8\u6570\u6a21\uff0c\u53ef\u7ee7\u7eed\u8c03\u53c2\u91cd\u751f\u6210\u6216\u70b9\u51fb\u786e\u8ba4\u3002" : statusMessage);
    } else {
        setDialogProgress(
            dialog,
            state,
            0,
            statusMessage.empty() ? L"\u6784\u7f51\u5931\u8d25\uff0c\u8bf7\u68c0\u67e5\u53c2\u6570\u6216\u6e90\u6570\u636e\u3002" : statusMessage);
        MessageBoxW(
            dialog,
            statusMessage.empty() ? L"\u6784\u7f51\u5931\u8d25\u3002" : statusMessage.c_str(),
            L"RoadProto",
            MB_ICONERROR);
    }

    setActionButtonsEnabled(state, true);
}

void confirmCreate(HWND dialog, DialogState& state)
{
    roadproto::domain::terrain::TinExtractOptions extractOptions;
    TinBuildOptions buildOptions;
    if (!collectCreateOptions(dialog, state, extractOptions, buildOptions)) {
        return;
    }

    if (!state.hasGenerated) {
        MessageBoxW(dialog, L"\u8bf7\u5148\u751f\u6210\u5730\u5f62\u6570\u6a21\uff0c\u518d\u786e\u8ba4\u5199\u5165\u56fe\u5f62\u3002", L"RoadProto", MB_ICONINFORMATION);
        return;
    }

    state.createResult->accepted = true;
    state.createResult->extractOptions = extractOptions;
    state.createResult->buildOptions = buildOptions;
    state.done = true;
    DestroyWindow(dialog);
}

void acceptEdit(HWND dialog, DialogState& state, bool rebuild)
{
    TinBuildOptions buildOptions;
    if (!collectBuildOptions(dialog, state, buildOptions)) {
        return;
    }

    state.editResult->accepted = true;
    state.editResult->rebuildRequested = rebuild;
    state.editResult->buildOptions = buildOptions;
    state.done = true;
    DestroyWindow(dialog);
}

void previewDrawing(HWND dialog, DialogState& state)
{
    setDialogProgress(dialog, state, 0, L"\u9884\u89c8\u4e2d\uff1a\u53ef\u67e5\u770b DWG\uff0c\u6309 ESC \u8fd4\u56de\u53c2\u6570\u7a97\u53e3\u3002");
    ShowWindow(dialog, SW_HIDE);
    if (state.owner != nullptr) {
        EnableWindow(state.owner, TRUE);
        SetForegroundWindow(state.owner);
    }

    bool done = false;
    while (!done) {
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x0001) != 0) {
            done = true;
            break;
        }

        MSG message = {};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE) {
                done = true;
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(30);
    }

    if (state.owner != nullptr) {
        EnableWindow(state.owner, FALSE);
    }
    ShowWindow(dialog, SW_SHOW);
    SetForegroundWindow(dialog);
    setDialogProgress(dialog, state, state.hasGenerated ? 100 : 0, L"\u5df2\u8fd4\u56de\u5730\u5f62\u6784\u7f51\u7a97\u53e3\u3002");
}

void destroyStateResources(DialogState& state)
{
    if (state.titleFont != nullptr) {
        DeleteObject(state.titleFont);
        state.titleFont = nullptr;
    }
    if (state.bodyFont != nullptr) {
        DeleteObject(state.bodyFont);
        state.bodyFont = nullptr;
    }
    if (state.windowBrush != nullptr) {
        DeleteObject(state.windowBrush);
        state.windowBrush = nullptr;
    }
    if (state.panelBrush != nullptr) {
        DeleteObject(state.panelBrush);
        state.panelBrush = nullptr;
    }
    if (state.editBrush != nullptr) {
        DeleteObject(state.editBrush);
        state.editBrush = nullptr;
    }
}

LRESULT CALLBACK dialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

    switch (message) {
    case WM_CREATE: {
        auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        state = reinterpret_cast<DialogState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
        buildControls(hwnd, *state);
        return 0;
    }
    case WM_COMMAND:
        if (state == nullptr) {
            break;
        }
        switch (LOWORD(wParam)) {
        case kIdGenerate:
            if (state->mode == DialogMode::Create) {
                generateCreate(hwnd, *state);
            } else {
                setDialogProgress(hwnd, *state, 15, L"\u6b63\u5728\u91cd\u65b0\u751f\u6210\u6570\u6a21...");
                acceptEdit(hwnd, *state, true);
            }
            return 0;
        case kIdConfirm:
        case IDOK:
            if (state->mode == DialogMode::Create) {
                confirmCreate(hwnd, *state);
            } else {
                acceptEdit(hwnd, *state, false);
            }
            return 0;
        case kIdPreview:
            previewDrawing(hwnd, *state);
            return 0;
        case kIdApply:
            acceptEdit(hwnd, *state, false);
            return 0;
        case kIdCancel:
        case IDCANCEL:
            state->done = true;
            DestroyWindow(hwnd);
            return 0;
        default:
            break;
        }
        break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
        if (state != nullptr) {
            SetBkMode(reinterpret_cast<HDC>(wParam), TRANSPARENT);
            SetTextColor(reinterpret_cast<HDC>(wParam), RGB(32, 36, 42));
            if (reinterpret_cast<HWND>(lParam) == GetDlgItem(hwnd, kIdStats)) {
                SetBkMode(reinterpret_cast<HDC>(wParam), OPAQUE);
                SetBkColor(reinterpret_cast<HDC>(wParam), RGB(255, 255, 255));
                return reinterpret_cast<LRESULT>(state->editBrush);
            }
            return reinterpret_cast<LRESULT>(state->windowBrush);
        }
        break;
    case WM_CLOSE:
        if (state != nullptr) {
            state->done = true;
        }
        DestroyWindow(hwnd);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void registerDialogClass(HINSTANCE instance)
{
    static bool registered = false;
    if (registered) {
        return;
    }

    INITCOMMONCONTROLSEX commonControls = {};
    commonControls.dwSize = sizeof(commonControls);
    commonControls.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&commonControls);

    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = dialogProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = L"RoadProtoTerrainTinDialog";
    RegisterClassExW(&windowClass);
    registered = true;
}

void initializeResources(DialogState& state)
{
    state.titleFont = CreateFontW(
        22,
        0,
        0,
        0,
        FW_SEMIBOLD,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    state.bodyFont = CreateFontW(
        16,
        0,
        0,
        0,
        FW_NORMAL,
        FALSE,
        FALSE,
        FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        L"Segoe UI");
    state.windowBrush = CreateSolidBrush(RGB(248, 249, 251));
    state.panelBrush = CreateSolidBrush(RGB(244, 246, 248));
    state.editBrush = CreateSolidBrush(RGB(255, 255, 255));
}

bool runDialog(DialogState& state, const wchar_t* windowTitle)
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    registerDialogClass(instance);
    initializeResources(state);

    HWND owner = GetForegroundWindow();
    state.owner = owner;
    const auto screenWidth = GetSystemMetrics(SM_CXSCREEN);
    const auto screenHeight = GetSystemMetrics(SM_CYSCREEN);
    const auto x = (screenWidth - kDialogWidth) / 2;
    const auto y = (screenHeight - kDialogHeight) / 2;

    HWND dialog = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE,
        L"RoadProtoTerrainTinDialog",
        windowTitle,
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x,
        y,
        kDialogWidth,
        kDialogHeight,
        owner,
        nullptr,
        instance,
        &state);

    if (dialog == nullptr) {
        destroyStateResources(state);
        return false;
    }

    if (owner != nullptr) {
        EnableWindow(owner, FALSE);
    }

    ShowWindow(dialog, SW_SHOW);
    UpdateWindow(dialog);

    MSG message = {};
    while (!state.done && GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(dialog, &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (owner != nullptr) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    destroyStateResources(state);
    return true;
}

} // namespace

bool showTerrainTinCreateDialog(
    const TerrainTinCreateDialogInput& input,
    TerrainTinCreateDialogResult& result,
    const TerrainTinGenerateCallback& generateCallback)
{
    DialogState state;
    state.mode = DialogMode::Create;
    state.createInput = input;
    state.createResult = &result;
    state.createGenerateCallback = generateCallback;
    if (!runDialog(state, L"\u5730\u5f62\u6784\u7f51")) {
        return false;
    }
    return result.accepted;
}

bool showTerrainTinEditDialog(
    const TerrainTinEditDialogInput& input,
    TerrainTinEditDialogResult& result)
{
    DialogState state;
    state.mode = DialogMode::Edit;
    state.editInput = input;
    state.editResult = &result;
    if (!runDialog(state, L"\u7f16\u8f91\u5730\u5f62\u6570\u6a21")) {
        return false;
    }
    return result.accepted;
}

} // namespace roadproto::cad_adapter::objectarx
