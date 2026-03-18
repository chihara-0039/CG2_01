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

void Camera::UpdateBlenderStyle(const Input* input) {
    // 前回の修正で Input.h をインクルードしたので、
    // ここで GetMouseState() や DIK_LSHIFT が使えるようになります。
    const auto& mouse = input->GetMouseState();
    const float rotateSpeed = 0.005f;
    const float panSpeed = 0.02f;

    // 1. 回転 (マウス中ボタンのみ)
    if (mouse.buttons[2] && !input->PushKey(DIK_LSHIFT)) {
        transform_.rotate.y += mouse.x * rotateSpeed;
        transform_.rotate.x += mouse.y * rotateSpeed;
    }

    // 2. パン/並行移動 (Shift + マウス中ボタン)
    if (mouse.buttons[2] && input->PushKey(DIK_LSHIFT)) {
        // カメラの現在の回転から向きを計算
        Matrix4x4 matRot = Math::Multiply(Math::MakeRotateXMatrix(transform_.rotate.x), Math::MakeRotateYMatrix(transform_.rotate.y));

        // カメラのローカル軸（X:右, Y:上）に沿って注視点を移動させる
        target_.x -= (matRot.m[0][0] * mouse.x - matRot.m[1][0] * mouse.y) * panSpeed;
        target_.y += (matRot.m[0][1] * mouse.x - matRot.m[1][1] * mouse.y) * panSpeed;
        target_.z -= (matRot.m[0][2] * mouse.x - matRot.m[1][2] * mouse.y) * panSpeed;
    }

    // 3. ズーム (マウスホイール)
    distance_ -= mouse.wheel * 0.01f;
    if (distance_ < 1.0f) distance_ = 1.0f; // 寄りすぎ防止

    // --- 最終的な座標計算 (注視点 + 回転 + 距離) ---
    Matrix4x4 matRot = Math::Multiply(Math::MakeRotateXMatrix(transform_.rotate.x), Math::MakeRotateYMatrix(transform_.rotate.y));
    Vector3 offset = { 0, 0, -distance_ };

    transform_.translate.x = target_.x + (offset.x * matRot.m[0][0] + offset.y * matRot.m[1][0] + offset.z * matRot.m[2][0]);
    transform_.translate.y = target_.y + (offset.x * matRot.m[0][1] + offset.y * matRot.m[1][1] + offset.z * matRot.m[2][1]);
    transform_.translate.z = target_.z + (offset.x * matRot.m[0][2] + offset.y * matRot.m[1][2] + offset.z * matRot.m[2][2]);

    Update(); // View行列の再計算
}