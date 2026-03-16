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

    // 追加
    Transform& GetTransform() { return transform; }
    float* GetFovPtr() { return &fovY; }

    void SetPosition(const Vector3& pos) { transform.translate = pos; }
    void SetRotation(const Vector3& rot) { transform.rotate = rot; }
    void SetFov(float fov) { fovY = fov; }

public:
    Transform transform;
    float fovY;
    float aspectRatio;
    float nearClip;
    float farClip;

private:
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;
};