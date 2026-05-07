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
    // 1. 親クラスの更新（入力情報の取得など）
    Framework::Update();

    // --- 【重要】ImGuiのフレーム開始宣言 ---
    // これを Begin() を呼ぶ前に実行する必要があります
#ifndef NDEBUG // リリース時はImGuiを処理しない
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif

    // 2. 終了リクエストがあれば何もしない
    if (Framework::IsRunning() == false) {
        return;
    }

    // --- 【移植】F1キーによるシーン即時切り替え ---
    if (input->TriggerKey(DIK_F1)) {
        // 現在のシーンが Editor なら Play へ、そうでなければ Editor へ
        if (dynamic_cast<EditorScene*>(scene_.get())) {
            auto nextScene = std::make_unique<GamePlayScene>();
            nextScene->SetEnginePointers(object3dCommon.get(), input.get(), textureManager.get(), shadowMap.get(), lightCamera.get());
            nextScene->Initialize();
            scene_ = std::move(nextScene);
        } else {
            auto nextScene = std::make_unique<EditorScene>();
            nextScene->SetEnginePointers(object3dCommon.get(), input.get(), textureManager.get());
            nextScene->Initialize();
            scene_ = std::move(nextScene);
        }
    }

    // 3. 現在のシーンの更新を実行
    if (scene_) {
        scene_->Update();

        // --- シーン遷移のハンドル ---[cite: 17, 21]
        if (scene_->IsFinished()) {
            // 現在が TitleScene なら GamePlayScene へ
            if (dynamic_cast<TitleScene*>(scene_.get())) {
                auto nextScene = std::make_unique<GamePlayScene>();
                nextScene->SetEnginePointers(object3dCommon.get(), input.get(), textureManager.get(), shadowMap.get(), lightCamera.get());
                nextScene->Initialize();
                scene_ = std::move(nextScene);
            }
            // 現在が GamePlayScene なら GameClearScene へ
            else if (dynamic_cast<GamePlayScene*>(scene_.get())) {
                if (scene_->IsFinished()) {
                    auto nextScene = std::make_unique<TitleScene>();

                    // 【修正】まず道具を渡す
                    nextScene->SetEnginePointers(object3dCommon.get(), input.get());

                    // 【修正】そのあと、引数なしで初期化を呼ぶ
                    nextScene->Initialize();

                    scene_ = std::move(nextScene);
                }
            }
        }
    }
#ifndef NDEBUG
    // F1キーなどで強制的にエディタへ切り替える隠しコマンドもここにあると便利です
    if (input->TriggerKey(DIK_F1)) {
        auto editor = std::make_unique<EditorScene>();
        editor->SetEnginePointers(object3dCommon.get(), input.get(), textureManager.get());
        editor->Initialize();
        scene_ = std::move(editor);
    }
#endif
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
    // 1. 影用パス
    if (scene_) {
        shadowMap->PreDraw(dxCommon->GetCommandList());
        scene_->DrawShadow();
        shadowMap->PostDraw(dxCommon->GetCommandList());
    }

    // 2. 本編パス
    dxCommon->PreDraw();

    if (scene_) {
        scene_->Draw();

	#ifndef NDEBUG
        // --- ImGuiの描画をここで復活させる ---
        // 以前の MyGame.cpp で書いていた ImGui::Begin... などの処理を
        // シーン側の DrawUI() に任せる
        // 【ImGui サイクル: 構築】
        DrawCommonUI();
        scene_->DrawUI();

        // これを呼ばないと「Render() を呼び忘れてない？」というエラーになる
        // 【ImGui サイクル: 確定】
        ImGui::Render();


        // 【ImGui サイクル: 転送】
        ID3D12DescriptorHeap* ppHeaps[] = { dxCommon->GetImguiSrvHeap() };
        dxCommon->GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
        // 確定したUIの描画データを、現在のコマンドリストに焼き付ける
        // 第2引数には dxCommon から取得したコマンドリストを渡します
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());
	#endif
    }

    // 3. 画面表示（ここでImGuiのデータも一緒にGPUへ送られます）
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