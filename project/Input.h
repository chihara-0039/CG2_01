#pragma once
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>

class Input {
public:
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    void Initialize(HINSTANCE hInstance, HWND hwnd);
    void Update();

private:
    // ずっと生存させる（←NEW）
    ComPtr<IDirectInput8>       directInput_;
    ComPtr<IDirectInputDevice8> keyboard_;

    // 再作成用に覚えておく（←NEW）
    HINSTANCE hInstance_ = nullptr;
    HWND      hwnd_ = nullptr;

    // なくなってたら安全に作り直す（←NEW）
    bool EnsureDevice_();
};
