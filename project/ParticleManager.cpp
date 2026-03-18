#include "ParticleManager.h"
#include <cassert>
#include <random>

using namespace Microsoft::WRL;

// 乱数生成器
static std::random_device seed_gen;
static std::mt19937_64 engine(seed_gen());

void ParticleManager::Initialize(DirectXCommon* dxCommon, TextureManager* textureManager) {
    assert(dxCommon);
    dxCommon_ = dxCommon;
    textureManager_ = textureManager;

    // 1. テクスチャ読み込み
    textureHandle_ = textureManager_->LoadTexture("Resources/uvChecker.png");

    // 2. パイプライン生成
    CreateRootSignature();
    CreatePipelineState();

    // 3. メッシュ生成
    CreateMesh();

    // 4. インスタンシング用バッファ生成
    {
        auto device = dxCommon_->GetDevice();
        UINT size = sizeof(InstanceData) * kMaxParticles;

        D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
        D3D12_RESOURCE_DESC resDesc = {};
        resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        resDesc.Width = size;
        resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
        resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

        HRESULT hr = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&instancingBuffer_));
        assert(SUCCEEDED(hr));

        instancingBuffer_->Map(0, nullptr, (void**)&instancingDataMapped_);

        instancingBufferView_.BufferLocation = instancingBuffer_->GetGPUVirtualAddress();
        instancingBufferView_.SizeInBytes = size;
        instancingBufferView_.StrideInBytes = sizeof(InstanceData);
    }
}

void ParticleManager::Update(const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix) {
    // 1. パーティクル更新
    for (auto it = particles_.begin(); it != particles_.end();) {
        it->lifeTime += 1.0f / 60.0f;
        if (it->lifeTime >= it->maxTime) {
            it = particles_.erase(it);
            continue;
        }
        it->transform.translate.x += it->velocity.x;
        it->transform.translate.y += it->velocity.y;
        it->transform.translate.z += it->velocity.z;
        float alpha = 1.0f - (it->lifeTime / it->maxTime);
        it->color.w = alpha;
        ++it;
    }

    // 2. データ書き込み
    uint32_t index = 0;
    Matrix4x4 cameraMatrix = Math::Inverse(viewMatrix);
    Matrix4x4 billboardMat = Math::MakeIdentity4x4();
    billboardMat.m[0][0] = cameraMatrix.m[0][0]; billboardMat.m[0][1] = cameraMatrix.m[0][1]; billboardMat.m[0][2] = cameraMatrix.m[0][2];
    billboardMat.m[1][0] = cameraMatrix.m[1][0]; billboardMat.m[1][1] = cameraMatrix.m[1][1]; billboardMat.m[1][2] = cameraMatrix.m[1][2];
    billboardMat.m[2][0] = cameraMatrix.m[2][0]; billboardMat.m[2][1] = cameraMatrix.m[2][1]; billboardMat.m[2][2] = cameraMatrix.m[2][2];

    for (const auto& particle : particles_) {
        if (index >= kMaxParticles) break;

        Matrix4x4 scaleMat = Math::Matrix4x4MakeScaleMatrix(particle.transform.scale);
        Matrix4x4 transMat = Math::MakeTranslateMatrix(particle.transform.translate);
        Matrix4x4 worldMat = Math::Multiply(scaleMat, Math::Multiply(billboardMat, transMat));
        Matrix4x4 wvp = Math::Multiply(worldMat, Math::Multiply(viewMatrix, projectionMatrix));

        instancingDataMapped_[index].WVP = wvp;
        instancingDataMapped_[index].color = particle.color;
        index++;
    }
}

void ParticleManager::Draw() {
    if (particles_.empty()) return;

    auto commandList = dxCommon_->GetCommandList();

    // ★ここがNULLだと落ちる。CreatePipelineStateが失敗しているとここでエラーになる
    assert(pipelineState_ != nullptr && "PipelineState not created!");

    commandList->SetPipelineState(pipelineState_.Get());
    commandList->SetGraphicsRootSignature(rootSignature_.Get());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetVertexBuffers(1, 1, &instancingBufferView_);

    auto srvHandle = textureManager_->GetSrvHandleGPU(textureHandle_);
    commandList->SetGraphicsRootDescriptorTable(0, srvHandle);

    uint32_t count = (uint32_t)particles_.size();
    if (count > kMaxParticles) count = kMaxParticles;
    commandList->DrawInstanced(6, count, 0, 0);
}

