#include "GamePlayScene.h"
#include "Goal.h"
#include "ModelManager.h"
#include <cassert>

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


    // --- 5. カメラの初期設定 ---
    camera_.SetPosition({ 0.0f, 5.0f, -10.0f });
}

void GamePlayScene::Update() {
    // --- 1. カメラ操作の復活 ---
    // マウスやキーボードで視点を回せるようにします
    if (input_->PushKey(DIK_LEFT)) { gameCameraAngle_ -= 0.05f; }
    if (input_->PushKey(DIK_RIGHT)) { gameCameraAngle_ += 0.05f; }

    // カメラの座標をプレイヤーの後ろに回り込ませる等の計算
    // ここでは簡易的に回転角のみ更新
    camera_.Update();

    // --- 2. 行列の反映 ---
    Matrix4x4 view = camera_.GetViewMatrix();
    Matrix4x4 proj = camera_.GetProjectionMatrix();

    // プレイヤーとステージにカメラを教える
    if (player_) {
        player_->SetCamera(view, proj);
        player_->Update(input_, stageMap_, gameCameraAngle_, lightCamera_->GetViewProjectionMatrix());
    }
    stageRenderer_.SetCamera(view, proj);
    stageRenderer_.Update(lightCamera_->GetViewProjectionMatrix());

    // --- 3. 天球（空）の修正 ---
    if (skydomeObject_) {
        skydomeObject_->SetCamera(view, proj);
        if (player_) {
            // 空は常にプレイヤーを中心に置く
            skydomeObject_->SetPosition(player_->GetPosition());
        }
        // 【修正】天球の更新には影用行列ではなく、空用の単位行列を渡す
        skydomeObject_->Update(Math::MakeIdentity4x4());
    }

    // 影用カメラの追従
    if (player_) {
        lightCamera_->Update({ 0.2f, -1.0f, 0.5f }, player_->GetPosition());
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

    if (ImGui::Button("脱出ボタン (座標リセット)")) {
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