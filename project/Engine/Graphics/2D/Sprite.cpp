#include "Sprite.h"
#include "MyMath.h" // 必ずインクルード
#include "WinApp.h"
#include <cassert>

// 2Dスプライトを準備し、初期値と必要なリソースを設定する。
void Sprite::Initialize(SpriteCommon *spriteCommon, uint32_t textureHandle) {
    assert(spriteCommon);
    spriteCommon_ = spriteCommon;
    textureHandle_ = textureHandle;
    transformInitialized_ = false;
    transferNeeded_ = true;

    // テクスチャ情報から初期サイズを設定
    auto &desc = spriteCommon_->GetTextureManager()->GetResourceDesc(textureHandle_);
    size_ = {(float)desc.Width, (float)desc.Height};
    textureSize_ = size_; // 初期状態は全範囲
    textureLeftTop_ = {0.0f, 0.0f};

    CreateBuffers();

    // 初期データを転送
    UpdateVertexData();
}

// 位置・大きさ・UV・色から、2D描画に使う頂点と変換定数を更新する。
void Sprite::Update() {
    // 頂点情報に変更があれば更新
    if (transferNeeded_) {
        UpdateVertexData();
        transferNeeded_ = false;
    }

    if (transformInitialized_ && position_.x == lastPosition_.x && position_.y == lastPosition_.y &&
        rotation_ == lastRotation_) {
        return;
    }
    lastPosition_ = position_;
    lastRotation_ = rotation_;
    transformInitialized_ = true;

    // 行列計算 (MyMathの関数を使用)
    // 名前空間 Math:: をつけ、関数名を合わせる
    Matrix4x4 scaleMat = Math::Matrix4x4MakeScaleMatrix({1.0f, 1.0f, 1.0f});
    Matrix4x4 rotateMat = Math::MakeRotateZMatrix(rotation_);
    Matrix4x4 translateMat = Math::MakeTranslateMatrix({position_.x, position_.y, 0.0f});

    // 行列の掛け算
    Matrix4x4 worldMatrix = Math::Multiply(scaleMat, Math::Multiply(rotateMat, translateMat));

    // ビュープロジェクション行列（正射影）
    // 画面サイズはWinAppのクライアント領域定数を参照する。
    Matrix4x4 viewMatrix = Math::MakeIdentity4x4();
    Matrix4x4 projectionMatrix = Math::MakeOrthographicMatrix(0.0f, 0.0f, static_cast<float>(WinApp::kClientWidth),
                                                              static_cast<float>(WinApp::kClientHeight), 0.0f, 100.0f);

    // ワールド・ビュー・プロジェクション行列の計算
    Matrix4x4 wvpMatrix = Math::Multiply(worldMatrix, Math::Multiply(viewMatrix, projectionMatrix));

    // シェーダーに転送
    transformationMatrixData_->WVP = wvpMatrix;
}

// 2Dスプライトの表示対象を選び、描画コマンドを記録する。
void Sprite::Draw() {
    // コマンドリスト取得
    // DirectXCommonに GetCommandList() を追加している前提
    auto commandList = spriteCommon_->GetDxCommon()->GetCommandList();

    // 1. VBVセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // 2. マテリアルCBV
    commandList->SetGraphicsRootConstantBufferView(0, materialAddress_);

    // 3. トランスフォームCBV
    commandList->SetGraphicsRootConstantBufferView(1, transformationAddress_);

    // 4. テクスチャSRV
    auto gpuHandle = spriteCommon_->GetTextureManager()->GetSrvHandleGPU(textureHandle_);
    commandList->SetGraphicsRootDescriptorTable(2, gpuHandle);

    // 5. 描画 (6頂点)
    commandList->DrawInstanced(6, 1, 0, 0);
}

// テクスチャ切り抜き設定
void Sprite::SetTextureRect(const Vector2 &position, const Vector2 &size) {
    if (textureLeftTop_.x == position.x && textureLeftTop_.y == position.y && textureSize_.x == size.x &&
        textureSize_.y == size.y) {
        return;
    }
    textureLeftTop_ = position;
    textureSize_ = size;
    // size_ = size; // 切り抜きサイズに合わせて表示サイズも変えたい場合はコメントアウトを外す
    transferNeeded_ = true;
}

// テクスチャ変更
void Sprite::SetTexture(uint32_t textureHandle) {
    textureHandle_ = textureHandle;
    auto &desc = spriteCommon_->GetTextureManager()->GetResourceDesc(textureHandle_);
    textureSize_ = {(float)desc.Width, (float)desc.Height};
    transferNeeded_ = true;
}

// 頂点・色・変換の保存領域を一括取得し、各定数を256バイト境界へそろえる。
void Sprite::CreateBuffers() {
    constexpr size_t alignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    static_assert(sizeof(VertexData) * 6 <= alignment);
    static_assert(sizeof(Material) <= alignment);
    static_assert(sizeof(TransformationMatrix) <= alignment);
    const auto storage = spriteCommon_->AllocateSpriteStorage();
    storageResource_ = storage.resource;
    vertexData_ = reinterpret_cast<VertexData *>(storage.cpu);
    vertexBufferView_.BufferLocation = storage.gpu;
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
    materialData_ = reinterpret_cast<Material *>(storage.cpu + alignment);
    materialAddress_ = storage.gpu + alignment;
    materialData_->color = {1, 1, 1, 1};
    transformationMatrixData_ = reinterpret_cast<TransformationMatrix *>(storage.cpu + alignment * 2);
    transformationAddress_ = storage.gpu + alignment * 2;
}

// 常時マップされた専用領域に、UVと頂点位置を書き込む。
void Sprite::UpdateVertexData() {
    if (!storageResource_ || !vertexData_ || !spriteCommon_) {
        return;
    }
    VertexData *vertMap = vertexData_;

    // テクスチャ全体のサイズ取得
    auto &texDesc = spriteCommon_->GetTextureManager()->GetResourceDesc(textureHandle_);
    float texWidth = (float)texDesc.Width;
    float texHeight = (float)texDesc.Height;

    // UV計算
    float left = textureLeftTop_.x / texWidth;
    float right = (textureLeftTop_.x + textureSize_.x) / texWidth;
    float top = textureLeftTop_.y / texHeight;
    float bottom = (textureLeftTop_.y + textureSize_.y) / texHeight;

    // 頂点座標計算（アンカーポイント考慮）
    float leftPos = 0.0f - (anchorPoint_.x * size_.x);
    float rightPos = size_.x - (anchorPoint_.x * size_.x);
    float topPos = 0.0f - (anchorPoint_.y * size_.y);
    float bottomPos = size_.y - (anchorPoint_.y * size_.y);

    // 6頂点
    // Triangle 1
    vertMap[0].position = {leftPos, topPos, 0.0f, 1.0f};
    vertMap[0].texcoord = {left, top};
    vertMap[1].position = {leftPos, bottomPos, 0.0f, 1.0f};
    vertMap[1].texcoord = {left, bottom};
    vertMap[2].position = {rightPos, topPos, 0.0f, 1.0f};
    vertMap[2].texcoord = {right, top};

    // Triangle 2
    vertMap[3].position = {leftPos, bottomPos, 0.0f, 1.0f};
    vertMap[3].texcoord = {left, bottom};
    vertMap[4].position = {rightPos, bottomPos, 0.0f, 1.0f};
    vertMap[4].texcoord = {right, bottom};
    vertMap[5].position = {rightPos, topPos, 0.0f, 1.0f};
    vertMap[5].texcoord = {right, top};

    // 保存ページは寿命中マップしたまま使用する。
}
