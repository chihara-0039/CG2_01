#include "Object3d.h"
#include "TextureManager.h" // GetSrvHandleGPUを使うために必要
#include <cassert>

void Object3d::Initialize(Object3dCommon* object3dCommon) {
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;
    auto device = object3dCommon_->GetDxCommon()->GetDevice();

    // Transform Buffer
    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = (sizeof(TransformationMatrix) + 0xff) & ~0xff;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&transformationResource_));
    transformationResource_->Map(0, nullptr, (void**)&transformationData_);
    transformationData_->WVP = Math::MakeIdentity4x4();
    transformationData_->World = Math::MakeIdentity4x4();

    // Material Buffer
    resDesc.Width = (sizeof(Material) + 0xff) & ~0xff;
    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&materialResource_));
    materialResource_->Map(0, nullptr, (void**)&materialData_);

    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = 1;
    materialData_->uvTransform = Math::MakeIdentity4x4();
}

void Object3d::Update() {
    Matrix4x4 worldMatrix = Math::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
    Matrix4x4 wvpMatrix = Math::Multiply(worldMatrix, Math::Multiply(viewMatrix_, projectionMatrix_));

    transformationData_->WVP = wvpMatrix;
    transformationData_->World = worldMatrix;
}

void Object3d::SetUVTransform(const Transform& t) {
    if (materialData_) {
        Matrix4x4 w = Math::MakeAffineMatrix(t.scale, t.rotate, t.translate);
        materialData_->uvTransform = w;
    }
}

void Object3d::Draw() {
    if (!model_) return;
    auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();

    // 0. Material
    commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
    // 1. Transform
    commandList->SetGraphicsRootConstantBufferView(1, transformationResource_->GetGPUVirtualAddress());
    // 2. Light (Commonが持つ)
    commandList->SetGraphicsRootConstantBufferView(2, object3dCommon_->GetLightGPUVirtualAddress());
    // 3. Texture
    // ★ここが修正ポイント: Common経由でTextureManagerを呼び出す
    if (object3dCommon_->GetTextureManager()) {
        auto gpuHandle = object3dCommon_->GetTextureManager()->GetSrvHandleGPU(model_->GetTextureHandle());
        commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);
    }

    model_->Draw(commandList);
}