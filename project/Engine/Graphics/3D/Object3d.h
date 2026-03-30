#pragma once
#include "Object3dCommon.h"
#include "Model.h"
#include "MyMath.h"

struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
};

struct Material {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform;
};

class Object3d {
public:
    void Initialize(Object3dCommon* object3dCommon);
    void Update();
    void Draw();

    void SetModel(Model* model) { model_ = model; }
    void SetPosition(const Vector3& position) { transform_.translate = position; }
    void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    // カメラ設定
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_ = view;
        projectionMatrix_ = projection;
    }

    // マテリアル制御
    void SetColor(const Vector4& color) { if (materialData_) materialData_->color = color; }
    void SetEnableLighting(bool enable) { if (materialData_) materialData_->enableLighting = (enable ? 1 : 0); }
    void SetUVTransform(const Transform& uvTransform);

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Model* model_ = nullptr;

    Transform transform_ = { {1,1,1}, {0,0,0}, {0,0,0} };
    Matrix4x4 viewMatrix_;
    Matrix4x4 projectionMatrix_;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
    TransformationMatrix* transformationData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};