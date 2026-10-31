#include "GameOverTimeline.h"
#include <algorithm>
#include <cmath>
namespace {
float Ease(float value) {
    const float t=std::clamp(value,0.0f,1.0f);
    return t*t*(3-2*t);
}
}
GameOverTimeline::Frame GameOverTimeline::Evaluate(float seconds) {
    seconds=(std::max)(seconds,0.0f);
    return {Ease((seconds-kHitStop)/.8f),Ease((seconds-kHitStop)/.65f),
        800*(1-Ease((seconds-1.2f)/.95f)),
        .18f*(1-Ease(seconds/.18f)),Ease((seconds-2.75f)/.4f),seconds>=kMenuTime};
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

GameOverTimeline::Letter GameOverTimeline::EvaluateLetter(float seconds,int index) {
    const float t=std::clamp((seconds-2.1f-static_cast<float>(std::clamp(index,0,7))*.055f)/.42f,0.0f,1.0f);
    const float ease=1-std::pow(1-t,3.0f);
    const float rebound=std::sin(t*3.14159265359f)*(1-t);
    return {Ease(t*2),35*(1-ease)-18*rebound,1+.5f*(1-ease),
        (index%2==0 ? -.15f : .15f)*(1-ease)};
}
GameOverTimeline::Pose GameOverTimeline::EvaluatePose(float seconds,bool fall,
    const Vector3& initial,const Vector3& rotation,const Vector3& cameraPosition,const Vector3& cameraRotation,float fov) {
    const auto frame=Evaluate(seconds);
    const float m=frame.motion,b=frame.cameraBlend;
    const float yaw=cameraRotation.y;
    Vector3 position=initial,body=rotation;
    // ヒットストップ後に小さく弾かれる。落下は徐々に速く沈み、カメラが見送る。
    const float kick=fall ? 0.0f : 1.2f*m;
    position.x-=std::sin(yaw)*kick; position.z-=std::cos(yaw)*kick;
    position.y+=fall ? -9*m*m : 1.4f*std::sin(m*3.14159265359f)-2*Ease((seconds-.95f)/1.0f);
    body.z+=m*(fall ? 5.4f : 3.8f); body.x+=m*(fall ? .5f : -.4f);
    const float orbit=yaw+.15f*m;
    const Vector3 target{position.x,initial.y+(position.y-initial.y)*(fall ? .35f : 1.0f)+.4f,position.z};
    const Vector3 end{initial.x-std::sin(orbit)*10,initial.y+5,initial.z-std::cos(orbit)*10};
    const float shakeTime=(std::max)(seconds-kHitStop,0.0f);
    const float shake=seconds<=kHitStop ? 0.0f : .055f*std::exp(-shakeTime*9);
    const Vector3 cam{cameraPosition.x+(end.x-cameraPosition.x)*b+std::sin(shakeTime*93)*shake,
        cameraPosition.y+(end.y-cameraPosition.y)*b+std::sin(shakeTime*77)*shake,
        cameraPosition.z+(end.z-cameraPosition.z)*b};
    const Vector3 d{target.x-cam.x,target.y-cam.y,target.z-cam.z};
    const float pitch=-std::atan2(d.y,std::sqrt(d.x*d.x+d.z*d.z));
    const float aimYaw=std::atan2(d.x,d.z);
    const Vector3 aim{cameraRotation.x+(pitch-cameraRotation.x)*b,
        cameraRotation.y+std::remainder(aimYaw-cameraRotation.y,6.28318530718f)*b,
        cameraRotation.z+.035f*m};
    return {position,body,cam,aim,fov+(.72f-fov)*b,1-.75f*Ease((seconds-.65f)/1.4f)};
}
