#pragma once
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <dxcapi.h>
#include <cstdint>
#include <string>
#include <chrono>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxcompiler.lib")

class WinApp;

class DirectXCommon {
public: // サブクラス定義
    template <class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

public: // メンバ関数
    DirectXCommon() = default;
    ~DirectXCommon() = default;

    // 初期化
    void Initialize(WinApp* winApp);

    // 描画前処理
    void PreDraw();
    // 描画後処理
    void PostDraw();

    // シェーダーコンパイル
    ComPtr<IDxcBlob> CompileShader(
        const std::wstring& filePath,
        const wchar_t* profile);

    // ゲッター
    ID3D12Device* GetDevice() const { return device_.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() const { return commandList_.Get(); }
    size_t GetBackBufferCount() const { return 2; }

private: // メンバ関数(内部処理)
    void InitializeDevice();
    void InitializeCommand();
    void InitializeSwapChain();
    void InitializeRenderTargetView();
    void InitializeDepthStencilView();
    void InitializeFence();
    void InitializeDXC();
    void InitializeFixFPS();
    void UpdateFixFPS();

private: // メンバ変数
    WinApp* winApp_ = nullptr;

    // DirectX主要オブジェクト
    ComPtr<IDXGIFactory7> dxgiFactory_;
    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> commandQueue_;
    ComPtr<ID3D12CommandAllocator> commandAllocator_;
    ComPtr<ID3D12GraphicsCommandList> commandList_;
    ComPtr<IDXGISwapChain4> swapChain_;

    // RTV (レンダーターゲットビュー)
    ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;
    ComPtr<ID3D12Resource> swapChainResources_[2];

    // DSV (深度ステンシルビュー)
    ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;
    ComPtr<ID3D12Resource> depthStencilResource_;

    // フェンス (同期用)
    ComPtr<ID3D12Fence> fence_;
    uint64_t fenceValue_ = 0;
    HANDLE fenceEvent_ = nullptr;

    // DXC (シェーダーコンパイラ)
    ComPtr<IDxcUtils> dxcUtils_;
    ComPtr<IDxcCompiler3> dxcCompiler_;
    ComPtr<IDxcIncludeHandler> includeHandler_;

    // FPS制御
    std::chrono::steady_clock::time_point reference_;
};