#include "Camera.h"
#include "WinApp.h"
#include <cmath>

Camera::Camera() {
    transform = { {1.0f, 1.0f, 1.0f}, {0.3f, 0.0f, 0.0f}, {0.0f, 5.0f, -10.0f} };
    fovY = 0.45f;
    aspectRatio = (float)WinApp::kClientWidth / (float)WinApp::kClientHeight;
    nearClip = 0.1f;
    farClip = 100.0f;
    Update();
}

void Camera::Update() {
    // 1. ビュー行列の計算
    // アフィン変換行列の逆行列をビュー行列とする
    Matrix4x4 cameraWorld = Math::MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
    viewMatrix_ = Math::Inverse(cameraWorld);

    // 2. プロジェクション行列の計算
    projectionMatrix_ = Math::MakePerspectiveFovMatrix(fovY, aspectRatio, nearClip, farClip);
}