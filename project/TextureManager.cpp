#include "TextureManager.h"
#include <DirectXTex.h>
#include <cassert>
#include <format>

using namespace Microsoft::WRL;

// 文字列変換用
static std::wstring ConvertString(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// データ転送関数
[[nodiscard]]
ComPtr<ID3D12Resource> UploadTextureData(ID3D12Device* device, const DirectX::ScratchImage& mipImages) {
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    D3D12_RESOURCE_DESC textureDesc{};
    textureDesc.Width = UINT(metadata.width);
    textureDesc.Height = UINT(metadata.height);
    textureDesc.MipLevels = UINT16(metadata.mipLevels);
    textureDesc.DepthOrArraySize = UINT16(metadata.arraySize);
    textureDesc.Format = metadata.format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_CUSTOM, D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0, 1, 1 };

    ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));

    const DirectX::Image* intermediateImages = mipImages.GetImages();
    for (size_t i = 0; i < metadata.mipLevels; ++i) {
        const DirectX::Image& img = intermediateImages[i];
        void* pData = nullptr;
        hr = resource->Map(UINT(i), nullptr, &pData);
        if (SUCCEEDED(hr)) {
            const uint8_t* src = img.pixels;
            uint8_t* dst = static_cast<uint8_t*>(pData);
            for (size_t y = 0; y < img.height; ++y) {
                memcpy(dst, src, img.rowPitch);
                src += img.rowPitch;
                dst += img.rowPitch;
            }
            resource->Unmap(UINT(i), nullptr);
        }
    }
    return resource;
}

void TextureManager::Initialize(DirectXCommon* dxCommon) {
    dxCommon_ = dxCommon;

    // ★修正点: dxCommon_->GetDevice() は既に ID3D12Device* なので .Get() は不要
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = kMaxTextures;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HRESULT hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_));
    assert(SUCCEEDED(hr));

    descriptorSizeSRV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

uint32_t TextureManager::LoadTexture(const std::string& filePath) {
    if (fileMap_.contains(filePath)) {
        return fileMap_[filePath];
    }
    assert(textures_.size() < kMaxTextures);

    DirectX::ScratchImage image;
    std::wstring wFilePath = ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    if (FAILED(hr)) {
        assert(false && "LoadTexture Failed");
    }

    DirectX::ScratchImage mipImages;
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
    assert(SUCCEEDED(hr));

    // ★修正点: 初期化済みの device を取得して使うか、dxCommonから直接取得
    // ここでは device 変数が見えないスコープなので dxCommon_ から取得
    auto resource = UploadTextureData(dxCommon_->GetDevice(), mipImages);

    TextureData data;
    data.resource = resource;
    data.resourceDesc = resource->GetDesc();

    uint32_t index = static_cast<uint32_t>(textures_.size());
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = srvHeap_->GetGPUDescriptorHandleForHeapStart();

    handleCPU.ptr += (descriptorSizeSRV_ * index);
    handleGPU.ptr += (descriptorSizeSRV_ * index);

    data.srvHandleCPU = handleCPU;
    data.srvHandleGPU = handleGPU;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = data.resourceDesc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(mipImages.GetMetadata().mipLevels);

    dxCommon_->GetDevice()->CreateShaderResourceView(data.resource.Get(), &srvDesc, data.srvHandleCPU);

    textures_.push_back(data);
    fileMap_[filePath] = index;

    return index;
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureHandle) {
    return textures_[textureHandle].srvHandleGPU;
}

const D3D12_RESOURCE_DESC& TextureManager::GetResourceDesc(uint32_t textureHandle) {
    return textures_[textureHandle].resourceDesc;
}