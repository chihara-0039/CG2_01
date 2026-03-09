#include "Input.h"
#include <cassert>
#include <cstring>

void Input::Initialize(WinApp* winApp) {
    winApp_ = winApp;
    HRESULT hr;

    // DirectInputオブジェクトの生成
    hr = DirectInput8Create(
        winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
        reinterpret_cast<void**>(directInput_.GetAddressOf()), nullptr);
    assert(SUCCEEDED(hr));

    // キーボードデバイスの生成
    // Ensure directInput_ is valid before creating the device
    if (directInput_) {
        hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
        assert(SUCCEEDED(hr));

        // 入力データ形式のセット
        hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
        assert(SUCCEEDED(hr));

        // 排他制御レベルのセット
        hr = keyboard_->SetCooperativeLevel(
            winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
        assert(SUCCEEDED(hr));

        // 初回はキー状態をゼロで初期化
        std::memset(key_, 0, sizeof(key_));
        std::memset(keyPre_, 0, sizeof(keyPre_));
    } else {
        // If DirectInput creation failed, clear buffers to safe defaults
        std::memset(key_, 0, sizeof(key_));
        std::memset(keyPre_, 0, sizeof(keyPre_));
    }
}

void Input::Update() {
    HRESULT hr;

    // 前回のキー入力を保存
    std::memcpy(keyPre_, key_, sizeof(key_));

    if (!keyboard_) {
        // If keyboard device not available, clear current state
        std::memset(key_, 0, sizeof(key_));
        return;
    }

    // キーボード情報の取得開始
    hr = keyboard_->Acquire();
    if (FAILED(hr)) {
        // Try to reacquire but don't assert in runtime
        // We'll still attempt to GetDeviceState below
    }

    // 全キーの入力状態を取得する
    hr = keyboard_->GetDeviceState(sizeof(key_), key_);
    if (FAILED(hr)) {
        keyboard_->Acquire();
        hr = keyboard_->GetDeviceState(sizeof(key_), key_);
    }
}

bool Input::PushKey(BYTE keyNumber) {
    // 0以外なら押されている
    return key_[keyNumber] != 0;
}

bool Input::TriggerKey(BYTE keyNumber) {
    // 今押されていて、前は押されていなかったらトリガー
    return (key_[keyNumber] != 0) && (keyPre_[keyNumber] == 0);
}