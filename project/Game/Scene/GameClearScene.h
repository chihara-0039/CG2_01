#pragma once

#include "BaseScene.h"
#include "Camera.h"
#include "Model.h"
#include "Object3d.h"

#include <memory>
#include <string>
#include <vector>

// ゴール取得演出の後に表示する、花火付きの正式なクリアシーン。
class GameClearScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override;
    void Update(GameRuntime& game, const SceneUpdateContext& context) override;
    void Draw(GameRuntime& game) override;
    void Finalize(GameRuntime& game) override;

private:
    struct Letter {
        std::unique_ptr<Model> model;
        std::unique_ptr<Object3d> object;
        Vector3 position = {};
        Vector3 scale = { 1.0f, 1.0f, 1.0f };
        float baseY = -10.0f;
        float bounceTime = 0.0f;
        bool visible = false;
    };

    std::vector<Letter> letters_;
    Camera camera_;
    float timer_ = 0.0f;
    float finishTimer_ = 0.0f;
    bool animationFinished_ = false;
    const std::string text_ = "COURSECLEAR";
};
