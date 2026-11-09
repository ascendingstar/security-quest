// Хост окна: Win32 + Direct3D 11 + Dear ImGui, значок в трее, один экземпляр.
// Ключи для проверки без рук: --screenshot <png> [--page имя] [--dialog имя] [--size ШxВ]; --hidden — старт в трее.

#include "app.h"
#include "theme.h"
#include "util.h"
#include "win.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <chrono>
#include <cstdio>
#include <map>
#include <thread>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace sq;
using namespace sq::gui;

namespace {

constexpr wchar_t kClassName[] = L"SecurityQuest.Window";
constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_SHOW_EXISTING = WM_APP + 2;
enum MenuCommand : UINT { CmdOpen = 1, CmdQuit };

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain* g_swap = nullptr;
ID3D11RenderTargetView* g_rtv = nullptr;
UINT g_resize_w = 0, g_resize_h = 0;
bool g_occluded = false;

App* g_app = nullptr;
Tray g_tray;
HICON g_icon = nullptr;
UINT g_taskbar_created = 0;
bool g_quitting = false;
bool g_told_about_tray = false;
float g_pending_dpi = 0;

void create_render_target() {
    ID3D11Texture2D* back = nullptr;
    g_swap->GetBuffer(0, IID_PPV_ARGS(&back));
    g_device->CreateRenderTargetView(back, nullptr, &g_rtv);
    back->Release();
}

void cleanup_render_target() {
    if (g_rtv) g_rtv->Release();
    g_rtv = nullptr;
}

bool create_device(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hwnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL got;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, 2, D3D11_SDK_VERSION,
                                               &sd, &g_swap, &g_device, &got, &g_context);
    if (hr == DXGI_ERROR_UNSUPPORTED || FAILED(hr))  // нет видеокарты (сервер, RDP) — программный WARP
        hr = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, levels, 2, D3D11_SDK_VERSION, &sd,
                                           &g_swap, &g_device, &got, &g_context);
    if (FAILED(hr)) return false;
    create_render_target();
    return true;
}

void cleanup_device() {
    cleanup_render_target();
    if (g_swap) g_swap->Release();
    if (g_context) g_context->Release();
    if (g_device) g_device->Release();
}

bool capture(const fs::path& file) {
    ID3D11Texture2D* back = nullptr;
    if (FAILED(g_swap->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
    D3D11_TEXTURE2D_DESC desc;
    back->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.BindFlags = 0;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    ID3D11Texture2D* staging = nullptr;
    bool ok = false;
    if (SUCCEEDED(g_device->CreateTexture2D(&desc, nullptr, &staging))) {
        g_context->CopyResource(staging, back);
        D3D11_MAPPED_SUBRESOURCE map;
        if (SUCCEEDED(g_context->Map(staging, 0, D3D11_MAP_READ, 0, &map))) {
            ok = write_png(file, int(desc.Width), int(desc.Height), static_cast<const uint8_t*>(map.pData), int(map.RowPitch));
            g_context->Unmap(staging, 0);
        }
        staging->Release();
    }
    back->Release();
    return ok;
}

void show_window(HWND hwnd) {
    if (IsIconic(hwnd)) ShowWindow(hwnd, SW_RESTORE);
    else ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);
    if (g_app) g_app->window_visible = true;
}

void tray_menu(HWND hwnd, POINT pt) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, CmdOpen, L"Открыть");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, CmdQuit, L"Выход");
    SetMenuDefaultItem(menu, CmdOpen, FALSE);
    SetForegroundWindow(hwnd);
    UINT cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, hwnd, nullptr);
    DestroyMenu(menu);
    if (cmd == CmdOpen) show_window(hwnd);
    if (cmd == CmdQuit) {
        g_quitting = true;
        DestroyWindow(hwnd);
    }
}

