#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <list>
#include <string>
#include "MyMath.h"
#include "DirectXCommon.h"
#include "TextureManager.h"

class StageMap;

// パーティクル1つのデータ構造
struct Particle {
    enum class Type {
        Fall,
        Splash,
        Ring,
        Cylinder
    };
    Type type = Type::Fall;
    Transform transform; // 位置、回転、スケール
    Vector3 velocity;    // 速度
    Vector4 color;       // 色
    float initialAlpha = 1.0f;
    float lifeTime;      // 時間(現在)
    float maxTime;       // (最大)
};

// マネージャークラス
class ParticleManager {
public: // サブクラスなど
    // 定数：最大パーティクル数
    static const uint32_t kMaxParticles = 4096; // 天候のために増やす

    // インスタンシング用データ構造（シェーダーに送る）
    struct InstanceData {
        Matrix4x4 WVP;
        Vector4 color;
    };

    // 頂点データ構造（板ポリゴン用）
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct WeatherEmitter {
        bool active = false;
        Vector3 center = {0,0,0};
        Vector3 size = {40, 20, 40};       // 発生範囲
        float emitRate = 100.0f;           // 1秒あたりの発生数
        float emitTimer = 0.0f;
        Vector3 velocity = {0, -5.0f, 0};  // 基本落下速度
        Vector3 velocityRandom = {1, 0.5f, 1}; // 速度のばらつき
        Vector3 particleSize = {0.2f, 0.2f, 0.2f};
        float particleLife = 4.0f;
        Vector4 color = {1,1,1,1};
    };

public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager);

    // 更新
    void Update(float deltaTime, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Vector3& playerPos, StageMap* stageMap = nullptr);

    // 描画
    void Draw();

    // パーティクル発生（エミッター）
    // pos: 発生位置, count: 発生数
    void Emit(const Vector3& pos, uint32_t count);

    // 飛沫を生成する（ブロック衝突時など）
    void EmitSplash(const Vector3& pos, const Vector4& color);

    // テクスチャ設定
    void SetTexture(uint32_t textureHandle) { textureHandle_ = textureHandle; }

    // 天候エミッターの取得・設定
    WeatherEmitter& GetWeatherEmitter() { return weatherEmitter_; }

    void ClearParticles() {
        particles_.clear();
        planeInstanceCount_ = 0;
        ringInstanceCount_ = 0;
        cylinderInstanceCount_ = 0;
    }

private: // 内部処理
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateMesh(); // 板ポリゴンの作成
    void CreateRingMesh();
    void CreateCylinderMesh();

private: // メンバ変数
    DirectXCommon* dxCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    // DirectXリソース
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> primitivePipelineState_;

    // モデルデータ（板ポリ）
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    uint32_t planeVertexCount_ = 0;

    // モデルデータ（Ring）
    Microsoft::WRL::ComPtr<ID3D12Resource> ringVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW ringVertexBufferView_{};
    uint32_t ringVertexCount_ = 0;

    // モデルデータ（Cylinder）
    Microsoft::WRL::ComPtr<ID3D12Resource> cylinderVertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW cylinderVertexBufferView_{};
    uint32_t cylinderVertexCount_ = 0;

    // インスタンシング用データ
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingBuffer_;
    D3D12_VERTEX_BUFFER_VIEW instancingBufferView_{};
    InstanceData* instancingDataMapped_ = nullptr;
    uint32_t planeInstanceCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> ringInstancingBuffer_;
    D3D12_VERTEX_BUFFER_VIEW ringInstancingBufferView_{};
    InstanceData* ringInstancingDataMapped_ = nullptr;
    uint32_t ringInstanceCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> cylinderInstancingBuffer_;
    D3D12_VERTEX_BUFFER_VIEW cylinderInstancingBufferView_{};
    InstanceData* cylinderInstancingDataMapped_ = nullptr;
    uint32_t cylinderInstanceCount_ = 0;

    // テクスチャハンドル
    uint32_t textureHandle_ = 0;

    // パーティクルリスト
    std::list<Particle> particles_;

    // 天候用エミッター
    WeatherEmitter weatherEmitter_;
};

