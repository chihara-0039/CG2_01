#include "MyGame.h"
#include "TitleScene.h"
#include "GamePlayScene.h" // 新しく作成するシーン
#include "GameClearScene.h"
#include "EditorScene.h"

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

/// --- 初期化 ---
void MyGame::Initialize() {
    // 1. 基底クラス(Framework)の初期化
    Framework::Initialize();

    // 2. 本編シーンを生成
    std::unique_ptr<GamePlayScene> newScene = std::make_unique<GamePlayScene>();

    // 3. エンジンの道具（Frameworkが管理しているもの）をシーンに渡す
    newScene->SetEnginePointers(
         this->object3dCommon.get(),
         this->input.get(),
         this->textureManager.get(),
         this->shadowMap.get(),
         this->lightCamera.get()
    );

    // 4. シーン自身の初期化処理を実行
    newScene->Initialize();

    // 5. 管理下に置く
    scene_ = std::move(newScene);

	// 6. 影マップとライトカメラの初期化
    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon.get(), textureManager.get());

	// ライトカメラの初期化
    lightCamera_ = std::make_unique<LightCamera>();
    lightCamera_->Initialize();
}

// --- 終了処理 ---
void MyGame::Finalize() {
    // シーンの解放（念のため明示的に）
    if (scene_) {
        scene_.reset();
    }

    // エンジン基盤の終了処理（GPU待機など）
    Framework::Finalize();
}

// --- 毎フレーム更新 ---
void MyGame::Update() {
    Framework::Update();
#ifndef NDEBUG
    ImGui_ImplDX12_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
#endif

    if (input->TriggerKey(DIK_F1)) {
        ChangeMode(currentMode_ == AppMode::StageEditor ? AppMode::GamePlay : AppMode::StageEditor);
    }

    if (scene_) {
        scene_->Update();
        // シーン終了時の自動遷移 (Play -> Clear など)
        if (scene_->IsFinished()) {
            if (dynamic_cast<GamePlayScene*>(scene_.get())) ChangeMode(AppMode::GameClear);
        }
    }
}

// --- シーン切り替え関数 ---
void MyGame::ChangeMode(AppMode newMode) {
    currentMode_ = newMode;
    std::unique_ptr<IScene> nextScene;

    switch (currentMode_) {
    case AppMode::DebugView:
    // 必要に応じてデバッグ専用シーンなど
    break;

    case AppMode::StageEditor:
    nextScene = std::make_unique<EditorScene>();
    static_cast<EditorScene*>(nextScene.get())->SetEnginePointers(object3dCommon.get(), input.get(), textureManager.get());
    break;

    case AppMode::GamePlay:
    nextScene = std::make_unique<GamePlayScene>();
    static_cast<GamePlayScene*>(nextScene.get())->SetEnginePointers(
        object3dCommon.get(), input.get(), textureManager.get(), shadowMap.get(), lightCamera.get()
    );
    break;
    }

    if (nextScene) {
        nextScene->Initialize();
        scene_ = std::move(nextScene);
    }
}

// --- 描画処理 ---
void MyGame::Draw() {
    auto commandList = dxCommon->GetCommandList();
    // 1. 影パス
    shadowMap_->PreDraw(commandList);
    if (scene_) scene_->DrawShadow();
    shadowMap_->PostDraw(commandList);

    // 2. 本編パス
    dxCommon->PreDraw();
    if (scene_) {
        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
        scene_->Draw();

    #ifndef NDEBUG
        DrawCommonUI(); // 左側のDebug Window
        scene_->DrawUI(); // 右側のScene専用UI
        ImGui::Render();
        ID3D12DescriptorHeap* ppHeaps[] = { dxCommon->GetImguiSrvHeap() };
        commandList->SetDescriptorHeaps(1, ppHeaps);
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
    #endif
    }
    dxCommon->PostDraw();
}

// --- 共通UIの描画関数 ---
void MyGame::DrawCommonUI() {
#ifndef NDEBUG
    ImGui::Begin("Debug Window");

    // --- モード切り替え ---
    const char* modes[] = { "DebugView", "StageEditor", "GamePlay" };
    int currentIdx = (int)currentMode_;
    
    //  コンボボックスで変更があったら ChangeMode を呼ぶ
    if (ImGui::Combo("App Mode", &currentIdx, modes, IM_ARRAYSIZE(modes))) {
        ChangeMode((AppMode)currentIdx); // モード変更関数を呼び出す
    }

    // --- 描画フラグ ---
    if (ImGui::CollapsingHeader("Draw Flags", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show 3D Objects", &debugFlags_.show3DObjects);
        ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
        ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);
    }

    ImGui::End();
#endif
}