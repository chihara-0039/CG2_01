#pragma once
#include <vector>
#include <string>
#include <memory> 
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "Camera.h"
#include "IScene.h"

class GameClearScene : public IScene {
public:
    void Initialize();
    void Update();
    void Draw();
    void DrawUI() override {}
    bool IsFinished() const override { return isFinished_; }

    void SetEnginePointers(Object3dCommon* objCommon);

private:
    // 各文字データを管理する構造体
    struct Letter {
        // 1文字ごとの 3D リソースを所有
        std::unique_ptr<Object3d> object; // unique_ptr に変更
        std::unique_ptr<Model> model;     // unique_ptr に変更

        Vector3 position = { 0,0,0 };
        Vector3 scale = { 1,1,1 };

        bool isVisible = false;
        float bounceTime = 0.0f;
        float baseY = 0.0f;
    };

    // vector がクリアされる際、中の Letter ごとに unique_ptr が自動解放されます
    std::vector<Letter> letters_;

    // 借りているポインタ
    Object3dCommon* object3dCommon_ = nullptr;

    Camera camera_;
    Vector3 cameraPos_ = { 0.0f, 0.0f, 0.0f };
    Vector3 cameraRot_ = { 0.0f, 0.0f, 0.0f };

    float timer_ = 0.0f;
    bool isAllFinished_ = false;
    bool isFinished_ = false;
    float finishTimer_ = 0.0f;

    std::string text_ = "COURSECLEAR";
};