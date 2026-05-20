#pragma once
#include "Model.h"
#include "MyMath.h"
#include <vector>
#include <string>
#include <memory>

// スキンウェイト情報 (頂点あたり最大4つの影響ボーン)
struct VertexInfluence {
    int jointIndices[4] = { -1, -1, -1, -1 };
    float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
};

// ボーン (ジョイント) 構造体
struct Joint {
    std::string name;
    Vector3 translation; // 親からの相対位置 (初期位置)
    Vector3 rotation;    // 回転角 (オイラー角: ラジアン)
    Vector3 scale;       // スケール

    Matrix4x4 localMatrix;
    Matrix4x4 globalMatrix;
    Matrix4x4 offsetMatrix; // Bind Poseのグローバル行列の逆行列

    int parentIndex = -1;
};

// キーフレームアニメーション用構造体
struct JointKeyframe {
    float time = 0.0f; // タイムスタンプ (秒)
    Vector3 translation = { 0.0f, 0.0f, 0.0f };
    Vector3 rotation = { 0.0f, 0.0f, 0.0f }; // オイラー角 (ラジアン)
    Vector3 scale = { 1.0f, 1.0f, 1.0f };
};

struct JointAnimation {
    std::string name;
    std::vector<JointKeyframe> keyframes;
};

struct MotionData {
    float duration = 2.0f; // アニメーションの総時間 (秒)
    std::vector<JointAnimation> jointAnimations;
};

// スキニング可能な人型モデルクラス
class SkinnedModel {
public:
    SkinnedModel() = default;
    ~SkinnedModel() = default;

    // 初期化 (人型モデルの生成とバッファ構築)
    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager);

    // アニメーション/ポーズの更新とスキニング計算
    void Update(DirectXCommon* dxCommon);

    // 描画
    void Draw(ID3D12GraphicsCommandList* commandList);

    // ボーンの取得・設定
    std::vector<Joint>& GetJoints() { return joints_; }
    const std::vector<Joint>& GetJoints() const { return joints_; }
    
    // 描画用のModelポインタを取得
    Model* GetModel() const { return model_.get(); }

    // ポーズをデフォルトに戻す
    void ResetPose();

    // 簡易的な走るアニメーションなどを適用する
    void ApplyTestAnimation(float time, float speed = 1.0f);

    // アニメーションデータ操作
    void AddKeyframe(float time);
    void ClearKeyframes();
    bool SaveMotion(const std::string& filePath);
    bool LoadMotion(const std::string& filePath);
    void ApplyMotion(float time);
    void GenerateWalkPreset();
    void GenerateRunPreset();

    // ゲッター・セッター
    float GetMotionDuration() const { return motionData_.duration; }
    void SetMotionDuration(float duration) { motionData_.duration = duration; }
    const MotionData& GetMotionData() const { return motionData_; }
    MotionData& GetMotionData() { return motionData_; }

private:
    // 人型メッシュとスケルトンの生成
    void CreateHumanoidSkeleton();
    void GenerateHumanoidMesh();

    // メッシュにウェイトを割り当てるヘルパー
    void AddCubeMesh(const Vector3& center, const Vector3& size, int jointIndex);
    // 関節部分のウェイトを滑らかにブレンドする処理
    void SmoothWeights();

private:
    std::vector<Joint> joints_;
    std::vector<ModelVertexData> bindPoseVertices_; // 初期姿勢の頂点データ
    std::vector<VertexInfluence> influences_;        // 頂点ウェイトデータ
    
    std::vector<ModelVertexData> animatedVertices_; // スキニング変形後の頂点データ

    std::unique_ptr<Model> model_;                  // 内部描画用モデル

    MotionData motionData_;                         // 現在編集・再生中のモーションデータ
};
