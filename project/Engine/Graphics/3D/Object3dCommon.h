#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"

// GPU に渡すライト情報。
// Object3d.PS.hlsl の LightBuffer(register b1) と同じ並びで保持する。
struct DirectionalLight {
    Vector4 color;           // 平行光源の色
    Vector3 direction;       // 平行光源の向き
    float   intensity;       // 平行光源の強さ
    Vector3 cameraPosition;  // スペキュラ計算用のカメラ位置
    float   paddingLight;
    Vector3 pointLightPosition;
    float   pointLightIntensity;
    Vector4 pointLightColor;
};

// 3D オブジェクト描画で共有する RootSignature / PSO / ライトバッファを管理する。
// Object3d は描画直前に PreDraw() を呼び、このクラスの描画設定を CommandList にバインドする。
class Object3dCommon {
public:
    // 通常描画・影描画・インスタンシング描画に必要な GPU リソースを作成する。
    void Initialize(DirectXCommon* dxCommon);

    // 通常の 3D 描画用 RootSignature / PSO / プリミティブ種別を CommandList に設定する。
    void PreDraw();

    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }
    ID3D12PipelineState* GetSkinnedPipelineState() const { return skinnedPipelineState_.Get(); }
    ID3D12PipelineState* GetShadowPipelineState() const { return shadowPipelineState_.Get(); }

    ID3D12RootSignature* GetInstancedRootSignature() const { return instancedRootSignature_.Get(); }
    ID3D12PipelineState* GetInstancedPipelineState() const { return instancedPipelineState_.Get(); }
    ID3D12PipelineState* GetInstancedShadowPipelineState() const { return instancedShadowPipelineState_.Get(); }
    ID3D12PipelineState* GetInstancedAlphaPipelineState() const { return instancedAlphaPipelineState_.Get(); }

    // ライトを標準値に戻す。
    void SetDefaultLight();

    void SetLightDirection(const Vector3& direction) {
        if (lightData_) {
            lightData_->direction = Math::Normalize(direction);
        }
    }

    void SetLightColor(const Vector4& color) {
        if (lightData_) {
            lightData_->color = color;
        }
    }

    void SetLightIntensity(float intensity) { lightData_->intensity = intensity; }

    void SetPointLight(const Vector3& pos, float intensity, const Vector4& color) {
        if (lightData_) {
            lightData_->pointLightPosition = pos;
            lightData_->pointLightIntensity = intensity;
            lightData_->pointLightColor = color;
        }
    }

    // スペキュラ計算用のカメラ位置を更新する。
    void SetCameraPosition(const Vector3& cameraPosition) {
        if (lightData_) {
            lightData_->cameraPosition = cameraPosition;
        }
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetLightGPUVirtualAddress() const {
        return lightResource_->GetGPUVirtualAddress();
    }

    // テクスチャ SRV ヒープの設定に使う TextureManager を登録する。
    void SetTextureManager(TextureManager* textureManager) { textureManager_ = textureManager; }
    TextureManager* GetTextureManager() const { return textureManager_; }

    void SetEnvironmentTextureHandle(uint32_t textureHandle) { environmentTextureHandle_ = textureHandle; }
    uint32_t GetEnvironmentTextureHandle() const { return environmentTextureHandle_; }

    // 壁越しのプレイヤーシルエット表示用 PSO を CommandList に設定する。
    void PreDrawPlayerHighlight();

private:
    void CreateRootSignature();
    void CreateGraphicsPipeline();
    void CreateSkinnedPipeline();
    void CreateLightBuffer();
    void CreateShadowPipeline();
    void CreateInstancedRootSignature();
    void CreateInstancedGraphicsPipeline();
    void CreateInstancedShadowPipeline();
    void CreatePlayerHighlightPipeline();
    void CreateInstancedAlphaPipeline();

private:
    DirectXCommon*  dxCommon_       = nullptr;
    TextureManager* textureManager_ = nullptr;
    uint32_t environmentTextureHandle_ = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skinnedPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> playerHighlightPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> instancedRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedShadowPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedAlphaPipelineState_;
};
