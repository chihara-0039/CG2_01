#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <vector>
#include <list>
#include "MyMath.h"
#include "DirectXCommon.h"
#include "TextureManager.h"

// パーティクル1粒のデータ構造
struct Particle {
    Transform transform; // 位置、回転、スケール
    Vector3 velocity;    // 速度
    Vector4 color;       // 色
    float lifeTime;      // 生存時間（現在）
    float maxTime;       // 寿命（最大）
};

// マネージャークラス
class ParticleManager {
public: // サブクラスなど
    // 定数：最大パーティクル数
    static const uint32_t kMaxParticles = 1024;

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

public: // メンバ関数
    // 初期化
    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager);

    // 更新
    void Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix);

    // 描画
    void Draw();

    // パーティクル発生（エミッター）
    // pos: 発生位置, count: 発生数
    void Emit(const Vector3& pos, uint32_t count);

private: // 内部処理
    void CreateRootSignature();
    void CreatePipelineState();
    void CreateMesh(); // 板ポリゴンの作成

private: // メンバ変数
    DirectXCommon* dxCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    // DirectXリソース
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // モデルデータ（板ポリ）
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    // インスタンシング用データ
    Microsoft::WRL::ComPtr<ID3D12Resource> instancingBuffer_;
    D3D12_VERTEX_BUFFER_VIEW instancingBufferView_{};
    InstanceData* instancingDataMapped_ = nullptr;

    // テクスチャハンドル
    uint32_t textureHandle_ = 0;

    // パーティクルリスト
    std::list<Particle> particles_;
};