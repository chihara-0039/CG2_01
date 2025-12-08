#include "SpriteCommon.h"
#include "DirectXCommon.h"

#include <cassert>
#include <sstream>

using Microsoft::WRL::ComPtr;


void SpriteCommon::Initialize(DirectXCommon* dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;

    CreateRootSignature();
    CreateGraphicsPipeline();
}

void SpriteCommon::PreDraw() {
    auto commandList = dxCommon_->GetCommandList();
    ID3D12GraphicsCommandList* list = commandList.Get();

    // ルートシグネチャ & PSO 設定
    list->SetGraphicsRootSignature(rootSignature_.Get());
    list->SetPipelineState(pipelineState_.Get());

    // スプライトは TRIANGLESTRIP で描画
    list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
}

void SpriteCommon::CreateGraphicsPipeline() {
    auto device = dxCommon_->GetDevice();
    assert(device);

    std::ostringstream log; // シェーダーコンパイル用ログ

    // ★ スプライト用 HLSL のパスは君の環境に合わせて
    //   いまは例として Sprite.VS.hlsl / Sprite.PS.hlsl としておく
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob =
        dxCommon_->CompileShader(
            L"Resources/shaders/hlsl/Sprite.VS.hlsl",
            L"vs_6_0",
            log);
    Microsoft::WRL::ComPtr<IDxcBlob> psBlob =
        dxCommon_->CompileShader(
            L"Resources/shaders/hlsl/Sprite.PS.hlsl",
            L"ps_6_0",
            log);

    assert(vsBlob && psBlob); // ここで落ちるようならパスが間違っている

    // 入力レイアウト（例：pos(float3), uv(float2)）
    D3D12_INPUT_ELEMENT_DESC inputElements[2]{};

    inputElements[0].SemanticName = "POSITION";
    inputElements[0].SemanticIndex = 0;
    inputElements[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
    inputElements[0].InputSlot = 0;
    inputElements[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElements[0].InstanceDataStepRate = 0;

    inputElements[1].SemanticName = "TEXCOORD";
    inputElements[1].SemanticIndex = 0;
    inputElements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    inputElements[1].InputSlot = 0;
    inputElements[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
    inputElements[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
    inputElements[1].InstanceDataStepRate = 0;

    D3D12_INPUT_LAYOUT_DESC inputLayout{};
    inputLayout.pInputElementDescs = inputElements;
    inputLayout.NumElements = _countof(inputElements);

    // ブレンド（アルファブレンド）
    D3D12_BLEND_DESC blendDesc{};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    // ラスタライザ
    D3D12_RASTERIZER_DESC rasterDesc{};
    rasterDesc.FillMode = D3D12_FILL_MODE_SOLID;
    rasterDesc.CullMode = D3D12_CULL_MODE_NONE;
    rasterDesc.DepthClipEnable = TRUE;

    // 深度ステンシル（スプライトなので深度は無効）
    D3D12_DEPTH_STENCIL_DESC depthDesc{};
    depthDesc.DepthEnable = FALSE;
    depthDesc.StencilEnable = FALSE;

    // PSO 記述
    D3D12_GRAPHICS_PIPELINE_STATE_DESC gpsDesc{};
    gpsDesc.pRootSignature = rootSignature_.Get();
    gpsDesc.InputLayout = inputLayout;
    gpsDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    gpsDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    gpsDesc.BlendState = blendDesc;
    gpsDesc.RasterizerState = rasterDesc;
    gpsDesc.DepthStencilState = depthDesc;
    gpsDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    gpsDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    gpsDesc.NumRenderTargets = 1;
    gpsDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    gpsDesc.SampleDesc.Count = 1;
    gpsDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    HRESULT hr = device->CreateGraphicsPipelineState(
        &gpsDesc, IID_PPV_ARGS(&pipelineState_));
    assert(SUCCEEDED(hr));
}


void SpriteCommon::CreateRootSignature() {
    ID3D12Device* device = dxCommon_->GetDevice().Get();
    assert(device);

    // ルートシグネチャ記述
    D3D12_ROOT_SIGNATURE_DESC desc{};
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // ★ サンプラ
    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0;
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ★ RootParameter
    D3D12_ROOT_PARAMETER rootParams[4]{};

    // 0: PixelShader 用 MaterialCBV（例）
    rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[0].Descriptor.ShaderRegister = 0;

    // 1: VertexShader 用 TransformCBV（例）
    rootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    rootParams[1].Descriptor.ShaderRegister = 0;

    // 2: テクスチャ SRV 用 DescriptorTable
    D3D12_DESCRIPTOR_RANGE range{};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.BaseShaderRegister = 0;
    range.NumDescriptors = 1;
    range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[2].DescriptorTable.pDescriptorRanges = &range;
    rootParams[2].DescriptorTable.NumDescriptorRanges = 1;

    // 3: PixelShader 用 その他CBV（例：色など）
    rootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParams[3].Descriptor.ShaderRegister = 1;

    desc.pParameters = rootParams;
    desc.NumParameters = _countof(rootParams);
    desc.pStaticSamplers = &staticSampler;
    desc.NumStaticSamplers = 1;

    // シリアライズ & 生成
    ComPtr<ID3DBlob> rsBlob;
    ComPtr<ID3DBlob> errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1,
        &rsBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        assert(false);
    }

    hr = device->CreateRootSignature(
        0,
        rsBlob->GetBufferPointer(),
        rsBlob->GetBufferSize(),
        IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}
