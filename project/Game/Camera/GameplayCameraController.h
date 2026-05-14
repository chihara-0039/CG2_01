#pragma once
#include "Input.h"
#include "Camera.h"
#include "WinApp.h"

class GameplayCameraController {
public:
    void Initialize();
    void Update(Input* input, Camera* camera, WinApp* winApp);

    float GetAngle() const { return cameraAngle_; }
    float GetPitch() const { return cameraPitch_; }

    void SetAngle(float angle) { cameraAngle_ = angle; }
    void SetPitch(float pitch) { cameraPitch_ = pitch; }

private:
    float cameraAngle_ = 0.0f;
    float cameraPitch_ = 0.75f;
};
