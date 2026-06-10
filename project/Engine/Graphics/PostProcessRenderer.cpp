#include "PostProcessRenderer.h"
#include "externals/imgui/imgui.h"
#include <cassert>

// ==========================================================
//  PostProcessRenderer::Initialize
//  全リソース (RenderTexture / RTV / SRV / RS / PSO 群 / 定数バッファ) を生成する
// ==========================================================
void PostProcessRenderer::Initialize(DirectXCommon* dxCommon, const Vector4& clearColor) {
    clearColor_ = clearColor;
    auto device = dxCommon->GetDevice();

    // ----------------------------------------------------------
    // 1. RenderTexture リソースの生成 (1280x720 / RGBA8 / RT 可)
    // ----------------------------------------------------------
    renderTexture_ = CreateRenderTextureResource(
        device, 1280, 720, DXGI_FORMAT_R8G8B8A8_UNORM, clearColor_);

    // ----------------------------------------------------------
    // 2. RTV デスクリプタヒープと RTV の生成
    //    RenderTexture を「書き込み先」として設定するために必要
    // ----------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // CPU からのみアクセス
    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_));
    assert(SUCCEEDED(hr));

    device->CreateRenderTargetView(
        renderTexture_.Get(), nullptr,
        rtvHeap_->GetCPUDescriptorHandleForHeapStart());

    // ----------------------------------------------------------
    // 3. SRV デスクリプタヒープと SRV の生成
    //    コピーパスでシェーダーがテクスチャをサンプリングするため SHADER_VISIBLE 必須
    // ----------------------------------------------------------
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_));
    assert(SUCCEEDED(hr));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels     = 1;
    device->CreateShaderResourceView(
        renderTexture_.Get(), &srvDesc,
        srvHeap_->GetCPUDescriptorHandleForHeapStart());

    // ----------------------------------------------------------
    // 4. コピー用 RootSignature の生成
    //    スロット構成：
    //      [0] DescriptorTable : SRV t0 (RenderTexture)
    //      [1] CBV             : b0  (ヴィネット定数)
    //    StaticSampler        : s0  (LINEAR / CLAMP)
    // ----------------------------------------------------------
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.NumDescriptors                    = 1;
    descriptorRange.BaseShaderRegister                = 0; // t0
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameters[2]{};
    // スロット 0 : SRV テーブル (Pixel Shader のみ参照)
    rootParameters[0].ParameterType                       = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameters[0].ShaderVisibility                    = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[0].DescriptorTable.NumDescriptorRanges = 1;
    rootParameters[0].DescriptorTable.pDescriptorRanges   = &descriptorRange;
    // スロット 1 : CBV (ヴィネット定数、Pixel Shader のみ参照)
    rootParameters[1].ParameterType                     = D3D12_ROOT_PARAMETER_TYPE_CBV;
    rootParameters[1].ShaderVisibility                  = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameters[1].Descriptor.ShaderRegister         = 0; // b0
    rootParameters[1].Descriptor.RegisterSpace          = 0;

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW         = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc   = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD           = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister   = 0; // s0
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters     = 2;
    rootSignatureDesc.pParameters       = rootParameters;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers   = &staticSampler;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(
        &rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) { OutputDebugStringA((char*)errorBlob->GetBufferPointer()); }
        assert(false);
    }
    hr = device->CreateRootSignature(
        0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(),
        IID_PPV_ARGS(&copyRootSignature_));
    assert(SUCCEEDED(hr));

    // ----------------------------------------------------------
    // 5. ポストプロセス用 PSO 群の生成
    //    頂点シェーダーは全エフェクト共通 (頂点バッファ不要の全画面三角形)
    //    ピクセルシェーダーがエフェクトごとに異なる
    // ----------------------------------------------------------
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob      = dxCommon->CompileShader(L"Resources/shaders/hlsl/Fullscreen.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psCopyBlob  = dxCommon->CompileShader(L"Resources/shaders/hlsl/CopyImage.PS.hlsl",  L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psGrayBlob  = dxCommon->CompileShader(L"Resources/shaders/hlsl/Grayscale.PS.hlsl",  L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psSepiaBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/Sepia.PS.hlsl",      L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psVigBlob   = dxCommon->CompileShader(L"Resources/shaders/hlsl/Vignette.PS.hlsl",   L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psBox3Blob  = dxCommon->CompileShader(L"Resources/shaders/hlsl/BoxFilter3x3.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psBox5Blob  = dxCommon->CompileShader(L"Resources/shaders/hlsl/BoxFilter5x5.PS.hlsl", L"ps_6_0");

    // PSO の共通設定 (入力レイアウトなし・深度テストなし・三角形リスト)
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = copyRootSignature_.Get();
    psoDesc.VS             = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.InputLayout.pInputElementDescs = nullptr;
    psoDesc.InputLayout.NumElements        = 0;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState.RenderTarget[0].BlendEnable           = false;
    psoDesc.RasterizerState.FillMode        = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode        = D3D12_CULL_MODE_NONE; // 全画面三角形は裏面が当たるため NONE
    psoDesc.DepthStencilState.DepthEnable   = false;                // 深度テスト不要
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.PrimitiveTopologyType           = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets                = 1;
    psoDesc.RTVFormats[0]                   = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat                       = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count               = 1;
    psoDesc.SampleMask                     = D3D12_DEFAULT_SAMPLE_MASK;

    // A. 通常コピー (エフェクトなし)
    psoDesc.PS = { psCopyBlob->GetBufferPointer(), psCopyBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&copyPipelineState_));
    assert(SUCCEEDED(hr));

    // B. グレースケール
    psoDesc.PS = { psGrayBlob->GetBufferPointer(), psGrayBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&grayscalePipelineState_));
    assert(SUCCEEDED(hr));

    // C. セピア調
    psoDesc.PS = { psSepiaBlob->GetBufferPointer(), psSepiaBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sepiaPipelineState_));
    assert(SUCCEEDED(hr));

    // D. ヴィネッティング
    psoDesc.PS = { psVigBlob->GetBufferPointer(), psVigBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&vignettePipelineState_));
    assert(SUCCEEDED(hr));

    // E. BoxFilter 3x3
    psoDesc.PS = { psBox3Blob->GetBufferPointer(), psBox3Blob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&boxFilter3x3PipelineState_));
    assert(SUCCEEDED(hr));

    // F. BoxFilter 5x5
    psoDesc.PS = { psBox5Blob->GetBufferPointer(), psBox5Blob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&boxFilter5x5PipelineState_));
    assert(SUCCEEDED(hr));

    // ----------------------------------------------------------
    // 6. ヴィネッティング用定数バッファの生成
    //    Upload ヒープで CPU から毎フレーム書き換え可能にする
    //    サイズは 256バイト境界にアライン (D3D12 の制約)
    // ----------------------------------------------------------
    D3D12_HEAP_PROPERTIES cbHeapProps = {
        D3D12_HEAP_TYPE_UPLOAD,
        D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
        D3D12_MEMORY_POOL_UNKNOWN, 1, 1
    };
    D3D12_RESOURCE_DESC cbResDesc{};
    cbResDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
    cbResDesc.Width              = (sizeof(VignetteParams) + 0xff) & ~0xff; // 256バイトアライン
    cbResDesc.Height             = 1;
    cbResDesc.DepthOrArraySize   = 1;
    cbResDesc.MipLevels          = 1;
    cbResDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    cbResDesc.SampleDesc.Count   = 1;
    hr = device->CreateCommittedResource(
        &cbHeapProps, D3D12_HEAP_FLAG_NONE, &cbResDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&vignetteConstantBuffer_));
    assert(SUCCEEDED(hr));

    // Map して CPU から直接書き込めるようにしておく (アプリ終了まで Unmap しない)
    vignetteConstantBuffer_->Map(0, nullptr, (void**)&vignetteParamsData_);

    // 初期値の設定
    vignetteParamsData_->scale    = 16.0f;
    vignetteParamsData_->exponent = 0.8f;
}

