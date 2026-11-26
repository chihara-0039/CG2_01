#include "Camera.h"
#include "WinApp.h"
#include "Input.h"
#include <cmath>
#include <algorithm>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

// カメラの初期姿勢と投影設定を用意する。
Camera::Camera() {
    transform_ = {{1.0f, 1.0f, 1.0f}, {0.3f, 0.0f, 0.0f}, {0.0f, 5.0f, -10.0f}};
    fov_ = 0.45f;
    aspectRatio_ = (float)WinApp::kClientWidth / (float)WinApp::kClientHeight;
    nearClip_ = 0.1f;
    farClip_ = 1000.0f;
    Update();
}

// 現在の姿勢と投影設定から、ビュー行列と投影行列を再計算する。
void Camera::Update() {
    matrixCache_.Update(transform_, fov_, aspectRatio_, nearClip_, farClip_);
}

// マウス操作から、Blender風の周回・移動・ズームを反映する。
void Camera::UpdateBlenderStyle(const Input *input, bool isGuiCaptured, HWND hwnd, bool invertOrbit) {

    const auto &mouse = input->GetMouseState();
    const auto &pad = input->GetGamePadState();
    CameraControlInput frame;
    frame.enabled = GetActiveWindow() == hwnd && !isGuiCaptured;
    frame.middle = mouse.buttons[2];
    frame.shift = input->PushKey(DIK_LSHIFT);
    frame.invert = invertOrbit;
    frame.mouseX = static_cast<float>(mouse.x);
    frame.mouseY = static_cast<float>(mouse.y);
    frame.wheel = static_cast<float>(mouse.wheel);
    frame.pad = pad.connected;
    frame.rightX = pad.rightStickX;
    frame.rightY = pad.rightStickY;
    frame.leftX = pad.leftStickX;
    frame.leftY = pad.leftStickY;
    frame.zoomIn = (pad.buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
    frame.zoomOut = (pad.buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
    CameraControlPose pose{transform_.rotate,target_,distance_};
    controlFlow_.Update(pose,frame);
    if (!frame.enabled) { return; }
    transform_.rotate = pose.rotation;
    target_ = pose.target;
    distance_ = pose.distance;

    // 最終的な座標計算
    Matrix4x4 matRot =
        Math::Multiply(Math::MakeRotateXMatrix(transform_.rotate.x), Math::MakeRotateYMatrix(transform_.rotate.y));
    Vector3 offset = {0, 0, -distance_};
    transform_.translate.x =
        target_.x + (offset.x * matRot.m[0][0] + offset.y * matRot.m[1][0] + offset.z * matRot.m[2][0]);
    transform_.translate.y =
        target_.y + (offset.x * matRot.m[0][1] + offset.y * matRot.m[1][1] + offset.z * matRot.m[2][1]);
    transform_.translate.z =
        target_.z + (offset.x * matRot.m[0][2] + offset.y * matRot.m[1][2] + offset.z * matRot.m[2][2]);

    Update();
}

// 注視点・距離・角度を同時に指定し、カメラ位置を即座に再計算する。
void Camera::ForceReset(const Vector3 &target, float distance, const Vector3 &rotation) {
    target_ = target;
    distance_ = distance;
    transform_.rotate = rotation;

    // 回転行列とオフセットを用いてカメラ位置を強制再計算
    Matrix4x4 matRot =
        Math::Multiply(Math::MakeRotateXMatrix(transform_.rotate.x), Math::MakeRotateYMatrix(transform_.rotate.y));
    Vector3 offset = {0.0f, 0.0f, -distance_};
    transform_.translate.x =
        target_.x + (offset.x * matRot.m[0][0] + offset.y * matRot.m[1][0] + offset.z * matRot.m[2][0]);
    transform_.translate.y =
        target_.y + (offset.x * matRot.m[0][1] + offset.y * matRot.m[1][1] + offset.z * matRot.m[2][1]);
    transform_.translate.z =
        target_.z + (offset.x * matRot.m[0][2] + offset.y * matRot.m[1][2] + offset.z * matRot.m[2][2]);

    Update();
}

// 表示用カメラの設定と状態をGUIへ表示し、編集入力を反映する。
void Camera::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::DragFloat3("Position", &transform_.translate.x, 0.1f);
    ImGui::DragFloat3("Rotation", &transform_.rotate.x, 0.01f);
    ImGui::SliderFloat("FOV", &fov_, 0.01f, 3.14f);

    if (ImGui::Button("Reset Camera")) {
        SetPosition({6.0f, 8.0f, -12.0f});
        SetRotation({0.6f, 0.0f, 0.0f});
        SetFov(0.45f);
        target_ = {8.0f, 0.0f, 8.0f};
        distance_ = 20.0f;
    }
#endif
}
