#pragma once
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "Camera.h"
#include "Input.h"

class TitleScene
{
public:
	void Initialize(Object3dCommon* objCommon, Input* input);
	void Update();
	void Draw();
	
	bool IsFinished() const { return isFinished_; }

    Vector3 position_ = { 0,-10,10 };
    Vector3 rotation_ = { 0,0,0 };

    Vector3 cameraPos_;
    Vector3 cameraRot_;

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Input* input_ = nullptr;
    Camera camera_;
    Model* titleModel_ = nullptr;
    Object3d* titleObject_ = nullptr;
private:

    bool isFinished_ = false;
    float timer_ = 0.0f;
    float spiralAngle_ = 0.0f;
};

