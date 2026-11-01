#include "StageFlyover.h"
#include <algorithm>
#include <cmath>

namespace {
float Smooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return std::clamp(t*t*t*(t*(t*6-15)+10),0.0f,1.0f); // 開始・終了の速度と加速度を0にする
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
    const float orbitTime=std::clamp(seconds/kOrbitDuration,0.0f,1.0f);
    const float orbitProgress=orbitTime*orbitTime*(2-orbitTime);
    const float landing=Smooth((seconds-kOrbitDuration)/kLandingDuration);
    const float dx=spawn.x-center.x,dz=spawn.z-center.z;
    const float spawnRadius=std::sqrt(dx*dx+dz*dz);
    const float startAngle=spawnRadius>.1f ? std::atan2(dx,dz) : destination.rotation.y+3.14159265359f;
    const float angle=startAngle+twoPi*orbitProgress;
    const float baseRadius=(std::max)(spawnRadius+5,std::clamp(radius,10.0f,36.0f));
    const float ringRadius=baseRadius*(1+.10f*std::sin(orbitProgress*3.14159265359f));
    const float cruiseHeight=(std::max)(spawn.y+10,center.y+(std::min)(radius,32.0f)+4);
    const float height=cruiseHeight+4*(1-orbitProgress);
    const Vector3 orbit{center.x+std::sin(angle)*ringRadius,height,center.z+std::cos(angle)*ringRadius};
    Vector3 player=orbit;
    if(seconds>=kOrbitDuration) {
        // Carry the orbit's velocity into a curved descent. Do not ease to a stop
        // at either the orbit seam or just above the ground.
        const float q=std::clamp((seconds-kOrbitDuration)/kLandingDuration,0.0f,1.0f);
        const float q2=q*q,q3=q2*q;
        const float h00=2*q3-3*q2+1,h10=q3-2*q2+q,h01=-2*q3+3*q2,h11=q3-q2;
        const Vector3 entry{center.x+std::sin(startAngle)*baseRadius,cruiseHeight,center.z+std::cos(startAngle)*baseRadius};
        const float radial=-baseRadius*.1f*3.14159265359f/kOrbitDuration;
        const float angular=twoPi/kOrbitDuration;
        const Vector3 velocity{std::cos(startAngle)*baseRadius*angular+std::sin(startAngle)*radial,
            -4/kOrbitDuration,-std::sin(startAngle)*baseRadius*angular+std::cos(startAngle)*radial};
        auto descend=[&](float from,float speed,float to,float impact){return h00*from+h10*speed*kLandingDuration+h01*to+h11*impact*kLandingDuration;};
        player={descend(entry.x,velocity.x,spawn.x,0),descend(entry.y,velocity.y,spawn.y,-8),descend(entry.z,velocity.z,spawn.z,0)};
    }
    if(t>=1)player=spawn;
    const float yaw=angle+1.57079632679f;
    // Keep a constant offset from the player. Orbiting a second camera around
    // the descent made the apparent flight direction swing away from spawn.
    const Vector3 displacement{player.x-spawn.x,player.y-spawn.y,player.z-spawn.z};
    StageFlyover::Pose camera{{destination.position.x+displacement.x,
        destination.position.y+displacement.y,destination.position.z+displacement.z},
        destination.rotation,Mix((std::max)(.75f,destination.fov),destination.fov,landing)};
    if (t >= 1) camera = destination;
    const float fade = Smooth((seconds-kFlightDuration)/kFadeDuration);
    // モデルの正面は-Z。進入方向へ向け、胴体を寝かせて飛行姿勢にする。
    // 終盤でバンクも解き、入力解禁前に元の立ち姿へ正確に戻す。
    const float upright = landing;
    const Vector3 flightRotation{-.95f,yaw+3.14159265359f,-.48f*std::sin(orbitProgress*3.14159265359f)};
    const Vector3 playerRotation = t >= 1 ? spawnRotation : Vector3{
        Mix(flightRotation.x,spawnRotation.x,upright),
        MixAngle(flightRotation.y,spawnRotation.y,upright),
        Mix(flightRotation.z,spawnRotation.z,upright)};
    return {camera,player,playerRotation,1-Smooth(seconds/1.25f),1-fade,
        seconds >= kFlightDuration,seconds >= kFlightDuration+kFadeDuration,seconds >= kCloudExitTime};
}
