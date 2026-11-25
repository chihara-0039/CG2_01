#include "Input.h"
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

// キーボード・マウス・パッドの入力を準備し、初期値と必要なリソースを設定する。
void Input::Initialize(WinApp *winApp) {
    winApp_ = winApp;
    HRESULT resultCode;

    // キーボード用のDirectInput本体を生成する。
    resultCode = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
                                    reinterpret_cast<void **>(directInput_.GetAddressOf()), nullptr);
    assert(SUCCEEDED(resultCode));

    if (directInput_) {
        // キーボードデバイスを生成する。
        resultCode = directInput_->CreateDevice(GUID_SysKeyboard, keyboard_.GetAddressOf(), nullptr);
        assert(SUCCEEDED(resultCode));

        // キーボードの入力データ形式を DirectInput 標準形式に設定する。
        resultCode = keyboard_->SetDataFormat(&c_dfDIKeyboard);
        assert(SUCCEEDED(resultCode));

        // ゲームウィンドウが前面にある間だけ、他アプリと共存する形で入力を受け取る。
        resultCode =
            keyboard_->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
        assert(SUCCEEDED(resultCode));
    }
    // マウスのRaw Input登録をDirectInputと競合させないため、DIはキーボードだけで使う。
    rawMouseEnabled_ = winApp_->EnableRawMouse();

    // 初回更新前に前回入力が残らないよう、入力バッファを明示的にクリアする。
    std::memset(key_, 0, sizeof(key_));
    std::memset(keyPre_, 0, sizeof(keyPre_));
}

