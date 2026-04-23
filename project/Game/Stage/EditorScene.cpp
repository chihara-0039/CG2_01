#include "EditorScene.h"
#include "externals/imgui/imgui.h"

// 道具のポインタを受け取る
void EditorScene::SetEnginePointers(Object3dCommon* objCommon, Input* input, TextureManager* texManager) {
    objCommon_ = objCommon;
    input_ = input;
    texManager_ = texManager;
}

void EditorScene::Initialize() {
    // ステージレンダラーの初期化
    if (objCommon_) {
        stageRenderer_.Initialize(objCommon_);
    }

    // エディタ用カメラの初期位置
    editorCamera_.SetPosition({ 0.0f, 15.0f, -30.0f });
    editorCamera_.Update();
}

void EditorScene::Update() {
    // カメラの更新
    editorCamera_.Update();

    // ここに以前 MyGame.cpp に書いていた「マウスでブロックを置く」などの
    // エディタ用ロジックを引っ越してきます
}

void EditorScene::Draw() {
    // 3D描画共通設定
    if (objCommon_) {
        objCommon_->PreDraw();
    }

    // ステージの描画
    stageRenderer_.Draw();
}

void EditorScene::DrawUI() {
    // エディタ専用のUIを表示
    ImGui::Begin("Stage Editor");
    ImGui::Text("Editor Mode");
    // ここにブロック選択や保存ボタンを配置します
    ImGui::End();
}