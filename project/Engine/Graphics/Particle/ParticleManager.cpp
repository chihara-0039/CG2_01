#include "ParticleManager.h"
#include "StageMap.h"
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

    // 1. テクスチャ読み込み (デフォルト)
    textureHandle_ = textureManager_->LoadTexture("Resources/UI/inventory/white.png");

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

void ParticleManager::Update(float deltaTime, const Matrix4x4& viewMatrix, const Matrix4x4& projectionMatrix, const Vector3& playerPos, StageMap* stageMap) {
    // 0. 天候エミッターの処理
    if (weatherEmitter_.active && weatherEmitter_.emitRate > 0.0f) {
        weatherEmitter_.emitTimer += deltaTime;
        float emitInterval = 1.0f / weatherEmitter_.emitRate;
        
        while (weatherEmitter_.emitTimer >= emitInterval) {
            weatherEmitter_.emitTimer -= emitInterval;
            
            if (particles_.size() < kMaxParticles) {
                std::uniform_real_distribution<float> distX(-weatherEmitter_.size.x / 2.0f, weatherEmitter_.size.x / 2.0f);
                std::uniform_real_distribution<float> distY(-weatherEmitter_.size.y / 2.0f, weatherEmitter_.size.y / 2.0f);
                std::uniform_real_distribution<float> distZ(-weatherEmitter_.size.z / 2.0f, weatherEmitter_.size.z / 2.0f);
                
                std::uniform_real_distribution<float> randV(-1.0f, 1.0f);
                
                Particle p;
                p.transform.scale = weatherEmitter_.particleSize;
                p.transform.rotate = { 0.0f, 0.0f, 0.0f };
                
                // プレイヤーの周囲に発生させる
                p.transform.translate = {
                    playerPos.x + weatherEmitter_.center.x + distX(engine),
                    playerPos.y + weatherEmitter_.center.y + distY(engine),
                    playerPos.z + weatherEmitter_.center.z + distZ(engine)
                };
                
                p.velocity = {
                    weatherEmitter_.velocity.x + randV(engine) * weatherEmitter_.velocityRandom.x,
                    weatherEmitter_.velocity.y + randV(engine) * weatherEmitter_.velocityRandom.y,
                    weatherEmitter_.velocity.z + randV(engine) * weatherEmitter_.velocityRandom.z
                };
                
                p.type = Particle::Type::Fall;
                p.color = weatherEmitter_.color;
                p.lifeTime = 0.0f;
                p.maxTime = weatherEmitter_.particleLife;
                particles_.push_back(p);
            }
        }
    }

    // 1. パーティクル更新
    for (auto it = particles_.begin(); it != particles_.end();) {
        it->lifeTime += deltaTime;
        if (it->lifeTime >= it->maxTime) {
            it = particles_.erase(it);
            continue;
        }
        
        float oldY = it->transform.translate.y;
        
        it->transform.translate.x += it->velocity.x * deltaTime * 60.0f;
        it->transform.translate.y += it->velocity.y * deltaTime * 60.0f;
        it->transform.translate.z += it->velocity.z * deltaTime * 60.0f;
        
        // 当たり判定 (StageMapとの衝突判定)
        if (it->type == Particle::Type::Fall && stageMap) {
            int bx = (int)std::floor(it->transform.translate.x + 0.5f);
            int bz = (int)std::floor(it->transform.translate.z + 0.5f);
            int oldBy = (int)std::floor(oldY + 0.5f);
            int newBy = (int)std::floor(it->transform.translate.y + 0.5f);
            
            bool hit = false;
            int hitY = newBy;
            // 落下前の位置から落下後の位置までの間のセルを確認（すり抜け防止）
            for (int y = oldBy; y >= newBy; --y) {
                const MapCell* cell = stageMap->GetCell(bx, y, bz);
                if (cell != nullptr && cell->type != BlockType::None) {
                    hit = true;
                    hitY = y;
                    break;
                }
            }
            
            if (hit) {
                // ブロック上面(hitY + 0.5f)の少し上で飛沫を生成して元のパーティクルを消滅させる
                Vector3 splashPos = it->transform.translate;
                splashPos.y = (float)hitY + 0.6f;
                EmitSplash(splashPos, it->color);
                it = particles_.erase(it);
                continue;
            }
        }
        
        // フェードアウト
        float alpha = 1.0f - (it->lifeTime / it->maxTime);
        it->color.w = weatherEmitter_.color.w * alpha; // ベースアルファも考慮
        
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
        Matrix4x4 rotateMat = Math::MakeRotateZMatrix(particle.transform.rotate.z);
        Matrix4x4 transMat = Math::MakeTranslateMatrix(particle.transform.translate);
        Matrix4x4 worldMat = Math::Multiply(scaleMat, Math::Multiply(rotateMat, Math::Multiply(billboardMat, transMat)));
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

void ParticleManager::EmitSplash(const Vector3& pos, const Vector4& color) {
    constexpr int kSplashCount = 8;
    constexpr float kPi = 3.14159265f;

    std::uniform_real_distribution<float> distRotate(-kPi, kPi);
    std::uniform_real_distribution<float> distScale(0.4f, 1.5f);
    std::uniform_real_distribution<float> distOffset(-0.08f, 0.08f);
    std::uniform_real_distribution<float> distLife(0.18f, 0.28f);

    for (int i = 0; i < kSplashCount; ++i) {
        if (particles_.size() >= kMaxParticles) break;

        Particle p;
        p.type = Particle::Type::Splash;
        p.transform.translate = {
            pos.x + distOffset(engine),
            pos.y + 0.2f + distOffset(engine),
            pos.z + distOffset(engine)
        };
        p.transform.scale = { 0.05f, distScale(engine), 1.0f };
        p.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };

        p.velocity = { 0.0f, 0.0f, 0.0f };
        p.color = color;
        p.lifeTime = 0.0f;
        p.maxTime = distLife(engine);

        particles_.push_back(p);
    }
}

void ParticleManager::Emit(const Vector3& pos, uint32_t count) {
    std::uniform_real_distribution<float> distRotate(-3.14159265f, 3.14159265f);
    std::uniform_real_distribution<float> distScale(0.4f, 1.5f);
    std::uniform_real_distribution<float> distColor(0.7f, 1.0f);
    std::uniform_real_distribution<float> distTime(0.18f, 0.35f);
    std::uniform_real_distribution<float> distOffset(-0.2f, 0.2f);

    for (uint32_t i = 0; i < count; ++i) {
        if (particles_.size() >= kMaxParticles) return;

        Particle p;
        p.type = Particle::Type::Splash;
        p.transform.scale = { 0.05f, distScale(engine), 1.0f };
        p.transform.rotate = { 0.0f, 0.0f, distRotate(engine) };
        p.transform.translate = {
            pos.x + distOffset(engine),
            pos.y + distOffset(engine),
            pos.z + distOffset(engine)
        };
        p.velocity = { 0.0f, 0.0f, 0.0f };
        p.color = { distColor(engine), distColor(engine), distColor(engine), 1.0f };
        p.lifeTime = 0.0f;
        p.maxTime = distTime(engine);
        particles_.push_back(p);
    }
}