void ParticleManager::Emit(const Vector3& pos, uint32_t count) {
    std::uniform_real_distribution<float> distVel(-0.1f, 0.1f);
    std::uniform_real_distribution<float> distColor(0.5f, 1.0f);
    std::uniform_real_distribution<float> distTime(1.0f, 3.0f);

    for (uint32_t i = 0; i < count; ++i) {
        if (particles_.size() >= kMaxParticles) return;
        Particle p;
        p.transform.scale = { 1.0f, 1.0f, 1.0f };
        p.transform.rotate = { 0.0f, 0.0f, 0.0f };
        p.transform.translate = pos;
        p.velocity = { distVel(engine), distVel(engine), distVel(engine) };
        p.color = { distColor(engine), distColor(engine), distColor(engine), 1.0f };
        p.lifeTime = 0.0f;
        p.maxTime = distTime(engine);
        particles_.push_back(p);
    }
}

void ParticleManager::CreateRootSignature() {
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER rootParam = {};
    rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParam.DescriptorTable.NumDescriptorRanges = 1;
    rootParam.DescriptorTable.pDescriptorRanges = &range;
    rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_STATIC_SAMPLER_DESC sampler = {};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
    sampler.ShaderRegister = 0;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 1;
    desc.pParameters = &rootParam;
    desc.NumStaticSamplers = 1;
    desc.pStaticSamplers = &sampler;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ComPtr<ID3DBlob> blob, errorBlob;
    HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &blob, &errorBlob);
    assert(SUCCEEDED(hr));
    hr = dxCommon_->GetDevice()->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rootSignature_));
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreatePipelineState() {
    // --- ★修正ポイント: InputLayoutをHLSLと完全に一致させる ---
    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        // Slot 0: メッシュデータ
        { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

        // Slot 1: インスタンスデータ (WVP行列を4つのfloat4に分割して定義)
        // HLSL側: INSTANCE_WVP0, 1, 2, 3 に対応
        { "INSTANCE_WVP", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0,  D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WVP", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WVP", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
        { "INSTANCE_WVP", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },

        // Color
        { "INSTANCE_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA, 1 },
    };

    // シェーダーコンパイル (パスにhlsl/を追加済み)
    auto vsBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Particle.VS.hlsl", L"vs_6_0");
    auto psBlob = dxCommon_->CompileShader(L"Resources/shaders/hlsl/Particle.PS.hlsl", L"ps_6_0");
    assert(vsBlob != nullptr && "VS Compile Failed");
    assert(psBlob != nullptr && "PS Compile Failed");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature_.Get();
    psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };

    // ブレンド設定 (加算合成)
    auto& blend = psoDesc.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_ONE;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_ZERO;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.DepthStencilState.DepthEnable = TRUE;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.SampleDesc.Count = 1;

    // ★重要: ここで失敗すると pipelineState_ がNULLになり、描画時に落ちる
    HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
    if (FAILED(hr)) {
        OutputDebugStringA("Failed to create GraphicsPipelineState for Particles!\n");
        assert(false);
    }
}

void ParticleManager::CreateMesh() {
    VertexData vertices[] = {
        {{-0.5f,  0.5f, 0, 1}, {0.0f, 0.0f}, {0, 0, -1}},
        {{ 0.5f,  0.5f, 0, 1}, {1.0f, 0.0f}, {0, 0, -1}},
        {{-0.5f, -0.5f, 0, 1}, {0.0f, 1.0f}, {0, 0, -1}},
        {{-0.5f, -0.5f, 0, 1}, {0.0f, 1.0f}, {0, 0, -1}},
        {{ 0.5f,  0.5f, 0, 1}, {1.0f, 0.0f}, {0, 0, -1}},
        {{ 0.5f, -0.5f, 0, 1}, {1.0f, 1.0f}, {0, 0, -1}},
    };

    UINT size = sizeof(vertices);

    D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = size;
    resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

    HRESULT hr = dxCommon_->GetDevice()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertexBuffer_));
    assert(SUCCEEDED(hr));

    VertexData* data = nullptr;
    vertexBuffer_->Map(0, nullptr, (void**)&data);
    memcpy(data, vertices, size);
    vertexBuffer_->Unmap(0, nullptr);

    vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = size;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);
}