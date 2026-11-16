#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <d3d12.h>
#include <wrl.h>
#include "MyMath.h"

// GPU に渡すライト情報。
// Object3d.PS.hlsl の LightBuffer(register b1) と同じ並びで保持する。
static constexpr uint32_t kMaxPointLights = 8;

struct PointLightData {
    Vector3 position;
    float intensity;
    Vector4 color;
    float radius;
    float padding[3];          // x: inner cone cosine
    Vector4 directionOuterCos; // xyz: emitted direction, w: outer cosine; -2 = point light
};

static_assert(sizeof(PointLightData) == 64, "Light layout must match the HLSL 16-byte packing");

struct DirectionalLight {
    Vector4 color;          // 平行光源の色
    Vector3 direction;      // 平行光源の向き
    float intensity;        // 平行光源の強さ
    Vector3 cameraPosition; // スペキュラ計算用のカメラ位置
    float paddingLight;
    PointLightData pointLights[kMaxPointLights];
    uint32_t pointLightCount;
    float pointLightPadding[3];
    Vector4 fogSettings{};  // enabled / near / far / density
    Vector4 fogOrigin{};    // xyz / comparison mode
    Vector4 fogDirection{}; // xyz / corridor length
    Vector4 fogScan{};      // strength / wave radius / reserved
};

// 3D オブジェクト描画で共有する RootSignature / PSO / ライトバッファを管理する。
// Object3d は描画直前に PreDraw() を呼び、このクラスの描画設定を CommandList にバインドする。
class Object3dCommon {
  public:
    // 通常描画・影描画・インスタンシング描画に必要な GPU リソースを作成する。
    void Initialize(DirectXCommon *dxCommon);

    // 通常の 3D 描画用 RootSignature / PSO / プリミティブ種別を CommandList に設定する。
    void PreDraw();

    // 深度判定を行わず、不透明なデバッグ形状を常に手前へ描画する。
    void PreDrawDebugOverlay();
    void PreDrawSkydome();
    void PreDrawBook();

    DirectXCommon *GetDxCommon() const { return dxCommon_; }

    ID3D12RootSignature *GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState *GetPipelineState() const { return pipelineState_.Get(); }
    ID3D12PipelineState *GetSkinnedPipelineState() const { return skinnedPipelineState_.Get(); }
    ID3D12PipelineState *GetShadowPipelineState() const { return shadowPipelineState_.Get(); }

    ID3D12RootSignature *GetInstancedRootSignature() const { return instancedRootSignature_.Get(); }
    ID3D12PipelineState *GetInstancedPipelineState() const { return instancedPipelineState_.Get(); }
    ID3D12PipelineState *GetInstancedShadowPipelineState() const { return instancedShadowPipelineState_.Get(); }
    ID3D12PipelineState *GetInstancedAlphaPipelineState() const { return instancedAlphaPipelineState_.Get(); }

    // ライトを標準値に戻す。
    void SetDefaultLight();
    // 探索済み範囲と霧の設定を、シェーダーへ渡す値へ反映する。
    void SetExplorationFog(const Vector4 &settings, const Vector4 &origin, const Vector4 &direction,
                           const Vector4 &scan) {
        if (!lightData_) {
            return;
        }
        lightData_->fogSettings = settings;
        lightData_->fogOrigin = origin;
        lightData_->fogDirection = direction;
        lightData_->fogScan = scan;
    }

    // 光源方向を描画用データへ反映する。
    void SetLightDirection(const Vector3 &direction) {
        if (lightData_) {
            lightData_->direction = Math::Normalize(direction);
        }
    }

    // 光源色を描画用データへ反映する。
    void SetLightColor(const Vector4 &color) {
        if (lightData_) {
            lightData_->color = color;
        }
    }

    void SetLightIntensity(float intensity) { lightData_->intensity = intensity; }

    // このフレームへ登録した点光源を消し、次の登録に備える。
    void ClearPointLights() {
        if (lightData_) {
            lightData_->pointLightCount = 0;
        }
    }
    // 灯数の上限を確認し、位置と届く範囲を持つ点光源を追加する。
    bool AddPointLight(const Vector3 &pos, float intensity, const Vector4 &color, float radius = 10.0f) {
        if (!lightData_ || lightData_->pointLightCount >= kMaxPointLights) {
            return false;
        }
        PointLightData &light = lightData_->pointLights[lightData_->pointLightCount++];
        light.position = pos;
        light.intensity = intensity;
        light.color = color;
        light.radius = radius > 0.01f ? radius : 0.01f;
        light.padding[0] = light.padding[1] = light.padding[2] = 0.0f;
        light.directionOuterCos = {0.0f, 0.0f, 0.0f, -2.0f};
        return true;
    }
    // Cone angles are cosines; the existing light loop handles both point and spot lights.
    bool AddSpotLight(const Vector3 &pos, const Vector3 &direction, float intensity, const Vector4 &color, float radius,
                      float innerCos, float outerCos) {
        if (!AddPointLight(pos, intensity, color, radius)) {
            return false;
        }
        auto &light = lightData_->pointLights[lightData_->pointLightCount - 1];
        const auto unit = Math::Normalize(direction);
        light.directionOuterCos = {unit.x, unit.y, unit.z, outerCos};
        light.padding[0] = innerCos;
        return true;
    }
    // 旧コード互換: 登録済みライトを置き換えて1灯だけ設定する。
    void SetPointLight(const Vector3 &pos, float intensity, const Vector4 &color) {
        ClearPointLights();
        if (intensity > 0.0f) {
            AddPointLight(pos, intensity, color);
        }
    }

    // スペキュラ計算用のカメラ位置を更新する。
    void SetCloudTime(float time) {
        if (lightData_) {
            lightData_->paddingLight = time;
        }
    }
    // 視線方向の計算に使うカメラ位置を、シェーダー用データへ反映する。
    void SetCameraPosition(const Vector3 &cameraPosition) {
        if (lightData_) {
            lightData_->cameraPosition = cameraPosition;
        }
    }

    D3D12_GPU_VIRTUAL_ADDRESS GetLightGPUVirtualAddress() const { return lightResource_->GetGPUVirtualAddress(); }

    // テクスチャ SRV ヒープの設定に使う TextureManager を登録する。
    void SetTextureManager(TextureManager *textureManager) { textureManager_ = textureManager; }
    TextureManager *GetTextureManager() const { return textureManager_; }

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
    DirectXCommon *dxCommon_ = nullptr;
    TextureManager *textureManager_ = nullptr;
    uint32_t environmentTextureHandle_ = 0;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> debugOverlayPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skydomePipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> bookPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> skinnedPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> lightResource_;
    DirectionalLight *lightData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> playerHighlightPipelineState_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> instancedRootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedShadowPipelineState_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> instancedAlphaPipelineState_;
};
