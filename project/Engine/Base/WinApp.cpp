#include "WinApp.h"
#include <cstdint>
#include <mmsystem.h>

// ImGuiなどを使う場合
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "winmm.lib")

namespace {
// 開発構成では常に表示し、ReleaseだけAlt中・非アクティブ時以外は隠す。
void SetGameCursor(HWND hwnd) {
#if defined(NDEBUG) && !defined(DEVELOPMENT)
    constexpr bool release = true;
#else
    constexpr bool release = false;
#endif
    const bool visible =
        MouseLookPolicy::Visible(release, GetForegroundWindow() == hwnd, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
    SetCursor(visible ? LoadCursor(nullptr, IDC_ARROW) : nullptr);
}

// マウスが静止していてもAltの押し離しを反映する。ゲーム領域の外には干渉しない。
void UpdateGameCursor(HWND hwnd) {
    POINT screen{};
    RECT client{};
    if (!GetCursorPos(&screen) || WindowFromPoint(screen) != hwnd || !GetClientRect(hwnd, &client)) {
        return;
    }
    POINT local = screen;
    if (ScreenToClient(hwnd, &local) && PtInRect(&client, local)) {
        SetGameCursor(hwnd);
    }
}
}

// ゲーム用ウィンドウを準備し、初期値と必要なリソースを設定する。
void WinApp::Initialize() {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

#ifdef USE_IMGUI
    // Keep Win32 mouse coordinates and the pixels rendered by ImGui in the
    // same coordinate space, including on displays using DPI scaling.
    ImGui_ImplWin32_EnableDpiAwareness();
#endif

    // --- メンバ変数名 wc_ に統一 ---
    wc_.lpfnWndProc = WindowProc;
    wc_.lpszClassName = L"CG2WindowClass";
    wc_.hInstance = GetModuleHandle(nullptr);
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc_);

    RECT wrc = {0, 0, kWindowWidth, kWindowHeight};
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // --- メンバ変数名 hwnd_ に統一 ---
    hwnd_ = CreateWindowW(wc_.lpszClassName, L"自作エンジン", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                          wrc.right - wrc.left, wrc.bottom - wrc.top, nullptr, nullptr, wc_.hInstance, this);

    ShowWindow(hwnd_, SW_SHOW);
    timeBeginPeriod(1);
}

// カーソルを通常表示へ戻し、ウィンドウとCOMの終了処理を行う。
void WinApp::Finalize() {
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    // 入力の通知先とWinAppへの参照を解除してからウィンドウを破棄する。
    if (rawRegistered_) {
        RAWINPUTDEVICE device{0x01, 0x02, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&device, 1, sizeof(device));
        rawRegistered_ = false;
    }
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, 0);
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }
    timeEndPeriod(1);
    CoUninitialize();
}

// Windowsメッセージを処理し、終了要求の有無を返す。
bool WinApp::ProcessMessage() {
#ifdef USE_IMGUI
    if (ImGui::GetCurrentContext()) {
        // ImGuiが独自に矢印へ戻すことを防ぎ、カーソルの表示方針を一元化する。
        ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
    }
#endif
    MSG msg{};
    // Drain the entire queue every frame. Processing only one message causes
    // mouse move/click events to build up and makes ImGui feel delayed.
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            return true;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    UpdateGameCursor(hwnd_);
    return false;
}

// ウィンドウの入力・サイズ変更・終了メッセージを振り分ける。
LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto *self = reinterpret_cast<WinApp *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        self = static_cast<WinApp *>(reinterpret_cast<CREATESTRUCTW *>(lparam)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (msg == WM_INPUT && self) {
        RAWINPUT raw{};
        UINT size = sizeof(raw);
        const UINT read =
            GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER));
        if (read != static_cast<UINT>(-1) && read >= sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE) &&
            raw.header.dwType == RIM_TYPEMOUSE) {
            const auto &mouse = raw.data.mouse;
            if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0) {
                self->rawMouse_.Add(mouse.lLastX, mouse.lLastY);
                self->absoluteMouseReady_ = false;
            } else {
                // 絶対座標を送る機器も差分へ変換する。最初の位置は回転へ反映しない。
                const bool virtualDesktop = (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
                const int width = GetSystemMetrics(virtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
                const int height = GetSystemMetrics(virtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
                if (self->absoluteMouseReady_ && self->absoluteMouseDevice_ == raw.header.hDevice) {
                    self->rawMouse_.Add(
                        static_cast<LONG>((mouse.lLastX - self->absoluteMouseX_) * static_cast<double>(width) / 65535),
                        static_cast<LONG>((mouse.lLastY - self->absoluteMouseY_) * static_cast<double>(height) /
                                          65535));
                }
                self->absoluteMouseX_ = mouse.lLastX;
                self->absoluteMouseY_ = mouse.lLastY;
                self->absoluteMouseDevice_ = raw.header.hDevice;
                self->absoluteMouseReady_ = true;
            }
            if (mouse.usButtonFlags & RI_MOUSE_WHEEL) {
                self->rawMouse_.AddWheel(static_cast<SHORT>(mouse.usButtonData));
            }
        }
        // フォアグラウンドのRaw InputはDefWindowProcで後始末する。
        return DefWindowProcW(hwnd, msg, wparam, lparam);
    }

    if (msg == WM_MOUSEWHEEL && self && !self->rawRegistered_) {
        self->rawMouse_.AddWheel(static_cast<SHORT>(HIWORD(wparam)));
    }
    if (msg == WM_SETCURSOR && LOWORD(lparam) == HTCLIENT) {
        SetGameCursor(hwnd);
        return TRUE;
    }
    if (msg == WM_ACTIVATE && LOWORD(wparam) == WA_INACTIVE) {
        if (self) {
            self->rawMouse_.Consume(false);
            self->absoluteMouseReady_ = false;
        }
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
    }
    if (msg == WM_SYSCOMMAND && (wparam & 0xFFF0) == SC_KEYMENU && lparam == 0) {
        // Alt単体でシステムメニューへフォーカスが移ることを防ぐ。
        return 0;
    }
#ifdef USE_IMGUI
    // ImGuiのメッセージ処理を優先
    if (ImGui::GetCurrentContext() != nullptr) {
        if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
            return true;
        }
    }

#endif
    switch (msg) {
    case WM_CLOSE:
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// ImGuiの通常メッセージは残し、マウスの移動量とホイールをRaw Inputで取得する。
bool WinApp::EnableRawMouse() {
    RAWINPUTDEVICE device{};
    device.usUsagePage = 0x01;
    device.usUsage = 0x02;
    device.hwndTarget = hwnd_;
    rawRegistered_ = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
    return rawRegistered_;
}
