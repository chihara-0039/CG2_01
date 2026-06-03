#include "WinApp.h"
#include <cstdint>
#include <mmsystem.h>


// ImGuiなどを使う場合
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


#pragma comment(lib, "winmm.lib")

void WinApp::Initialize() {
    HRESULT hr = CoInitializeEx(0, COINIT_MULTITHREADED);

    // --- メンバ変数名 wc_ に統一 ---
    wc_.lpfnWndProc = WindowProc;
    wc_.lpszClassName = L"CG2WindowClass";
    wc_.hInstance = GetModuleHandle(nullptr);
    wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc_);

    RECT wrc = { 0, 0, kWindowWidth, kWindowHeight };
    AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

    // --- メンバ変数名 hwnd_ に統一 ---
    hwnd_ = CreateWindowW(
        wc_.lpszClassName,
        L"自作エンジン",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        wrc.right - wrc.left,
        wrc.bottom - wrc.top,
        nullptr,
        nullptr,
        wc_.hInstance,
        nullptr);

    ShowWindow(hwnd_, SW_SHOW);
    timeBeginPeriod(1);
}

void WinApp::Finalize() {
    CloseWindow(hwnd_);
    CoUninitialize();
}

bool WinApp::ProcessMessage() {
    MSG msg{};
    if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (msg.message == WM_QUIT) {
        return true;
    }
    return false;
}

LRESULT CALLBACK WinApp::WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
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
    PostQuitMessage(0);
    return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}