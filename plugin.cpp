// -----------------------------------------------------------------------------
//  Cze 对轨 (CzeAlign) for AviUtl ExEdit2
//
//  泛用插件 (.aux2)。
//  读取 Reaper 工程 (.rpp)，把选中的轨道按时间/音高拆成一段段，
//  在 AviUtl2 的图层上分批生成「画像ファイル」空白物件。
//
//  图层排布：从起始层开始往下排，每 N 层或每 N 秒回到起始层继续，
//  所以无论多长都「一直生成下去」。
// -----------------------------------------------------------------------------

#include <windows.h>
#include <commctrl.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "importer.h"
#include "lang.h"
#include "plugin2.h"
#include "rpp.h"
#include "ui.h"

namespace {

EDIT_HANDLE* g_edit = nullptr;
HINSTANCE g_instance = nullptr;

constexpr wchar_t kProgressClass[] = L"CzeAlignProgress";
constexpr int kIdBar = 3001;
constexpr int kIdText = 3002;
constexpr UINT kMsgNext = WM_APP + 10;

struct Job {
    bool active = false;
    std::vector<ra::PlannedObject> plan;
    ra::CreateState state;
    size_t index = 0;
    bool name_objects = true;

    HWND hwnd = nullptr;
    HWND text = nullptr;
    HWND bar = nullptr;
    bool cancelled = false;
    double ms_per_object = 0.0;
    int busy_retries = 0;
    LARGE_INTEGER freq{};
};

Job g_job;

double ProjectFps(EDIT_SECTION* edit) {
    if (!edit || !edit->info) {
        return 60.0;
    }
    const int rate = edit->info->rate > 0 ? edit->info->rate : 60;
    const int scale = edit->info->scale > 0 ? edit->info->scale : 1;
    return static_cast<double>(rate) / static_cast<double>(scale);
}

HWND HostWindow() {
    return (g_edit && g_edit->get_host_app_window) ? g_edit->get_host_app_window() : nullptr;
}

void ShowError(const std::wstring& text) {
    MessageBoxW(HostWindow(), text.c_str(), lang::T(L"Cze 对轨", L"CzeAlign", L"CzeAlign"),
                MB_ICONWARNING | MB_OK);
}

struct BatchCtx {
    size_t begin = 0;
    size_t end = 0;
};

void BatchProc(void* param, EDIT_SECTION* edit) {
    BatchCtx* ctx = static_cast<BatchCtx*>(param);
    ra::CreateSlice(edit, g_job.plan, ctx->begin, ctx->end, g_job.name_objects, &g_job.state);
}

size_t NextChunkSize() {
    const size_t remaining = g_job.plan.size() - g_job.index;
    if (g_job.ms_per_object <= 0.0) {
        return std::min<size_t>(12, remaining);
    }
    double count = 250.0 / g_job.ms_per_object;
    count = std::clamp(count, 4.0, 120.0);
    return std::min<size_t>(static_cast<size_t>(count), remaining);
}

void UpdateProgress() {
    if (!g_job.text || !g_job.bar) {
        return;
    }
    wchar_t buffer[256]{};
    switch (lang::Current()) {
        case lang::Id::En:
            std::swprintf(buffer, 256, L"Creating objects: %d / %d", static_cast<int>(g_job.index),
                          static_cast<int>(g_job.plan.size()));
            break;
        case lang::Id::Ja:
            std::swprintf(buffer, 256, L"オブジェクト作成中: %d / %d",
                          static_cast<int>(g_job.index), static_cast<int>(g_job.plan.size()));
            break;
        default:
            std::swprintf(buffer, 256, L"正在生成物件：%d / %d", static_cast<int>(g_job.index),
                          static_cast<int>(g_job.plan.size()));
            break;
    }
    SetWindowTextW(g_job.text, buffer);
    SendMessageW(g_job.bar, PBM_SETPOS, static_cast<WPARAM>(g_job.index), 0);
}

void FinishJob(const wchar_t* message) {
    if (!g_job.active) {
        return;
    }
    g_job.active = false;

    if (g_job.hwnd) {
        KillTimer(g_job.hwnd, 1);
        DestroyWindow(g_job.hwnd);
        g_job.hwnd = nullptr;
        g_job.text = nullptr;
        g_job.bar = nullptr;
    }

    wchar_t buffer[512]{};
    if (message && message[0]) {
        std::swprintf(buffer, 512, L"%s", message);
    } else {
        switch (lang::Current()) {
            case lang::Id::En:
                std::swprintf(buffer, 512,
                              g_job.cancelled ? L"Cancelled.\n\n%d created, %d skipped."
                                              : L"Done: %d created, %d skipped.",
                              g_job.state.created, g_job.state.failed);
                break;
            case lang::Id::Ja:
                std::swprintf(buffer, 512,
                              g_job.cancelled ? L"キャンセルしました。\n\n%d 個作成、%d 個スキップ。"
                                              : L"完了: %d 個作成、%d 個スキップ。",
                              g_job.state.created, g_job.state.failed);
                break;
            default:
                std::swprintf(buffer, 512,
                              g_job.cancelled ? L"已取消。\n\n已生成 %d 个，跳过 %d 个。"
                                              : L"完成：已生成 %d 个物件，跳过 %d 个。",
                              g_job.state.created, g_job.state.failed);
                break;
        }
    }

    MessageBoxW(HostWindow(), buffer, lang::T(L"Cze 对轨", L"CzeAlign", L"CzeAlign"),
                MB_ICONINFORMATION | MB_OK);

    g_job.plan.clear();
    g_job.index = 0;
    g_job.cancelled = false;
    g_job.busy_retries = 0;
    g_job.ms_per_object = 0.0;
    g_job.state = ra::CreateState();
}

void StepJob() {
    if (!g_job.active) {
        return;
    }
    if (g_job.cancelled) {
        FinishJob(nullptr);
        return;
    }
    if (g_job.index >= g_job.plan.size()) {
        FinishJob(nullptr);
        return;
    }
    if (!g_edit || !g_edit->call_edit_section_param) {
        FinishJob(lang::T(L"无法访问编辑区（EDIT_HANDLE 不可用）。",
                          L"Cannot access the edit section.",
                          L"編集セクションにアクセスできません。"));
        return;
    }

    BatchCtx ctx{g_job.index, g_job.index + NextChunkSize()};

    LARGE_INTEGER start{};
    LARGE_INTEGER end{};
    QueryPerformanceCounter(&start);
    const bool ok = g_edit->call_edit_section_param(&ctx, BatchProc);
    QueryPerformanceCounter(&end);

    if (!ok) {
        if (++g_job.busy_retries > 60) {
            FinishJob(lang::T(L"宿主一直处于忙碌状态（可能在播放或输出中），已停止。",
                              L"The host stayed busy (playing/exporting); stopped.",
                              L"ホストがビジー状態のため停止しました。"));
            return;
        }
        SetTimer(g_job.hwnd, 1, 250, nullptr);
        return;
    }
    g_job.busy_retries = 0;

    const size_t advanced = ctx.end - ctx.begin;
    if (advanced > 0) {
        const double elapsed_ms =
            static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 / g_job.freq.QuadPart;
        const double per = elapsed_ms / static_cast<double>(advanced);
        g_job.ms_per_object =
            (g_job.ms_per_object > 0.0) ? (g_job.ms_per_object * 0.5 + per * 0.5) : per;
    }

    g_job.index = ctx.end;
    UpdateProgress();

    if (g_job.index < g_job.plan.size()) {
        PostMessageW(g_job.hwnd, kMsgNext, 0, 0);
    } else {
        FinishJob(nullptr);
    }
}

LRESULT CALLBACK ProgressProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
        case WM_CREATE: {
            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            g_job.text = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE, 16, 14, 360, 20,
                                         hwnd, reinterpret_cast<HMENU>(kIdText), g_instance,
                                         nullptr);
            g_job.bar = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE, 16, 44, 360,
                                        18, hwnd, reinterpret_cast<HMENU>(kIdBar), g_instance,
                                        nullptr);
            HWND cancel = CreateWindowExW(0, L"BUTTON",
                                          lang::T(L"取消", L"Cancel", L"キャンセル"),
                                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 288, 76, 88, 26,
                                          hwnd, reinterpret_cast<HMENU>(IDCANCEL), g_instance,
                                          nullptr);
            for (HWND control : {g_job.text, g_job.bar, cancel}) {
                if (control) {
                    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
                }
            }
            SendMessageW(g_job.bar, PBM_SETRANGE32, 0,
                         static_cast<LPARAM>(std::max<size_t>(1, g_job.plan.size())));
            return 0;
        }

        case kMsgNext:
            StepJob();
            return 0;

        case WM_TIMER:
            if (wparam == 1) {
                KillTimer(hwnd, 1);
                StepJob();
            }
            return 0;

        case WM_COMMAND:
            if (LOWORD(wparam) == IDCANCEL) {
                g_job.cancelled = true;
                if (g_job.text) {
                    SetWindowTextW(g_job.text, lang::T(L"正在停止…", L"Stopping…", L"停止中…"));
                }
                if (g_job.bar) {
                    EnableWindow(g_job.bar, FALSE);
                }
            }
            return 0;

        case WM_CLOSE:
            g_job.cancelled = true;
            return 0;

        default:
            break;
    }
    return DefWindowProcW(hwnd, message, wparam, lparam);
}

