#pragma once
#include "DirectXCommon.h"
#include "TextureManager.h"
#include <d3d12.h>
#include <wrl.h>

class ShadowMap {
public:
    // 影用テクスチャのサイズ
    static const int kWidth = 2048;
    static const int kHeight = 2048;

    void Initialize(DirectXCommon* dxCommon, TextureManager* textureManager);

    // ★ 修正：引数 (ID3D12GraphicsCommandList* commandList) を追加
    void PreDraw(ID3D12GraphicsCommandList* commandList);
    void PostDraw(ID3D12GraphicsCommandList* commandList);

    // ゲッター
    ID3D12Resource* GetResource() const { return resource_.Get(); }
    D3D12_CPU_DESCRIPTOR_HANDLE GetDsvHandle() const { return dsvHandle_; }
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandle() const { return srvHandle_; }
    //ID3D12DescriptorHeap* GetSrvHeap() const { return srvHeap_.Get(); }

private:
    Microsoft::WRL::ComPtr<ID3D12Resource> resource_;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_; // 描き込み用 (DSV)
    D3D12_GPU_DESCRIPTOR_HANDLE srvHandle_; // 読み込み用 (SRV)

    // 影専用のディスクリプタヒープ（棚）を追加
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap_; // DSV用（書き込み）
    //Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_; // SRV用（読み取り）
};