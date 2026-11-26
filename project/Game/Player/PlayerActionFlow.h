#pragma once
#include <vector>
#include <string>

struct PlayerActionFacts {
    bool ladder, grounded, moving, running;
    bool jumpRequested = false;
};
// 状態が必要とする物理操作だけを公開し、Playerの内部データは渡さない。
class IPlayerActionBody {
  public:
    virtual ~IPlayerActionBody() = default;
    virtual PlayerActionFacts ActionFacts() const = 0;
    virtual void MoveGround(float inputScale, bool jumpRequested) = 0;
    virtual void MoveLadder() = 0;
};
class IPlayerState {
  public:
    virtual ~IPlayerState() = default;
    virtual const IPlayerState &Next(const PlayerActionFacts &facts) const = 0;
    virtual void Move(IPlayerActionBody &body) const = 0;
    virtual const char *Name() const = 0;
    virtual std::vector<std::string> MotionHints() const = 0;
    virtual std::vector<std::string> FallbackHints() const { return {}; }
    virtual float AnimationSpeed() const { return 1; }
    virtual int FallbackMotion(int count) const { return count >= 2 ? 1 : 0; }
    virtual bool HoldPose() const { return false; }
    virtual bool CanHoldAfter() const { return true; }
};
// 本体は状態を保持して呼び出すだけ。次の状態は各状態クラスが選ぶ。
class PlayerActionFlow {
  public:
    void Update(IPlayerActionBody &body);
    void Reset();
    const IPlayerState &Current() const;

  private:
    const IPlayerState *state_ = nullptr;
};
