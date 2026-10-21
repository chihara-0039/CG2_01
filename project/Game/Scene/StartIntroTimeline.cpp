#include "StartIntroTimeline.h"
#include <algorithm>

StartIntroTimeline::Frame StartIntroTimeline::Evaluate(float seconds) {
    seconds = (std::max)(seconds, 0.0f);
    // 表示・効果音・操作解禁が、すべて同じ1秒の拍を参照する。
    constexpr float starts[] = {0, kCountInterval, 2*kCountInterval, kGoTime};
    const int cue = seconds < starts[1] ? 0 : seconds < starts[2] ? 1 :
        seconds < starts[3] ? 2 : 3;
    const float local = seconds - starts[cue];
    const float settle = std::clamp(local / .22f, 0.0f, 1.0f);
    const float pop = (1.0f-settle)*(1.0f-settle);
    const float blend = std::clamp(seconds / kGoTime, 0.0f, 1.0f);
    const float exit = std::clamp((seconds-kGoTime) / (kDuration-kGoTime), 0.0f, 1.0f);
    return {cue, 1.0f + .32f * pop + (cue == 3 ? .18f * exit : 0.0f),
        1.0f-exit, blend*blend*(3.0f-2.0f*blend), 1.0f-exit,
        seconds < kGoTime, seconds >= kDuration};
}
