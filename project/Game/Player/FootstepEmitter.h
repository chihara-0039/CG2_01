#pragma once
#include "MyMath.h"
#include <algorithm>
#include <cmath>
#include <optional>

// 歩幅の蓄積と砂埃の発生条件を一か所で管理する。内部の距離は外部へ公開しない。
class FootstepEmitter {
  public:
    // 発生した一歩の情報だけを値で返す。書き換えても内部の歩幅には影響しない。
    struct Emission {
        Vector3 motion;
        float strength;
    };

    // 接地して歩いた距離を蓄積し、一歩分を超えたときだけ発生情報を返す。
    std::optional<Emission> Update(const Vector3 &before, const Vector3 &after, bool walking) {
        const Vector3 motion{after.x - before.x, 0, after.z - before.z};
        const float distance = std::hypot(motion.x, motion.z);
        if (!walking || !std::isfinite(distance) || distance >= kMaximumFrameDistance) {
            Reset();
            return std::nullopt;
        }
        accumulatedDistance_ += distance;
        if (accumulatedDistance_ <= kStepDistance) {
            return std::nullopt;
        }
        Reset();
        return Emission{motion, std::clamp(distance * 8.0f, .45f, .9f)};
    }

    // 離陸・梯子・ワープ・ステージ切替では、前の歩幅を次へ持ち越さない。
    void Reset() { accumulatedDistance_ = 0; }

  private:
    static constexpr float kStepDistance = .65f;
    static constexpr float kMaximumFrameDistance = .4f;
    float accumulatedDistance_ = 0;
};
