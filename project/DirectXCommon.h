#pragma once
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>

// DirectX基盤クラス
class DirectXCommon {
public:
    // デバイス周り初期化
    void Initialize();

    // Getter ― main から使う用
    Microsoft::WRL::ComPtr<ID3D12Device>   GetDevice()     const { return device_; }
    Microsoft::WRL::ComPtr<IDXGIFactory7>  GetDxgiFactory() const { return dxgiFactory_; }

private:
    // DXGIファクトリ
    Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;
    // D3D12デバイス
    Microsoft::WRL::ComPtr<ID3D12Device>  device_;
};


// ===============================================
// ここから下：DirectX基盤まわりのユーティリティ関数
// （main から呼ぶユーティリティ）
// ===============================================

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>
CreateDescriptorHeap(
    Microsoft::WRL::ComPtr<ID3D12Device> device,
    D3D12_DESCRIPTOR_HEAP_TYPE heapType,
    UINT numDescriptors,
    bool shaderVisible);

D3D12_CPU_DESCRIPTOR_HANDLE
GetCPUDescriptorHandle(
    ID3D12DescriptorHeap* descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index);

D3D12_GPU_DESCRIPTOR_HANDLE
GetGPUDescriptorHandle(
    ID3D12DescriptorHeap* descriptorHeap,
    uint32_t descriptorSize,
    uint32_t index);
