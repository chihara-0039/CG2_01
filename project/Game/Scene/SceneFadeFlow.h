#pragma once

// フェードの進行と遷移を所有する。シーンの読込・描画そのものは呼出元へ任せる。
class SceneFadeFlow {
  public:
    enum class Phase {
        None,
        FadingOut,
        HoldingBlack,
        FadingIn
    };
    void Begin(bool recovery);
    // 完全な黒へ到達したフレームだけtrueを返し、読込を一度だけ依頼する。
    bool Advance(float deltaTime);
    void Reset();
    Phase CurrentPhase() const;
    float Alpha() const { return alpha_; }

  private:
    enum class EventKind {
        Begin,
        Advance,
        Reset
    };
    struct Event {
        EventKind kind;
        float delta = 0;
        bool recovery = false;
    };
    class State;
    class IdleState;
    class ClosingState;
    class CoveredState;
    class OpeningState;
    const State &Current() const;
    static const State &StateFor(Phase phase);
    void Change(Phase phase);
    bool HandleReset(const Event &event);
    const State *state_ = nullptr;
    float alpha_ = 0, holdRemaining_ = 0;
    bool recovery_ = false, covered_ = false;
};
