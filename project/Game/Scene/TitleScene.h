#pragma once

#include "BaseScene.h"
#include "Camera.h"
#include "Model.h"
#include "Object3d.h"

#include <memory>
#include <chrono>
#include <array>

// ゲーム起動時のタイトル画面を担当する正式なシーンクラス。
class TitleScene final : public BaseScene {
public:
    void Initialize(GameRuntime& game) override;
    void Update(GameRuntime& game, const SceneUpdateContext& context) override;
    void Draw(GameRuntime& game) override;
    void Finalize(GameRuntime& game) override;

private:
    std::unique_ptr<Model> titleModel_;
    std::unique_ptr<Object3d> titleShadowObject_;
    std::unique_ptr<Object3d> titleObject_;
    std::unique_ptr<Model> pressSpaceModel_;
    std::unique_ptr<Object3d> pressSpaceObject_;
    std::unique_ptr<Model> decorationStarModel_;
    std::array<std::unique_ptr<Object3d>, 7> decorationStars_;
    static constexpr size_t kBackgroundStarCount = 64;
    std::array<std::unique_ptr<Object3d>, kBackgroundStarCount> backgroundStars_;
    static constexpr size_t kTrailCount = 4;
    std::array<std::array<std::unique_ptr<Object3d>, kTrailCount>, 7> starTrails_;
    std::array<std::unique_ptr<Object3d>, 2> titleFlares_;
    Camera camera_;
    Vector3 titlePosition_ = { 0.0f, -10.0f, 10.0f };
    Vector3 titleRotation_ = {};
    float spiralAngle_ = 0.0f;
    float pressSpaceTimer_ = 0.0f;
    bool showPressSpace_ = false;
    bool startRequested_ = false;
    float startAnimationTimer_ = 0.0f;
    float idleTimer_ = 0.0f;
    float environmentTimer_ = 0.0f;
    float titleFlareTimer_ = 2.0f;
    bool titleArrivalFlareStarted_ = false;
    float titleNightAmount_ = 0.0f;
    std::chrono::steady_clock::time_point lastUpdateTime_{};
};
