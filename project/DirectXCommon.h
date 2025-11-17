#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxcapi.h>
#include <cstdint>
#include <iostream>
#include <string>
#include <sstream>
#include <format>

class WinApp;

//=== グローバルヘルパー関数の宣言 ===
namespace DirectX { struct TexMetadata; };

class DirectXCommon {
public:
	DirectXCommon();
	~DirectXCommon();

	//シェーダーのコンパイル
	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader(
	const std::wstring& filepath,
	const wchar_t* profile,
	std::ostream& os);

	// ★ main.cpp で作った各種 ComPtr をここに渡す形にする
	void Initialize(
		WinApp* winApp,
		Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory,
		Microsoft::WRL::ComPtr<ID3D12Device> device,
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue,
		Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator,
		Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList,
		Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain,
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap);

	
	void PreDraw();
	void PostDraw();

	// 必要な getter
	Microsoft::WRL::ComPtr<IDXGIFactory7> GetDxgiFactory() const { return dxgiFactory_; }
	Microsoft::WRL::ComPtr<ID3D12Device> GetDevice() const { return device_; }
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> GetCommandList() const { return commandList_; }
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetCommandQueue() const { return commandQueue_; }
	Microsoft::WRL::ComPtr<IDXGISwapChain4> GetSwapChain() const { return swapChain_; }
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetSRVDescriptorHeap() const { return srvDescriptorHeap_; }


	const D3D12_VIEWPORT& GetViewport() const { return viewport_; }
	const D3D12_RECT& GetScissorRect() const { return scissorRect_; }
	D3D12_CPU_DESCRIPTOR_HANDLE* GetRtvHandles() { return rtvHandles_; }

private:
	using ComPtrFactory = Microsoft::WRL::ComPtr<IDXGIFactory7>;
	using ComPtrDevice = Microsoft::WRL::ComPtr<ID3D12Device>;
	using ComPtrQueue = Microsoft::WRL::ComPtr<ID3D12CommandQueue>;
	using ComPtrAllocator = Microsoft::WRL::ComPtr<ID3D12CommandAllocator>;
	using ComPtrCmdList = Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>;
	using ComPtrSwapChain = Microsoft::WRL::ComPtr<IDXGISwapChain4>;
	using ComPtrHeap = Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>;
	using ComPtrResource = Microsoft::WRL::ComPtr<ID3D12Resource>;
	using ComPtrFence = Microsoft::WRL::ComPtr<ID3D12Fence>;

	WinApp* winApp_ = nullptr;

	ComPtrFactory   dxgiFactory_ = nullptr;
	ComPtrDevice    device_ = nullptr;
	ComPtrQueue     commandQueue_ = nullptr;
	ComPtrAllocator commandAllocator_ = nullptr;
	ComPtrCmdList   commandList_ = nullptr;
	ComPtrSwapChain swapChain_ = nullptr;
	ComPtrHeap      srvDescriptorHeap_ = nullptr;

	// RTV / DSV 関連
	ComPtrHeap      rtvDescriptorHeap_ = nullptr;
	ComPtrHeap      dsvDescriptorHeap_ = nullptr;
	ComPtrResource  swapChainResources_[2]{};
	ComPtrResource  depthStencilResource_ = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[2]{};

	// ビューポート / シザー
	D3D12_VIEWPORT viewport_{};
	D3D12_RECT     scissorRect_{};

	// フェンス
	ComPtrFence fence_ = nullptr;
	uint64_t    fenceVal_ = 0;
	HANDLE      fenceEvent_ = nullptr;

	//getter
	ID3D12Device* GetDevicePtr() const { return device_.Get(); }
	ID3D12GraphicsCommandList* GetCommandListPtr() const { return commandList_.Get(); }

	// DXC 関連
	Microsoft::WRL::ComPtr<IDxcUtils>       dxcUtils_;
	Microsoft::WRL::ComPtr<IDxcCompiler3>   dxcCompiler_;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_;

};

//=== グローバルヘルパー関数の宣言 ===
Microsoft::WRL::ComPtr<ID3D12Resource>
CreateBufferResource(ID3D12Device* device, size_t sizeInBytes);

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
CreateDescriptorHeap(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	UINT numDescriptors,
	bool shaderVisivle);

namespace DirectX { struct TexMetadata; }  // これがまだ無ければ追加

Microsoft::WRL::ComPtr<ID3D12Resource>
CreateTextureResource(
	Microsoft::WRL::ComPtr<ID3D12Device> device,
	const DirectX::TexMetadata& metadata);