LRESULT WINAPI wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) return true;

    if (msg == g_taskbar_created && g_taskbar_created) {  // Explorer перезапустился — значок нужно вернуть
        g_tray.add(hwnd, g_icon, WM_TRAY);
        return 0;
    }
    switch (msg) {
        case WM_SIZE:
            if (wparam != SIZE_MINIMIZED) {
                g_resize_w = LOWORD(lparam);
                g_resize_h = HIWORD(lparam);
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wparam & 0xfff0) == SC_KEYMENU) return 0;
            break;
        case WM_DPICHANGED: {
            g_pending_dpi = HIWORD(wparam) / 96.0f;
            auto* r = reinterpret_cast<RECT*>(lparam);
            SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            return 0;
        }
        case WM_CLOSE:
            if (g_app && g_app->close_to_tray() && !g_quitting) {
                ShowWindow(hwnd, SW_HIDE);
                g_app->window_visible = false;
                if (!g_told_about_tray) {
                    g_tray.balloon("Security Quest работает в трее", "Синхронизация и уведомления продолжаются. Выход — через меню значка.");
                    g_told_about_tray = true;
                }
                return 0;
            }
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SHOW_EXISTING:
            show_window(hwnd);
            return 0;
        case WM_TRAY:
            switch (LOWORD(lparam)) {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                case WM_LBUTTONDBLCLK: show_window(hwnd); break;
                case WM_CONTEXTMENU: tray_menu(hwnd, POINT{GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam)}); break;
            }
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

struct Options {
    std::string screenshot, page, dialog;
    int width = 0, height = 0;
    bool hidden = false;
};

