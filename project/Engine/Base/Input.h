#pragma once
#include <Windows.h>
#include <wrl.h>
#include <dinput.h> // DirectInputのヘッダー
#include <Xinput.h>
#include "WinApp.h"

// ライブラリのリンク指示
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

struct MouseState {
    LONG x, y;       // 前フレームからの移動量
    int wheel;       // ホイール回転量
    bool buttons[3]; // 0:左, 1:右, 2:中

    //4/20佐倉追加
    LONG posX, posY;
};

struct GamePadState {
    bool connected = false;
    WORD buttons = 0;
    WORD prevButtons = 0;
    float leftStickX = 0.0f;
    float leftStickY = 0.0f;
    float rightStickX = 0.0f;
    float rightStickY = 0.0f;
    float leftTrigger = 0.0f;
    float rightTrigger = 0.0f;
};

class Input {
public:
    // 名前空間省略
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public:
    // 初期化
    void Initialize(WinApp* winApp);
    // 更新
    void Update();

    // キーが押されているか判定
    // keyNumber: DIK_SPACE などのキーコード
    bool PushKey(BYTE keyNumber) const;

    // キーがトリガーされたか（押した瞬間）判定
    bool TriggerKey(BYTE keyNumber) const;

	// マウスの状態を取得する関数
    const MouseState& GetMouseState() const { return mouseState_; }
    const GamePadState& GetGamePadState() const { return gamePadState_; }
    bool IsGamePadConnected() const { return gamePadState_.connected; }
    bool PushControllerButton(WORD button) const;
    bool TriggerControllerButton(WORD button) const;

private:
    float NormalizeStickAxis(SHORT value, SHORT deadZone) const;
    float NormalizeTrigger(BYTE value) const;


    WinApp* winApp_ = nullptr;

    ComPtr<IDirectInput8> directInput_;
    ComPtr<IDirectInputDevice8> keyboard_;

    ComPtr<IDirectInputDevice8> mouse_; // マウス用デバイスを追加
	MouseState mouseState_ = {};        // マウスの状態を保持する構造体
    GamePadState gamePadState_ = {};

    // キーボードの入力状態（全キー256個）
    BYTE key_[256] = {};
    BYTE keyPre_[256] = {}; // 1フレーム前の状態（トリガー判定用）

};
