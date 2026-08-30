#include "GameClearScene.h"

#include "GameRuntime.h"

#include <cmath>

void GameClearScene::Initialize(GameRuntime& game) {
    game.OnSceneEntered(SceneType::GameClear);

    Object3dCommon* object3dCommon = game.GetObject3dCommon();
    letters_.clear();
    letters_.reserve(text_.size());
    for (size_t i = 0; i < text_.size(); ++i) {
        Letter letter;
        const std::string name(1, text_[i]);
        letter.model = Model::CreateFromOBJ(
            object3dCommon->GetDxCommon(), "Resources/Models/ClearText/" + name,
            name + ".obj", object3dCommon->GetTextureManager());
        letter.object = std::make_unique<Object3d>();
        letter.object->Initialize(object3dCommon);
        letter.object->SetModel(letter.model.get());
        letter.object->SetRotation({ 0.0f, 3.1415926f, 0.0f });
        letter.position = { -6.0f + static_cast<float>(i) * 1.2f, -10.0f, 0.0f };
        letters_.push_back(std::move(letter));
    }

    timer_ = 0.0f;
    finishTimer_ = 0.0f;
    animationFinished_ = false;
    camera_.SetPosition({ 0.0f, 2.0f, -20.0f });
    camera_.SetRotation({ 0.25f, 0.0f, 0.0f });
    camera_.Update();
}

void GameClearScene::Update(GameRuntime& game, const SceneUpdateContext& context) {
    (void)context;
    timer_ += 1.0f;
    bool allFinished = true;
    for (size_t i = 0; i < letters_.size(); ++i) {
        Letter& letter = letters_[i];
        if (timer_ > static_cast<float>(i) * 12.0f) {
            letter.visible = true;
            if (letter.baseY < -3.0f) {
                letter.baseY += 0.4f;
                allFinished = false;
            }
            if (letter.bounceTime < 30.0f) {
                letter.bounceTime += 1.0f;
                letter.position.y = letter.baseY + std::sin(letter.bounceTime * 0.3f) * 0.8f;
                allFinished = false;
            } else {
                letter.position.y = letter.baseY;
            }
        } else {
            allFinished = false;
        }

        letter.object->SetPosition(letter.position);
        letter.object->SetScale(letter.scale);
        letter.object->SetCamera(camera_.GetViewMatrix(), camera_.GetProjectionMatrix());
        letter.object->Update(Math::MakeIdentity4x4());
    }

    animationFinished_ = allFinished;
    if (animationFinished_) {
        finishTimer_ += 1.0f;
        const float pulse = finishTimer_ < 10.0f
            ? 1.0f + finishTimer_ * 0.05f
            : (finishTimer_ < 20.0f ? 1.5f - (finishTimer_ - 10.0f) * 0.05f : 1.0f);
        for (Letter& letter : letters_) {
            letter.scale = { pulse, pulse, pulse };
        }
    }
    game.RunGameClearScene(animationFinished_);
}

void GameClearScene::Draw(GameRuntime& game) {
    (void)game;
    for (Letter& letter : letters_) {
        if (letter.visible && letter.object) {
            letter.object->Draw();
        }
    }
}

void GameClearScene::Finalize(GameRuntime& game) {
    game.OnSceneExited(SceneType::GameClear);
    letters_.clear();
}
