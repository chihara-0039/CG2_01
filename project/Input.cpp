#include "Input.h"
#include <cassert>
#include <cstring>

namespace {
    constexpr BYTE kPressedMask = 0x80; // DirectInput の「押下」ビット
}


void Input::Initialize(HINSTANCE hInstance, HWND hwnd) {
    hInstance_ = hInstance;
    hwnd_ = hwnd;

    // DirectInput 本体をメンバに保持（親を生かし続ける）
    HRESULT hr = DirectInput8Create(
        hInstance_, DIRECTINPUT_VERSION, IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.GetAddressOf()), nullptr);
    assert(SUCCEEDED(hr));

    // デバイスの確保
    EnsureDevice_();
}

bool Input::EnsureDevice_() {
    if (!directInput_) return false;

    if (!keyboard_) {
        HRESULT hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
        if (FAILED(hr)) { keyboard_.Reset(); return false; }

        hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
        if (FAILED(hr)) { keyboard_.Reset(); return false; }

        hr = keyboard_->SetCooperativeLevel(hwnd_,
                 DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
        if (FAILED(hr)) { keyboard_.Reset(); return false; }
    }
    return true;
}

bool Input::PushKey(BYTE keyNumber) {
    // 押下フラグ(0x80)で判定するのがDirectInputの定石
    return (key[keyNumber] & kPressedMask) != 0;
}

bool Input::TriggerKey(BYTE keyNumber) {
    // 「前フレームは離していた && 今フレームで押された」
    const bool wasDown = (keyPre[keyNumber] & kPressedMask) != 0;
    const bool isDown = (key[keyNumber] & kPressedMask) != 0;
    return (!wasDown && isDown);
}

void Input::Update() {
    // 親 or 子が無効なら作り直しを試みる
    if (!directInput_) {
        if (FAILED(DirectInput8Create(hInstance_, DIRECTINPUT_VERSION, IID_IDirectInput8,
                                      reinterpret_cast<void**>(directInput_.GetAddressOf()), nullptr))) {
            return; // 今フレームは諦める
        }
    }
    if (!EnsureDevice_()) return;

    // Acquire リトライ（Alt+Tab 等）
    HRESULT hr = keyboard_->Acquire();
    if (FAILED(hr)) {
        keyboard_->Unacquire();
        hr = keyboard_->Acquire();
        if (FAILED(hr)) return; // 未取得ならこのフレームは未入力扱い
    }

    //前回のキー入力を保存
    memcpy(keyPre, key, sizeof(key));
    
    hr = keyboard_->GetDeviceState(sizeof(key), key);
    if (FAILED(hr)) {
        // 入力ロスト等。次フレーム以降に再取得を試みる。
        keyboard_->Unacquire();
        std::memset(key, 0, sizeof(key));
    }


}


