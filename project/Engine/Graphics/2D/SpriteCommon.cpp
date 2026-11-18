#include "SpriteCommon.h"
#include <cassert>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

// 2Dスプライトの共有描画設定を準備し、初期値と必要なリソースを設定する。
void SpriteCommon::Initialize(DirectXCommon *dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;

    // ここで TextureManager の初期化などを行うなら記述

    CreateRootSignature();
    CreateGraphicsPipeline();
}

// 2Dスプライトの共有描画設定に使うパイプラインと参照先をコマンドリストへ設定する。
void SpriteCommon::PreDraw() {
    assert(dxCommon_);
    auto commandList = dxCommon_->GetCommandList();
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// シェーダーへ渡す定数・テクスチャの配置を定義する。
void SpriteCommon::CreateRootSignature() {
    HRESULT hr = S_FALSE;

    // ルートパラメータ配列 (3つ)
    D3D12_ROOT_PARAMETER rootParams[3] = {};

    // 0. Material (Pixel Shader の register b0)
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].Descriptor.ShaderRegister = 0; // b0
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 1. Transformation (Vertex Shader の register b0)
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].Descriptor.ShaderRegister = 0; // b0
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // 2. Texture (Pixel Shader の register t0)
    D3D12_DESCRIPTOR_RANGE descriptorRange = {};
    descriptorRange.BaseShaderRegister = 0; // t0
    descriptorRange.NumDescriptors = 1;
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].DescriptorTable.pDescriptorRanges = &descriptorRange;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // サンプラー (Pixel Shader の register s0)
    D3D12_STATIC_SAMPLER_DESC staticSampler = {};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // リニア補間
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0; // s0
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ルートシグネチャ生成
    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature = {};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    descriptionRootSignature.pParameters = rootParams;
    descriptionRootSignature.NumParameters = _countof(rootParams);
    descriptionRootSignature.pStaticSamplers = &staticSampler;
    descriptionRootSignature.NumStaticSamplers = 1;

    // シリアライズと作成
    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob,
                                     &errorBlob);
    if (FAILED(hr)) {
        OutputDebugStringA(reinterpret_cast<char *>(errorBlob->GetBufferPointer()));
        assert(false);
    }
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(),
                                                     signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

// 頂点形式・シェーダー・深度・合成方法をまとめた描画設定を作成する。
void SpriteCommon::CreateGraphicsPipeline() {
    auto device = dxCommon_->GetDevice();
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Sprite.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Sprite.PS.hlsl", L"ps_6_0");
    assert(vsBlob && psBlob);

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        // 座標は float4 なので 4 * 4 = 16バイト
        {"POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        // そのため、次の要素はオフセット 16 から始まる必要があります
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = {inputElementDescs, 2};
    psoDesc.VS = {vsBlob->GetBufferPointer(), vsBlob->GetBufferSize()};
    psoDesc.PS = {psBlob->GetBufferPointer(), psBlob->GetBufferSize()};
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
    psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;

    device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
}
// 小さいGPU割当を一ページにまとめる。領域を再利用せず、GPU参照中の上書きを防ぐ。
SpriteCommon::SpriteStorage SpriteCommon::AllocateSpriteStorage() {
    constexpr size_t kPageBytes = 64 * 1024;
    constexpr size_t kSpriteBytes = 3 * D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    if (spritePages_.empty() || spritePages_.back().used + kSpriteBytes > kPageBytes) {
        UploadPage page;
        D3D12_HEAP_PROPERTIES heap{};
        heap.Type = D3D12_HEAP_TYPE_UPLOAD;
        D3D12_RESOURCE_DESC desc{};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = kPageBytes;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        const auto created = dxCommon_->GetDevice()->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc,
                                                                             D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
                                                                             IID_PPV_ARGS(&page.resource));
        if (FAILED(created)) {
            throw std::runtime_error("Cannot allocate sprite upload page");
        }
        const D3D12_RANGE noRead{0, 0};
        if (FAILED(page.resource->Map(0, &noRead, reinterpret_cast<void **>(&page.cpu)))) {
            throw std::runtime_error("Cannot map sprite upload page");
        }
        spritePages_.push_back(std::move(page));
    }
    auto &page = spritePages_.back();
    const size_t offset = page.used;
    page.used += kSpriteBytes;
    return {page.resource, page.cpu + offset, page.resource->GetGPUVirtualAddress() + offset};
}
