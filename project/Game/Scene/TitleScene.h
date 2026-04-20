#pragma once
#include <memory> // 追加
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "Camera.h"
#include "Input.h"

class TitleScene {
public:
    // 引数の objCommon や input は MyGame が所有しているものを「借りる」だけなので、生ポインタのままでOKです
    void Initialize(Object3dCommon* objCommon, Input* input);
    void Update();
    void Draw();

    bool IsFinished() const { return isFinished_; }

    Vector3 position_ = { 0,-10,10 };
    Vector3 rotation_ = { 0,0,0 };
    Vector3 cameraPos_;
    Vector3 cameraRot_;

private:
    // 所有権を持たない（MyGameから借りているだけの）ポインタ
    Object3dCommon* object3dCommon_ = nullptr;
    Input* input_ = nullptr;

    // このシーンが「自分専用」として所有するリソース
    Camera camera_;
    std::unique_ptr<Model> titleModel_; // unique_ptr に変更
    std::unique_ptr<Object3d> titleObject_; // unique_ptr に変更

    bool isFinished_ = false;
    float timer_ = 0.0f;
    float spiralAngle_ = 0.0f;
};