#include "DirectXCommon.h"
#include "WinApp.h"
#include <cassert>
#include <format>
#include <thread>
#include <DirectXTex.h>

using namespace Microsoft::WRL;

// main.cpp 側で定義しているユーティリティ関数の「宣言だけ」借りる
void Log(std::ostream& os, const std::string& message);
std::wstring ConvertString(const std::string& str);
std::string  ConvertString(const std::wstring& str);

// main.cpp 側で定義されているグローバル関数を参照する宣言
extern ComPtr<ID3D12Resource> CreateDepthStencilTextureResource(
    ComPtr<ID3D12Device> device,
    uint32_t width,
    uint32_t height);


DirectXCommon::DirectXCommon() = default;

DirectXCommon::~DirectXCommon() {
    if (fenceEvent_) {
        CloseHandle(fenceEvent_);
        fenceEvent_ = nullptr;
    }
}


void DirectXCommon::Initialize(
    WinApp* winApp,
    ComPtr<IDXGIFactory7> dxgiFactory,
    ComPtr<ID3D12Device> device,
    ComPtr<ID3D12CommandQueue> commandQueue,
    ComPtr<ID3D12CommandAllocator> commandAllocator,
    ComPtr<ID3D12GraphicsCommandList> commandList,
    ComPtr<IDXGISwapChain4> swapChain,
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap) {

    HRESULT hr;

    // ---------
    // 渡されたポインタをそのままメンバに保持
    // ---------
    winApp_ = winApp;
    dxgiFactory_ = dxgiFactory;
    device_ = device;
    commandQueue_ = commandQueue;
    commandAllocator_ = commandAllocator;
    commandList_ = commandList;
    swapChain_ = swapChain;
    srvDescriptorHeap_ = srvDescriptorHeap;

    assert(device_);
    assert(swapChain_);
    assert(commandQueue_);
    assert(commandAllocator_);
    assert(commandList_);
    assert(srvDescriptorHeap_);

    // =========================
    // RTV / DSV 用ディスクリプタヒープ作成
    // =========================
    if (!rtvDescriptorHeap_) {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.NumDescriptors = 2; // ダブルバッファ
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = device_->CreateDescriptorHeap(
            &rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap_));
        assert(SUCCEEDED(hr));
    }

    if (!dsvDescriptorHeap_) {
        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{};
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        hr = device_->CreateDescriptorHeap(
            &dsvHeapDesc, IID_PPV_ARGS(&dsvDescriptorHeap_));
        assert(SUCCEEDED(hr));
    }

    // =========================
    // スワップチェーンのバックバッファを取得
    // =========================
    if (!swapChainResources_[0]) {
        hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
        assert(SUCCEEDED(hr));
        hr = swapChain_->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));
        assert(SUCCEEDED(hr));
    }

    // =========================
    // バックバッファ用 RTV の設定
    // =========================
    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
    rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

    // RTV の先頭ハンドル
    D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle =
        rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    UINT rtvIncSize = device_->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    rtvHandles_[0] = rtvStartHandle;
    device_->CreateRenderTargetView(
        swapChainResources_[0].Get(), &rtvDesc, rtvHandles_[0]);

    rtvHandles_[1].ptr = rtvHandles_[0].ptr + rtvIncSize;
    device_->CreateRenderTargetView(
        swapChainResources_[1].Get(), &rtvDesc, rtvHandles_[1]);

    // =========================
    // 深度ステンシルバッファ & DSV の設定
    // =========================
    if (!depthStencilResource_) {
        depthStencilResource_ = CreateDepthStencilTextureResource(
            device_, WinApp::kClientWidth, WinApp::kClientHeight);
        assert(depthStencilResource_);
    }

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
    dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

    device_->CreateDepthStencilView(
        depthStencilResource_.Get(), &dsvDesc,
        dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

    //
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
    assert(SUCCEEDED(hr));

    //
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
    assert(SUCCEEDED(hr));

    //
    hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
    assert(SUCCEEDED(hr));

    // =========================
    // ビューポート / シザー
    // =========================
    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;

    // =========================
    // フェンス初期化
    // =========================
    fenceVal_ = 0;
    hr = device_->CreateFence(
        fenceVal_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
    assert(SUCCEEDED(hr));

    fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    assert(fenceEvent_ != nullptr);

    //FPS 固定用の初期化
    InitializeFixFPS();
}

void DirectXCommon::PreDraw() {
    // バックバッファのインデックス
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    // Present → RenderTarget へのバリア
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

    commandList_->ResourceBarrier(1, &barrier);

    // 描画先の RTV / DSV を設定
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
        dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

    commandList_->OMSetRenderTargets(
        1, &rtvHandles_[backBufferIndex], FALSE, &dsvHandle);

    // クリアカラー
    float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };

    commandList_->ClearRenderTargetView(
        rtvHandles_[backBufferIndex], clearColor, 0, nullptr);

    commandList_->ClearDepthStencilView(
        dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // SRV ヒープ
    ID3D12DescriptorHeap* descriptorHeaps[] = {
        srvDescriptorHeap_.Get()
    };
    commandList_->SetDescriptorHeaps(1, descriptorHeaps);

    // ビューポート / シザー
    commandList_->RSSetViewports(1, &viewport_);
    commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PostDraw() {
    // バックバッファのインデックス
    UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

    // RenderTarget → Present へのバリア
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = swapChainResources_[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    commandList_->ResourceBarrier(1, &barrier);

    // コマンドリストを閉じる
    HRESULT hr = commandList_->Close();
    assert(SUCCEEDED(hr));

    // コマンドを実行
    ID3D12CommandList* cmdLists[] = { commandList_.Get() };
    commandQueue_->ExecuteCommandLists(1, cmdLists);

    // Present
    swapChain_->Present(1, 0);

    // フェンス
    fenceVal_++;
    commandQueue_->Signal(fence_.Get(), fenceVal_);

    if (fence_->GetCompletedValue() < fenceVal_) {
        fence_->SetEventOnCompletion(fenceVal_, fenceEvent_);
        WaitForSingleObject(fenceEvent_, INFINITE);
    }

    //ここで1フレームの経過時間を調整して60FPS固定にする
    UpdateFixFPS();

    // 次フレーム用にリセット
    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}


// CompileShader関数02_00
Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileShader(
    const std::wstring& filepath,
    const wchar_t* profile,
    std::ostream& os) {

    // ログ
    Log(os, ConvertString(std::format(
        L"Begin CompileShader, path:{}, profile:{}\n", filepath, profile)));

    // hlslファイルを読む
    Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;
    HRESULT hr = dxcUtils_->LoadFile(filepath.c_str(), nullptr, &shaderSource);
    assert(SUCCEEDED(hr));

    DxcBuffer shaderSourceBuffer{};
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8

    // コンパイル引数
    LPCWSTR arguments[] = {
        filepath.c_str(),
        L"-E", L"main",
        L"-T", profile,
        L"-Zi", L"-Qembed_debug",
        L"-Od",
        L"-Zpr"
    };

    Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;
    hr = dxcCompiler_->Compile(
        &shaderSourceBuffer,
        arguments,
        _countof(arguments),
        includeHandler_.Get(),
        IID_PPV_ARGS(&shaderResult));
    assert(SUCCEEDED(hr));

    // エラー出力
    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(os, shaderError->GetStringPointer());
        assert(false);
    }

    // 実行用バイナリ取得
    Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    Log(os, ConvertString(std::format(
        L"Compile Succeeded, path:{}, profile:{}\n", filepath, profile)));

    return shaderBlob;
}
//=== D3D12バッファリソース作成（UPLOADヒープ） ===
Microsoft::WRL::ComPtr<ID3D12Resource>
CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {

    // 頂点リソース用のヒープの設定02_03
    D3D12_HEAP_PROPERTIES uploadHeapProperties{};
    uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // Uploadheapを使う
    // 頂点リソースの設定02_03
    D3D12_RESOURCE_DESC vertexResourceDesc{};
    // バッファリソース。テクスチャの場合はまた別の設定をする02_03
    vertexResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    vertexResourceDesc.Width = sizeInBytes; // リソースのサイズ　02_03
    // バッファの場合はこれらは１にする決まり02_03
    vertexResourceDesc.Height = 1;
    vertexResourceDesc.DepthOrArraySize = 1;
    vertexResourceDesc.MipLevels = 1;
    vertexResourceDesc.SampleDesc.Count = 1;
    // バッファの場合はこれにする決まり02_03
    vertexResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // 実際に頂点リソースを作る02_03
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &vertexResourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&vertexResource));
    assert(SUCCEEDED(hr));

    return vertexResource;
}

