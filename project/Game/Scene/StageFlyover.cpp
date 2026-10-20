#include "StageFlyover.h"
#include <algorithm>
#include <cmath>

namespace {
float Smooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t*t*t*(t*(t*6-15)+10); // 開始・終了の速度と加速度を0にする
}
float Mix(float a, float b, float t) { return a+(b-a)*t; }
float MixAngle(float a, float b, float t) {
    return a + std::remainder(b-a, 6.28318530718f)*t;
}
}

StageFlyover::Pose StageFlyover::Evaluate(float seconds, const Vector3& center,
    float radius, const Pose& destination) {
    if (seconds >= kDuration) return destination;
    seconds = (std::max)(seconds, 0.0f);
    radius = (std::max)(radius, 3.0f);
    const float sweep = Smooth(seconds/1.8f);
    const float yaw = destination.rotation.y - .60f + .85f*sweep;
    constexpr float pitch = .65f;
    constexpr float tourFov = .90f;
    const float distance = radius/std::sin(tourFov*.5f)*1.10f;
    const Vector3 orbit = {center.x-std::sin(yaw)*std::cos(pitch)*distance,
        center.y+std::sin(pitch)*distance, center.z-std::cos(yaw)*std::cos(pitch)*distance};
    // 後半で俯瞰から実際のプレイ開始姿勢へ滑らかに寄せる。
    const float landing = Smooth((seconds-1.4f)/(kDuration-1.4f));
    return {{Mix(orbit.x,destination.position.x,landing),
             Mix(orbit.y,destination.position.y,landing),
             Mix(orbit.z,destination.position.z,landing)},
            {MixAngle(pitch,destination.rotation.x,landing),
             MixAngle(yaw,destination.rotation.y,landing),
             MixAngle(0,destination.rotation.z,landing)},
            Mix(tourFov,destination.fov,landing)};
}
