#include "SceneFadeFlow.h"
#include <algorithm>
#include <cmath>

class SceneFadeFlow::State {
  public:
    virtual ~State() = default;
    virtual void Handle(SceneFadeFlow &flow, const Event &event) const = 0;
    virtual Phase Kind() const = 0;
};
class SceneFadeFlow::IdleState final : public State {
  public:
    // 停止中の開始要求だけを受け付け、幕を閉じる状態へ移る。
    void Handle(SceneFadeFlow &flow, const Event &event) const override {
        if (flow.HandleReset(event)) {
            return;
        }
        if (event.kind == EventKind::Begin) {
            flow.alpha_ = flow.holdRemaining_ = 0;
            flow.recovery_ = event.recovery;
            flow.Change(Phase::FadingOut);
        }
    }
    Phase Kind() const override { return Phase::None; }
};
class SceneFadeFlow::ClosingState final : public State {
  public:
    // 黒へ到達した瞬間に読込要求を出し、黒画面保持へ進む。
    void Handle(SceneFadeFlow &flow, const Event &event) const override {
        if (flow.HandleReset(event) || event.kind != EventKind::Advance) {
            return;
        }
        const float speed = flow.recovery_ ? 1.0f / .55f : 3.0f;
        flow.alpha_ = (std::min)(1.0f, flow.alpha_ + speed * event.delta);
        if (flow.alpha_ >= 1) {
            flow.covered_ = true;
            flow.holdRemaining_ = .12f;
            flow.Change(Phase::HoldingBlack);
        }
    }
    Phase Kind() const override { return Phase::FadingOut; }
};
class SceneFadeFlow::CoveredState final : public State {
  public:
    // 遷移先の初回描画を黒の裏で済ませ、保持時間が終わったら開く。
    void Handle(SceneFadeFlow &flow, const Event &event) const override {
        if (flow.HandleReset(event) || event.kind != EventKind::Advance) {
            return;
        }
        flow.alpha_ = 1;
        flow.holdRemaining_ -= event.delta;
        if (flow.holdRemaining_ <= 0) {
            flow.Change(Phase::FadingIn);
        }
    }
    Phase Kind() const override { return Phase::HoldingBlack; }
};
class SceneFadeFlow::OpeningState final : public State {
  public:
    // 開き切ったら停止状態へ戻る。途中の開始要求は無視する。
    void Handle(SceneFadeFlow &flow, const Event &event) const override {
        if (flow.HandleReset(event) || event.kind != EventKind::Advance) {
            return;
        }
        const float speed = flow.recovery_ ? 1.0f / .65f : 3.0f;
        flow.alpha_ = (std::max)(0.0f, flow.alpha_ - speed * event.delta);
        if (flow.alpha_ <= 0) {
            flow.Change(Phase::None);
        }
    }
    Phase Kind() const override { return Phase::FadingIn; }
};

// 状態クラスは不変で共有し、進行時間と透明度はFlowごとに持つ。
const SceneFadeFlow::State &SceneFadeFlow::StateFor(Phase phase) {
    static const IdleState idle;
    static const ClosingState closing;
    static const CoveredState covered;
    static const OpeningState opening;
    switch (phase) {
    case Phase::None:
        return idle;
    case Phase::FadingOut:
        return closing;
    case Phase::HoldingBlack:
        return covered;
    case Phase::FadingIn:
        return opening;
    }
    return idle;
}
const SceneFadeFlow::State &SceneFadeFlow::Current() const {
    return state_ ? *state_ : StateFor(Phase::None);
}
void SceneFadeFlow::Change(Phase phase) {
    state_ = &StateFor(phase);
}
// 取消・検証初期化はどの状態からでも同じ規則で停止状態へ戻す。
bool SceneFadeFlow::HandleReset(const Event &event) {
    if (event.kind != EventKind::Reset) {
        return false;
    }
    alpha_ = holdRemaining_ = 0;
    recovery_ = covered_ = false;
    Change(Phase::None);
    return true;
}
void SceneFadeFlow::Begin(bool recovery) {
    Current().Handle(*this, {EventKind::Begin, 0, recovery});
}
// 一フレームに一状態だけ進め、時間の余りで保持状態を飛ばさない。
bool SceneFadeFlow::Advance(float deltaTime) {
    covered_ = false;
    const float delta = std::isfinite(deltaTime) ? std::clamp(deltaTime, 0.0f, .1f) : 0;
    Current().Handle(*this, {EventKind::Advance, delta});
    return covered_;
}
void SceneFadeFlow::Reset() {
    Current().Handle(*this, {EventKind::Reset});
}
SceneFadeFlow::Phase SceneFadeFlow::CurrentPhase() const {
    return Current().Kind();
}
