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

	// ゲッター
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
	// ルートシグネチャとパイプラインステートは描画時に必要なのでゲッターを用意
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
	// ここで落ちる場合、Initializeで作成失敗している可能性が高い
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }
	// 影用のパイプラインステートも必要になるのでゲッターを追加
    ID3D12PipelineState* GetShadowPipelineState() const { return shadowPipelineState_.Get(); }

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
	// ルートシグネチャの作成関数を追加
    void CreateRootSignature();
	// パイプラインステートの作成関数を追加
    void CreateGraphicsPipeline();
	// 影用のルートシグネチャも作成する関数を追加
    void CreateLightBuffer();
	// 影用のパイプラインステートも作成する関数を追加
    void CreateShadowPipeline();

private:
    // 
    DirectXCommon* dxCommon_ = nullptr;
    TextureManager* textureManager_ = nullptr; // テクスチャ管理クラスへのポインタ

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    // 平行光源用
    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight* lightData_ = nullptr;

	// 影用のルートシグネチャとパイプラインステートも追加
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;
};