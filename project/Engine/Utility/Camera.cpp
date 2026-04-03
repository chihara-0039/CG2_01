#include "Camera.h"
#include "WinApp.h"
#include "Input.h"
#include <cmath>
#include <algorithm>

Camera::Camera() {
    transform_ = { {1.0f, 1.0f, 1.0f}, {0.3f, 0.0f, 0.0f}, {0.0f, 5.0f, -10.0f} };
    fov_ = 0.45f;
    aspectRatio_ = (float)WinApp::kClientWidth / (float)WinApp::kClientHeight;
    nearClip_ = 0.1f;
    farClip_ = 100.0f;
    Update();
}


void Camera::Update() {
    // 1. ビュー行列の計算
    // アフィン変換行列の逆行列をビュー行列とする
    Matrix4x4 cameraWorld = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    viewMatrix_ = Math::Inverse(cameraWorld);

    // 2. プロジェクション行列の計算
    projectionMatrix_ = Math::MakePerspectiveFovMatrix(fov_, aspectRatio_, nearClip_, farClip_);
}

void Camera::UpdateBlenderStyle(Input* input, bool isGuiCaptured, HWND hwnd) {
    // 1. 鉄壁のガード処理
    // ウィンドウにフォーカスがない、または ImGui を触っている時は何もしない
    if (GetActiveWindow() != hwnd || isGuiCaptured) {
        return;
    }

    // 2. マウスの状態を取得 (Inputクラスの MouseState 構造体)
    const MouseState& mouseState = input->GetMouseState();

    // 3. 右ボタン(buttons[1]) または 中ボタン(buttons[2]) が押されているか判定
    if (mouseState.buttons[1] || mouseState.buttons[2]) {

        // --- 4. 移動量（x, y）を使って回転を更新 ---
        // MouseState の x, y は DirectInput から取得した「前フレームからの移動量」です
        float deltaX = (float)mouseState.x;
        float deltaY = (float)mouseState.y;

        // 回転角に反映 (0.005f は操作感に合わせて調整してください)
        transform_.rotate.y += deltaX * 0.005f;
        transform_.rotate.x += deltaY * 0.005f;

        // 5. 行列を更新して画面に反映させる
        Update();
    }
}