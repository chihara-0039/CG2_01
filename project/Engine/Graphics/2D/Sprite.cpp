#include "Sprite.h"
#include "MyMath.h" // 必ずインクルード
#include <cassert>

void Sprite::Initialize(SpriteCommon* spriteCommon, uint32_t textureHandle) {
    assert(spriteCommon);
    spriteCommon_ = spriteCommon;
    textureHandle_ = textureHandle;

    // テクスチャ情報から初期サイズを設定
    auto& desc = spriteCommon_->GetTextureManager()->GetResourceDesc(textureHandle_);
    size_ = { (float)desc.Width, (float)desc.Height };
    textureSize_ = size_; // 初期状態は全範囲
    textureLeftTop_ = { 0.0f, 0.0f };

    CreateVertexBuffer();
    CreateMaterialBuffer();
    CreateTransformationMatrixBuffer();

    // 初期データを転送
    UpdateVertexData();
}

void Sprite::Update() {
    // 頂点情報に変更があれば更新
    if (transferNeeded_) {
        UpdateVertexData();
        transferNeeded_ = false;
    }

    // 行列計算 (MyMathの関数を使用)
    // 名前空間 Math:: をつけ、関数名を合わせる
    Matrix4x4 scaleMat = Math::Matrix4x4MakeScaleMatrix({ 1.0f, 1.0f, 1.0f });
    Matrix4x4 rotateMat = Math::MakeRotateZMatrix(rotation_);
    Matrix4x4 translateMat = Math::MakeTranslateMatrix({ position_.x, position_.y, 0.0f });

    // 行列の掛け算
    Matrix4x4 worldMatrix = Math::Multiply(scaleMat, Math::Multiply(rotateMat, translateMat));

    // ビュープロジェクション行列（正射影）
    // 画面サイズ 1280x720 を想定
    Matrix4x4 viewMatrix = Math::MakeIdentity4x4();
    Matrix4x4 projectionMatrix = Math::MakeOrthographicMatrix(0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 100.0f);

    Matrix4x4 wvpMatrix = Math::Multiply(worldMatrix, Math::Multiply(viewMatrix, projectionMatrix));

    transformationMatrixData_->WVP = wvpMatrix;
}

void Sprite::Draw() {
    // コマンドリスト取得
    // DirectXCommonに GetCommandList() を追加している前提
    auto commandList = spriteCommon_->GetDxCommon()->GetCommandList();

    // 1. VBVセット
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // 2. マテリアルCBV
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

    // 3. トランスフォームCBV
    commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());

    // 4. テクスチャSRV
    auto gpuHandle = spriteCommon_->GetTextureManager()->GetSrvHandleGPU(textureHandle_);
    commandList->SetGraphicsRootDescriptorTable(2, gpuHandle);

    // 5. 描画 (6頂点)
    commandList->DrawInstanced(6, 1, 0, 0);
}

void Sprite::SetTextureRect(const Vector2& position, const Vector2& size) {
    textureLeftTop_ = position;
    textureSize_ = size;
    // size_ = size; // 切り抜きサイズに合わせて表示サイズも変えたい場合はコメントアウトを外す
    transferNeeded_ = true;
}

void Sprite::SetTexture(uint32_t textureHandle) {
    textureHandle_ = textureHandle;
    auto& desc = spriteCommon_->GetTextureManager()->GetResourceDesc(textureHandle_);
    textureSize_ = { (float)desc.Width, (float)desc.Height };
    transferNeeded_ = true;
}

void Sprite::CreateVertexBuffer() {
    auto device = spriteCommon_->GetDxCommon()->GetDevice();

    // 頂点リソース作成
    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeof(VertexData) * 6;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}

void Sprite::CreateMaterialBuffer() {
    auto device = spriteCommon_->GetDxCommon()->GetDevice();
    size_t sizeIB = (sizeof(Material) + 0xff) & ~0xff;
    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeIB;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&materialResource_));
    materialResource_->Map(0, nullptr, (void**)&materialData_);
    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
}

void Sprite::CreateTransformationMatrixBuffer() {
    auto device = spriteCommon_->GetDxCommon()->GetDevice();
    size_t sizeIB = (sizeof(TransformationMatrix) + 0xff) & ~0xff;
    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = sizeIB;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&transformationMatrixResource_));
    transformationMatrixResource_->Map(0, nullptr, (void**)&transformationMatrixData_);
    transformationMatrixData_->WVP = Math::MakeIdentity4x4();
}

void Sprite::UpdateVertexData() {
    VertexData* vertMap = nullptr;
    vertexBuffer_->Map(0, nullptr, (void**)&vertMap);

    // テクスチャ全体のサイズ取得
    auto& texDesc = spriteCommon_->GetTextureManager()->GetResourceDesc(textureHandle_);
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
    vertMap[0].position = { leftPos, topPos, 0.0f, 1.0f };
    vertMap[0].texcoord = { left, top };
    vertMap[1].position = { leftPos, bottomPos, 0.0f, 1.0f };
    vertMap[1].texcoord = { left, bottom };
    vertMap[2].position = { rightPos, topPos, 0.0f, 1.0f };
    vertMap[2].texcoord = { right, top };

    // Triangle 2
    vertMap[3].position = { leftPos, bottomPos, 0.0f, 1.0f };
    vertMap[3].texcoord = { left, bottom };
    vertMap[4].position = { rightPos, bottomPos, 0.0f, 1.0f };
    vertMap[4].texcoord = { right, bottom };
    vertMap[5].position = { rightPos, topPos, 0.0f, 1.0f };
    vertMap[5].texcoord = { right, top };

    vertexBuffer_->Unmap(0, nullptr);
}