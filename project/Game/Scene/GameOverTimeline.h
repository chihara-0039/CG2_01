#pragma once
// 描画APIから独立した失敗演出の時間設計。実時間で評価する。
namespace GameOverTimeline {
constexpr float kHitStop = .18f;
constexpr float kMenuTime = 2.2f;
constexpr float kRevealTime = .45f;
struct Frame {
    float motion;
    float cameraBlend;
    float radius;
    float flash;
    float menuAlpha;
    bool menuReady;
};
Frame Evaluate(float seconds);
float RetryRadius(float seconds);
// 入力はTriggerの時だけ渡す。連続決定・演出中の入力・再開始を描画なしで検証できる。
enum class Action { None, Retry, StageSelect };
class Flow {
public:
    void Begin();
    void Advance(float delta);
    void MoveSelection();
    Action Confirm(bool back);
    void Cancel();
    bool Active() const { return phase_!=Phase::Inactive; }
    bool Revealing() const { return phase_==Phase::Reveal; }
    bool Leaving() const { return phase_==Phase::Leaving; }
    bool MenuReady() const { return phase_==Phase::Failure && elapsed_>=kMenuTime; }
    float Elapsed() const { return elapsed_; }
    float RevealElapsed() const { return revealElapsed_; }
    int Selected() const { return selected_; }
private:
    enum class Phase { Inactive, Failure, Reveal, Leaving };
    Phase phase_=Phase::Inactive;
    float elapsed_=0, revealElapsed_=0;
    int selected_=0;
};
}
