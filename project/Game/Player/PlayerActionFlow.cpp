#include "PlayerActionFlow.h"
namespace {
const IPlayerState &Idle();
const IPlayerState &Walk();
const IPlayerState &Run();
const IPlayerState &Jump();
const IPlayerState &Ladder();
// 接地状態で共通の、移動入力による遷移先を求める。
const IPlayerState &GroundChoice(const PlayerActionFacts &f) {
    if (!f.moving) {
        return Idle();
    }
    return f.running ? Run() : Walk();
}
class GroundState : public IPlayerState {
  public:
    // 地形センサーを優先し、接地中だけ歩行入力で状態を選ぶ。
    const IPlayerState &Next(const PlayerActionFacts &f) const override {
        if (f.ladder) {
            return Ladder();
        }
        if (f.jumpRequested || !f.grounded) {
            return Jump();
        }
        return GroundChoice(f);
    }
};
class PlayerStateIdle final : public GroundState {
  public:
    void Move(IPlayerActionBody &body) const override { body.MoveGround(.5f, false); }
    const char *Name() const override { return "Idle"; }
    std::vector<std::string> MotionHints() const override { return {"walk", "run", "sprint"}; }
    bool HoldPose() const override { return true; }
    float AnimationSpeed() const override { return 0; }
};
class PlayerStateWalk final : public GroundState {
  public:
    void Move(IPlayerActionBody &body) const override { body.MoveGround(.5f, false); }
    const char *Name() const override { return "Walk"; }
    std::vector<std::string> MotionHints() const override { return {"walk"}; }
};
class PlayerStateRun final : public GroundState {
  public:
    void Move(IPlayerActionBody &body) const override { body.MoveGround(.8f, false); }
    const char *Name() const override { return "Run"; }
    std::vector<std::string> MotionHints() const override { return {"run", "sprint"}; }
    std::vector<std::string> FallbackHints() const override { return {"walk"}; }
    float AnimationSpeed() const override { return 1.4f; }
};
class PlayerStateJump final : public IPlayerState {
  public:
    // 空中からは梯子の捕捉と着地だけで遷移し、移動入力だけでは接地状態へ戻さない。
    const IPlayerState &Next(const PlayerActionFacts &f) const override {
        if (f.ladder) {
            return Ladder();
        }
        if (f.grounded && !f.jumpRequested) {
            return GroundChoice(f);
        }
        return *this;
    }
    void Move(IPlayerActionBody &body) const override { body.MoveGround(body.ActionFacts().running ? .8f : .5f, body.ActionFacts().jumpRequested); }
    const char *Name() const override { return "Jump"; }
    std::vector<std::string> MotionHints() const override { return {"jump", "air", "fall"}; }
    std::vector<std::string> FallbackHints() const override { return {"walk", "run", "sprint"}; }
    bool CanHoldAfter() const override { return false; }
};
class PlayerStateLadder final : public IPlayerState {
  public:
    // 梯子から降りた時点の接地結果で、地上か空中の状態へ戻す。
    const IPlayerState &Next(const PlayerActionFacts &f) const override {
        if (f.ladder) {
            return *this;
        }
        if (!f.grounded) {
            return Jump();
        }
        return GroundChoice(f);
    }
    void Move(IPlayerActionBody &body) const override { body.MoveLadder(); }
    const char *Name() const override { return "Ladder"; }
    std::vector<std::string> MotionHints() const override { return {"ladder", "climb"}; }
    int FallbackMotion(int count) const override { return count >= 4 ? 3 : 0; }
    bool CanHoldAfter() const override { return false; }
};
const IPlayerState &Idle() {
    static const PlayerStateIdle s;
    return s;
}
const IPlayerState &Walk() {
    static const PlayerStateWalk s;
    return s;
}
const IPlayerState &Run() {
    static const PlayerStateRun s;
    return s;
}
const IPlayerState &Jump() {
    static const PlayerStateJump s;
    return s;
}
const IPlayerState &Ladder() {
    static const PlayerStateLadder s;
    return s;
}
}
const IPlayerState &PlayerActionFlow::Current() const {
    return state_ ? *state_ : Idle();
}
// 入力受付前と物理更新後の両方で状態自身に遷移を判断させる。
void PlayerActionFlow::Update(IPlayerActionBody &body) {
    state_ = &Current().Next(body.ActionFacts());
    state_->Move(body);
    state_ = &state_->Next(body.ActionFacts());
}
void PlayerActionFlow::Reset() {
    state_ = &Idle();
}
