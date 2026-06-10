#include "Object3dCommon.h"
#include "Logger.h" // 繝ｭ繧ｰ逕ｨ
#include <cassert>

using namespace Microsoft::WRL;

// 繝ｫ繝ｼ繝医す繧ｰ繝阪メ繝｣縺ｨ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医・菴懈・縺ｫ螟ｱ謨励＠縺ｦ縺・ｋ蜿ｯ閭ｽ諤ｧ縺後≠繧九◆繧√・
// Initialize髢｢謨ｰ縺ｫ繝ｭ繧ｰ縺ｨ繧｢繧ｵ繝ｼ繧ｷ繝ｧ繝ｳ繧定ｿｽ蜉縺励※縺ｿ縺ｾ縺吶・
void Object3dCommon::Initialize(DirectXCommon* dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;

    // 繝ｭ繧ｰ繧貞・縺励※騾ｲ陦檎憾豕√ｒ遒ｺ隱・
    OutputDebugStringA("Object3dCommon::Initialize Start\n");

    CreateRootSignature();
    // 繝ｫ繝ｼ繝医す繧ｰ繝阪メ繝｣縺後〒縺阪※縺・ｋ縺九メ繧ｧ繝・け
    if (rootSignature_) {
        OutputDebugStringA("CreateRootSignature: OK\n");
    } else {
        OutputDebugStringA("CreateRootSignature: FAILED (RootSignature is NULL)\n");
        assert(false && "RootSignature creation failed!");
    }

    CreateGraphicsPipeline();
    // 繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医′縺ｧ縺阪※縺・ｋ縺九メ繧ｧ繝・け
    if (pipelineState_) {
        OutputDebugStringA("CreateGraphicsPipeline: OK\n");
    } else {
        OutputDebugStringA("CreateGraphicsPipeline: FAILED (PipelineState is NULL)\n");
        // 縺薙％縺ｧ關ｽ縺｡繧句庄閭ｽ諤ｧ縺碁ｫ倥＞
    }

   

    CreatePlayerHighlightPipeline();
    CreateSkinnedPipeline(); // 霑ｽ蜉5/7菴仙・

    CreateLightBuffer();
    SetDefaultLight();

	// 蠖ｱ逕ｨ縺ｮ繝ｫ繝ｼ繝医す繧ｰ繝阪メ繝｣縺ｨ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医ｂ菴懈・
    CreateShadowPipeline();

    // 繧､繝ｳ繧ｹ繧ｿ繝ｳ繧ｷ繝ｳ繧ｰ逕ｨ
    CreateInstancedRootSignature();
    CreateInstancedGraphicsPipeline();
    CreateInstancedShadowPipeline();

    //蜊企乗・繝代う繝励Λ繧､繝ｳ
    CreateInstancedAlphaPipeline();

    OutputDebugStringA("Object3dCommon::Initialize Finish\n");
}

