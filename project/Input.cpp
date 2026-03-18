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

    // デバイス生成
    // Ensure directInput_ is valid before creating the device
    if (directInput_) {

		// --- キーボードの初期化 ---

        hr = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
        assert(SUCCEEDED(hr));

        // 入力データ形式のセット
        hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
        assert(SUCCEEDED(hr));

        // 排他制御レベルのセット
        hr = keyboard_->SetCooperativeLevel(
            winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
        assert(SUCCEEDED(hr));

		// --- マウスの初期化 ---

		// マウスデバイスの生成
        hr = directInput_->CreateDevice(GUID_SysMouse, mouse_.GetAddressOf(), nullptr);
        assert(SUCCEEDED(hr));

		// 入力データ形式のセット
        hr = mouse_->SetDataFormat(&c_dfDIMouse);
        assert(SUCCEEDED(hr));
        // マウスはウィンドウ外でも動くように NONEXCLUSIVE を設定
        hr = mouse_->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
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

    // --- マウス情報の取得 ---
    DIMOUSESTATE mouseData;
    hr = mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseData);
    if (FAILED(hr)) {
        // ウィンドウがアクティブでない場合は再取得を試みる
        mouse_->Acquire();
        mouse_->GetDeviceState(sizeof(DIMOUSESTATE), &mouseData);
    }

    // 使いやすい形式に変換して保存
    mouseState_.x = mouseData.lX;
    mouseState_.y = mouseData.lY;
    mouseState_.wheel = mouseData.lZ;
    for (int i = 0; i < 3; i++) {
        mouseState_.buttons[i] = (mouseData.rgbButtons[i] & 0x80) != 0;
    }
}

// キーが押されているか判定
bool Input::PushKey(BYTE keyNumber) const {
    if (key_[keyNumber]) {
        return true;
    }
    return false;
}

// キーがトリガーされたか（押した瞬間）判定
bool Input::TriggerKey(BYTE keyNumber) const {
    if (key_[keyNumber] && !keyPre_[keyNumber]) {
        return true;
    }
    return false;
}