#include "GameOverTimeline.h"
#include <algorithm>
namespace {
float Ease(float value) {
    const float t=std::clamp(value,0.0f,1.0f);
    return t*t*(3-2*t);
}
}
GameOverTimeline::Frame GameOverTimeline::Evaluate(float seconds) {
    seconds=(std::max)(seconds,0.0f);
    return {Ease((seconds-kHitStop)/.8f),Ease((seconds-kHitStop)/.65f),
        800*(1-Ease((seconds-.95f)/.95f)),
        .22f*(1-Ease(seconds/.22f)),Ease((seconds-1.9f)/.3f),seconds>=kMenuTime};
}
float GameOverTimeline::RetryRadius(float seconds) { return 800*Ease(seconds/kRevealTime); }

void GameOverTimeline::Flow::Begin() { phase_=Phase::Failure; elapsed_=0; revealElapsed_=0; selected_=0; }
void GameOverTimeline::Flow::Advance(float delta) {
    delta=(std::max)(delta,0.0f);
    if (phase_==Phase::Failure) elapsed_+=delta;
    else if (phase_==Phase::Reveal) {
        revealElapsed_+=delta;
        if (revealElapsed_>=kRevealTime) phase_=Phase::Inactive;
    }
}
void GameOverTimeline::Flow::MoveSelection() { if (MenuReady()) selected_=1-selected_; }
GameOverTimeline::Action GameOverTimeline::Flow::Confirm(bool back) {
    if (!MenuReady()) return Action::None;
    if (back || selected_==1) { phase_=Phase::Leaving; return Action::StageSelect; }
    phase_=Phase::Reveal; revealElapsed_=0; return Action::Retry;
}
void GameOverTimeline::Flow::Cancel() { phase_=Phase::Inactive; }
