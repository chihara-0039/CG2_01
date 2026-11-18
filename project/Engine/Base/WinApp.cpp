#include "WinApp.h"
#include <cstdint>
#include <mmsystem.h>

// ImGuiなどを使う場合
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

#pragma comment(lib, "winmm.lib")

namespace {
// Alt中と非アクティブ時は通常の矢印を使い、それ以外のゲーム画面では隠す。
void SetGameCursor(HWND hwnd) {
    const bool visible = GetForegroundWindow() != hwnd || (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
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
                          wrc.right - wrc.left, wrc.bottom - wrc.top, nullptr, nullptr, wc_.hInstance, nullptr);

    ShowWindow(hwnd_, SW_SHOW);
    timeBeginPeriod(1);
}

// カーソルを通常表示へ戻し、ウィンドウとCOMの終了処理を行う。
void WinApp::Finalize() {
    SetCursor(LoadCursor(nullptr, IDC_ARROW));
    CloseWindow(hwnd_);
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
    if (msg == WM_SETCURSOR && LOWORD(lparam) == HTCLIENT) {
        SetGameCursor(hwnd);
        return TRUE;
    }
    if (msg == WM_ACTIVATE && LOWORD(wparam) == WA_INACTIVE) {
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
