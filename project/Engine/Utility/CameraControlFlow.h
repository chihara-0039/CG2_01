#pragma once
#include "MyMath.h"

// OS・ゲーム・Inputクラスに依存しない、編集カメラの一フレーム分の値。
struct CameraControlInput {
    bool enabled = true, middle = false, shift = false, invert = false;
    float mouseX = 0, mouseY = 0, wheel = 0;
    bool pad = false, zoomIn = false, zoomOut = false;
    float rightX = 0, rightY = 0, leftX = 0, leftY = 0;
};
struct CameraControlPose {
    Vector3 rotation, target;
    float distance;
};
class ICameraControlState {
  public:
    virtual ~ICameraControlState() = default;
    virtual const ICameraControlState &Next(const CameraControlInput &input) const = 0;
    virtual void Update(CameraControlPose &pose, const CameraControlInput &input) const = 0;
    virtual const char *Name() const = 0;
};
// カメラ自身が状態を保持し、入力条件による遷移を状態クラスへ委譲する。
class CameraControlFlow {
  public:
    void Update(CameraControlPose &pose, const CameraControlInput &input);
    const ICameraControlState &Current() const;
    void Reset();

  private:
    const ICameraControlState *state_ = nullptr;
};
