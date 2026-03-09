#include "Camera.h"
#include "WinApp.h"

Camera::Camera() {
    transform_ = { {1.0f, 1.0f, 1.0f}, {0.3f, 0.0f, 0.0f}, {0.0f, 5.0f, -10.0f} };
    fov_ = 0.45f;
    aspectRatio_ = (float)WinApp::kClientWidth / (float)WinApp::kClientHeight;
    nearClip_ = 0.1f;
    farClip_ = 100.0f;
    Update();
}

void Camera::Update() {
    // アフィン行列と逆行列でビュー行列を作成
    Matrix4x4 cameraWorld = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    viewMatrix_ = Math::Inverse(cameraWorld);

    // 透視投影行列の作成
    projectionMatrix_ = Math::MakePerspectiveFovMatrix(fov_, aspectRatio_, nearClip_, farClip_);
}