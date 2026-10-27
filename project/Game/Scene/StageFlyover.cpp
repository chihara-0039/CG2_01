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

StageArrival::Frame StageArrival::Evaluate(float seconds, const Vector3& center,
    float radius, const Vector3& spawn, const StageFlyover::Pose& destination) {
    seconds = (std::max)(seconds,0.0f);
    const float t = std::clamp(seconds/kFlightDuration,0.0f,1.0f);
    const float travel = Smooth(t);
    const float remaining = 1-travel;
    // 通常視点と同じ側から進入する。舞台の大きさだけで遠ざかり続けない。
    float yaw = destination.rotation.y;
    const float approachX = center.x-spawn.x, approachZ = center.z-spawn.z;
    if (approachX*approachX+approachZ*approachZ > 4) yaw = std::atan2(approachX,approachZ);
    const Vector3 back{-std::sin(yaw),0,-std::cos(yaw)};
    const Vector3 right{std::cos(yaw),0,-std::sin(yaw)};
    const float height = std::clamp(center.y+radius+8-spawn.y,18.0f,36.0f);
    const float arc = std::sin(t*3.14159265359f);
    const Vector3 player = t >= 1 ? spawn : Vector3{spawn.x+back.x*14*remaining+right.x*3*arc,
        spawn.y+height*remaining,spawn.z+back.z*14*remaining+right.z*3*arc};
    // 肩越しに追従し、注視点を少しステージへ寄せる。プレイヤーは手前の左寄り。
    const Vector3 torso{player.x,player.y+.8f,player.z};
    const Vector3 away{torso.x-center.x,torso.y-center.y,torso.z-center.z};
    const float length = (std::max)(std::sqrt(away.x*away.x+away.y*away.y+away.z*away.z),.001f);
    const Vector3 axis{away.x/length,away.y/length,away.z/length};
    // 舞台→主人公の軸上から追うため、大ステージでも舞台だけ画面外へ落ちない。
    const Vector3 follow{torso.x+axis.x*9+right.x*3,torso.y+axis.y*9+1.2f,
        torso.z+axis.z*9+right.z*3};
    const Vector3 target{torso.x-axis.x*3,torso.y-axis.y*3,torso.z-axis.z*3};
    const Vector3 direction{target.x-follow.x,target.y-follow.y,target.z-follow.z};
    const float horizontal = std::sqrt(direction.x*direction.x+direction.z*direction.z);
    const Vector3 rotation{-std::atan2(direction.y,horizontal),std::atan2(direction.x,direction.z),0};
    // 最後の三割で通常視点へ接続し、着地時のカメラの飛びを防ぐ。
    const float landing = Smooth((t-.68f)/.32f);
    StageFlyover::Pose camera{{Mix(follow.x,destination.position.x,landing),
        Mix(follow.y,destination.position.y,landing),Mix(follow.z,destination.position.z,landing)},
        {MixAngle(rotation.x,destination.rotation.x,landing),
        MixAngle(rotation.y,destination.rotation.y,landing),MixAngle(0,destination.rotation.z,landing)},
        Mix(.95f,destination.fov,landing)};
    if (t >= 1) camera = destination;
    const float fade = Smooth((seconds-kFlightDuration)/kFadeDuration);
    return {camera,player,1-Smooth(seconds/1.25f),1-fade,
        seconds >= kFlightDuration,seconds >= kFlightDuration+kFadeDuration};
}
