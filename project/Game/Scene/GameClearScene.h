#pragma once

#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "Camera.h"

#include <vector>
#include <string>

class GameClearScene
{
public:
    void Initialize(Object3dCommon* objCommon);
    void Update();
    void Draw();

private:

    struct Letter {
        Object3d* object = nullptr;
        Model* model = nullptr;

        Vector3 position = { 0,0,0 };
        Vector3 scale = { 1,1,1 };

        bool isVisible = false;
        float bounceTime = 0.0f;
        float baseY = 0.0f;
    };

    std::vector<Letter> letters_;

    Object3dCommon* object3dCommon_ = nullptr;

    Camera camera_;
    Vector3 cameraPos_;
    Vector3 cameraRot_;

    float timer_ = 0.0f;
    bool isAllFinished_ = false;
    float finishTimer_ = 0.0f;

    std::string text_ = "COURSECLEAR";
};

