#include "TextureManager.h"
#include <DirectXTex.h>
#include <vector>
#include <cassert>
#include <format>

using namespace Microsoft::WRL;

// 文字列変換用
static std::wstring ConvertString(const std::string& str) {

	// 文字列が空の場合は空のワイド文字列を返す
    if (str.empty()) { 

		// ここで失敗を検知して止める
        return std::wstring();
    }

	// UTF-8からUTF-16への変換
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	// 変換後のワイド文字列を格納するためのバッファを確保
    std::wstring wstrTo(size_needed, 0);
	// 変換を実行
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	// 変換後のワイド文字列を返す
    return wstrTo;
}


// データ転送関数
[[nodiscard]]

// テクスチャデータをアップロードするための関数。CPUアクセス可能なリソースを作成し、ミップマップごとにデータを転送していく。
ComPtr<ID3D12Resource> UploadTextureData(ID3D12Device* device, const DirectX::ScratchImage& mipImages) {
	
    // テクスチャのメタデータからリソースの説明を作成
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
	// D3D12_RESOURCE_DESC構造体を初期化して、テクスチャの幅、高さ、ミップレベル数、フォーマットなどを設定
    D3D12_RESOURCE_DESC textureDesc{};
	// テクスチャの幅をDirectXTexのメタデータから取得して設定
    textureDesc.Width = UINT(metadata.width);
	// テクスチャの高さをDirectXTexのメタデータから取得して設定
    textureDesc.Height = UINT(metadata.height);
	// ミップレベル数はDirectXTexのメタデータから取得
    textureDesc.MipLevels = UINT16(metadata.mipLevels);
	// 3Dテクスチャの場合はDepthを設定し、2Dテクスチャの場合はArraySizeを設定する。DirectXTexのメタデータから取得
    textureDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	// フォーマットはDirectXTexのメタデータから取得
    textureDesc.Format = metadata.format;
	// サンプル数は通常1で、マルチサンプリングを使用しない場合は0に設定
    textureDesc.SampleDesc.Count = 1;
	// テクスチャの次元を設定（1D、2D、3Dなど）
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	// アップロード用のリソースを作成するためのヒーププロパティを設定。アップロードヒープはCPUから書き込み可能で、GPUからは読み取り専用のリソースを作成するために使用される。
    D3D12_HEAP_PROPERTIES heapProps{ D3D12_HEAP_TYPE_CUSTOM, D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0, 1, 1 };

	// CPUアクセス可能なリソースを作成。D3D12_RESOURCE_STATE_GENERIC_READは、GPUがこのリソースを読み取るための状態であることを示す。
    ComPtr<ID3D12Resource> resource;
	// CreateCommittedResource関数を使用して、アップロード用のリソースを作成する。ヒーププロパティ、リソースの説明、初期状態などを指定する。
    HRESULT hr = device->CreateCommittedResource(
		// ヒーププロパティを指定。アップロードヒープはCPUから書き込み可能で、GPUからは読み取り専用のリソースを作成するために使用される。
        &heapProps, D3D12_HEAP_FLAG_NONE, &textureDesc,
		// 初期状態はD3D12_RESOURCE_STATE_GENERIC_READで、GPUがこのリソースを読み取るための状態であることを示す。
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
	// リソースの作成に失敗した場合は、HRESULTをチェックしてエラーを検出する。失敗した場合は、assertで強制停止させる。
    assert(SUCCEEDED(hr));

	// ミップマップごとにデータを転送するためのループ。DirectXTexのScratchImageからミップマップのイメージデータを取得し、リソースにマッピングしてデータをコピーする。
    const DirectX::Image* intermediateImages = mipImages.GetImages();

	// 各ミップレベルに対して、リソースをマッピングしてデータをコピーする。Map関数を使用してリソースのサブリソースをマッピングし、ポインタを取得する。
    for (size_t i = 0; i < metadata.mipLevels; ++i) {
		// ミップマップのイメージデータを取得
        const DirectX::Image& img = intermediateImages[i];
		// リソースのサブリソースをマッピングして、CPUがアクセスできるポインタを取得する。Map関数は、リソースの特定のサブリソースをマッピングし、データへのポインタを返す。
        void* pData = nullptr;
		// Map関数を呼び出して、リソースのサブリソースをマッピングする。UINT(i)は、マップするサブリソースのインデックスを指定する。nullptrは、読み取り範囲を指定するためのD3D12_RANGE構造体へのポインタで、ここでは全体をマッピングするためにnullptrを指定している。&pDataは、マッピングされたデータへのポインタを受け取るための引数。
        hr = resource->Map(UINT(i), nullptr, &pData);

		// マッピングに成功した場合は、イメージデータをリソースにコピーする。img.pixelsは、DirectXTexのImage構造体からピクセルデータへのポインタを取得する。dstは、マッピングされたリソースのデータへのポインタで、pDataから取得する。ループを使用して、各行ごとにデータをコピーする。img.rowPitchは、1行あたりのバイト数を示す。
        if (SUCCEEDED(hr)) {
			// データをコピーするためのループ。各行ごとにデータをコピーする。img.rowPitchは、1行あたりのバイト数を示す。
            const uint8_t* src = img.pixels;
			// コピー先のポインタをuint8_t*にキャストして、行ごとにデータをコピーする。memcpy関数を使用して、srcからdstにimg.rowPitchバイト分のデータをコピーする。ループは、テクスチャの高さ分だけ繰り返される。
            uint8_t* dst = static_cast<uint8_t*>(pData);
			// 各行ごとにデータをコピーする。img.rowPitchは、1行あたりのバイト数を示す。
            for (size_t y = 0; y < img.height; ++y) {
				// memcpy関数を使用して、srcからdstにimg.rowPitchバイト分のデータをコピーする。ループは、テクスチャの高さ分だけ繰り返される。
                memcpy(dst, src, img.rowPitch);
				// コピー後、srcとdstのポインタを次の行に進めるために、img.rowPitchバイト分だけ増加させる。これにより、次の行のデータがコピーされる。
                src += img.rowPitch;
                dst += img.rowPitch;
            }
			// データのコピーが完了したら、リソースのサブリソースをアンマップする。Unmap関数を呼び出して、リソースのサブリソースをアンマップする。UINT(i)は、アンマップするサブリソースのインデックスを指定する。nullptrは、書き込み範囲を指定するためのD3D12_RANGE構造体へのポインタで、ここでは全体をアンマップするためにnullptrを指定している。
            resource->Unmap(UINT(i), nullptr);
        }
    }
	// データ転送が完了したリソースを返す。呼び出し元は、このリソースを使用してSRVを作成し、テクスチャとして使用することができる。
    return resource;
}

// TextureManagerクラスのメンバー関数の実装。テクスチャの初期化、読み込み、SRVヒープの取得、GPUハンドルの取得、リソース説明の取得などを行う。
void TextureManager::Initialize(DirectXCommon* dxCommon) {
	// DirectXCommonのポインタを保存
    dxCommon_ = dxCommon;

	// SRVヒープの作成
    ID3D12Device* device = dxCommon_->GetDevice();
	// D3D12_DESCRIPTOR_HEAP_DESC構造体を初期化して、SRVヒープのタイプ、ディスクリプタ数、
    // フラグなどを設定する。SRVヒープは、シェーダーリソースビュー（SRV）を格納するためのデスクリプタヒープで、
    // GPUがテクスチャリソースにアクセスするために使用される。
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};

	// Typeは、SRV、CBV、UAVのいずれかを指定する。ここではSRVを指定している。
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	// NumDescriptorsは、ヒープ内のディスクリプタの数を指定する。
    // ここでは、最大テクスチャ数を指定している。
    srvHeapDesc.NumDescriptors = kMaxTextures;

	// Flagsは、ヒープのフラグを指定する。
    // D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLEを指定すると、
    // このヒープがシェーダーからアクセス可能になる。
    // これにより、GPUがテクスチャリソースにアクセスできるようになる。
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	// NodeMaskは、マルチGPU環境で使用されるノードマスクを指定する。
    // ここでは、単一GPU環境を想定しているため、0を指定している。
    HRESULT hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_));

	// ヒープの作成に失敗した場合は、HRESULTをチェックしてエラーを検出する。
    // 失敗した場合は、assertで強制停止させる。
    assert(SUCCEEDED(hr));

	// ディスクリプタのサイズを取得する。GetDescriptorHandleIncrementSize関数を呼び出して、SRVヒープ内のディスクリプタのサイズを取得する。これにより、テクスチャごとにSRVを作成する際に、正しいオフセットでディスクリプタを配置することができる。
    descriptorSizeSRV_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// テクスチャデータの初期化。
    // テクスチャデータを格納するための構造体を初期化する。
    // ここでは、テクスチャリソース、アップロード用の中間リソース、
    // SRVのCPUハンドルとGPUハンドル、
    // リソースの説明などを格納するための構造体を定義している。
