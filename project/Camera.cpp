#include "Camera.h"
#include "WinApp.h"
#include <cmath>

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