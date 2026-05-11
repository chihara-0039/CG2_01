#include "GamePlayScene.h"
#include "Goal.h"
#include "ModelManager.h"
#include <cassert>
#include <algorithm>

#include "externals/imgui/imgui.h"

void GamePlayScene::SetEnginePointers(
    Object3dCommon* objCommon,
    Input* input,
    TextureManager* texManager,
    ShadowMap* shadowMap,
    LightCamera* lightCamera
) {
    // 必要なポインタをメンバ変数に保存する
    objCommon_ = objCommon;
    input_ = input;
    texManager_ = texManager;
    shadowMap_ = shadowMap;
    lightCamera_ = lightCamera;
}

void GamePlayScene::Initialize() {
    // すべてのポインタがセットされているかチェック
    if (objCommon_ == nullptr || input_ == nullptr) {
        return;
    }

    // --- 【安全装置】道具が届いていない場合は、ここで実行を止めて知らせる ---
    assert(objCommon_ != nullptr && "Object3dCommonがシーンに渡されていません");
    assert(texManager_ != nullptr && "TextureManagerがシーンに渡されていません");

    // --- 1. ステージデータの読み込み ---
    stageMap_.Initialize(32, 20, 32);
    stageMap_.LoadFromFile("Resources/Stages/prototype.txt");

    // --- 2. ステージレンダラーの準備 ---
    stageRenderer_.Initialize(objCommon_);
    stageRenderer_.BuildFromStageMap(stageMap_);

    // --- 3. プレイヤーの生成 ---
    player_ = std::make_unique<Player>();
    // モデル読み込み
    Model* playerModel = Model::CreateFromOBJ(
        objCommon_->GetDxCommon(),
        "Resources/Models/block",
        "block.obj",
        texManager_
    );
    player_->Initialize(objCommon_, playerModel);
    player_->SetPosition({ 2.0f, 1.5f, 2.0f });


    // --- 4. 天球の生成 ---
    // モデルを読み込んでからオブジェクトを生成・初期化する
    skydomeModel_.reset(Model::CreateFromOBJ(
        objCommon_->GetDxCommon(),
        "Resources/Models/skydome", // 天球用モデルのパス
        "skydome.obj",
        texManager_
    ));

    skydomeObject_ = std::make_unique<Object3d>();
    skydomeObject_->Initialize(objCommon_);
    skydomeObject_->SetModel(skydomeModel_.get());

    skydomeObject_->SetScale({ 500.0f, 500.0f, 500.0f });

    // --- 5. カメラの初期設定 ---
    camera_.SetPosition({ 0.0f, 5.0f, -10.0f });
}

void GamePlayScene::Update() {
    // --- 1. マウス画面端によるカメラ旋回 (masterから移植) ---
    const auto& mouse = input_->GetMouseState();
    float screenW = (float)WinApp::kClientWidth;
    float screenH = (float)WinApp::kClientHeight;
    float edge = 0.1f;
    const float rotateSpeed = 0.025f;

    if (mouse.buttons[0]) { // 左クリック中
        if (mouse.posX < screenW * edge) { gameCameraAngle_ -= rotateSpeed; } else if (mouse.posX > screenW * (1.0f - edge)) { gameCameraAngle_ += rotateSpeed; }

        if (mouse.posY < screenH * edge) { cameraPitch_ += rotateSpeed; } else if (mouse.posY > screenH * (1.0f - edge)) { cameraPitch_ -= rotateSpeed; }
    }
    // 角度制限
    cameraPitch_ = std::clamp(cameraPitch_, 0.4f, 1.5f);

    // --- 2. カメラ座標計算 (masterの数式) ---
    Vector3 pivot = { 4.0f, 9.0f, 4.5f };
    float distance = 35.0f;
    Vector3 pos;
    pos.x = pivot.x - std::cos(cameraPitch_) * std::sin(gameCameraAngle_) * distance;
    pos.y = pivot.y + std::sin(cameraPitch_) * 20.0f;
    pos.z = pivot.z - std::cos(cameraPitch_) * std::cos(gameCameraAngle_) * distance;

    camera_.SetPosition(pos);
    camera_.SetRotation({ cameraPitch_, gameCameraAngle_, 0.0f });
    camera_.Update();

    // --- 3. プレイヤーとアイテム・ゴール判定 ---
    player_->Update(input_, stageMap_, gameCameraAngle_, lightCamera_->GetViewProjectionMatrix());

    Vector3 pPos = player_->GetPosition();
    int gx = static_cast<int>(std::floor(pPos.x + 0.5f));
    int gy = static_cast<int>(std::floor(pPos.y));
    int gz = static_cast<int>(std::floor(pPos.z + 0.5f));

    // バブル取得
    MapCell* cell = stageMap_.GetCell(gx, gy, gz);
    if (cell && cell->type == BlockType::BubblePickup) {
        // blockCountの管理が必要な場合はここにフラグ処理を追加
        stageMap_.RemoveBlock(gx, gy, gz);
        stageRenderer_.BuildFromStageMap(stageMap_);
    }

    // ゴール判定 (Goalクラス使用)
    if (Goal::Check(pPos, { 0.4f, 0.9f, 0.4f }, stageMap_)) {
        isFinished_ = true; // MyGame側でClearSceneへ飛ばす
    }
}

void GamePlayScene::DrawShadow() {
    // --- 影用パスの共通設定 ---
    objCommon_->PreDrawShadow();

    // シャドウマップへの描き込み
    // ライト視点の行列を使用して描画する
    player_->DrawShadow(lightCamera_->GetViewProjectionMatrix());
    stageRenderer_.DrawShadow(lightCamera_->GetViewProjectionMatrix()); // ステージも影を落とす
}

void GamePlayScene::Draw() {
    // 描画前に、最新のシャドウマップのハンドルを共通設定に登録する
    objCommon_->SetShadowMapHandle(shadowMap_->GetSrvHandle());

    // 3D描画の共通設定
    objCommon_->PreDraw();

    // 1. 天球の描画（背景）
    skydomeObject_->Draw();

    // 2. ステージの描画
    stageRenderer_.Draw();

    // 3. プレイヤーの描画
    player_->Draw();
}

void GamePlayScene::DrawUI() {
    // 以前 MyGame::Draw 内で書いていた ImGui のコードをここに移します
    ImGui::Begin("Game Debug");

    if (ImGui::Button("Reset Player Position")) {
        player_->SetPosition({ 2.0f, 10.0f, 2.0f }); // 空中にテレポート
    }

    ImGui::Text("Application running with Scene-based architecture.");

    if (player_) {
        Vector3 pos = player_->GetPosition();
        ImGui::Text("Player Position: (%.2f, %.2f, %.2f)", pos.x, pos.y, pos.z);
    }

    ImGui::Text("Camera Angle: %.2f", gameCameraAngle_);

    if (ImGui::Button("Back to Title")) {
        isFinished_ = true; // これでシーン遷移のきっかけが作れます
    }

    if (ImGui::Button("Reset Player")) {
        player_->SetPosition({ 2.0f, 1.5f, 2.0f });
    }

    ImGui::End();
}