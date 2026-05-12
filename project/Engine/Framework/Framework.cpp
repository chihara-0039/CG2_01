#include "Framework.h"

void Framework::Initialize() {
    // 1. ウィンドウとDirectX基盤の起動
    winApp = std::make_unique<WinApp>();
    winApp->Initialize();

    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get());

    input = std::make_unique<Input>();
    input->Initialize(winApp.get());

    // 2. 描画マネージャー系の初期化（依存関係をセット）
    textureManager = std::make_unique<TextureManager>();
    textureManager->Initialize(dxCommon.get());

    spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->SetTextureManager(textureManager.get());
    spriteCommon->Initialize(dxCommon.get());

    object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->SetTextureManager(textureManager.get());
    object3dCommon->Initialize(dxCommon.get());

    // 3. 特殊描画（パーティクル・影・カメラ）の準備
    particleManager = std::make_unique<ParticleManager>();
    particleManager->Initialize(dxCommon.get(), textureManager.get());

    shadowMap = std::make_unique<ShadowMap>();
    shadowMap->Initialize(dxCommon.get(), textureManager.get());

    lightCamera = std::make_unique<LightCamera>();
    lightCamera->Initialize();

    camera = std::make_unique<Camera>();

    // 4. 静的マネージャーのセットアップ
    ModelManager::Initialize(object3dCommon.get());
}

void Framework::Update() {
    // どのゲームでも必ずやる入力更新
    input->Update();
}

void Framework::Finalize() {
    // 終了前のGPU待機と後片付け
    if (dxCommon) {
        dxCommon->WaitForGpu();
        dxCommon->FinalizeImGui();
    }
    ModelManager::Finalize();
}

bool Framework::IsRunning() {
    return !winApp->ProcessMessage();
}

void Framework::Run() {
    Initialize();
    while (IsRunning()) {
        Update();
        Draw();
    }
    Finalize();
}