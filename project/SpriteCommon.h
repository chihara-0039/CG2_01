#pragma once
#include "DirectXCommon.h"
#include <wrl.h>
#include <d3d12.h>

class SpriteCommon {
public:
    // 初期化（DirectXCommon へのポインタを受け取る）
    void Initialize(DirectXCommon* dxCommon);

    // 共通描画設定（毎フレーム呼ぶ）
    void PreDraw();

    // getter（後で Sprite から使う）
    DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
    // ルートシグネチャ作成
    void CreateRootSignature();
    // グラフィックスパイプライン生成
    void CreateGraphicsPipeline();

private:
    DirectXCommon* dxCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};
