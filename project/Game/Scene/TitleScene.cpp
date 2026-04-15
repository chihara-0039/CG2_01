#include "TitleScene.h"

void TitleScene::Initialize(Object3dCommon* objCommon, Input* input) {
    object3dCommon_ = objCommon;
    input_ = input;

    // カメラ用変数
    cameraPos_ = { 0.0f, 2.0f, -20.0f };
    cameraRot_ = { 0.0f, 0.0f, 0.0f };

    camera_.SetPosition(cameraPos_);
    camera_.SetRotation(cameraRot_);

    // モデル読み込み（好きなモデルに変更OK）
    titleModel_ = Model::CreateFromOBJ(
        object3dCommon_->GetDxCommon(),
        "Resources/Models/title",
        "title.obj",
        object3dCommon_->GetTextureManager()
    );


    // オブジェクト生成
    titleObject_ = new Object3d();
    titleObject_->Initialize(object3dCommon_);
    titleObject_->SetModel(titleModel_);

    position_ = { 0, -10, 10 };
    rotation_ = { 0, 0, 0 };

    titleObject_->SetPosition(position_);
    titleObject_->SetRotation(rotation_);
}

void TitleScene::Update() {

    timer_ += 0.02f;

    // 渦巻き
    if (spiralAngle_ < 6.28f) {
        spiralAngle_ += 0.05f;

        // 下から上がる
        if (position_.y < 0) {
            position_.y += 0.1f;
        }

        float radius = 2.0f * (1.0f - (position_.y + 10.0f) / 10.0f);
        if (radius < 0) radius = 0;

        position_.x = std::cos(spiralAngle_) * radius;
        position_.z = std::sin(spiralAngle_) * radius;

        rotation_.y += 0.02f;
    }

    // 常時回転
    rotation_.y += 0.02f;

    // ★ Objectに反映
    titleObject_->SetPosition(position_);
    titleObject_->SetRotation(rotation_);


    // =========================
   // カメラ演出
   // =========================

   // ズームイン
    if (cameraPos_.z < -10.0f) {
        cameraPos_.z += 0.03f;
    }

    // 少し見下ろす
    cameraRot_.x = 0.25f;

    camera_.SetPosition(cameraPos_);
    camera_.SetRotation(cameraRot_);
    camera_.Update();

    // =========================
    // カメラ反映
    // =========================
    const Matrix4x4& view = camera_.GetViewMatrix();
    const Matrix4x4& proj = camera_.GetProjectionMatrix();

    titleObject_->SetCamera(view, proj);
    titleObject_->Update();

    // =========================
    // シーン遷移
    // =========================
    if (input_->TriggerKey(DIK_RETURN)) {
        isFinished_ = true;
    }
}

void TitleScene::Draw() {
    titleObject_->Draw();
}