Options parse_options() {
    Options o;
    auto args = utf8_args();
    for (size_t i = 1; i < args.size(); ++i) {
        auto next = [&] { return i + 1 < args.size() ? args[++i] : std::string(); };
        if (args[i] == "--screenshot") o.screenshot = next();
        else if (args[i] == "--page") o.page = next();
        else if (args[i] == "--dialog") o.dialog = next();
        else if (args[i] == "--size") std::sscanf(next().c_str(), "%dx%d", &o.width, &o.height);
        else if (args[i] == "--hidden") o.hidden = true;
    }
    return o;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    Options opt = parse_options();
    bool shot = !opt.screenshot.empty();

    // Один экземпляр на папку данных: повторный запуск просто показывает уже открытое окно.
    // С другой папкой (демо, SQ_HOME) окно открывается рядом и не мешает основному.
    HANDLE single = nullptr;
    if (!shot) {
        std::error_code ec;
        auto home = std::hash<std::string>{}(from_path(fs::weakly_canonical(data_home(), ec)));
        std::wstring name = L"Local\\SecurityQuest.Gui." + std::to_wstring(home);
        single = CreateMutexW(nullptr, TRUE, name.c_str());
        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            if (HWND existing = FindWindowW(kClassName, nullptr)) PostMessageW(existing, WM_SHOW_EXISTING, 0, 0);
            return 0;
        }
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ImGui_ImplWin32_EnableDpiAwareness();
    float scale = ImGui_ImplWin32_GetDpiScaleForMonitor(MonitorFromPoint(POINT{0, 0}, MONITOR_DEFAULTTOPRIMARY));

    g_icon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    WNDCLASSEXW wc{sizeof(wc), CS_CLASSDC, wnd_proc, 0, 0, instance, g_icon, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, kClassName, g_icon};
    RegisterClassExW(&wc);

    RECT work;
    SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
    int w = opt.width ? opt.width : std::min(int(1260 * scale), int(work.right - work.left) - 40);
    int h = opt.height ? opt.height : std::min(int(820 * scale), int(work.bottom - work.top) - 40);
    HWND hwnd = CreateWindowExW(0, kClassName, L"Security Quest", WS_OVERLAPPEDWINDOW, work.left + (work.right - work.left - w) / 2,
                                work.top + (work.bottom - work.top - h) / 2, w, h, nullptr, nullptr, instance, nullptr);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &dark, sizeof(dark));
    if (shot) {
        // Для снимка нужна клиентская область ровно заданного размера.
        RECT rc{0, 0, w, h};
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
        SetWindowPos(hwnd, nullptr, 0, 0, rc.right - rc.left, rc.bottom - rc.top, SWP_NOMOVE | SWP_NOZORDER);
    }

    if (!create_device(hwnd)) {
        MessageBoxW(hwnd, L"Не удалось инициализировать Direct3D 11.", L"Security Quest", MB_ICONERROR);
        return 1;
    }
    if (!shot && !opt.hidden) {
        ShowWindow(hwnd, SW_SHOWDEFAULT);
        UpdateWindow(hwnd);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // портативность: никаких imgui.ini рядом с программой
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    load_fonts();
    setup_style(scale);
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device, g_context);

    App app(hwnd);
    g_app = &app;
    app.window_visible = !opt.hidden && !shot;
    static const std::map<std::string, Page> pages = {{"overview", Page::Overview}, {"missions", Page::Missions}, {"sides", Page::Sides},
                                                      {"campaign", Page::Campaign}, {"town", Page::Town},
                                                      {"history", Page::History}, {"settings", Page::Settings}};
    if (pages.count(opt.page)) app.page = pages.at(opt.page);
    if (!opt.dialog.empty()) app.open_dialog(opt.dialog);

    if (!shot) {
        g_taskbar_created = RegisterWindowMessageW(L"TaskbarCreated");
        g_tray.add(hwnd, g_icon, WM_TRAY);
        g_tray.set_tip(app.tray_tip());
        app.notify = [](const std::string& title, const std::string& text) { g_tray.balloon(title, text); };
    }

    auto last_input = std::chrono::steady_clock::now();
    std::string tip = app.tray_tip();
    int frame = 0;
    bool done = false;
    while (!done) {
        bool idle = std::chrono::steady_clock::now() - last_input > std::chrono::milliseconds(600) && !app.busy();
        if (!shot && (idle || !app.window_visible))
            MsgWaitForMultipleObjects(0, nullptr, FALSE, app.window_visible ? 250 : 1000, QS_ALLINPUT);
        else if (!shot && app.frame_cap_ms() > 0)
            MsgWaitForMultipleObjects(0, nullptr, FALSE, DWORD(app.frame_cap_ms()), QS_ALLINPUT);

        MSG msg;
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
            if (msg.message == WM_QUIT) done = true;
            if (msg.message != WM_TIMER && msg.message != WM_PAINT) last_input = std::chrono::steady_clock::now();
        }
        if (done) break;

        if (!app.window_visible && !shot) {
            app.tick();  // в трее: таймер синхронизации и уведомления работают без отрисовки
        } else {
            if (g_occluded && g_swap->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            g_occluded = false;
            if (g_resize_w && g_resize_h) {
                cleanup_render_target();
                g_swap->ResizeBuffers(0, g_resize_w, g_resize_h, DXGI_FORMAT_UNKNOWN, 0);
                g_resize_w = g_resize_h = 0;
                create_render_target();
            }
            if (g_pending_dpi > 0) {
                ImGui::GetStyle() = ImGuiStyle();
                setup_style(g_pending_dpi);
                g_pending_dpi = 0;
            }

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();
            app.tick();
            app.frame();
            ImGui::Render();
            const float clear[4] = {14 / 255.f, 18 / 255.f, 29 / 255.f, 1};
            g_context->OMSetRenderTargets(1, &g_rtv, nullptr);
            g_context->ClearRenderTargetView(g_rtv, clear);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // Шрифты подгружаются динамически, поэтому снимок делается не с первого кадра.
            if (shot && ++frame == 8) {
                bool ok = capture(to_path(opt.screenshot));
                done = true;
                if (!ok) return 2;
            }
            if (!shot) g_occluded = g_swap->Present(1, 0) == DXGI_STATUS_OCCLUDED;
        }

        if (!shot && app.tray_tip() != tip) {
            tip = app.tray_tip();
            g_tray.set_tip(tip);
        }
    }

    g_tray.remove();
    g_app = nullptr;
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    cleanup_device();
    DestroyWindow(hwnd);
    UnregisterClassW(kClassName, instance);
    if (single) CloseHandle(single);
    CoUninitialize();
    return 0;
}