// ==========================================================
//  PostProcessRenderer::BeginRender
//  RenderTexture を RT 状態へ遷移し、ビューポート・クリアを設定する
// ==========================================================
void PostProcessRenderer::BeginRender(ID3D12GraphicsCommandList* cmdList, DirectXCommon* dxCommon) {
    // 1. RenderTexture を RENDER_TARGET 状態へ遷移 (現在の状態と異なるときのみバリアを張る)
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = renderTexture_.Get();
    barrier.Transition.StateBefore = renderTextureState_;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    if (barrier.Transition.StateBefore != barrier.Transition.StateAfter) {
        cmdList->ResourceBarrier(1, &barrier);
    }
    renderTextureState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

    // 2. レンダーターゲットに RenderTexture をセット (深度バッファは通常 DSV を流用)
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHeap()->GetCPUDescriptorHandleForHeapStart();
    cmdList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

    // 3. ビューポートとシザーを RenderTexture のサイズ (1280x720) に合わせる
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    D3D12_RECT     scissor  = { 0, 0, 1280, 720 };
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissor);

    // 4. カラーバッファと深度バッファのクリア
    float cc[4] = { clearColor_.x, clearColor_.y, clearColor_.z, clearColor_.w };
    cmdList->ClearRenderTargetView(rtvHandle, cc, 0, nullptr);
    cmdList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

