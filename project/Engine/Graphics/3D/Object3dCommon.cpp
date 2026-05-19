#include "Object3dCommon.h"
#include "Logger.h" // ログ用
#include <cassert>

using namespace Microsoft::WRL;

// ルートシグネチャとパイプラインステートの作成に失敗している可能性があるため、
// Initialize関数にログとアサーションを追加してみます。
void Object3dCommon::Initialize(DirectXCommon* dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;

    // ログを出して進行状況を確認
    OutputDebugStringA("Object3dCommon::Initialize Start\n");

    CreateRootSignature();
    // ルートシグネチャができているかチェック
    if (rootSignature_) {
        OutputDebugStringA("CreateRootSignature: OK\n");
    } else {
        OutputDebugStringA("CreateRootSignature: FAILED (RootSignature is NULL)\n");
        assert(false && "RootSignature creation failed!");
    }

    CreateGraphicsPipeline();
    // パイプラインステートができているかチェック
    if (pipelineState_) {
        OutputDebugStringA("CreateGraphicsPipeline: OK\n");
    } else {
        OutputDebugStringA("CreateGraphicsPipeline: FAILED (PipelineState is NULL)\n");
        // ここで落ちる可能性が高い
    }

    CreatePlayerHighlightPipeline(); // 追加5/7佐倉

    CreateLightBuffer();
    SetDefaultLight();

	// 影用のルートシグネチャとパイプラインステートも作成
    CreateShadowPipeline();

    OutputDebugStringA("Object3dCommon::Initialize Finish\n");
}

// 描画前の共通設定
void Object3dCommon::PreDraw() {
    auto commandList = dxCommon_->GetCommandList();

    // ここで落ちる場合、Initializeで作成失敗している
    if (!rootSignature_ || !pipelineState_) {
        assert(false && "RootSignature or PipelineState is NULL in PreDraw!");
        return;
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(pipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void Object3dCommon::PreDrawPlayerHighlight() {
    auto commandList = dxCommon_->GetCommandList();

    if (!rootSignature_ || !playerHighlightPipelineState_) {
        assert(false && "RootSignature or PlayerHighlightPipelineState is NULL!");
        return;
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(playerHighlightPipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// ライトの初期値を設定する関数
void Object3dCommon::SetDefaultLight() {
    if (lightData_) {
        lightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
        lightData_->direction = { 0.0f, -1.0f, 0.0f };
        lightData_->intensity = 1.0f;
        lightData_->cameraPosition = { 0.0f, 0.0f, -10.0f };
        lightData_->paddingLight = 0.0f;
    }
}

// ルートシグネチャの作成関数
void Object3dCommon::CreateRootSignature() {
    OutputDebugStringA("CreateRootSignature Start\n");

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // 0: Material (PS b0)
    // 1: TransformationMatrix (VS b0)
    // 2: DirectionalLight (PS b1)
    // 3: Texture (PS t0)
	// 4: Static Sampler (PS s0)
    D3D12_ROOT_PARAMETER rootParameters[5] = {};

    // 0. Material
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 1. Transform
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // 2. Light
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].Descriptor.ShaderRegister = 1;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 3. Texture
    D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
    descriptorRange[0].BaseShaderRegister = 0;
    descriptorRange[0].NumDescriptors = 1;
    descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[3].DescriptorTable.pDescriptorRanges = descriptorRange;
    rootParameters[3].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);
    rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // ShadowMap
    D3D12_DESCRIPTOR_RANGE shadowRange[1] = {};
    shadowRange[0].BaseShaderRegister = 1; // t1 レジスタを使用
    shadowRange[0].NumDescriptors = 1;
    shadowRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    shadowRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].DescriptorTable.pDescriptorRanges = shadowRange;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = _countof(shadowRange);
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで影判定するため

    // Sampler
    D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
    staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
    staticSamplers[0].ShaderRegister = 0;
    staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    descriptionRootSignature.pParameters = rootParameters;
    descriptionRootSignature.NumParameters = _countof(rootParameters);
    descriptionRootSignature.pStaticSamplers = staticSamplers;
    descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

    ID3DBlob* signatureBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;

    // シリアライズ
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        OutputDebugStringA("D3D12SerializeRootSignature Failed:\n");
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        assert(false);
    }

    // 作成
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr)) {
        OutputDebugStringA("CreateRootSignature Failed\n");
        assert(false);
    }
}

