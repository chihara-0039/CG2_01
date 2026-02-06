#include "TextureManager.h"
#include <DirectXTex.h>
#include <vector>
#include <cassert>
#include <format>

using namespace Microsoft::WRL;

// 文字列変換用
static std::wstring ConvertString(const std::string& str) {
    if (str.empty()) { 
        return std::wstring();
    }
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
    // 1. 既に読み込み済みかチェック
    if (fileMap_.contains(filePath)) {
        return fileMap_[filePath];
    }
    assert(textures_.size() < kMaxTextures);

    // 2. ファイル読み込み (DirectXTex)
    DirectX::ScratchImage image;
    std::wstring wFilePath = ConvertString(filePath);
    HRESULT hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    if (FAILED(hr)) {
        assert(false && "LoadTexture Failed");
    }

    // 3. ミップマップ生成
    DirectX::ScratchImage mipImages;
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
    assert(SUCCEEDED(hr));

    // 4. テクスチャリソースの作成
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Width = UINT(metadata.width);
    textureDesc.Height = UINT(metadata.height);
    textureDesc.MipLevels = UINT16(metadata.mipLevels);
    textureDesc.DepthOrArraySize = UINT16(metadata.arraySize);
    textureDesc.Format = metadata.format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

    // Heap Properties for Texture (Default Heap)
    D3D12_HEAP_PROPERTIES textureHeapProps = {};
    textureHeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    Microsoft::WRL::ComPtr<ID3D12Resource> textureResource;
    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &textureHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, // データ転送前はコピー先状態にしておく
        nullptr,
        IID_PPV_ARGS(&textureResource));
    assert(SUCCEEDED(hr));

    // 5. 中間リソース（Upload Heap）の作成
    // データ転送に必要なサイズやレイアウトを計算
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> layouts(metadata.mipLevels);
    UINT64 uploadBufferSize = 0;
    dxCommon_->GetDevice()->GetCopyableFootprints(&textureDesc, 0, UINT(metadata.mipLevels), 0, layouts.data(), nullptr, nullptr, &uploadBufferSize);

    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC uploadBufferDesc = {};
    uploadBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    uploadBufferDesc.Alignment = 0;
    uploadBufferDesc.Width = uploadBufferSize;
    uploadBufferDesc.Height = 1;
    uploadBufferDesc.DepthOrArraySize = 1;
    uploadBufferDesc.MipLevels = 1;
    uploadBufferDesc.Format = DXGI_FORMAT_UNKNOWN;
    uploadBufferDesc.SampleDesc.Count = 1;
    uploadBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;
    hr = dxCommon_->GetDevice()->CreateCommittedResource(
        &uploadHeapProps,
        D3D12_HEAP_FLAG_NONE,
        &uploadBufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&intermediateResource)
    );
    assert(SUCCEEDED(hr));

    // 6. データ転送（CPU -> Upload Heap）
    uint8_t* pData = nullptr;
    hr = intermediateResource->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    assert(SUCCEEDED(hr));

    for (size_t i = 0; i < metadata.mipLevels; ++i) {
        const DirectX::Image* img = mipImages.GetImage(i, 0, 0);

        // 書き込み先のポインタ計算（レイアウトのオフセットを加算）
        uint8_t* dstStart = pData + layouts[i].Offset;
        const uint8_t* srcStart = img->pixels;

        // 行ごとにコピー（アライメント対応）
        for (size_t y = 0; y < img->height; ++y) {
            memcpy(
                dstStart + y * layouts[i].Footprint.RowPitch,
                srcStart + y * img->rowPitch,
                img->rowPitch
            );
        }
    }
    intermediateResource->Unmap(0, nullptr);

    // 7. データ転送コマンド発行（Upload Heap -> Texture Resource）
    auto commandList = dxCommon_->GetCommandList();

    for (size_t i = 0; i < metadata.mipLevels; ++i) {
        D3D12_TEXTURE_COPY_LOCATION dst = {};
        dst.pResource = textureResource.Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = UINT(i);

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.pResource = intermediateResource.Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = layouts[i];

        commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
    }

    // 8. リソースバリア（CopyDest -> PixelShaderResource）
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = textureResource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    commandList->ResourceBarrier(1, &barrier);


    // 9. 構造体に保存
    TextureData data;
    data.resource = textureResource;
    data.intermediateResource = intermediateResource;
    data.resourceDesc = textureResource->GetDesc();

    // 10. SRV作成
    uint32_t index = static_cast<uint32_t>(textures_.size());
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = srvHeap_->GetGPUDescriptorHandleForHeapStart();

    handleCPU.ptr += (descriptorSizeSRV_ * index);
    handleGPU.ptr += (descriptorSizeSRV_ * index);

    data.srvHandleCPU = handleCPU;
    data.srvHandleGPU = handleGPU;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = data.resourceDesc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

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