#include "MyGame.h"
#include "TitleScene.h"
#include "GamePlayScene.h" // 新しく作成するシーン
#include "GameClearScene.h"
#include "EditorScene.h"

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

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

void MyGame::Finalize() {
    // シーンの解放（念のため明示的に）
    if (scene_) {
        scene_.reset();
    }

    // エンジン基盤の終了処理（GPU待機など）
    Framework::Finalize();
}

void MyGame::Update() {
    // 1. 親クラスの更新（入力情報の取得など）
    Framework::Update();

    // --- 【重要】ImGuiのフレーム開始宣言 ---
    // これを Begin() を呼ぶ前に実行する必要があります
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // 2. 終了リクエストがあれば何もしない
    if (Framework::IsRunning() == false) {
        return;
    }

    // --- モード切り替えロジック ---
    if (input->TriggerKey(DIK_F1)) {
        // 現在が GamePlay なら Editor へ、Editor なら GamePlay へ
        // ※ ここでシーンを入れ替える処理を書く
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

    // F1キーなどで強制的にエディタへ切り替える隠しコマンドもここにあると便利です
    if (input->TriggerKey(DIK_F1)) {
        auto editor = std::make_unique<EditorScene>();
        editor->SetEnginePointers(object3dCommon.get(), input.get(), textureManager.get());
        editor->Initialize();
        scene_ = std::move(editor);
    }
}

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

        // --- ImGuiの描画をここで復活させる ---
        // 以前の MyGame.cpp で書いていた ImGui::Begin... などの処理を
        // シーン側の DrawUI() に任せる
        scene_->DrawUI();

        // これを呼ばないと「Render() を呼び忘れてない？」というエラーになる
        ImGui::Render();

        ID3D12DescriptorHeap* ppHeaps[] = { dxCommon->GetImguiSrvHeap() };
        dxCommon->GetCommandList()->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        // 確定したUIの描画データを、現在のコマンドリストに焼き付ける
        // 第2引数には dxCommon から取得したコマンドリストを渡します
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), dxCommon->GetCommandList());
    }

    // 3. 画面表示（ここでImGuiのデータも一緒にGPUへ送られます）
    dxCommon->PostDraw();
}