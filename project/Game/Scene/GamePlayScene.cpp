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

    skydomeObject_->SetScale({ 500.0f, 500.0f, 500.0f });

    // --- 5. カメラの初期設定 ---
    camera_.SetPosition({ 0.0f, 5.0f, -10.0f });
}

void GamePlayScene::Update() {
    // --- 1. マウスによる旋回操作の復元 ---
    const auto& mouse = input_->GetMouseState();
    float screenWidth = 1280.0f; // ウィンドウ幅
    float screenHeight = 720.0f;

    // 左クリック中の旋回
    if (mouse.buttons[0]) {
        // 左右の回転
        if (mouse.posX < screenWidth * 0.1f) {
            gameCameraAngle_ -= 0.025f;
        } else if (mouse.posX > screenWidth * 0.9f) {
            gameCameraAngle_ += 0.025f;
        }

		// 上下の回転（俯瞰角度の調整）
        if (mouse.posY < screenHeight * 0.1f) {
            cameraPitch_ -= 0.015f; 
        } else if (mouse.posY > screenHeight * 0.9f) {
            cameraPitch_ += 0.015f;
        }
    }

    // 角度制限（真上や真下で反転しないようにする）
    if (cameraPitch_ > 1.5f) { cameraPitch_ = 1.5f; }
    if (cameraPitch_ < 0.1f) { cameraPitch_ = 0.1f; }

    // --- 2. ピボットカメラの計算 ---
    Vector3 pivot = { 4.0f, 9.0f, 4.5f };
    float distance = 35.0f;

    Vector3 pos;
    // 左右角(gameCameraAngle_)と上下角(cameraPitch_)を組み合わせて座標を計算
    pos.x = pivot.x - std::cos(cameraPitch_) * std::sin(gameCameraAngle_) * distance;
    pos.y = pivot.y + std::sin(cameraPitch_) * 20.0f;
    pos.z = pivot.z - std::cos(cameraPitch_) * std::cos(gameCameraAngle_) * distance;

    camera_.SetPosition(pos);
    camera_.SetRotation({ cameraPitch_, gameCameraAngle_, 0.0f });
    camera_.Update();

    // --- 3. 行列の反映 ---
    Matrix4x4 view = camera_.GetViewMatrix();
    Matrix4x4 proj = camera_.GetProjectionMatrix();

	// プレイヤーとステージレンダラーにカメラ行列をセットして更新する
    if (player_) {
        player_->SetCamera(view, proj);
        player_->Update(input_, stageMap_, gameCameraAngle_, lightCamera_->GetViewProjectionMatrix());
    }
    stageRenderer_.SetCamera(view, proj);
    stageRenderer_.Update(lightCamera_->GetViewProjectionMatrix());

    // Pスイッチ等のギミック更新 (省略せず現状維持)
    stageMap_.UpdatePSwitch();
    if (stageMap_.WasPSwitchJustFinished()) {
        stageRenderer_.BuildFromStageMap(stageMap_);
    }

    // 天球（空）の追従
    if (skydomeObject_) {
        skydomeObject_->SetCamera(view, proj);
        if (player_) {
            skydomeObject_->SetPosition(player_->GetPosition());
        }
        skydomeObject_->Update(Math::MakeIdentity4x4());
    }

	// --- 4. ライトカメラの更新 ---
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