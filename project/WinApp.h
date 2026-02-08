#pragma once
#include <Windows.h>
#include <wrl.h>
#include <cstdint>

class WinApp {
public:
    // 静的メンバ関数
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

public:
    // 定数（クライアント領域のサイズ）
    static constexpr int32_t kClientWidth = 1280;
    static constexpr int32_t kClientHeight = 720;

    // 初期化
    void Initialize();
    // メッセージ処理
    bool ProcessMessage();
    // 終了
    void Finalize();

    // Getter
    HWND GetHwnd() const { return hwnd_; }
    HINSTANCE GetHInstance() const { return wc_.hInstance; }

    // ★ここを追加（DirectXCommonやInputクラスから呼ばれるため必須）
    int32_t GetWidth() const { return kClientWidth; }
    int32_t GetHeight() const { return kClientHeight; }

private:
    // ウィンドウハンドル
    HWND hwnd_ = nullptr;
    // ウィンドウクラス
    WNDCLASS wc_{};
};