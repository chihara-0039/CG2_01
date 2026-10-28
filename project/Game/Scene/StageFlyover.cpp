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
    // ステージ中心の周りを必ず360度回ってから着地へ進む。
    const float sweep = Smooth(seconds/kOrbitDuration);
    const float yaw = destination.rotation.y - .60f + 6.28318530718f*sweep;
    constexpr float pitch = .65f;
    // 大ステージを球ごと収めようとして引き続けず、画角を広げて距離を抑える。
    const float tourFov = .90f + .15f*std::clamp((radius-18.0f)/12.0f,0.0f,1.0f);
    const float framedRadius = (std::min)(radius,22.0f);
    const float distance = (std::min)(framedRadius/std::sin(tourFov*.5f)*1.10f,50.0f);
    const Vector3 orbit = {center.x-std::sin(yaw)*std::cos(pitch)*distance,
        center.y+std::sin(pitch)*distance, center.z-std::cos(yaw)*std::cos(pitch)*distance};
    // 周回の途中でスポーンへ寄せない。一周完了後の別フェーズで移動する。
    const float landing = Smooth((seconds-kOrbitDuration)/kLandingDuration);
    return {{Mix(orbit.x,destination.position.x,landing),
             Mix(orbit.y,destination.position.y,landing),
             Mix(orbit.z,destination.position.z,landing)},
            {MixAngle(pitch,destination.rotation.x,landing),
             MixAngle(yaw,destination.rotation.y,landing),
             MixAngle(0,destination.rotation.z,landing)},
            Mix(tourFov,destination.fov,landing)};
}
