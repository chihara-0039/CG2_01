#pragma once
#include "SkinnedModel.h"
#include "Object3d.h"
#include "Object3dCommon.h"
#include <memory>
#include <vector>

// スキニングモデルの配置と制御を司るオブジェクトクラス
class SkinnedObject {
public:
    SkinnedObject() = default;
    ~SkinnedObject() = default;

    // 初期化
    void Initialize(Object3dCommon* object3dCommon, DirectXCommon* dxCommon, TextureManager* textureManager);

    // 更新 (行列計算、アニメーション、スキニング適用)
    void Update(DirectXCommon* dxCommon, const Matrix4x4& lightVP);

    // 通常描画
    void Draw();

    // 影マップ描画
    void DrawShadow(const Matrix4x4& lightViewProjection);

    // スケルトンの可視化描画
    void DrawSkeleton(Object3dCommon* object3dCommon, Model* cubeModel, const Matrix4x4& view, const Matrix4x4& projection);

    // セッター・ゲッター
    SkinnedModel* GetModel() const { return skinnedModel_.get(); }
    Object3d* GetObject3d() const { return object3d_.get(); }

    void SetPosition(const Vector3& pos) { position_ = pos; }
    const Vector3& GetPosition() const { return position_; }

    void SetRotation(const Vector3& rot) { rotation_ = rot; }
    const Vector3& GetRotation() const { return rotation_; }

    void SetScale(const Vector3& scale) { scale_ = scale; }
    const Vector3& GetScale() const { return scale_; }

    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_ = view;
        projectionMatrix_ = projection;
    }

    void SetPlayAnimation(bool play) { playAnimation_ = play; }
    bool IsPlayAnimation() const { return playAnimation_; }

    void SetAnimationSpeed(float speed) { animationSpeed_ = speed; }
    float GetAnimationSpeed() const { return animationSpeed_; }

    void SetShowSkeleton(bool show) { showSkeleton_ = show; }
    bool IsShowSkeleton() const { return showSkeleton_; }

    // 選択中のジョイントインデックス
    void SetSelectedJointIndex(int index) { selectedJointIndex_ = index; }
    int GetSelectedJointIndex() const { return selectedJointIndex_; }

    // カスタムモーション関連
    void AddKeyframe(float time) { skinnedModel_->AddKeyframe(time); }
    void ClearKeyframes() { skinnedModel_->ClearKeyframes(); }
    bool SaveMotion(const std::string& filePath) { return skinnedModel_->SaveMotion(filePath); }
    bool LoadMotion(const std::string& filePath) { return skinnedModel_->LoadMotion(filePath); }
    void ApplyMotion(float time) { skinnedModel_->ApplyMotion(time); }
    void GenerateWalkPreset() { skinnedModel_->GenerateWalkPreset(); }

    void SetPlayCustomAnimation(bool play) { playCustomAnimation_ = play; }
    bool IsPlayCustomAnimation() const { return playCustomAnimation_; }

    float GetCurrentKeyframeTime() const { return currentKeyframeTime_; }
    void SetCurrentKeyframeTime(float time) { currentKeyframeTime_ = time; }

private:
    std::unique_ptr<SkinnedModel> skinnedModel_;
    std::unique_ptr<Object3d> object3d_; // スキンメッシュを描画するためのObject3d本体

    // トランスフォーム
    Vector3 position_ = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };
    Vector3 scale_ = { 1.0f, 1.0f, 1.0f };

    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    // アニメーション制御
    bool playAnimation_ = false;
    float animationTime_ = 0.0f;
    float animationSpeed_ = 1.0f;

    // カスタムモーション
    bool playCustomAnimation_ = false;
    float currentKeyframeTime_ = 0.0f;

    // デバッグ表示
    bool showSkeleton_ = true;
    int selectedJointIndex_ = -1;

    // スケルトン描画用のジョイントビジュアル
    std::vector<std::unique_ptr<Object3d>> jointVisuals_;
    std::vector<std::unique_ptr<Object3d>> boneVisuals_; // 骨をつなぐ棒
};
