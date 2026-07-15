#include "Input.h"
#include <cassert>
#include <cstring>
#include <algorithm>
#include <cstdlib>

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
    gamePadState_.prevButtons = gamePadState_.buttons;

    if (keyboard_) {
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
    } else {
		// もしkeyboard_がnullptrの場合、キー状態をゼロで初期化
        std::memset(key_, 0, sizeof(key_));
    }

    // --- マウス情報の取得 ---
    DIMOUSESTATE mouseData{};
    if (mouse_) {
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

	// --- マウス座標の取得 ---
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(winApp_->GetHwnd(), &p);
    mouseState_.posX = p.x;
    mouseState_.posY = p.y;

	// --- ゲームパッド情報の取得 ---
    XINPUT_STATE xinputState{};
    DWORD result = XInputGetState(0, &xinputState);
    gamePadState_.connected = (result == ERROR_SUCCESS);
	// ゲームパッドが接続されている場合は状態を更新
    if (gamePadState_.connected) {
        const XINPUT_GAMEPAD& pad = xinputState.Gamepad;
        gamePadState_.buttons = pad.wButtons;
        gamePadState_.leftStickX = NormalizeStickAxis(pad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        gamePadState_.leftStickY = NormalizeStickAxis(pad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        gamePadState_.rightStickX = NormalizeStickAxis(pad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        gamePadState_.rightStickY = NormalizeStickAxis(pad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        gamePadState_.leftTrigger = NormalizeTrigger(pad.bLeftTrigger);
        gamePadState_.rightTrigger = NormalizeTrigger(pad.bRightTrigger);
    } else {
		// ゲームパッドが接続されていない場合は状態をリセット
        gamePadState_.buttons = 0;
        gamePadState_.leftStickX = 0.0f;
        gamePadState_.leftStickY = 0.0f;
        gamePadState_.rightStickX = 0.0f;
        gamePadState_.rightStickY = 0.0f;
        gamePadState_.leftTrigger = 0.0f;
        gamePadState_.rightTrigger = 0.0f;
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

// コントローラーのボタンが押されているか判定
bool Input::PushControllerButton(WORD button) const {
    return gamePadState_.connected && (gamePadState_.buttons & button) != 0;
}

// コントローラーのボタンがトリガーされたか（押した瞬間）判定
bool Input::TriggerControllerButton(WORD button) const {
    return gamePadState_.connected &&
        (gamePadState_.buttons & button) != 0 &&
        (gamePadState_.prevButtons & button) == 0;
}

// スティックの値を正規化する関数
float Input::NormalizeStickAxis(SHORT value, SHORT deadZone) const {
    const int absValue = std::abs(static_cast<int>(value));
	// デッドゾーン内の値は0にする
    if (absValue <= deadZone) {
        return 0.0f;
    }

	// 正規化の計算
    const float sign = value < 0 ? -1.0f : 1.0f;
    const float normalized = static_cast<float>(absValue - deadZone) /
        static_cast<float>(32767 - deadZone);
    return sign * std::clamp(normalized, 0.0f, 1.0f);
}

// トリガーの値を正規化する関数
float Input::NormalizeTrigger(BYTE value) const {
    if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        return 0.0f;
    }

	// 正規化の計算
    return static_cast<float>(value - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) /
        static_cast<float>(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}
