#include "StartIntroTimeline.h"
#include <algorithm>

StartIntroTimeline::Frame StartIntroTimeline::Evaluate(float seconds) {
    seconds = (std::max)(seconds, 0.0f);
    const int cue = seconds < .75f ? 0 : seconds < 1.4f ? 1 :
        seconds < 2.05f ? 2 : seconds < kGoTime ? 3 : 4;
    constexpr float starts[] = {0, .75f, 1.4f, 2.05f, kGoTime};
    const float local = seconds - starts[cue];
    const float settle = std::clamp(local / .22f, 0.0f, 1.0f);
    const float pop = (1.0f-settle)*(1.0f-settle);
    const float blend = std::clamp(seconds / kGoTime, 0.0f, 1.0f);
    const float exit = std::clamp((seconds-kGoTime) / (kDuration-kGoTime), 0.0f, 1.0f);
    return {cue, 1.0f + .32f * pop + (cue == 4 ? .18f * exit : 0.0f),
        1.0f-exit, blend*blend*(3.0f-2.0f*blend), 1.0f-exit,
        seconds < kGoTime, seconds >= kDuration};
}