// パイプラインステートの作成関数
void Object3dCommon::CreateGraphicsPipeline() {
    OutputDebugStringA("CreateGraphicsPipeline Start\n");

    // Vertex Shader
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Object3d.VS.hlsl", L"vs_6_0");
    assert(vsBlob); // ここは通過しているはず

    // Pixel Shader
    auto psBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Object3d.PS.hlsl", L"ps_6_0");
    assert(psBlob); // ここも通過しているはず

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get(); // ★ここがNULLだとPSO作成に失敗する
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
    //psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;

    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    auto& target = psoDesc.BlendState.RenderTarget[0];
    target.BlendEnable = FALSE;
    target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    OutputDebugStringA("Calling CreateGraphicsPipelineState...\n");

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));

    if (FAILED(hr)) {
        OutputDebugStringA("CreateGraphicsPipelineState Failed!! HRESULT error.\n");
        // もしここで落ちるなら、ルートシグネチャかシェーダーblobがおかしい
        assert(false);
    } else {
        OutputDebugStringA("CreateGraphicsPipelineState Success!\n");
    }
}

// 影用のパイプラインステートの作成関数
void Object3dCommon::CreateShadowPipeline() {
    // 影専用シェーダーをコンパイル
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/ShadowMap.VS.hlsl", L"vs_6_0");
    // PSは nullptr （空）でOK！色が不要なため処理が高速になります

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();

    // 入力レイアウト（座標 POSITION だけで十分です）
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };

    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { nullptr, 0 }; // ★ Pixel Shaderは使わない

    // ラスタライザ：影を確実に出すためカリングは無し(NONE)が安定します
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

    // 深度設定：書き込みを有効にする
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    // ★ 重要：レンダーターゲット（色）は 0 個にする
    psoDesc.NumRenderTargets = 0;

    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // 影用PSOの生成
    dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&shadowPipelineState_));
}

// ライト用の定数バッファを作成する関数
void Object3dCommon::CreateLightBuffer() {
    auto device = dxCommon_->GetDevice();
    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = (sizeof(DirectionalLight) + 0xff) & ~0xff;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&lightResource_));
    lightResource_->Map(0, nullptr, (void**)&lightData_);
}

void Object3dCommon::CreatePlayerHighlightPipeline() {
    OutputDebugStringA("CreatePlayerHighlightPipeline Start\n");

    auto vsBlob = dxCommon_->CompileShader(
        L"Resources/shaders/hlsl/Object3d.VS.hlsl",
        L"vs_6_0"
    );
    assert(vsBlob);

    auto psBlob = dxCommon_->CompileShader(
        L"Resources/shaders/hlsl/Object3d.PS.hlsl",
        L"ps_6_0"
    );
    assert(psBlob);

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;

    // ★重要：壁越しに見せるため、深度テストをOFF
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 半透明も使えるようにブレンドON
    auto& target = psoDesc.BlendState.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend = D3D12_BLEND_ONE;
    target.BlendOp = D3D12_BLEND_OP_ADD;
    target.SrcBlendAlpha = D3D12_BLEND_ONE;
    target.DestBlendAlpha = D3D12_BLEND_ZERO;
    target.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    target.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(
        &psoDesc,
        IID_PPV_ARGS(&playerHighlightPipelineState_)
    );

    if (FAILED(hr)) {
        OutputDebugStringA("CreatePlayerHighlightPipeline Failed!!\n");
        assert(false);
    } else {
        OutputDebugStringA("CreatePlayerHighlightPipeline Success!\n");
    }
}

