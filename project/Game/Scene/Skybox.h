#pragma once
#include "Object3dCommon.h"
#include "Object3d.h"
#include "MyMath.h"
#include <d3d12.h>
#include <wrl.h>

class Skybox {
public:
    void Initialize(Object3dCommon* object3dCommon, uint32_t textureHandle);
    void Update();
    void Draw();

    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        viewMatrix_ = view;
        projectionMatrix_ = projection;
    }

    void SetColor(const Vector4& color) {
        if (materialData_) {
            materialData_->color = color;
        }
    }

    void SetScale(const Vector3& scale) {
        transform_.scale = scale;
    }

    void SetPosition(const Vector3& pos) {
        transform_.translate = pos;
    }

private:
    void CreateMesh();
    void CreateGraphicsPipeline();

private:
    Object3dCommon* object3dCommon_ = nullptr;
    uint32_t textureHandle_ = 0;

    // トランスフォーム
    Transform transform_ = { {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
    Matrix4x4 viewMatrix_{};
    Matrix4x4 projectionMatrix_{};

    // 頂点・インデックス
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

    // 定数バッファ
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationResource_;
    TransformationMatrix* transformationData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    // パイプラインステート (RootSignatureは共通のものを使用)
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
