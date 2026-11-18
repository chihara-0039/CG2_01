#pragma once
#include <algorithm>
#include <cmath>

namespace StarGet {
constexpr float kDuration = 3.6f;
inline float Ease(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
struct Frame {
    float lift, hop, spin, scale, cameraBlend, poseTime, caption;
};
// 星の受け取り・跳躍・決めポーズを同じ時刻から再現する。
inline Frame Evaluate(float time) {
    const float t = (std::max)(time, 0.0f);
    const float jump = std::clamp((t - .2f) / .65f, 0.0f, 1.0f);
    return {Ease(t / .75f),
            .3f * std::sin(jump * 3.14159265f),
            (1 - Ease(t / .9f)) * 6.2831853f,
            .25f + .85f * Ease(t / .7f) + .09f * std::sin(t * 14) * std::exp(-t * 3),
            Ease(t / .75f),
            (std::min)(t, 1.0f),
            Ease((t - .65f) / .3f)};
}
}
