#pragma once
#include <algorithm>
#include <cmath>

namespace JournalResult {
constexpr float kImpact = 2.20f, kReady = 3.15f;
inline float Smooth(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
struct Frame {
    float rise, open, stampZ, press, shake;
    bool ink, hand, ready;
};
// 経過時間から、手帳の開き・ハンコの位置・印影・入力解禁を求める。
inline Frame Evaluate(float t) {
    t = (std::max)(t, 0.0f);
    const float approach = std::clamp((t - 1.72f) / .48f, 0.0f, 1.0f);
    const float lift = Smooth((t - 2.36f) / .62f);
    const float z = t < kImpact ? -9 + 8.64f * approach * approach * approach : -.36f - 8.64f * lift;
    const float age = (std::max)(t - kImpact, 0.0f);
    return {Smooth(t / .65f),
            Smooth((t - .45f) / 1.15f),
            z,
            t >= kImpact ? std::exp(-age * 15) : 0,
            t >= kImpact ? std::sin(age * 54) * std::exp(-age * 11) : 0,
            t >= kImpact,
            t >= 1.72f && t < 2.98f,
            t >= kReady};
}
inline bool Crossed(float before, float after, float event) {
    return before < event && after >= event;
}

// 打刻後の開閉だけを扱い、印影と最初のクリア演出の時刻は巻き戻さない。
class Inspection {
  public:
    static constexpr float kDuration = .8f;

    float Open() const { return start_ + (target_ - start_) * Smooth(elapsed_ / kDuration); }

    // 途中で逆操作しても、現在の角度を始点にして滑らかに折り返す。
    bool Toggle(bool ready) {
        if (!ready) {
            return false;
        }
        start_ = Open();
        target_ = target_ > .5f ? 0.0f : 1.0f;
        elapsed_ = 0;
        return true;
    }

    void Advance(float dt) { elapsed_ = std::clamp(elapsed_ + (std::max)(dt, 0.0f), 0.0f, kDuration); }

  private:
    float start_ = 1, target_ = 1, elapsed_ = kDuration;
};
}
