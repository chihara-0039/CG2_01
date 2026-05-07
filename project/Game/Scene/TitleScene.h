#pragma once
#include "IScene.h" // 継承するために追加[cite: 16]
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "Camera.h"
#include "Input.h"
#include <memory>

// IScene を継承するように変更[cite: 18]
class TitleScene : public IScene {
public:
    virtual ~TitleScene() = default;

    // Initializeの引数を IScene のルールに合わせるか、
    // 生成直後に呼ぶ専用の初期化関数にする（今回は後者で実装）
    void SetEnginePointers(Object3dCommon* objCommon, Input* input);

    void Initialize() override; // ISceneの仮想関数[cite: 16]
    void Update() override;
    void Draw() override;
    void DrawUI() override {} // タイトルでUIが不要なら空でOK

    bool IsFinished() const override { return isFinished_; }

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Input* input_ = nullptr;

    Camera camera_;
    std::unique_ptr<Model> titleModel_;
    std::unique_ptr<Object3d> titleObject_;

    bool isFinished_ = false;
    float timer_ = 0.0f;
    float spiralAngle_ = 0.0f;
    Vector3 position_ = { 0, -10, 10 };
    Vector3 rotation_ = { 0, 0, 0 };

    // 初期値セット
    Vector3 cameraPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraRot_ = { 0.0f, 0.0f, 0.0f };
};