#ifdef USE_IMGUI
    TextureData reserve;
    reserve.resource = nullptr;
    textures_.push_back(reserve);
#endif
}

// テクスチャの読み込み関数。指定されたファイルパスからテクスチャを読み込み、SRVを作成して管理する。既に同じファイルパスのテクスチャが読み込まれている場合は、そのテクスチャのハンドルを返す。
uint32_t TextureManager::LoadTexture(const std::string& filePath) {
    // 1. 既に読み込み済みかチェック
    if (fileMap_.contains(filePath)) {
        return fileMap_[filePath];
    }
	// 新規読み込みの場合は、テクスチャ数の上限をチェックしてから読み込む。上限を超える場合は、assertで強制停止させる。
    assert(textures_.size() < kMaxTextures);


    // 2. ファイル読み込み (DirectXTex)
    DirectX::ScratchImage image;

	// 文字列をUTF-8からUTF-16に変換して、DirectXTexのLoadFromWICFile関数に渡す。LoadFromWICFile関数は、指定されたファイルパスからテクスチャを読み込み、
    // ScratchImage構造体に格納する。
    // WIC_FLAGS_FORCE_SRGBフラグを指定して、sRGBカラースペースで読み込むようにしている。
    std::wstring wFilePath = ConvertString(filePath);

	// LoadFromWICFile関数の呼び出しに失敗した場合は、
    // HRESULTをチェックしてエラーを検出する。
    // 失敗した場合は、コンソールに失敗したファイル名を表示し、
    // assertで強制停止させる。
    HRESULT hr = DirectX::LoadFromWICFile(wFilePath.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
    
    // ★ここで失敗を検知して止める
    if (FAILED(hr)) {
        // コンソールに失敗したファイル名を表示
        std::string message = "Failed to load texture: " + filePath + "\n";
        OutputDebugStringA(message.c_str());
        assert(SUCCEEDED(hr)); // ここで強制停止させる
        return 0;
    }

    // 3. ミップマップ生成
    DirectX::ScratchImage mipImages;

	// GenerateMipMaps関数を呼び出して、
    // 読み込んだテクスチャからミップマップを生成する。
    // 生成されたミップマップは、別のScratchImage構造体に格納される。
    hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	// ★ここで失敗を検知して止める
    assert(SUCCEEDED(hr));


    // 4. テクスチャリソースの作成
	// ミップマップのメタデータからリソースの説明を作成する。
    // DirectXTexのScratchImageからテクスチャのメタデータを取得し、
    // D3D12_RESOURCE_DESC構造体を初期化して、
    // テクスチャの幅、高さ、ミップレベル数、フォーマットなどを設定する。
    const DirectX::TexMetadata& metadata = mipImages.GetMetadata();
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Width = UINT(metadata.width);
    textureDesc.Height = UINT(metadata.height);
    textureDesc.MipLevels = UINT16(metadata.mipLevels);
    textureDesc.DepthOrArraySize = UINT16(metadata.arraySize);
    textureDesc.Format = metadata.format;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	// D3D12_HEAP_PROPERTIES構造体を初期化して、
    // テクスチャリソースを作成するためのヒーププロパティを設定する。
    // テクスチャリソースは、GPUがアクセスするためのリソースであり、
    // D3D12_HEAP_TYPE_DEFAULTを指定して、GPU専用のヒープに配置されるようにしている。
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

	// CPUアクセス可能なアップロードヒープを作成。
    // D3D12_HEAP_TYPE_UPLOADを指定して、CPUから書き込み可能で、
    // GPUからは読み取り専用のリソースを作成するためのヒーププロパティを設定する。
    D3D12_HEAP_PROPERTIES uploadHeapProps = {};
    uploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;


	// D3D12_RESOURCE_DESC構造体を初期化して、
    // アップロード用のバッファの説明を設定する。
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

	// アップロード用の中間リソースを作成。
    // CreateCommittedResource関数を使用して、
    // アップロード用のバッファを作成する。
    // ヒーププロパティ、リソースの説明、初期状態などを指定する。
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

	// ミップマップごとにデータを転送するためのループ。
    // DirectXTexのScratchImageからミップマップのイメージデータを取得し、
    // アップロード用の中間リソースにコピーする。
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
	// データのコピーが完了したら、リソースをアンマップする。
    // Unmap関数を呼び出して、リソースをアンマップする。
    // 0は、アンマップするサブリソースのインデックスを指定する。
    // nullptrは、書き込み範囲を指定するためのD3D12_RANGE構造体へのポインタで、
    // ここでは全体をアンマップするためにnullptrを指定している。
    intermediateResource->Unmap(0, nullptr);

    // 7. データ転送コマンド発行（Upload Heap -> Texture Resource）
    auto commandList = dxCommon_->GetCommandList();

	// 各ミップレベルごとに、CopyTextureRegionコマンドを発行して、
	// アップロード用の中間リソースからテクスチャリソースにデータを転送する。
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

	// D3D12_SHADER_RESOURCE_VIEW_DESC構造体を初期化して、
    // SRVの説明を設定する。
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = data.resourceDesc.Format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	// CreateShaderResourceView関数を呼び出して、
    // テクスチャリソースに対するSRVを作成する。
    // SRVは、GPUがテクスチャリソースにアクセスするためのビューであり、
    // シェーダーからテクスチャを使用するために必要である。
    dxCommon_->GetDevice()->CreateShaderResourceView(data.resource.Get(), &srvDesc, data.srvHandleCPU);

    textures_.push_back(data);
    fileMap_[filePath] = index;

    return index;

#ifdef USE_IMGUI
    // SRVの0番はImGuiのフォント用に予約する（Textureは1番から）
    textures_.resize(1);

    // 念のため、0番のCPU/GPUハンドルだけは埋めておく（resourceはnullptrのままでOK）
    textures_[0].srvHandleCPU = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    textures_[0].srvHandleGPU = srvHeap_->GetGPUDescriptorHandleForHeapStart();
    textures_[0].resourceDesc = {};
#endif
}

// SRVヒープのCPUハンドルを取得する関数。
// 指定されたテクスチャハンドルに対応するSRVのCPUハンドルを返す。
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureHandle) {
    return textures_[textureHandle].srvHandleGPU;
}

