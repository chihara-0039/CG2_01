#include "Input.h"
#include <cassert>

void Input::Initialize(WinApp* winApp) {
    winApp_ = winApp;
    HRESULT hr;

    // DirectInputオブジェクトの生成
    hr = DirectInput8Create(
        winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8,
        (void**)&directInput_, nullptr);
    assert(SUCCEEDED(hr));

    // キーボードデバイスの生成
    hr = directInput_->CreateDevice(GUID_SysKeyboard, &keyboard_, nullptr);
    assert(SUCCEEDED(hr));

    // 入力データ形式のセット
    hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);
    assert(SUCCEEDED(hr));

    // 排他制御レベルのセット
    hr = keyboard_->SetCooperativeLevel(
        winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
    assert(SUCCEEDED(hr));
}

void Input::Update() {
    HRESULT hr;

    // 前回のキー入力を保存
    memcpy(keyPre_, key_, sizeof(key_));

    // キーボード情報の取得開始
    keyboard_->Acquire();

    // 全キーの入力状態を取得する
    hr = keyboard_->GetDeviceState(sizeof(key_), key_);
}

bool Input::PushKey(BYTE keyNumber) {
    // 0以外なら押されている
    return key_[keyNumber];
}

bool Input::TriggerKey(BYTE keyNumber) {
    // 今押されていて、前は押されていなかったらトリガー
    return key_[keyNumber] && !keyPre_[keyNumber];
}