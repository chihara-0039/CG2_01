#pragma once
#include "Object3dCommon.h"
#include "Model.h"
#include "MyMath.h"


// 描画に必要な定数バッファ用の構造体
struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
	Matrix4x4 lightViewProjection;
};

// マテリアル情報を保持する構造体
struct Material {
    Vector4 color;
    int32_t enableLighting;
    float shininess;      // 光沢の強さと鋭さ (0.0:マット 〜 1.0:極めて滑らか・高光沢)
    float metallic;       // 金属感 (0.0:非金属 〜 1.0:金属的な色の反射)
    float emissive;       // 自己発光 (0.0:発光なし 〜 1.0以上:暗闇でも光る)
    Matrix4x4 uvTransform;
};

// 3Dオブジェクトクラス
class Object3d {
public:
    void Initialize(Object3dCommon* object3dCommon);
    void Update(const Matrix4x4& lightVP);
    void Draw();

	// モデルと変換の設定
    void SetModel(Model* model) { model_ = model; }
	// 変換のセッター
    void SetPosition(const Vector3& position) { transform_.translate = position; }
	// 回転はラジアンで指定することを想定
    void SetRotation(const Vector3& rotation) { transform_.rotate = rotation; }
	// 拡大縮小は1.0fが等倍で、0.5fなら半分、2.0fなら2倍になるイメージ
    void SetScale(const Vector3& scale) { transform_.scale = scale; }
	// 影描画用の関数を追加
    void DrawShadow(const Matrix4x4& lightViewProjection);

    // カメラ設定
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_ = view;
        projectionMatrix_ = projection;
    }

    // マテリアル制御
    void SetColor(const Vector4& color) { if (materialData_) materialData_->color = color; }
    void SetEnableLighting(bool enable) { if (materialData_) materialData_->enableLighting = (enable ? 1 : 0); }
    void SetShininess(float shininess) { if (materialData_) materialData_->shininess = shininess; }
    void SetMetallic(float metallic) { if (materialData_) materialData_->metallic = metallic; }
    void SetEmissive(float emissive) { if (materialData_) materialData_->emissive = emissive; }
    void SetUVTransform(const Transform& uvTransform);

    // ゲッター
    Model* GetModel() const { return model_; }
    const Transform& GetTransform() const { return transform_; }
    const Matrix4x4& GetViewMatrix() const { return viewMatrix_; }
    const Matrix4x4& GetProjectionMatrix() const { return projectionMatrix_; }
    const Material& GetMaterial() const { return *materialData_; }


private:
	// Object3dCommonのポインタを保持しておく（描画時に必要）
    Object3dCommon* object3dCommon_ = nullptr;
	// モデルのポインタを保持しておく（描画時に必要）
    Model* model_ = nullptr;

	// 位置、回転、拡大縮小をまとめた構造体を用意
    Transform transform_ = { {1,1,1}, {0,0,0}, {0,0,0} };
	// ビュー行列とプロジェクション行列も保持しておく（描画時に必要）
    Matrix4x4 viewMatrix_{};
	// プロジェクション行列も保持しておく（描画時に必要）
    Matrix4x4 projectionMatrix_{};

	// 変換行列用の定数バッファとマテリアル用の定数バッファを用意
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
	// 定数バッファはCPUからGPUにデータを転送するため、マップしてアクセスできるようにしておく
    TransformationMatrix* transformationData_ = nullptr;

	// マテリアル用の定数バッファとデータポインタも用意
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    // マテリアルデータのポインタも用意（CPUからアクセスするため）
    Material* materialData_ = nullptr;
};