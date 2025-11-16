#include "DirectXCommon.h"
#include <cassert>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <dxgidebug.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

using namespace Microsoft::WRL;

// ================================
// DirectXCommon 本体
// ================================
void DirectXCommon::Initialize() {
    HRESULT hr = S_OK;

    // ------------------------
    // デバッグレイヤー ON + GPU based validation
    // ------------------------
#if defined(_DEBUG)
    if (ComPtr<ID3D12Debug1> debug; SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug)))) {
        debug->EnableDebugLayer();
        // GPU側でも検証する（元の main の SetEnableGPUBasedValidation 相当）
        debug->SetEnableGPUBasedValidation(TRUE);
    }
#endif

    // ------------------------
    // DXGIファクトリー生成
    // ------------------------
    ComPtr<IDXGIFactory7> dxgiFactory;
    UINT factoryFlags = 0;
#if defined(_DEBUG)
    factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
    hr = CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory));
    assert(SUCCEEDED(hr));

    // ------------------------
    // ハードウェアアダプタ列挙（dGPU優先）
    // ------------------------
    ComPtr<IDXGIAdapter4> adapter;
    for (UINT i = 0; ; ++i) {
        ComPtr<IDXGIAdapter1> tmpAdapter;
        if (dxgiFactory->EnumAdapters1(i, &tmpAdapter) == DXGI_ERROR_NOT_FOUND) {
            break;  // アダプタ取り切り
        }

        DXGI_ADAPTER_DESC1 desc{};
        tmpAdapter->GetDesc1(&desc);

        // ソフトウェアアダプタはスキップ
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // このアダプタでデバイスが作れるなら採用
        if (SUCCEEDED(D3D12CreateDevice(tmpAdapter.Get(), D3D_FEATURE_LEVEL_12_1,
                                        __uuidof(ID3D12Device), nullptr))) {
            hr = tmpAdapter.As(&adapter);
            assert(SUCCEEDED(hr));
            break;
        }
    }

    // 見つからなければ WARP を使用
    if (!adapter) {
        hr = dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
        assert(SUCCEEDED(hr));
    }

    // ------------------------
    // デバイス生成（機能レベルを高い方から試す）
    // ------------------------
    ComPtr<ID3D12Device> device;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_12_2,
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
    };

    for (size_t i = 0; i < _countof(featureLevels); ++i) {
        hr = D3D12CreateDevice(
            adapter.Get(),
            featureLevels[i],
            IID_PPV_ARGS(&device)
        );
        if (SUCCEEDED(hr)) {
            // 成功したレベルで確定
            break;
        }
    }

    assert(device);  // どのレベルでも作れなかったら終了

    // ------------------------
    // InfoQueue（エラーでブレーク & フィルタ）設定
    // ------------------------
#if defined(_DEBUG)
    if (ComPtr<ID3D12InfoQueue> infoQueue; SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {

        // やばいレベルはブレーク
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // 不要なメッセージを抑制（元 main の denyIds 相当）
        D3D12_MESSAGE_ID denyIds[] = {
            // Windows11 DXGI / D3D12 デバッグレイヤー相互作用バグのメッセージ
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
        };

        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO,
        };

        D3D12_INFO_QUEUE_FILTER filter{};
        filter.DenyList.NumIDs = _countof(denyIds);
        filter.DenyList.pIDList = denyIds;
        filter.DenyList.NumSeverities = _countof(severities);
        filter.DenyList.pSeverityList = severities;

        infoQueue->PushStorageFilter(&filter);
    }
#endif

    // ------------------------
    // メンバへ格納
    // ------------------------
    dxgiFactory_ = dxgiFactory;
    device_ = device;
}


// ================================
// 基盤用ユーティリティ関数
// ================================

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
CreateDescriptorHeap(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    UINT numDescriptors,
    bool shaderVisible) {
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = heapType;
    desc.NumDescriptors = numDescriptors;
    desc.Flags = shaderVisible
        ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE
        : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    HRESULT hr = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap));
    assert(SUCCEEDED(hr));

    return descriptorHeap;
}

D3D12_CPU_DESCRIPTOR_HANDLE
GetCPUDescriptorHandle(
    ID3D12DescriptorHeap* descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index) {
    D3D12_CPU_DESCRIPTOR_HANDLE handleCPU =
        descriptorHeap->GetCPUDescriptorHandleForHeapStart();
    handleCPU.ptr += static_cast<SIZE_T>(descriptorSize) * index;
    return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE
GetGPUDescriptorHandle(
    ID3D12DescriptorHeap* descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index) {
    D3D12_GPU_DESCRIPTOR_HANDLE handleGPU =
        descriptorHeap->GetGPUDescriptorHandleForHeapStart();
    handleGPU.ptr += static_cast<UINT64>(descriptorSize) * index;
    return handleGPU;
}
