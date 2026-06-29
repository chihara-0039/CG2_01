#pragma once
#include "Object3dCommon.h"
#include "Model.h"
#include "MyMath.h"

// Object3d.VS.hlsl の b0 に渡す行列セット。
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 lightViewProjection;
};

// Object3d.PS.hlsl のマテリアル定数。
struct Material {
    Vector4   color;
    int32_t   enableLighting;
    float     shininess;
    float     metallic;
    float     emissive;
    Matrix4x4 uvTransform;
    float     environmentCoefficient;
};

// 1つの3Dモデルのワールド変換・マテリアル・描画を担当するクラス。
// RootSignature や PSO は Object3dCommon が共有管理する。
class Object3d {
public:
    // 定数バッファを作成し、初期マテリアル値を書き込む。
    void Initialize(Object3dCommon* object3dCommon);

    // World / WVP / lightVP を更新して GPU 定数バッファへ書き込む。
    void Update(const Matrix4x4& lightVP);

    // 登録済み Model を通常描画する。
    void Draw();

    // 描画に使うモデルを設定する。所有権は持たない。
    void SetModel(Model* model) { model_ = model; }

    void SetPosition(const Vector3& position) { transform_.translate = position; }
    void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
    void SetScale(const Vector3& scale) { transform_.scale = scale; }

    // シャドウマップ用の深度描画を行う。
    void DrawShadow(const Matrix4x4& lightViewProjection);

    // Update() 前に現在のカメラ行列を渡す。
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_       = view;
        projectionMatrix_ = projection;
    }

    void SetColor(const Vector4& color) { if (materialData_) materialData_->color = color; }
    void SetEnableLighting(bool enable) { if (materialData_) materialData_->enableLighting = (enable ? 1 : 0); }
    void SetShininess(float shininess) { if (materialData_) materialData_->shininess = shininess; }
    void SetMetallic(float metallic) { if (materialData_) materialData_->metallic = metallic; }
    void SetEmissive(float emissive) { if (materialData_) materialData_->emissive = emissive; }
    void SetEnvironmentCoefficient(float coefficient) { if (materialData_) materialData_->environmentCoefficient = coefficient; }
    void SetUVTransform(const Transform& uvTransform);

    Model* GetModel() const { return model_; }
    const Transform& GetTransform() const { return transform_; }
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
    const Material& GetMaterial() const { return *materialData_; }
    const Vector3& GetPosition() const { return transform_.translate; }

    ID3D12Resource* GetTransformationResource() const { return transformationResource_.Get(); }
    ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
    Object3dCommon* GetObject3dCommon() const { return object3dCommon_; }

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Model*          model_          = nullptr;

    Transform transform_ = { {1,1,1}, {0,0,0}, {0,0,0} };

    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
    TransformationMatrix* transformationData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};