// SRVヒープのGPUハンドルを取得する関数。
const D3D12_RESOURCE_DESC& TextureManager::GetResourceDesc(uint32_t textureHandle) {
    return textures_[textureHandle].resourceDesc;
}

uint32_t TextureManager::RegisterExternalTexture(ID3D12Resource* resource) {
    assert(textures_.size() < kMaxTextures);

    // 1. 新しい空きスロットを確保
    uint32_t index = static_cast<uint32_t>(textures_.size());
    TextureData data;
    data.resource = resource;
    data.resourceDesc = resource->GetDesc();

    // 2. ヒープ内の住所(CPU/GPUハンドル)を計算
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = srvHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = srvHeap_->GetGPUDescriptorHandleForHeapStart();
    handleCPU.ptr += (descriptorSizeSRV_ * index);
    handleGPU.ptr += (descriptorSizeSRV_ * index);

    data.srvHandleCPU = handleCPU;
    data.srvHandleGPU = handleGPU;

    // 3. 「このリソースを読み取り用として使うよ」というカードを作成してヒープに登録
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    // 影用テクスチャは R32_TYPELESS なので、SRVでは R32_FLOAT として解釈させる
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    dxCommon_->GetDevice()->CreateShaderResourceView(resource, &srvDesc, data.srvHandleCPU);

    // 4. リストに追加してインデックス（ハンドル）を返す
    textures_.push_back(data);
    return index;
}

