#pragma once
#include "MyMath.h"

class Camera {
public:
    Camera();

    // 更新処理（行列の再計算）
    void Update();

    // ゲッター
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }

public:
    // ImGuiから直接触れるように public にしておくと便利です
    Transform transform;
    float fovY;
    float aspectRatio;
    float nearClip;
    float farClip;

private:
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;


};