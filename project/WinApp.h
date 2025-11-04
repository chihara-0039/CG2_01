#pragma once
#include <Windows.h>
#include <wrl.h>
#include <stdint.h>

using namespace Microsoft::WRL;

class WinApp {
public:
    //静的メンバ関数
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT asg, WPARAM wparam, LPARAM lparam);

public: //メンバ関数
    //初期化
    void Initialize();

    //更新
    void Update();

public://定数
    //クライアント領域のサイズ
    static constexpr int32_t kClientWidth = 1280;
    static constexpr int32_t kClientHeight = 720;

    //getter
    HWND GetHwnd() const { return hwnd; }
    HINSTANCE GetHInstance() const { return wc.hInstance; }

private:
    //ウィンドウハンドル
    HWND hwnd = nullptr;

    //ウィンドウクラスの設定
    WNDCLASS wc{};

};