void CreateProgressWindow() {
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = ProgressProc;
    wc.hInstance = g_instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
    wc.lpszClassName = kProgressClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return;
    }

    RECT rect{0, 0, 392, 118};
    AdjustWindowRectEx(&rect, WS_POPUP | WS_CAPTION | WS_SYSMENU, FALSE, 0);

    HWND owner = HostWindow();
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;
    if (owner && IsWindow(owner)) {
        RECT owner_rect{};
        GetWindowRect(owner, &owner_rect);
        x = owner_rect.left + ((owner_rect.right - owner_rect.left) - (rect.right - rect.left)) / 2;
        y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - (rect.bottom - rect.top)) / 2;
    }

    g_job.hwnd = CreateWindowExW(WS_EX_DLGMODALFRAME, kProgressClass,
                                 lang::T(L"Cze 对轨", L"CzeAlign", L"CzeAlign"),
                                 WS_POPUP | WS_CAPTION | WS_SYSMENU, x, y, rect.right - rect.left,
                                 rect.bottom - rect.top, owner, nullptr, g_instance, nullptr);
    if (!g_job.hwnd) {
        return;
    }
    ShowWindow(g_job.hwnd, SW_SHOW);
    UpdateProgress();
    PostMessageW(g_job.hwnd, kMsgNext, 0, 0);
}

