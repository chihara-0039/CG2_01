#pragma once
#include "MyMath.h"

class Camera {
public:
    Camera();

    // 行列の更新
    void Update();

    // --- ゲッター ---
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
    const Vector3& GetPosition() const { return transform_.translate; }
    const Vector3& GetRotation() const { return transform_.rotate; }

    // --- セッター ---
    void SetPosition(const Vector3& pos) { transform_.translate = pos; }
    void SetRotation(const Vector3& rot) { transform_.rotate = rot; }
    void SetFov(float fov) { fov_ = fov; }

    // ImGui等で直接触れるようにTransformを公開、または参照を返す
    Transform& GetTransform() { return transform_; }
    float* GetFovPtr() { return &fov_; }

private:
    Transform transform_;
    float fov_;
    float aspectRatio_;
    float nearClip_;
    float farClip_;

    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
};