// -----------------------------------------------------------------------------
//  Cze 对轨 (CzeAlign) 导入对话框 — 中文 / English / 日本語
// -----------------------------------------------------------------------------

#include "ui.h"

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <vector>

#include "lang.h"
#include "rpp.h"

namespace {

constexpr wchar_t kClassName[] = L"CzeAlignImportDialog";
constexpr int kWinW = 640;
constexpr int kWinH = 520;

constexpr int IDC_PATH = 1001;
constexpr int IDC_BROWSE = 1002;
constexpr int IDC_TRACKS = 1003;
constexpr int IDC_SELALL = 1004;
constexpr int IDC_SELNONE = 1005;
constexpr int IDC_START_LAYER = 1006;
constexpr int IDC_EMPTY_LAYERS = 1007;
constexpr int IDC_START_FRAME = 1008;
constexpr int IDC_KIND = 1009;
constexpr int IDC_NAME_OBJECTS = 1010;
constexpr int IDC_STATUS = 1012;
constexpr int IDC_RANGE_START = 1013;
constexpr int IDC_RANGE_END = 1014;
constexpr int IDC_STATS = 1017;
constexpr int IDC_WRAP_MODE = 1018;
constexpr int IDC_WRAP_VALUE = 1019;
constexpr int IDC_WRAP_UNIT = 1020;
constexpr int IDC_LANG = 1021;

struct TextReg {
    HWND hwnd = nullptr;
    const wchar_t* zh = L"";
    const wchar_t* en = L"";
    const wchar_t* ja = L"";
};

struct UiState {
    HWND hwnd = nullptr;
    HWND list = nullptr;
    HWND path = nullptr;
    HWND status = nullptr;
    HWND wrap_mode = nullptr;
    HWND wrap_unit = nullptr;
    HWND lang_combo = nullptr;

    ra::RppProject project;
    std::wstring file;
    bool accepted = false;
    ra::ImportOptions pending;
    std::vector<TextReg> texts;
};

UiState* g_ui = nullptr;

const wchar_t* T(const wchar_t* zh, const wchar_t* en, const wchar_t* ja) {
    return lang::T(zh, en, ja);
}

HWND MakeRaw(const wchar_t* cls, const wchar_t* text, DWORD style, int x, int y, int w, int h,
             int id) {
    return CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_ui->hwnd,
                           reinterpret_cast<HMENU>(static_cast<intptr_t>(id)), nullptr, nullptr);
}

