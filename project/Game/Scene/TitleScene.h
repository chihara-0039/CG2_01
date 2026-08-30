#pragma once

#include "BaseScene.h"
#include "Camera.h"
#include "Model.h"
#include "Object3d.h"

#include <memory>

// ゲーム起動時のタイトル画面を担当する正式なシーンクラス。
class TitleScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override;
    void Update(GameRuntime& game, const SceneUpdateContext& context) override;
    void Draw(GameRuntime& game) override;
    void Finalize(GameRuntime& game) override;

private:
    std::unique_ptr<Model> titleModel_;
    std::unique_ptr<Object3d> titleObject_;
    std::unique_ptr<Model> pressSpaceModel_;
    std::unique_ptr<Object3d> pressSpaceObject_;
    Camera camera_;
    Vector3 titlePosition_ = { 0.0f, -10.0f, 10.0f };
    Vector3 titleRotation_ = {};
    float spiralAngle_ = 0.0f;
    float pressSpaceTimer_ = 0.0f;
    bool showPressSpace_ = false;
};
