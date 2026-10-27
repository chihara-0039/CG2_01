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
    float radius, const Vector3& spawn, const StageFlyover::Pose& destination, const Vector3& spawnRotation) {
    seconds = (std::max)(seconds,0.0f);
    const float t = std::clamp(seconds/kFlightDuration,0.0f,1.0f);
    // 主人公そのものが舞台中心を一周する。周回を終えてから開始地点へ降りる。
    constexpr float twoPi=6.28318530718f;
    const float orbitProgress=Smooth(seconds/kOrbitDuration);
    const float landing=Smooth((seconds-kOrbitDuration)/kLandingDuration);
    const float dx=spawn.x-center.x,dz=spawn.z-center.z;
    const float spawnRadius=std::sqrt(dx*dx+dz*dz);
    const float startAngle=spawnRadius>.1f ? std::atan2(dx,dz) : destination.rotation.y+3.14159265359f;
    const float angle=startAngle+twoPi*orbitProgress;
    const float ringRadius=(std::max)(spawnRadius+5,std::clamp(radius,10.0f,36.0f));
    const float height=(std::max)(spawn.y+12,center.y+(std::min)(radius,32.0f)+6);
    const Vector3 orbit{center.x+std::sin(angle)*ringRadius,height,center.z+std::cos(angle)*ringRadius};
    const Vector3 player=t>=1 ? spawn : Vector3{Mix(orbit.x,spawn.x,landing),
        Mix(orbit.y,spawn.y,landing),Mix(orbit.z,spawn.z,landing)};
    const float yaw=angle+1.57079632679f;
    const Vector3 right{std::cos(angle),0,-std::sin(angle)};
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
    // 一周完了後の着地フェーズだけで通常視点へつなぐ。
    StageFlyover::Pose camera{{Mix(follow.x,destination.position.x,landing),
        Mix(follow.y,destination.position.y,landing),Mix(follow.z,destination.position.z,landing)},
        {MixAngle(rotation.x,destination.rotation.x,landing),
        MixAngle(rotation.y,destination.rotation.y,landing),MixAngle(0,destination.rotation.z,landing)},
        Mix(.95f,destination.fov,landing)};
    if (t >= 1) camera = destination;
    const float fade = Smooth((seconds-kFlightDuration)/kFadeDuration);
    // モデルの正面は-Z。進入方向へ向け、胴体を寝かせて飛行姿勢にする。
    // 終盤でバンクも解き、入力解禁前に元の立ち姿へ正確に戻す。
    const float upright = landing;
    const Vector3 flightRotation{-.95f,yaw+3.14159265359f,-.3f*std::sin(orbitProgress*3.14159265359f)};
    const Vector3 playerRotation = t >= 1 ? spawnRotation : Vector3{
        Mix(flightRotation.x,spawnRotation.x,upright),
        MixAngle(flightRotation.y,spawnRotation.y,upright),
        Mix(flightRotation.z,spawnRotation.z,upright)};
    return {camera,player,playerRotation,1-Smooth(seconds/1.25f),1-fade,
        seconds >= kFlightDuration,seconds >= kFlightDuration+kFadeDuration};
}