// ==========================================================
//  PostProcessRenderer::EndRender
//  RenderTexture を PIXEL_SHADER_RESOURCE 状態へ遷移させる
// ==========================================================
void PostProcessRenderer::EndRender(ID3D12GraphicsCommandList* cmdList) {
    // コピーパスでシェーダーからサンプリングするために状態を遷移させる
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = renderTexture_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cmdList->ResourceBarrier(1, &barrier);
    renderTextureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

// ==========================================================
//  PostProcessRenderer::DrawToBackBuffer
//  選択されたポストエフェクト PSO でバックバッファに全画面コピー描画する
// ==========================================================
void PostProcessRenderer::DrawToBackBuffer(ID3D12GraphicsCommandList* cmdList) {
    // RootSignature とエフェクトに対応した PSO をバインド
    cmdList->SetGraphicsRootSignature(copyRootSignature_.Get());
    switch (postEffectMode_) {
    case 1: // グレースケール
        cmdList->SetPipelineState(grayscalePipelineState_.Get());
        break;
    case 2: // セピア調
        cmdList->SetPipelineState(sepiaPipelineState_.Get());
        break;
    case 3: // ヴィネッティング (定数バッファも一緒にバインド)
        cmdList->SetPipelineState(vignettePipelineState_.Get());
        cmdList->SetGraphicsRootConstantBufferView(
            1, vignetteConstantBuffer_->GetGPUVirtualAddress());
        break;
    case 4: // BoxFilter 3x3
        cmdList->SetPipelineState(boxFilter3x3PipelineState_.Get());
        break;
    case 5: // BoxFilter 5x5
        cmdList->SetPipelineState(boxFilter5x5PipelineState_.Get());
        break;
    default: // 通常コピー (エフェクトなし)
        cmdList->SetPipelineState(copyPipelineState_.Get());
        break;
    }

    // SRV ヒープをバインドし、RenderTexture の SRV をスロット 0 (t0) にセット
    ID3D12DescriptorHeap* heaps[] = { srvHeap_.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetGraphicsRootDescriptorTable(
        0, srvHeap_->GetGPUDescriptorHandleForHeapStart());

    // 全画面三角形を描画 (Fullscreen.VS.hlsl が SV_VertexID から頂点を内部生成するため VB 不要)
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(3, 1, 0, 0);
}

// ==========================================================
//  PostProcessRenderer::DrawImGui
//  設定パネルの描画 (MyGame の左パネル "Information" 内から呼ばれる)
// ==========================================================
void PostProcessRenderer::DrawImGui() {
    if (ImGui::CollapsingHeader("Offscreen Rendering (RenderTexture)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Offscreen Rendering", &enabled_);
        ImGui::ColorEdit4("Clear Color (VRAM)", &clearColor_.x);

        const char* skyboxModes[] = { "Ignore", "Link (Multiply)" };
        ImGui::Combo("Skybox Color Link", &skyboxLinkMode_, skyboxModes, IM_ARRAYSIZE(skyboxModes));

        const char* effectNames[] = { "Normal", "Grayscale", "Sepia", "Vignette", "BoxFilter 3x3", "BoxFilter 5x5" };
        ImGui::Combo("Post Effect", &postEffectMode_, effectNames, IM_ARRAYSIZE(effectNames));

        // ヴィネット選択時のみパラメータスライダーを表示
        if (postEffectMode_ == 3 && vignetteParamsData_) {
            ImGui::DragFloat("Vignette Scale",    &vignetteParamsData_->scale,    0.1f, 0.0f, 100.0f, "%.1f");
            ImGui::DragFloat("Vignette Exponent", &vignetteParamsData_->exponent, 0.05f, 0.0f, 10.0f, "%.2f");
        }
    }
}

// ==========================================================
//  PostProcessRenderer::CreateRenderTextureResource  [private]
//  D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET 付きの Texture2D リソースを生成する
// ==========================================================
Microsoft::WRL::ComPtr<ID3D12Resource> PostProcessRenderer::CreateRenderTextureResource(
    ID3D12Device* device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4& clearColor)
{
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width            = width;
    resourceDesc.Height           = height;
    resourceDesc.MipLevels        = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format           = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET; // RT として使うため必須

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // GPU 専用メモリ

    // クリアカラーの最適化ヒントを渡しておく (同じ値でクリアすると GPU が最適化できる)
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format   = format;
    clearValue.Color[0] = clearColor.x;
    clearValue.Color[1] = clearColor.y;
    clearValue.Color[2] = clearColor.z;
    clearValue.Color[3] = clearColor.w;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue,
        IID_PPV_ARGS(&resource));
    assert(SUCCEEDED(hr));
    return resource;
}
