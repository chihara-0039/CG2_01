#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h" // 追加
#include <wrl.h>
#include <d3d12.h>
#include <memory>
#include <vector>
#include <cstddef>

// Sprite描画で共通利用するRootSignature、PSO、TextureManager参照を管理する。
class SpriteCommon {
  public:
    // 2D描画用のRootSignatureとGraphicsPipelineを作成する。
    void Initialize(DirectXCommon *dxCommon);

    // Sprite描画前に共通PSOとRootSignatureをCommandListへ設定する。
    void PreDraw();

    struct SpriteStorage {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        std::byte *cpu = nullptr;
        D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
    };
    // 一つの共有ページから、スプライト専用の重ならない保存領域を割り当てる。
    SpriteStorage AllocateSpriteStorage();

    DirectXCommon *GetDxCommon() const { return dxCommon_; }
    TextureManager *GetTextureManager() { return textureManager_; }
    ID3D12RootSignature *GetRootSignature() { return rootSignature_.Get(); }
    ID3D12PipelineState *GetPipelineState() { return pipelineState_.Get(); }

    // SpriteがテクスチャSRVを取得できるよう、TextureManagerを関連付ける。
    void SetTextureManager(TextureManager *textureManager) { textureManager_ = textureManager; }

  private:
    // Sprite用RootSignatureを作成する。
    void CreateRootSignature();
    // Sprite用のGraphicsPipelineStateを作成する。
    void CreateGraphicsPipeline();

  private:
    // DirectXCommonとTextureManagerは外部所有。ここでは参照だけ保持する。
    struct UploadPage {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        std::byte *cpu = nullptr;
        size_t used = 0;
    };
    std::vector<UploadPage> spritePages_;
    DirectXCommon *dxCommon_ = nullptr;
    TextureManager *textureManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};
