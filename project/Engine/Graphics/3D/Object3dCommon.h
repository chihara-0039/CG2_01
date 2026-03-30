#pragma once
#include "DirectXCommon.h"
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"

// 前方宣言
class TextureManager;

// 共通のライト構造体
struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
};

class Object3dCommon {
public:
    void Initialize(DirectXCommon* dxCommon);
    void PreDraw(); // 描画前設定

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

    // ライト制御
    void SetDefaultLight();
    void SetLightDirection(const Vector3& direction) { if (lightData_) lightData_->direction = Math::Normalize(direction); }
    void SetLightColor(const Vector4& color) { if (lightData_) lightData_->color = color; }
    void SetLightIntensity(float intensity) { if (lightData_) lightData_->intensity = intensity; }

    D3D12_GPU_VIRTUAL_ADDRESS GetLightGPUVirtualAddress() const { return lightResource_->GetGPUVirtualAddress(); }

    // ★重要: TextureManagerのセット
    void SetTextureManager(TextureManager* textureManager) { textureManager_ = textureManager; }
    TextureManager* GetTextureManager() const { return textureManager_; }

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();
    void CreateLightBuffer();

private:
    DirectXCommon* dxCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr; // テクスチャ管理クラスへのポインタ

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // 平行光源用
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;
};