//=== D3D12ディスクリプタヒープ作成 ===
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
CreateDescriptorHeap(Microsoft::WRL::ComPtr<ID3D12Device> device,
    D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors,
    bool shaderVisivle) {
    // ディスクリプタヒープの生成02_02
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DescriptorHeap = nullptr;

    D3D12_DESCRIPTOR_HEAP_DESC DescriptorHeapDesc{};
    DescriptorHeapDesc.Type = heapType;
    DescriptorHeapDesc.NumDescriptors = numDescriptors;
    DescriptorHeapDesc.Flags = shaderVisivle
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&DescriptorHeapDesc,
        IID_PPV_ARGS(&DescriptorHeap));
    // ディスクリプタヒープが作れなかったので起動できない
    assert(SUCCEEDED(hr)); // 1
    return DescriptorHeap;
}

//=== D3D12テクスチャリソース作成（DEFAULTヒープ） ===
Microsoft::WRL::ComPtr<ID3D12Resource>
CreateTextureResource(Microsoft::WRL::ComPtr<ID3D12Device> device,
    const DirectX::TexMetadata& metadata) {
    // 1.metadataをもとにResourceの設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = UINT(metadata.width); // Textureの幅
    resourceDesc.Height = UINT(metadata.height); // Textureの高さ
    resourceDesc.MipLevels = UINT16(metadata.mipLevels); // mipdmapの数
    resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize); // 奥行き　or 配列Textureの配列数
    resourceDesc.Format = metadata.format; // TextureのFormat
    resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(
        metadata.dimension); // Textureの次元数　普段使っているのは二次元
    // 2.利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う//03_00EX
    // heapProperties.CPUPageProperty =
    //     D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; //
    //     WriteBaackポリシーでCPUアクセス可能
    // heapProperties.MemoryPoolPreference =
    //     D3D12_MEMORY_POOL_L0; // プロセッサの近くに配置

    // 3.Resourceを生成する
    Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, // Heapの固定
        D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
        &resourceDesc, // Resourceの設定
        D3D12_RESOURCE_STATE_COPY_DEST, // 初回のResourceState.Textureは基本読むだけ//03_00EX
        nullptr, // Clear最適地。使わないのでnullptr
        IID_PPV_ARGS(&resource)); // 作成するResourceポインタへのポインタ
    assert(SUCCEEDED(hr));
    return resource;
}

// FPS固定初期化
void DirectXCommon::InitializeFixFPS() {
	reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS() {
    using namespace std::chrono;

    // 1/60秒分の時間（マイクロ秒）
    const microseconds kMinTime(static_cast<int64_t>(1000000.0f / 60.0f));
    // 1/60秒より少し短い時間（半端なリフレッシュレート対策用）
    const microseconds kMinCheckTime(static_cast<int64_t>(1000000.0f / 65.0f));

    // 現在時間
    const steady_clock::time_point now = steady_clock::now();
    // 前フレームからの経過時間
    const microseconds elapsed = duration_cast<microseconds>(now - reference_);

    // 60Hz 付近のモニタでは、VSYNC だけだと待ち過ぎになることがあるので
    // 「十分に短いフレームのときだけ」追加でスリープする
    if (elapsed < kMinCheckTime) {
        // 1/60秒に達するまで 1マイクロ秒ずつ sleep する
        while (steady_clock::now() - reference_ < kMinTime) {
            std::this_thread::sleep_for(microseconds(1));
        }
    }

    // 次フレーム用に基準時間を更新
    reference_ = steady_clock::now();
}