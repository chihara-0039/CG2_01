#pragma once
#include "../Engine/Utility/CameraControlFlow.h"
#include "../Game/Player/PlayerActionFlow.h"
#include <cassert>
#include <cmath>
#include <string_view>
#include <memory>
#include <type_traits>
#include <iostream>
namespace ResubmissionStateChecks {
class Body final : public IPlayerActionBody {
  public:
    PlayerActionFacts facts{false, true, false, false};
    float scale = 0;
    int groundCalls = 0, ladderCalls = 0;
    bool jumpAccepted = false;
    bool takeoff = false, landing = false, leaveLadder = false;
    PlayerActionFacts ActionFacts() const override { return facts; }
    // 物理更新が接地結果を変更したケースを、描画に依存せず再現する。
    void MoveGround(float inputScale, bool jumpRequested) override {
        jumpAccepted = jumpRequested;
        scale = inputScale;
        ++groundCalls;
        if (takeoff) {
            facts.grounded = false;
        }
        if (landing) {
            facts.grounded = true;
        }
    }
    // 梯子の出口でセンサーが解除されるケースを再現する。
    void MoveLadder() override {
        ++ladderCalls;
        if (leaveLadder) {
            facts.ladder = false;
        }
    }
};
// 全行動状態を基底インターフェースから呼び、入力と衝突結果で遷移することを確認する。
inline void PlayerStates() {
    PlayerActionFlow flow, other;
    Body body;
    flow.Update(body);
    assert(std::string_view(flow.Current().Name()) == "Idle" && body.groundCalls == 1);
    body.facts.moving = true;
    flow.Update(body);
    assert(std::string_view(flow.Current().Name()) == "Walk" && body.scale == .5f);
    body.facts.running = true;
    flow.Update(body);
    assert(std::string_view(flow.Current().Name()) == "Run" && body.scale == .8f);
    body.facts.jumpRequested = true;
    body.takeoff = true;
    flow.Update(body);
    assert(std::string_view(flow.Current().Name()) == "Jump");
    assert(body.jumpAccepted);
    body.facts.jumpRequested = false;
    body.takeoff = false;
    body.facts.moving = false;
    flow.Update(body);
    assert(std::string_view(flow.Current().Name()) == "Jump");
    body.facts.ladder = true;
    flow.Update(body);
    assert(std::string_view(flow.Current().Name()) == "Ladder" && body.ladderCalls == 1);
    body.leaveLadder = true;
    body.facts.grounded = true;
    flow.Update(body);
    assert(std::string_view(flow.Current().Name()) == "Idle");
    assert(std::string_view(other.Current().Name()) == "Idle");
    flow.Reset();
    assert(flow.Current().HoldPose());
}
// 編集カメラの各モード、GUI停止、復帰、ホイールとパッドの動作を確認する。
inline void CameraStates() {
    CameraControlFlow flow, other;
    CameraControlPose pose{{0, 0, 0}, {0, 0, 0}, 10};
    CameraControlInput input;
    input.middle = true;
    input.mouseX = 20;
    input.mouseY = 10;
    flow.Update(pose, input);
    assert(std::string_view(flow.Current().Name()) == "Orbit");
    assert(std::abs(pose.rotation.y + .1f) < .00001f);
    input.shift = true;
    flow.Update(pose, input);
    assert(std::string_view(flow.Current().Name()) == "Pan");
    const auto held = pose;
    input.enabled = false;
    input.wheel = 100;
    flow.Update(pose, input);
    assert(std::string_view(flow.Current().Name()) == "Suspended");
    assert(pose.rotation.y == held.rotation.y && pose.target.x == held.target.x && pose.distance == held.distance);
    input.enabled = true;
    input.middle = false;
    flow.Update(pose, input);
    assert(std::string_view(flow.Current().Name()) == "Idle" && pose.distance == 9);
    input.wheel = 0;
    input.pad = true;
    input.zoomIn = true;
    input.rightX = 1;
    flow.Update(pose, input);
    assert(std::abs(pose.distance - 8.88f) < .00001f);
    assert(std::string_view(other.Current().Name()) == "Idle");
    flow.Reset();
    assert(std::string_view(flow.Current().Name()) == "Idle");
}
// エンジンとアプリの状態遷移をまとめて検証する。
inline void Run() {
    PlayerStates();
    CameraStates();
    static_assert(std::has_virtual_destructor_v<IPlayerState>);
    static_assert(std::has_virtual_destructor_v<ICameraControlState>);
    std::cout
        << "PASS: Engine camera states and Player action states own transitions and dispatch through base interfaces\n";
}
}