HWND MakeText(const wchar_t* cls, const wchar_t* zh, const wchar_t* en, const wchar_t* ja,
              DWORD style, int x, int y, int w, int h, int id) {
    HWND hwnd = MakeRaw(cls, lang::T(zh, en, ja), style, x, y, w, h, id);
    if (hwnd) {
        g_ui->texts.push_back({hwnd, zh, en, ja});
        SendMessageW(hwnd, WM_SETFONT,
                     reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    }
    return hwnd;
}

int ReadInt(HWND edit, int fallback) {
    wchar_t buffer[64]{};
    if (!edit) {
        return fallback;
    }
    GetWindowTextW(edit, buffer, 64);
    wchar_t* end = nullptr;
    const long value = std::wcstol(buffer, &end, 10);
    return (end == buffer) ? fallback : static_cast<int>(value);
}

double ReadDouble(HWND edit, double fallback) {
    wchar_t buffer[64]{};
    if (!edit) {
        return fallback;
    }
    GetWindowTextW(edit, buffer, 64);
    wchar_t* end = nullptr;
    const double value = std::wcstod(buffer, &end);
    return (end == buffer) ? fallback : value;
}

double ProjectEnd() {
    double end = 0.0;
    for (const ra::RppTrack& track : g_ui->project.tracks) {
        end = std::max(end, track.End());
    }
    return end;
}

void RefreshStatus() {
    if (!g_ui || !g_ui->status) {
        return;
    }
    int checked = 0;
    if (g_ui->list) {
        const int count = ListView_GetItemCount(g_ui->list);
        for (int i = 0; i < count; ++i) {
            if (ListView_GetCheckState(g_ui->list, i)) {
                ++checked;
            }
        }
    }
    wchar_t buffer[256]{};
    switch (lang::Current()) {
        case lang::Id::En:
            std::swprintf(buffer, 256, L"%d tracks / %d items / %d checked / %.2fs",
                          static_cast<int>(g_ui->project.tracks.size()),
                          g_ui->project.ItemCount(), checked, ProjectEnd());
            break;
        case lang::Id::Ja:
            std::swprintf(buffer, 256, L"%d トラック / %d item / %d 選択 / %.2f 秒",
                          static_cast<int>(g_ui->project.tracks.size()),
                          g_ui->project.ItemCount(), checked, ProjectEnd());
            break;
        default:
            std::swprintf(buffer, 256, L"%d 个轨道 / %d 个 item / 已选 %d / 时长 %.2f 秒",
                          static_cast<int>(g_ui->project.tracks.size()),
                          g_ui->project.ItemCount(), checked, ProjectEnd());
            break;
    }
    SetWindowTextW(g_ui->status, buffer);
}

void UpdateWrapUnit() {
    if (!g_ui->wrap_unit) {
        return;
    }
    const int mode = static_cast<int>(SendMessageW(g_ui->wrap_mode, CB_GETCURSEL, 0, 0));
    SetWindowTextW(g_ui->wrap_unit,
                   mode == 1 ? T(L"秒", L"sec", L"秒") : T(L"层", L"layers", L"レイヤー"));
}

void ApplyLanguage() {
    for (const TextReg& item : g_ui->texts) {
        SetWindowTextW(item.hwnd, lang::T(item.zh, item.en, item.ja));
    }

    if (g_ui->list) {
        const wchar_t* heads[4][3] = {{L"轨道", L"Track", L"トラック"},
                                      {L"名称", L"Name", L"名前"},
                                      {L"item", L"Items", L"item"},
                                      {L"时间范围", L"Time range", L"時間範囲"}};
        for (int i = 0; i < 4; ++i) {
            LVCOLUMNW column{};
            column.mask = LVCF_TEXT;
            column.pszText = const_cast<wchar_t*>(
                lang::T(heads[i][0], heads[i][1], heads[i][2]));
            ListView_SetColumn(g_ui->list, i, &column);
        }
        const int count = ListView_GetItemCount(g_ui->list);
        for (int i = 0; i < count; ++i) {
            if (g_ui->project.tracks[static_cast<size_t>(i)].name.empty()) {
                ListView_SetItemText(
                    g_ui->list, i, 1,
                    const_cast<wchar_t*>(T(L"(未命名)", L"(unnamed)", L"(無名)")));
            }
        }
    }

    if (g_ui->wrap_mode) {
        const int selection = static_cast<int>(SendMessageW(g_ui->wrap_mode, CB_GETCURSEL, 0, 0));
        SendMessageW(g_ui->wrap_mode, CB_RESETCONTENT, 0, 0);
        SendMessageW(g_ui->wrap_mode, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(T(L"按层数", L"By layers", L"レイヤー数")));
        SendMessageW(g_ui->wrap_mode, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(T(L"按时间", L"By time", L"時間")));
        SendMessageW(g_ui->wrap_mode, CB_SETCURSEL, selection < 0 ? 0 : selection, 0);
        UpdateWrapUnit();
    }
    if (HWND kind = GetDlgItem(g_ui->hwnd, IDC_KIND)) {
        const int selection = static_cast<int>(SendMessageW(kind, CB_GETCURSEL, 0, 0));
        SendMessageW(kind, CB_RESETCONTENT, 0, 0);
        SendMessageW(kind, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(
                         T(L"画像ファイル（图片）", L"Image file", L"画像ファイル")));
        SendMessageW(kind, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(T(L"図形", L"Figure", L"図形")));
        SendMessageW(kind, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(T(L"グループ制御", L"Group control", L"グループ制御")));
        SendMessageW(kind, CB_SETCURSEL, selection < 0 ? 0 : selection, 0);
    }

    RefreshStatus();
}

void FillTrackList() {
    ListView_DeleteAllItems(g_ui->list);

    for (size_t i = 0; i < g_ui->project.tracks.size(); ++i) {
        const ra::RppTrack& track = g_ui->project.tracks[i];

        wchar_t index_text[32]{};
        std::swprintf(index_text, 32, L"Track %d", static_cast<int>(i) + 1);

        wchar_t count_text[32]{};
        std::swprintf(count_text, 32, L"%d", static_cast<int>(track.items.size()));

        wchar_t range_text[64]{};
        std::swprintf(range_text, 64, L"%.2f - %.2f s", track.Start(), track.End());

        const wchar_t* name_text =
            track.name.empty() ? T(L"(未命名)", L"(unnamed)", L"(無名)") : track.name.c_str();

        LVITEMW item{};
        item.mask = LVIF_TEXT;
        item.iItem = static_cast<int>(i);
        item.iSubItem = 0;
        item.pszText = index_text;
        const int row = ListView_InsertItem(g_ui->list, &item);
        if (row < 0) {
            continue;
        }
        ListView_SetItemText(g_ui->list, row, 1, const_cast<wchar_t*>(name_text));
        ListView_SetItemText(g_ui->list, row, 2, count_text);
        ListView_SetItemText(g_ui->list, row, 3, range_text);
        ListView_SetCheckState(g_ui->list, row, TRUE);
    }

    RefreshStatus();
}

void SetAllChecks(BOOL state) {
    const int count = ListView_GetItemCount(g_ui->list);
    for (int i = 0; i < count; ++i) {
        ListView_SetCheckState(g_ui->list, i, state);
    }
    RefreshStatus();
}

bool LoadFile(const std::wstring& path) {
    ra::RppProject project;
    std::wstring error;
    if (!ra::ParseRppFile(path, &project, &error)) {
        MessageBoxW(g_ui->hwnd, error.c_str(), T(L"Cze 对轨", L"CzeAlign", L"CzeAlign"),
                    MB_ICONWARNING | MB_OK);
        return false;
    }

    g_ui->project = std::move(project);
    g_ui->file = path;
    SetWindowTextW(g_ui->path, path.c_str());
    FillTrackList();

    SetWindowTextW(GetDlgItem(g_ui->hwnd, IDC_RANGE_START), L"0");
    wchar_t end_text[32]{};
    std::swprintf(end_text, 32, L"%.2f", ProjectEnd());
    SetWindowTextW(GetDlgItem(g_ui->hwnd, IDC_RANGE_END), end_text);
    return true;
}

void BrowseForFile() {
    wchar_t buffer[2048]{};
    if (!g_ui->file.empty()) {
        wcsncpy_s(buffer, g_ui->file.c_str(), _TRUNCATE);
    }

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_ui->hwnd;
    ofn.lpstrFilter = L"Reaper (*.rpp)\0*.rpp\0*.*\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = 2048;
    ofn.lpstrTitle = T(L"选择 Reaper 工程 (.rpp)", L"Select a Reaper project (.rpp)",
                       L"Reaper プロジェクト (.rpp) を選択");
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

    if (GetOpenFileNameW(&ofn)) {
        LoadFile(buffer);
    }
}

bool CollectOptions(ra::ImportOptions* options, bool report_errors) {
    auto fail = [&](const wchar_t* zh, const wchar_t* en, const wchar_t* ja) {
        if (report_errors) {
            MessageBoxW(g_ui->hwnd, T(zh, en, ja), T(L"Cze 对轨", L"CzeAlign", L"CzeAlign"),
                        MB_ICONINFORMATION | MB_OK);
        }
        return false;
    };

    if (g_ui->file.empty()) {
        return fail(L"请先选择一个 .rpp 文件。", L"Please choose a .rpp file first.",
                    L"先に .rpp ファイルを選択してください。");
    }

    std::vector<int> tracks;
    const int count = ListView_GetItemCount(g_ui->list);
    for (int i = 0; i < count; ++i) {
        if (ListView_GetCheckState(g_ui->list, i)) {
            tracks.push_back(i);
        }
    }
    if (tracks.empty()) {
        return fail(L"请至少勾选一个轨道。", L"Please check at least one track.",
                    L"トラックを 1 つ以上選択してください。");
    }

    const int start_layer = ReadInt(GetDlgItem(g_ui->hwnd, IDC_START_LAYER), 1);
    const int empty_layers = ReadInt(GetDlgItem(g_ui->hwnd, IDC_EMPTY_LAYERS), 1);
    const int start_frame = ReadInt(GetDlgItem(g_ui->hwnd, IDC_START_FRAME), 0);
    const double range_start = ReadDouble(GetDlgItem(g_ui->hwnd, IDC_RANGE_START), 0.0);
    const double range_end = ReadDouble(GetDlgItem(g_ui->hwnd, IDC_RANGE_END), 0.0);
    const int wrap_mode = static_cast<int>(SendMessageW(g_ui->wrap_mode, CB_GETCURSEL, 0, 0));
    const double wrap_value = ReadDouble(GetDlgItem(g_ui->hwnd, IDC_WRAP_VALUE), 8.0);

    if (start_layer < 1 || start_layer > 100) {
        return fail(L"起始图层需要在 1 - 100 之间。", L"Start layer must be 1 - 100.",
                    L"開始レイヤーは 1 - 100 の範囲で指定してください。");
    }
    if (empty_layers < 0 || empty_layers > 99) {
        return fail(L"空层数需要在 0 - 99 之间。", L"Empty layers must be 0 - 99.",
                    L"空きレイヤーは 0 - 99 の範囲で指定してください。");
    }
    if (start_frame < 0) {
        return fail(L"起始帧不能是负数。", L"Start frame must not be negative.",
                    L"開始フレームは負の値にできません。");
    }
    if (range_start < 0.0 || range_end < 0.0) {
        return fail(L"时间范围不能是负数。", L"Time range must not be negative.",
                    L"時間範囲は負の値にできません。");
    }
    if (range_end > 0.0 && range_end <= range_start) {
        return fail(L"结束时间必须大于开始时间（结束填 0 = 到结尾）。",
                    L"End time must be greater than start (0 = to the end).",
                    L"終了時間は開始時間より後にしてください（終了 0 = 最後まで）。");
    }
    if (wrap_value <= 0.0) {
        return fail(L"回卷数值需要大于 0。", L"The wrap value must be greater than 0.",
                    L"折り返しの値は 0 より大きくしてください。");
    }

    const int kind =
        static_cast<int>(SendMessageW(GetDlgItem(g_ui->hwnd, IDC_KIND), CB_GETCURSEL, 0, 0));

    options->path = g_ui->file;
    options->track_indices = tracks;
    options->start_layer = start_layer - 1;
    options->empty_layers = empty_layers;
    options->start_frame = start_frame;
    options->range_start_sec = range_start;
    options->range_end_sec = range_end;
    options->wrap_mode = (wrap_mode == 1) ? ra::ImportOptions::WRAP_TIME
                                          : ra::ImportOptions::WRAP_LAYERS;
    options->wrap_layers = wrap_mode == 1 ? 8 : std::max(1, static_cast<int>(wrap_value));
    options->wrap_seconds = wrap_mode == 1 ? wrap_value : 10.0;
    options->name_objects =
        IsDlgButtonChecked(g_ui->hwnd, IDC_NAME_OBJECTS) == BST_CHECKED;
    switch (kind) {
        case 1:
            options->effect = L"図形";
            break;
        case 2:
            options->effect = L"グループ制御";
            break;
        default:
            options->effect = L"画像ファイル";
            break;
    }
    return true;
}

bool Accept() {
    ra::ImportOptions options;
    if (!CollectOptions(&options, true)) {
        return false;
    }
    g_ui->pending = std::move(options);
    g_ui->accepted = true;
    return true;
}

void ShowStats() {
    ra::ImportOptions options;
    if (!CollectOptions(&options, true)) {
        return;
    }
    options.fps = 60.0;

    const std::vector<ra::PlannedObject> plan = ra::BuildPlan(g_ui->project, options);
    if (plan.empty()) {
        MessageBoxW(g_ui->hwnd,
                    T(L"按当前设置不会生成任何物件。", L"No objects would be created.",
                      L"現在の設定ではオブジェクトが作成されません。"),
                    T(L"统计", L"Stats", L"統計"), MB_ICONINFORMATION | MB_OK);
        return;
    }

    int min_layer = 99;
    int max_layer = 0;
    int resets = 0;
    int prev = -1;
    for (const ra::PlannedObject& object : plan) {
        min_layer = std::min(min_layer, object.layer);
        max_layer = std::max(max_layer, object.layer);
        if (prev >= 0 && object.layer < prev) {
            ++resets;
        }
        prev = object.layer;
    }

    wchar_t buffer[512]{};
    switch (lang::Current()) {
        case lang::Id::En:
            std::swprintf(buffer, 512, L"Objects: %d\nLayers: %d - %d\nWraps: %d\nEmpty layers: %d",
                          static_cast<int>(plan.size()), min_layer + 1, max_layer + 1, resets,
                          options.empty_layers);
            break;
        case lang::Id::Ja:
            std::swprintf(buffer, 512,
                          L"オブジェクト: %d 個\nレイヤー: %d - %d\n折り返し: %d 回\n空きレイヤー: %d",
                          static_cast<int>(plan.size()), min_layer + 1, max_layer + 1, resets,
                          options.empty_layers);
            break;
        default:
            std::swprintf(buffer, 512, L"物件：%d 个\n图层：第 %d 层 - 第 %d 层\n回卷：%d 次\n空层数：%d",
                          static_cast<int>(plan.size()), min_layer + 1, max_layer + 1, resets,
                          options.empty_layers);
            break;
    }
    MessageBoxW(g_ui->hwnd, buffer, T(L"统计", L"Stats", L"統計"), MB_ICONINFORMATION | MB_OK);
}

void CreateControls() {
    g_ui->texts.clear();

    MakeText(L"BUTTON", L"RPP 文件", L"RPP file", L"RPP ファイル", BS_GROUPBOX, 12, 10, 616, 78, -1);
    MakeText(L"STATIC", L"文件：", L"File:", L"ファイル：", SS_LEFT, 24, 36, 56, 20, -1);
    g_ui->path = MakeRaw(L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY, 84, 32, 436, 26,
                         IDC_PATH);
    SendMessageW(g_ui->path, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
    MakeText(L"BUTTON", L"浏览…", L"Browse…", L"参照…", BS_PUSHBUTTON, 528, 32, 88, 26, IDC_BROWSE);

    MakeText(L"BUTTON", L"轨道", L"Tracks", L"トラック", BS_GROUPBOX, 12, 96, 616, 176, -1);
    g_ui->list = MakeRaw(WC_LISTVIEWW, L"",
                         WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS, 24, 116, 592,
                         110, IDC_TRACKS);
    SendMessageW(g_ui->list, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    ListView_SetExtendedListViewStyle(g_ui->list,
                                      LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_GRIDLINES);

    MakeText(L"BUTTON", L"全选", L"Select all", L"すべて選択", BS_PUSHBUTTON, 24, 234, 88, 26,
             IDC_SELALL);
    MakeText(L"BUTTON", L"全不选", L"Clear", L"選択解除", BS_PUSHBUTTON, 120, 234, 88, 26,
             IDC_SELNONE);
    g_ui->status = MakeText(L"STATIC", L"", L"", L"", SS_LEFT, 224, 238, 392, 20, IDC_STATUS);

    MakeText(L"BUTTON", L"图层与时间", L"Layers & time", L"レイヤーと時間", BS_GROUPBOX, 12, 280,
             616, 176, -1);

    MakeText(L"STATIC", L"起始图层", L"Start layer", L"開始レイヤー", SS_LEFT, 24, 306, 84, 20, -1);
    MakeRaw(L"EDIT", L"1", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 112, 302, 56, 26,
            IDC_START_LAYER);
    MakeText(L"STATIC", L"空层数", L"Empty layers", L"空きレイヤー", SS_LEFT, 190, 306, 84, 20, -1);
    MakeRaw(L"EDIT", L"1", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 278, 302, 56, 26,
            IDC_EMPTY_LAYERS);
    MakeText(L"STATIC", L"起始帧", L"Start frame", L"開始フレーム", SS_LEFT, 356, 306, 84, 20, -1);
    MakeRaw(L"EDIT", L"0", WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 444, 302, 72, 26,
            IDC_START_FRAME);

    MakeText(L"STATIC", L"RPP 时间（秒）", L"RPP time (s)", L"RPP 時間（秒）", SS_LEFT, 24, 340,
             104, 20, -1);
    MakeRaw(L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, 132, 336, 64, 26, IDC_RANGE_START);
    MakeText(L"STATIC", L"到", L"to", L"～", SS_LEFT, 202, 340, 20, 20, -1);
    MakeRaw(L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, 224, 336, 64, 26, IDC_RANGE_END);
    MakeText(L"STATIC", L"结束填 0 = 到工程结尾", L"0 = to the end", L"終了 0 = 最後まで",
             SS_LEFT, 300, 340, 260, 20, -1);

    MakeText(L"STATIC", L"回卷方式", L"Wrap", L"折り返し", SS_LEFT, 24, 374, 84, 20, -1);
    g_ui->wrap_mode = MakeRaw(L"COMBOBOX", L"", WS_BORDER | CBS_DROPDOWNLIST | WS_VSCROLL, 112, 370,
                              120, 200, IDC_WRAP_MODE);
    SendMessageW(g_ui->wrap_mode, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    MakeText(L"STATIC", L"每", L"every", L"ごと", SS_LEFT, 242, 374, 24, 20, -1);
    MakeRaw(L"EDIT", L"8", WS_BORDER | ES_AUTOHSCROLL, 268, 370, 56, 26, IDC_WRAP_VALUE);
    g_ui->wrap_unit = MakeText(L"STATIC", L"层", L"layers", L"レイヤー", SS_LEFT, 330, 374, 60, 20,
                               IDC_WRAP_UNIT);

    MakeText(L"STATIC", L"物件类型", L"Object type", L"オブジェクト種類", SS_LEFT, 400, 374, 84, 20,
             -1);
    HWND kind = MakeRaw(L"COMBOBOX", L"", WS_BORDER | CBS_DROPDOWNLIST | WS_VSCROLL, 488, 370, 128,
                        200, IDC_KIND);
    SendMessageW(kind, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);

    HWND name_check =
        MakeText(L"BUTTON", L"用 item 名命名物件", L"Name objects after items",
                 L"item 名で命名", BS_AUTOCHECKBOX, 112, 404, 260, 22, IDC_NAME_OBJECTS);
    SendMessageW(name_check, BM_SETCHECK, BST_CHECKED, 0);

    g_ui->lang_combo = MakeRaw(L"COMBOBOX", L"", WS_BORDER | CBS_DROPDOWNLIST | WS_VSCROLL, 12, 466,
                               130, 200, IDC_LANG);
    SendMessageW(g_ui->lang_combo, WM_SETFONT,
                 reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    SendMessageW(g_ui->lang_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"中文"));
    SendMessageW(g_ui->lang_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"English"));
    SendMessageW(g_ui->lang_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"日本語"));
    SendMessageW(g_ui->lang_combo, CB_SETCURSEL, static_cast<WPARAM>(lang::Current()), 0);

    MakeText(L"BUTTON", L"统计…", L"Stats…", L"統計…", BS_PUSHBUTTON, 340, 464, 90, 30, IDC_STATS);
    MakeText(L"BUTTON", L"生成", L"Generate", L"生成", BS_DEFPUSHBUTTON, 440, 464, 90, 30, IDOK);
    MakeText(L"BUTTON", L"取消", L"Cancel", L"キャンセル", BS_PUSHBUTTON, 540, 464, 90, 30,
             IDCANCEL);

    LVCOLUMNW column{};
    column.mask = LVCF_TEXT | LVCF_WIDTH;
    column.pszText = const_cast<wchar_t*>(L"");
    column.cx = 80;
    ListView_InsertColumn(g_ui->list, 0, &column);
    column.cx = 300;
    ListView_InsertColumn(g_ui->list, 1, &column);
    column.cx = 70;
    ListView_InsertColumn(g_ui->list, 2, &column);
    column.cx = 130;
    ListView_InsertColumn(g_ui->list, 3, &column);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE:
            g_ui->hwnd = hwnd;
            CreateControls();
            ApplyLanguage();
            DragAcceptFiles(hwnd, TRUE);
            return 0;

        case WM_COMMAND: {
            const int id = LOWORD(wparam);
            const int code = HIWORD(wparam);
            if (id == IDC_LANG && code == CBN_SELCHANGE) {
                const int selection =
                    static_cast<int>(SendMessageW(g_ui->lang_combo, CB_GETCURSEL, 0, 0));
                lang::SetLanguage(selection == 1 ? lang::Id::En
                                                 : (selection == 2 ? lang::Id::Ja : lang::Id::Zh));
                ApplyLanguage();
                return 0;
            }
            if (id == IDC_WRAP_MODE && code == CBN_SELCHANGE) {
                UpdateWrapUnit();
                return 0;
            }
            switch (id) {
                case IDC_BROWSE:
                    BrowseForFile();
                    return 0;
                case IDC_SELALL:
                    SetAllChecks(TRUE);
                    return 0;
                case IDC_SELNONE:
                    SetAllChecks(FALSE);
                    return 0;
                case IDC_STATS:
                    ShowStats();
                    return 0;
                case IDOK:
                    if (Accept()) {
                        DestroyWindow(hwnd);
                    }
                    return 0;
                case IDCANCEL:
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    break;
            }
            break;
        }

        case WM_NOTIFY: {
            const NMHDR* header = reinterpret_cast<const NMHDR*>(lparam);
            if (header && header->idFrom == IDC_TRACKS && header->code == LVN_ITEMCHANGED) {
                RefreshStatus();
            }
            break;
        }

        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(wparam);
            wchar_t path[2048]{};
            if (DragQueryFileW(drop, 0, path, 2048) > 0) {
                LoadFile(path);
            }
            DragFinish(drop);
            return 0;
        }

        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            break;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

}  // namespace

bool ShowImportDialog(HWND owner, HINSTANCE instance, const std::wstring& initial_path,
                      ra::ImportOptions* options, std::wstring* error_out) {
    if (error_out) {
        error_out->clear();
    }
    if (!options) {
        return false;
    }

    lang::EnsureDetected();

    UiState state;
    g_ui = &state;

    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_LISTVIEW_CLASSES | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        g_ui = nullptr;
        if (error_out) {
            *error_out = L"cannot register window class";
        }
        return false;
    }

    RECT rect{0, 0, kWinW, kWinH};
    AdjustWindowRectEx(&rect, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, 0);

    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (owner && IsWindow(owner)) {
        RECT owner_rect{};
        GetWindowRect(owner, &owner_rect);
        x = owner_rect.left + ((owner_rect.right - owner_rect.left) - (rect.right - rect.left)) / 2;
        y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - (rect.bottom - rect.top)) / 2;
    }

    HWND hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kClassName,
                                T(L"Cze 对轨导入", L"CzeAlign Import", L"CzeAlign インポート"),
                                WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, rect.right - rect.left,
                                rect.bottom - rect.top, owner, nullptr, instance, nullptr);
    if (!hwnd) {
        g_ui = nullptr;
        if (error_out) {
            *error_out = L"cannot create window";
        }
        return false;
    }

    if (owner && IsWindow(owner)) {
        EnableWindow(owner, FALSE);
    }

    if (!initial_path.empty()) {
        LoadFile(initial_path);
    }
    RefreshStatus();

    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    SetForegroundWindow(hwnd);

    MSG msg{};
    while (IsWindow(hwnd) && GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (owner && IsWindow(owner)) {
        EnableWindow(owner, TRUE);
        SetForegroundWindow(owner);
    }

    const bool accepted = state.accepted;
    if (accepted) {
        *options = state.pending;
    }
    g_ui = nullptr;
    return accepted;
}
