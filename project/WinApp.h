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
};
