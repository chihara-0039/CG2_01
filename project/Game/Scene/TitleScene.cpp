#include "TitleScene.h"

#include "GameRuntime.h"

#include <algorithm>
#include <cmath>

void TitleScene::Initialize(GameRuntime& game) {
    game.OnSceneEntered(SceneType::Title);

    Object3dCommon* object3dCommon = game.GetObject3dCommon();
    titleModel_ = Model::CreateFromOBJ(
        object3dCommon->GetDxCommon(), "Resources/Models/title", "title.obj",
        object3dCommon->GetTextureManager());
    titleObject_ = std::make_unique<Object3d>();
    titleObject_->Initialize(object3dCommon);
    titleObject_->SetModel(titleModel_.get());
    titleObject_->SetEnableLighting(false);
    titleObject_->SetScale({ 1.5f, 1.5f, 1.5f });

    pressSpaceModel_ = Model::CreateFromOBJ(
        object3dCommon->GetDxCommon(), "Resources/UI/pressSpace", "pressSpace.obj",
        object3dCommon->GetTextureManager());
    pressSpaceObject_ = std::make_unique<Object3d>();
    pressSpaceObject_->Initialize(object3dCommon);
    pressSpaceObject_->SetModel(pressSpaceModel_.get());
    pressSpaceObject_->SetEnableLighting(false);

    titlePosition_ = { 0.0f, -10.0f, 10.0f };
    titleRotation_ = {};
    spiralAngle_ = 0.0f;
    pressSpaceTimer_ = 0.0f;
    showPressSpace_ = false;
    camera_.SetPosition({ 0.0f, 2.0f, -20.0f });
    camera_.SetRotation({ 0.25f, 0.0f, 0.0f });
    camera_.Update();
}

void TitleScene::Update(GameRuntime& game, const SceneUpdateContext& context) {
    (void)context;

    if (spiralAngle_ < 6.2831853f) {
        spiralAngle_ += 0.05f;
        constexpr float kTitleStopY = -2.0f;
        if (titlePosition_.y < kTitleStopY) {
            titlePosition_.y += 0.1f;
        }
        if (titlePosition_.y > kTitleStopY) {
            titlePosition_.y = kTitleStopY;
        }
        const float calculatedRadius = 2.0f * (1.0f - (titlePosition_.y + 10.0f) / 8.0f);
        const float radius = calculatedRadius > 0.0f ? calculatedRadius : 0.0f;
        titlePosition_.x = std::cos(spiralAngle_) * radius;
        titlePosition_.z = 10.0f + std::sin(spiralAngle_) * radius;
    }
    titleRotation_.y += 0.02f;
    showPressSpace_ = titlePosition_.y >= -2.0f;
    pressSpaceTimer_ += 0.05f;

    const Matrix4x4& view = camera_.GetViewMatrix();
    const Matrix4x4& projection = camera_.GetProjectionMatrix();
    titleObject_->SetPosition(titlePosition_);
    titleObject_->SetRotation(titleRotation_);
    titleObject_->SetCamera(view, projection);
    titleObject_->Update(Math::MakeIdentity4x4());

    const float pulse = 1.0f + std::sin(pressSpaceTimer_) * 0.05f;
    pressSpaceObject_->SetPosition({ 0.0f, -5.5f, 10.0f });
    pressSpaceObject_->SetScale({ pulse, pulse, pulse });
    pressSpaceObject_->SetCamera(view, projection);
    pressSpaceObject_->Update(Math::MakeIdentity4x4());
    game.RunTitleScene();
}

void TitleScene::Draw(GameRuntime& game) {
    (void)game;
    if (titleObject_) {
        titleObject_->Draw();
    }
    if (showPressSpace_ && pressSpaceObject_) {
        pressSpaceObject_->Draw();
    }
}

void TitleScene::Finalize(GameRuntime& game) {
    game.OnSceneExited(SceneType::Title);
    pressSpaceObject_.reset();
    pressSpaceModel_.reset();
    titleObject_.reset();
    titleModel_.reset();
}
