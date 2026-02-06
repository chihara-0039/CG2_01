#pragma once
#include <Windows.h>
#include <wrl.h>
#include <dinput.h> // DirectInputのヘッダー
#include "WinApp.h"

// ライブラリのリンク指示
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

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
    bool PushKey(BYTE keyNumber);

    // キーがトリガーされたか（押した瞬間）判定
    bool TriggerKey(BYTE keyNumber);

private:
    WinApp* winApp_ = nullptr;

    ComPtr<IDirectInput8> directInput_;
    ComPtr<IDirectInputDevice8> keyboard_;

    // キーボードの入力状態（全キー256個）
    BYTE key_[256] = {};
    BYTE keyPre_[256] = {}; // 1フレーム前の状態（トリガー判定用）
};