// 謠冗判蜑阪・蜈ｱ騾夊ｨｭ螳・
void Object3dCommon::PreDraw() {
    auto commandList = dxCommon_->GetCommandList();

    // 縺薙％縺ｧ關ｽ縺｡繧句ｴ蜷医！nitialize縺ｧ菴懈・螟ｱ謨励＠縺ｦ縺・ｋ
    if (!rootSignature_ || !pipelineState_) {
        assert(false && "RootSignature or PipelineState is NULL in PreDraw!");
        return;
    }

    if (textureManager_) {
        ID3D12DescriptorHeap* heaps[] = { textureManager_->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, heaps);
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

    if (textureManager_) {
        ID3D12DescriptorHeap* heaps[] = { textureManager_->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, heaps);
    }

    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->SetPipelineState(playerHighlightPipelineState_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

// 繝ｩ繧､繝医・蛻晄悄蛟､繧定ｨｭ螳壹☆繧矩未謨ｰ
void Object3dCommon::SetDefaultLight() {
    if (lightData_) {
        lightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
        lightData_->direction = { 0.0f, -1.0f, 0.0f };
        lightData_->intensity = 1.0f;
    lightData_->pointLightIntensity = 0.0f;
        lightData_->cameraPosition = { 0.0f, 0.0f, -10.0f };
        lightData_->paddingLight = 0.0f;
    }
}

// 繝ｫ繝ｼ繝医す繧ｰ繝阪メ繝｣縺ｮ菴懈・髢｢謨ｰ
void Object3dCommon::CreateRootSignature() {
    OutputDebugStringA("CreateRootSignature Start\n");

    D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
    descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    // 0. マテリアル (PS b0)
    // 1. Transform (トランスフォーム)ationMatrix (VS b0)
    // 2: DirectionalLight (PS b1)
    // 3. Texture (テクスチャ) (PS t0)
    // 4. ShadowMap (シャドウマップ) (PS t1)
    // 5: InstanceBuffer (VS t2) - SRV (StructuredBuffer 逕ｨ)
    // 6: EnvironmentMap (PS t2)
    D3D12_ROOT_PARAMETER rootParameters[7] = {};

    // 0. マテリアル
    rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[0].Descriptor.ShaderRegister = 0;
    rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 1. Transform (トランスフォーム)
    rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].Descriptor.ShaderRegister = 0;
    rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    // 2. Light (平行光源)
    rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[2].Descriptor.ShaderRegister = 1;
    rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    // 3. Texture (テクスチャ)
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
    shadowRange[0].BaseShaderRegister = 1; // t1 繝ｬ繧ｸ繧ｹ繧ｿ繧剃ｽｿ逕ｨ
    shadowRange[0].NumDescriptors = 1;
    shadowRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    shadowRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[4].DescriptorTable.pDescriptorRanges = shadowRange;
    rootParameters[4].DescriptorTable.NumDescriptorRanges = _countof(shadowRange);
    rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // 繝斐け繧ｻ繝ｫ繧ｷ繧ｧ繝ｼ繝繝ｼ縺ｧ蠖ｱ蛻､螳壹☆繧九◆繧・

    // 5. InstanceBuffer (VS t2)
    rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    rootParameters[5].Descriptor.ShaderRegister = 2;
    rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_DESCRIPTOR_RANGE environmentRange[1] = {};
    environmentRange[0].BaseShaderRegister = 2;
    environmentRange[0].NumDescriptors = 1;
    environmentRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    environmentRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[6].DescriptorTable.pDescriptorRanges = environmentRange;
    rootParameters[6].DescriptorTable.NumDescriptorRanges = _countof(environmentRange);
    rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

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

    // 繧ｷ繝ｪ繧｢繝ｩ繧､繧ｺ
    HRESULT hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        OutputDebugStringA("D3D12SerializeRootSignature Failed:\n");
        if (errorBlob) OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        assert(false);
    }

    // 菴懈・
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    if (FAILED(hr)) {
        OutputDebugStringA("CreateRootSignature Failed\n");
        assert(false);
    }
}

// 繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医・菴懈・髢｢謨ｰ
void Object3dCommon::CreateSkinnedPipeline() {
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/SkinnedObject.VS.hlsl", L"vs_6_0");
    assert(vsBlob);
    auto psBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Object3d.PS.hlsl", L"ps_6_0");
    assert(psBlob);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    D3D12_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState = blendDesc;

    D3D12_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
    rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState = rasterizerDesc;

    psoDesc.DepthStencilState.DepthEnable = true;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "WEIGHT",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "INDEX",    0, DXGI_FORMAT_R32G32B32A32_SINT,  1, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    psoDesc.InputLayout.pInputElementDescs = inputElementDescs;
    psoDesc.InputLayout.NumElements = _countof(inputElementDescs);

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&skinnedPipelineState_));
    assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateGraphicsPipeline() {
    OutputDebugStringA("CreateGraphicsPipeline Start\n");

    // Vertex Shader
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Object3d.VS.hlsl", L"vs_6_0");
    assert(vsBlob); // 縺薙％縺ｯ騾夐℃縺励※縺・ｋ縺ｯ縺・

    // Pixel Shader
    auto psBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Object3d.PS.hlsl", L"ps_6_0");
    assert(psBlob); // 縺薙％繧る夐℃縺励※縺・ｋ縺ｯ縺・

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get(); // 笘・％縺薙′NULL縺縺ｨPSO菴懈・縺ｫ螟ｱ謨励☆繧・
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
        // 繧ゅ＠縺薙％縺ｧ關ｽ縺｡繧九↑繧峨√Ν繝ｼ繝医す繧ｰ繝阪メ繝｣縺九す繧ｧ繝ｼ繝繝ｼblob縺後♀縺九＠縺・
        assert(false);
    } else {
        OutputDebugStringA("CreateGraphicsPipelineState Success!\n");
    }
}

