#pragma once
#include <wrl.h>
#include <d3d12.h>
#include "MyMath.h"
#include "SpriteCommon.h"

class Sprite {
public:
    // 頂点データ構造体
    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
    };

    // マテリアルデータ（色）
    struct Material {
        Vector4 color;
    };

    // 座標変換データ
    struct TransformationMatrix {
        Matrix4x4 WVP;
    };

public:
    // 初期化（テクスチャインデックス指定）
    void Initialize(SpriteCommon* spriteCommon, uint32_t textureHandle);

    // 更新処理
    void Update();

    // 描画
    void Draw();

    // --- セッター群 ---
    void SetPosition(const Vector2& position) { position_ = position; transferNeeded_ = true; }
    void SetRotation(float rotation) { rotation_ = rotation; transferNeeded_ = true; }
    void SetSize(const Vector2& size) { size_ = size; transferNeeded_ = true; }
    void SetColor(const Vector4& color) { materialData_->color = color; }

    // アンカーポイント (0.0~1.0) 例: 中心なら{0.5, 0.5}
    void SetAnchorPoint(const Vector2& anchor) { anchorPoint_ = anchor; transferNeeded_ = true; }

    // テクスチャ切り抜き (画像上のピクセル座標とサイズ)
    void SetTextureRect(const Vector2& position, const Vector2& size);
    // テクスチャ変更
    void SetTexture(uint32_t textureHandle);

    // 既に Initialize 等があると思いますが、その下に以下を追加
    const Vector2& GetPosition() const { return position_; }
    float GetRotation() const { return rotation_; }
    const Vector2& GetSize() const { return size_; }

private:
    // 頂点バッファの作成
    void CreateVertexBuffer();
    // マテリアルバッファの作成
    void CreateMaterialBuffer();
    // トランスフォームバッファの作成
    void CreateTransformationMatrixBuffer();

    // 頂点データの更新（サイズや切り抜き変更時）
    void UpdateVertexData();

private:
    SpriteCommon* spriteCommon_ = nullptr;
    uint32_t textureHandle_ = 0;

    // トランスフォーム情報
    Vector2 position_ = { 0.0f, 0.0f };
    float rotation_ = 0.0f;
    Vector2 size_ = { 100.0f, 100.0f };
    Vector2 anchorPoint_ = { 0.0f, 0.0f };

    // テクスチャ切り抜き情報
    Vector2 textureLeftTop_ = { 0.0f, 0.0f };
    Vector2 textureSize_ = { 100.0f, 100.0f };

    // フラグ
    bool transferNeeded_ = true; // 頂点データの再転送が必要か

    // DirectXリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexBuffer_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;
};