#include "SrvManager.h"
#include "DirectXCommon.h"
#include <cassert>

// シェーダー参照用ディスクリプタを準備し、初期値と必要なリソースを設定する。
void SrvManager::Initialize(DirectXCommon *dxCommon, uint32_t maxDescriptors) {
    assert(dxCommon);
    dxCommon_ = dxCommon;
    maxDescriptors_ = maxDescriptors;
    useIndex_ = 0;

    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = maxDescriptors_;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    HRESULT hr = dxCommon_->GetDevice()->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap_));
    assert(SUCCEEDED(hr));

    descriptorSize_ = dxCommon_->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

uint32_t SrvManager::Allocate() {
    assert(useIndex_ < maxDescriptors_);
    return useIndex_++;
}

// ディスクリプタ番号からCPU側のハンドル位置を求める。
D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t index) const {
    assert(descriptorHeap_);
    assert(index < maxDescriptors_);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize_) * index;
    return handle;
}

// ディスクリプタ番号からGPU側のハンドル位置を求める。
D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t index) const {
    assert(descriptorHeap_);
    assert(index < maxDescriptors_);

    D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(descriptorSize_) * index;
    return handle;
}

// 2Dテクスチャをシェーダーから参照するSRVを作成する。
void SrvManager::CreateSRVforTexture2D(uint32_t index, ID3D12Resource *resource, DXGI_FORMAT format, UINT mipLevels) {
    assert(dxCommon_);
    assert(resource);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = mipLevels;

    dxCommon_->GetDevice()->CreateShaderResourceView(resource, &srvDesc, GetCPUDescriptorHandle(index));
}

// 構造化バッファをシェーダーから参照するSRVを作成する。
void SrvManager::CreateSRVforStructuredBuffer(uint32_t index, ID3D12Resource *resource, UINT numElements,
                                              UINT structureByteStride) {
    assert(dxCommon_);
    assert(resource);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.FirstElement = 0;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;

    dxCommon_->GetDevice()->CreateShaderResourceView(resource, &srvDesc, GetCPUDescriptorHandle(index));
}

// リソース形式に合わせたシェーダー参照用ディスクリプタを作成する。
void SrvManager::CreateSRV(uint32_t index, ID3D12Resource *resource, const D3D12_SHADER_RESOURCE_VIEW_DESC &srvDesc) {
    assert(dxCommon_);
    assert(resource);

    dxCommon_->GetDevice()->CreateShaderResourceView(resource, &srvDesc, GetCPUDescriptorHandle(index));
}
