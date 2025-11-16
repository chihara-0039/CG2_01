#include "DirectXCommon.h"
#include "WinApp.h"
#include <cassert>

using namespace Microsoft::WRL;

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

    HRESULT hr = S_OK;

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

    // 次フレーム用にリセット
    hr = commandAllocator_->Reset();
    assert(SUCCEEDED(hr));
    hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
    assert(SUCCEEDED(hr));
}