// 前フレームの状態を保存し、各入力機器の最新状態と使用中の機器を取得する。
void Input::Update() {
    HRESULT resultCode;

    // トリガー判定のため、更新前のキーボード/パッド状態を保存する。
    std::memcpy(keyPre_, key_, sizeof(key_));
    gamePadState_.prevButtons = gamePadState_.buttons;

    if (keyboard_) {
        // フォーカス復帰直後は Acquire が必要になることがある。
        resultCode = keyboard_->Acquire();

        resultCode = keyboard_->GetDeviceState(sizeof(key_), key_);
        if (FAILED(resultCode)) {
            keyboard_->Acquire();
            resultCode = keyboard_->GetDeviceState(sizeof(key_), key_);
        }
    } else {
        std::memset(key_, 0, sizeof(key_));
    }

    // カーソル位置と独立した相対移動を優先し、画面端でも物理的な移動量を得る。
    const bool foreground = GetForegroundWindow() == winApp_->GetHwnd();
    const auto raw = winApp_->ConsumeRawMouse(foreground);
    POINT physicalCursor{};
    GetCursorPos(&physicalCursor);
    mouseState_.x = 0;
    mouseState_.y = 0;
    mouseState_.wheel = foreground ? static_cast<int>(raw.wheel) : 0;
    if (foreground) {
        if (rawMouseEnabled_) {
            mouseState_.x = raw.x;
            mouseState_.y = raw.y;
        } else if (previousCursorValid_) {
            mouseState_.x = physicalCursor.x - previousCursor_.x;
            mouseState_.y = physicalCursor.y - previousCursor_.y;
        }
    } else {
        cameraMouseActive_ = false;
    }
    const int mouseButtons[] = {VK_LBUTTON, VK_RBUTTON, VK_MBUTTON};
    for (int button = 0; button < 3; ++button) {
        mouseState_.buttons[button] = foreground && (GetAsyncKeyState(mouseButtons[button]) & 0x8000) != 0;
    }
    previousCursor_ = physicalCursor;
    previousCursorValid_ = foreground;
    cameraMouseRequested_ = false;
    const bool alt = PushKey(DIK_LMENU) || PushKey(DIK_RMENU);
    if (alt || (cameraMouseMiddleDrag_ && !mouseState_.buttons[2])) {
        cameraMouseActive_ = false;
    }
    if (cameraMouseActive_) {
        RecenterCameraMouse();
    }

    // マウスカーソルの絶対位置はウィンドウのクライアント座標に変換して保持する。
    POINT cursorPosition;
    GetCursorPos(&cursorPosition);
    ScreenToClient(winApp_->GetHwnd(), &cursorPosition);
    mouseState_.posX = cursorPosition.x;
    mouseState_.posY = cursorPosition.y;

    // Xbox コントローラーは XInput で1番スロットを取得する。
    XINPUT_STATE xinputState{};
    DWORD xinputResult = XInputGetState(0, &xinputState);
    gamePadState_.connected = (xinputResult == ERROR_SUCCESS);

    if (gamePadState_.connected) {
        const XINPUT_GAMEPAD &gamePad = xinputState.Gamepad;
        gamePadState_.buttons = gamePad.wButtons;
        gamePadState_.leftStickX = NormalizeStickAxis(gamePad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        gamePadState_.leftStickY = NormalizeStickAxis(gamePad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
        gamePadState_.rightStickX = NormalizeStickAxis(gamePad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        gamePadState_.rightStickY = NormalizeStickAxis(gamePad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
        gamePadState_.leftTrigger = NormalizeTrigger(gamePad.bLeftTrigger);
        gamePadState_.rightTrigger = NormalizeTrigger(gamePad.bRightTrigger);
    } else {
        // 未接続時は前回値で動き続けないよう、ゲームパッド入力をリセットする。
        gamePadState_.buttons = 0;
        gamePadState_.leftStickX = 0.0f;
        gamePadState_.leftStickY = 0.0f;
        gamePadState_.rightStickX = 0.0f;
        gamePadState_.rightStickY = 0.0f;
        gamePadState_.leftTrigger = 0.0f;
        gamePadState_.rightTrigger = 0.0f;
    }

    bool keyboardMouseActive = mouseState_.x != 0 || mouseState_.y != 0 || mouseState_.wheel != 0 ||
                               mouseState_.buttons[0] || mouseState_.buttons[1] || mouseState_.buttons[2];
    for (const BYTE keyState : key_) {
        if ((keyState & 0x80) != 0) {
            keyboardMouseActive = true;
            break;
        }
    }

    const bool gamePadActive =
        gamePadState_.connected &&
        (gamePadState_.buttons != 0 || std::abs(gamePadState_.leftStickX) > 0.15f ||
         std::abs(gamePadState_.leftStickY) > 0.15f || std::abs(gamePadState_.rightStickX) > 0.15f ||
         std::abs(gamePadState_.rightStickY) > 0.15f || gamePadState_.leftTrigger > 0.05f ||
         gamePadState_.rightTrigger > 0.05f);

    if (gamePadActive) {
        activeInputDevice_ = ActiveInputDevice::GamePad;
    } else if (keyboardMouseActive) {
        activeInputDevice_ = ActiveInputDevice::KeyboardMouse;
    }
}

bool Input::PushKey(BYTE keyNumber) const {
    return key_[keyNumber] != 0;
}

bool Input::TriggerKey(BYTE keyNumber) const {
    return key_[keyNumber] != 0 && keyPre_[keyNumber] == 0;
}

bool Input::PushControllerButton(WORD button) const {
    return gamePadState_.connected && (gamePadState_.buttons & button) != 0;
}

bool Input::TriggerControllerButton(WORD button) const {
    return gamePadState_.connected && (gamePadState_.buttons & button) != 0 &&
           (gamePadState_.prevButtons & button) == 0;
}

// スティックの生値を正規化し、中央付近の微小入力を除く。
float Input::NormalizeStickAxis(SHORT value, SHORT deadZone) const {
    const int absoluteValue = std::abs(static_cast<int>(value));
    if (absoluteValue <= deadZone) {
        return 0.0f;
    }

    // デッドゾーンを除いた残りの範囲を -1.0 から 1.0 に正規化する。
    const float sign = value < 0 ? -1.0f : 1.0f;
    const float normalizedValue = static_cast<float>(absoluteValue - deadZone) / static_cast<float>(32767 - deadZone);
    return sign * std::clamp(normalizedValue, 0.0f, 1.0f);
}

// トリガーの生値を0～1に変換し、押し始めの誤入力を除く。
float Input::NormalizeTrigger(BYTE value) const {
    if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
        return 0.0f;
    }

    // トリガー閾値を越えた分だけを 0.0 から 1.0 に正規化する。
    return static_cast<float>(value - XINPUT_GAMEPAD_TRIGGER_THRESHOLD) /
           static_cast<float>(255 - XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
}

// エディタのパネルと、実際に見えているモニターの範囲を除外する。
RECT Input::CameraMouseRegion() const {
    RECT client{};
    GetClientRect(winApp_->GetHwnd(), &client);
#if defined(USE_IMGUI) && !defined(NDEBUG)
    constexpr bool editor = true;
#else
    constexpr bool editor = false;
#endif
    const auto region = MouseLookPolicy::Region(client.right, client.bottom, editor);
    RECT result{region.left, region.top, region.right, region.bottom};
    MONITORINFO monitor{sizeof(MONITORINFO)};
    if (GetMonitorInfoW(MonitorFromWindow(winApp_->GetHwnd(), MONITOR_DEFAULTTONEAREST), &monitor)) {
        POINT top{monitor.rcMonitor.left, monitor.rcMonitor.top},
            bottom{monitor.rcMonitor.right, monitor.rcMonitor.bottom};
        ScreenToClient(winApp_->GetHwnd(), &top);
        ScreenToClient(winApp_->GetHwnd(), &bottom);
        const RECT visible{top.x, top.y, bottom.x, bottom.y};
        RECT clipped{};
        IntersectRect(&clipped, &result, &visible);
        IntersectRect(&result, &clipped, &client);
    }
    return result;
}

// 中央へ戻してもRaw Inputの移動量には混ぜず、端まで動かし続けられるようにする。
void Input::RecenterCameraMouse() {
    if (!winApp_ || GetForegroundWindow() != winApp_->GetHwnd()) {
        cameraMouseActive_ = false;
        return;
    }
    POINT cursor{};
    if (!GetCursorPos(&cursor)) {
        return;
    }
    ScreenToClient(winApp_->GetHwnd(), &cursor);
    const auto rect = CameraMouseRegion();
    const MouseLookPolicy::Rect region{rect.left, rect.top, rect.right, rect.bottom};
    if (rect.right - rect.left < 64 || rect.bottom - rect.top < 64 ||
        !MouseLookPolicy::NearEdge(region, cursor.x, cursor.y)) {
        return;
    }
    POINT center{(rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2};
    mouseState_.posX = center.x;
    mouseState_.posY = center.y;
    ClientToScreen(winApp_->GetHwnd(), &center);
    if (SetCursorPos(center.x, center.y)) {
        previousCursor_ = center;
    }
}

// 操作の開始は3D表示領域内だけに限定し、UIから入った直後の大きな差分は使わない。
bool Input::PrepareCameraMouseLook(bool guiCaptured, bool middleDrag) {
    const bool allowed = winApp_ && GetForegroundWindow() == winApp_->GetHwnd() && !guiCaptured &&
                         !PushKey(DIK_LMENU) && !PushKey(DIK_RMENU) && (!middleDrag || mouseState_.buttons[2]);
    if (!allowed) {
        cameraMouseActive_ = false;
        return false;
    }
    if (!cameraMouseActive_) {
        POINT cursor{};
        GetCursorPos(&cursor);
        ScreenToClient(winApp_->GetHwnd(), &cursor);
        const auto rect = CameraMouseRegion();
        if (!PtInRect(&rect, cursor)) {
            return false;
        }
        mouseState_.x = 0;
        mouseState_.y = 0;
    }
    cameraMouseActive_ = true;
    cameraMouseRequested_ = true;
    cameraMouseMiddleDrag_ = middleDrag;
    RecenterCameraMouse();
    return true;
}

// ポーズ・インベントリ・別シーンなど、カメラ更新のないフレームでは拘束を残さない。
void Input::FinishCameraMouseLookFrame() {
    cameraMouseActive_ = cameraMouseActive_ && cameraMouseRequested_;
    cameraMouseRequested_ = false;
}