void RunImport(EDIT_SECTION* edit, const wchar_t* initial_path) {
    if (!edit) {
        return;
    }
    if (g_job.active) {
        ShowError(lang::T(L"上一批物件还在生成中，请先等它完成或取消。",
                          L"A previous batch is still running. Wait or cancel it first.",
                          L"前のバッチが実行中です。完了またはキャンセルしてください。"));
        return;
    }

    ra::ImportOptions options;
    options.fps = ProjectFps(edit);
    options.layer_limit = 100;

    std::wstring error;
    const std::wstring initial = initial_path ? initial_path : L"";
    if (!ShowImportDialog(HostWindow(), g_instance, initial, &options, &error)) {
        if (!error.empty()) {
            ShowError(error);
        }
        return;
    }

    ra::RppProject project;
    if (!ra::ParseRppFile(options.path, &project, &error)) {
        ShowError(error);
        return;
    }

    std::vector<ra::PlannedObject> plan = ra::BuildPlan(project, options);
    if (plan.empty()) {
        ShowError(lang::T(L"这个时间范围 / 轨道里没有可生成的 item。",
                          L"No items to create in this range / track selection.",
                          L"この範囲 / トラックに作成できる item がありません。"));
        return;
    }

    g_job = Job();
    g_job.active = true;
    g_job.plan = std::move(plan);
    g_job.name_objects = options.name_objects;
    g_job.state = ra::CreateState();
    g_job.state.effect = options.effect.empty() ? L"画像ファイル" : options.effect;
    QueryPerformanceFrequency(&g_job.freq);

    CreateProgressWindow();
    if (!g_job.hwnd) {
        g_job.active = false;
        ShowError(lang::T(L"无法创建进度窗口。", L"Cannot create the progress window.",
                          L"進行状況ウィンドウを作成できません。"));
    }
}

void OnImportMenu(EDIT_SECTION* edit) {
    RunImport(edit, nullptr);
}

void OnFileDrop(EDIT_SECTION* edit, LPCWSTR file) {
    RunImport(edit, file);
}

}  // namespace

extern "C" __declspec(dllexport) COMMON_PLUGIN_TABLE* GetCommonPluginTable(void) {
    lang::EnsureDetected();
    static COMMON_PLUGIN_TABLE table{};
    table.name = lang::T(L"Cze 对轨 (CzeAlign)", L"CzeAlign", L"CzeAlign");
    table.information = lang::T(
        L"CzeAlign 1.0.0 - 读取 .rpp，按时间/音高分批生成图片空白物件（可选轨道、时间范围、图层回卷）。",
        L"CzeAlign 1.0.0 - Creates blank image objects from a .rpp, batched, with track/range/layer wrap options.",
        L"CzeAlign 1.0.0 - .rpp から空の画像オブジェクトを一括生成（トラック・範囲・レイヤー折り返し対応）。");
    return &table;
}

extern "C" __declspec(dllexport) bool InitializePlugin(DWORD /*version*/) {
    return true;
}

extern "C" __declspec(dllexport) void UninitializePlugin(void) {
    g_edit = nullptr;
}

extern "C" __declspec(dllexport) void RegisterPlugin(HOST_APP_TABLE* host) {
    if (!host) {
        return;
    }

    lang::EnsureDetected();
    g_edit = host->create_edit_handle();

    if (host->register_import_menu) {
        host->register_import_menu(
            lang::T(L"Cze 对轨导入 (RPP)...", L"CzeAlign Import (RPP)...",
                    L"CzeAlign インポート (RPP)..."),
            OnImportMenu);
    }
    if (host->register_file_drop_handler) {
        host->register_file_drop_handler(
            lang::T(L"Cze 对轨导入", L"CzeAlign Import", L"CzeAlign インポート"), L"*.rpp",
            OnFileDrop);
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID /*reserved*/) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(module);
        g_instance = reinterpret_cast<HINSTANCE>(module);
    }
    return TRUE;
}
