#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h" // 追加
#include <wrl.h>
#include <d3d12.h>
#include <memory>

class SpriteCommon {
public:
    void Initialize(DirectXCommon* dxCommon);

    // 共通描画設定（ルートシグネチャ設定など）
    void PreDraw();

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    TextureManager* GetTextureManager() { return textureManager_; }
    ID3D12RootSignature* GetRootSignature() { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() { return pipelineState_.Get(); }

    void SetTextureManager(TextureManager* textureManager) {
        textureManager_ = textureManager;
    }

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();

private:
    DirectXCommon* dxCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};