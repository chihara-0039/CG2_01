#pragma once
#include "MyMath.h"
#include <algorithm>
#include <cmath>

namespace AdventureParticlePath {
// 配置のかけらは外側から弧を描いて集め、最後はゆっくり収束させる。
inline Vector3 Assemble(const Vector3 &origin, const Vector3 &target, float time) {
    const float t = std::clamp(time, 0.0f, 1.0f);
    const float q = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
    return {origin.x + (target.x - origin.x) * q,
            origin.y + (target.y - origin.y) * q + std::sin(t * 3.14159265f) * .45f,
            origin.z + (target.z - origin.z) * q};
}
// 取消は最初の25%で外へほどき、その後に袋へ引き戻す。始点・終点は動かさない。
inline Vector3 Undo(const Vector3 &origin, const Vector3 &target, float time, float phase) {
    const float t = std::clamp(time, 0.0f, 1.0f);
    const float travel = std::clamp((t - .25f) / .75f, 0.0f, 1.0f);
    const float q = travel * travel * (3.0f - 2.0f * travel);
    const float spread = std::sin(t * 3.14159265f) * (1.0f - q);
    return {origin.x + (target.x - origin.x) * q + std::cos(phase + t * 5) * spread * .65f,
            origin.y + (target.y - origin.y) * q + spread * .6f,
            origin.z + (target.z - origin.z) * q + std::sin(phase + t * 5) * spread * .65f};
}
// 寿命の割合から吸い込み位置を求める。開始点と到着点を保ち、中間だけ螺旋状に膨らませる。
inline Vector3 Sample(const Vector3 &origin, const Vector3 &target, float time, float phase) {
    const float t = std::clamp(time, 0.0f, 1.0f);
    const float q = t * t;
    const float angle = phase + t * 14;
    const float arc = std::sin(t * 3.14159265f);
    return {origin.x + (target.x - origin.x) * q + std::cos(angle) * .60f * arc,
            origin.y + (target.y - origin.y) * q + arc * .5f,
            origin.z + (target.z - origin.z) * q + std::sin(angle) * .60f * arc};
}
}
