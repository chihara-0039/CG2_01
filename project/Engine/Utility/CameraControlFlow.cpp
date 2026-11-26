#include "CameraControlFlow.h"
#include <algorithm>
namespace {
const ICameraControlState &Idle();
const ICameraControlState &Orbit();
const ICameraControlState &Pan();
const ICameraControlState &Suspended();
// マウス操作に共通する、ホイールとゲームパッドの操作を適用する。
void Common(CameraControlPose &pose, const CameraControlInput &input) {
    pose.distance -= input.wheel * .01f;
    if (input.pad) {
        const float direction = input.invert ? 1.f : -1.f;
        pose.rotation.y += input.rightX * .035f * direction;
        pose.rotation.x -= input.rightY * .035f * direction;
        const auto rotation =
            Math::Multiply(Math::MakeRotateXMatrix(pose.rotation.x), Math::MakeRotateYMatrix(pose.rotation.y));
        pose.target.x += (rotation.m[0][0] * input.leftX + rotation.m[2][0] * input.leftY) * .08f;
        pose.target.z += (rotation.m[0][2] * input.leftX + rotation.m[2][2] * input.leftY) * .08f;
        if (input.zoomIn) {
            pose.distance -= .12f;
        }
        if (input.zoomOut) {
            pose.distance += .12f;
        }
    }
    pose.distance = (std::max)(pose.distance, 1.f);
}
class CameraIdleState final : public ICameraControlState {
  public:
    // 操作開始か停止要求があるまで、軌道・平行移動を行わない。
    const ICameraControlState &Next(const CameraControlInput &i) const override {
        if (!i.enabled) {
            return Suspended();
        }
        if (i.middle) {
            return i.shift ? Pan() : Orbit();
        }
        return *this;
    }
    void Update(CameraControlPose &p, const CameraControlInput &i) const override { Common(p, i); }
    const char *Name() const override { return "Idle"; }
};
class CameraOrbitState final : public ICameraControlState {
  public:
    // 中ボタンを離すと待機、Shiftを押すと平行移動へ切り替える。
    const ICameraControlState &Next(const CameraControlInput &i) const override {
        if (!i.enabled) {
            return Suspended();
        }
        if (!i.middle) {
            return Idle();
        }
        if (i.shift) {
            return Pan();
        }
        return *this;
    }
    // 回転量を更新してからパッドの操作を重ねる。
    void Update(CameraControlPose &p, const CameraControlInput &i) const override {
        const float direction = i.invert ? 1.f : -1.f;
        p.rotation.y += i.mouseX * .005f * direction;
        p.rotation.x += i.mouseY * .005f * direction;
        Common(p, i);
    }
    const char *Name() const override { return "Orbit"; }
};
class CameraPanState final : public ICameraControlState {
  public:
    // Shiftを離すと回転へ戻り、フォーカス喪失やGUI操作で停止する。
    const ICameraControlState &Next(const CameraControlInput &i) const override {
        if (!i.enabled) {
            return Suspended();
        }
        if (!i.middle) {
            return Idle();
        }
        if (!i.shift) {
            return Orbit();
        }
        return *this;
    }
    // 現在のカメラ平面に沿って注視点を移す。
    void Update(CameraControlPose &p, const CameraControlInput &i) const override {
        const auto rotation =
            Math::Multiply(Math::MakeRotateXMatrix(p.rotation.x), Math::MakeRotateYMatrix(p.rotation.y));
        p.target.x -= (rotation.m[0][0] * i.mouseX - rotation.m[1][0] * i.mouseY) * .02f;
        p.target.y += (rotation.m[0][1] * i.mouseX - rotation.m[1][1] * i.mouseY) * .02f;
        p.target.z -= (rotation.m[0][2] * i.mouseX - rotation.m[1][2] * i.mouseY) * .02f;
        Common(p, i);
    }
    const char *Name() const override { return "Pan"; }
};
class CameraSuspendedState final : public ICameraControlState {
  public:
    // 操作可能になったフレームの入力を見て、復帰先を自分で決める。
    const ICameraControlState &Next(const CameraControlInput &i) const override {
        if (!i.enabled) {
            return *this;
        }
        if (i.middle) {
            return i.shift ? Pan() : Orbit();
        }
        return Idle();
    }
    void Update(CameraControlPose &, const CameraControlInput &) const override {}
    const char *Name() const override { return "Suspended"; }
};
const ICameraControlState &Idle() {
    static const CameraIdleState s;
    return s;
}
const ICameraControlState &Orbit() {
    static const CameraOrbitState s;
    return s;
}
const ICameraControlState &Pan() {
    static const CameraPanState s;
    return s;
}
const ICameraControlState &Suspended() {
    static const CameraSuspendedState s;
    return s;
}
}
const ICameraControlState &CameraControlFlow::Current() const {
    return state_ ? *state_ : Idle();
}
// 基底参照を介して遷移判断と操作を呼び出す。Flowにはモード条件を置かない。
void CameraControlFlow::Update(CameraControlPose &pose, const CameraControlInput &input) {
    state_ = &Current().Next(input);
    state_->Update(pose, input);
}
void CameraControlFlow::Reset() {
    state_ = &Idle();
}
