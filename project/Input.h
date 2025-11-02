#pragma once
#include <windows.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <wrl.h>

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")


class Input {
public:
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

    void Initialize(HINSTANCE hInstance, HWND hwnd);
    void Update();

/// <summary>
/// キーの押ししたをチェック
/// </summary>
/// <param name="keyNumber">キー番号(DIK_0 等)</param>
/// <returns>押されているか</returns>
    bool PushKey(BYTE keyNumber);


    /// <summary>
    /// キーのトリガーをチェック
    /// </summary>
    /// <param name="keyNumber">キー番号(DIK_0 等)</param>
    /// <returns>トリガーか</returns>
    bool TriggerKey(BYTE keyNumber);

private:
    // ずっと生存させる（←NEW）
    ComPtr<IDirectInput8>       directInput_;
    ComPtr<IDirectInputDevice8> keyboard_;

    // 再作成用に覚えておく（←NEW）
    HINSTANCE hInstance_ = nullptr;
    HWND      hwnd_ = nullptr;

    // なくなってたら安全に作り直す（←NEW）
    bool EnsureDevice_();

    //全キーの状態
    BYTE key[256] = {};

    //前回の全キーの状態
    BYTE keyPre[256] = {};
};