// 蠖ｱ逕ｨ縺ｮ繝代う繝励Λ繧､繝ｳ繧ｹ繝・・繝医・菴懈・髢｢謨ｰ
void Object3dCommon::CreateShadowPipeline() {
    // 蠖ｱ蟆ら畑繧ｷ繧ｧ繝ｼ繝繝ｼ繧偵さ繝ｳ繝代う繝ｫ
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/ShadowMap.VS.hlsl", L"vs_6_0");
    // PS縺ｯ nullptr ・育ｩｺ・峨〒OK・∬牡縺御ｸ崎ｦ√↑縺溘ａ蜃ｦ逅・′鬮倬溘↓縺ｪ繧翫∪縺・

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();

    // 蜈･蜉帙Ξ繧､繧｢繧ｦ繝茨ｼ亥ｺｧ讓・POSITION 縺縺代〒蜊∝・縺ｧ縺呻ｼ・
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };

    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { nullptr, 0 }; // 笘・Pixel Shader縺ｯ菴ｿ繧上↑縺・

    // 繝ｩ繧ｹ繧ｿ繝ｩ繧､繧ｶ・壼ｽｱ繧堤｢ｺ螳溘↓蜃ｺ縺吶◆繧√き繝ｪ繝ｳ繧ｰ縺ｯ辟｡縺・NONE)縺悟ｮ牙ｮ壹＠縺ｾ縺・
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

    // 豺ｱ蠎ｦ險ｭ螳夲ｼ壽嶌縺崎ｾｼ縺ｿ繧呈怏蜉ｹ縺ｫ縺吶ｋ
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    // 笘・驥崎ｦ・ｼ壹Ξ繝ｳ繝繝ｼ繧ｿ繝ｼ繧ｲ繝・ヨ・郁牡・峨・ 0 蛟九↓縺吶ｋ
    psoDesc.NumRenderTargets = 0;

    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // 蠖ｱ逕ｨPSO縺ｮ逕滓・
    dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&shadowPipelineState_));
}

// 繝ｩ繧､繝育畑縺ｮ螳壽焚繝舌ャ繝輔ぃ繧剃ｽ懈・縺吶ｋ髢｢謨ｰ
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

    // 笘・㍾隕・ｼ壼｣∬ｶ翫＠縺ｫ隕九○繧九◆繧√∵ｷｱ蠎ｦ繝・せ繝医ｒOFF
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    // 蜊企乗・繧ゆｽｿ縺医ｋ繧医≧縺ｫ繝悶Ξ繝ｳ繝碓N
    auto& target = psoDesc.BlendState.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
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

void Object3dCommon::CreateInstancedAlphaPipeline()
{
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Object3dInstanced.VS.hlsl", L"vs_6_0");
    assert(vsBlob);

    auto psBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Object3dInstanced.PS.hlsl", L"ps_6_0");
    assert(psBlob);

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = instancedRootSignature_.Get();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.FrontCounterClockwise = TRUE;

    // 蜊企乗・逕ｨ
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

    auto& target = psoDesc.BlendState.RenderTarget[0];
    target.BlendEnable = TRUE;
    target.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    target.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
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
        IID_PPV_ARGS(&instancedAlphaPipelineState_)
    );
    assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateInstancedRootSignature() {
    instancedRootSignature_ = rootSignature_;
}

void Object3dCommon::CreateInstancedGraphicsPipeline() {
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Object3dInstanced.VS.hlsl", L"vs_6_0");
    assert(vsBlob);

    auto psBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Object3dInstanced.PS.hlsl", L"ps_6_0");
    assert(psBlob);

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = instancedRootSignature_.Get();
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

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

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&instancedPipelineState_));
    assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateInstancedShadowPipeline() {
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/ShadowMapInstanced.VS.hlsl", L"vs_6_0");
    assert(vsBlob);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = instancedRootSignature_.Get();

    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
    psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) };

    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { nullptr, 0 };

    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;

    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

    psoDesc.NumRenderTargets = 0;

    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&instancedShadowPipelineState_));
    assert(SUCCEEDED(hr));
}






