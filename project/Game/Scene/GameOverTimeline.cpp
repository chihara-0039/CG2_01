#include "GameOverTimeline.h"
#include <algorithm>
#include <cmath>
namespace {
float Ease(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
}
// 経過時間から、失敗演出の暗転・フラッシュ・メニュー表示量を計算する。
GameOverTimeline::Frame GameOverTimeline::Evaluate(float seconds) {
    seconds = (std::max)(seconds, 0.0f);
    return {Ease((seconds - kHitStop) / .8f),
            Ease((seconds - kHitStop) / .65f),
            800 * (1 - Ease((seconds - 1.2f) / .95f)),
            0.0f,
            Ease((seconds - 2.75f) / .4f),
            seconds >= kMenuTime};
}
float GameOverTimeline::RetryRadius(float seconds) {
    return 800 * Ease(seconds / kRevealTime);
}

// 状態は入力と時間を受け取り、遷移先を自分で決める。保持するのは共有可能な振る舞いだけ。
class GameOverTimeline::Flow::State {
  public:
    virtual ~State() = default;
    virtual Action Handle(Flow &flow, const Event &event) const = 0;
    virtual Phase Kind() const = 0;
};
class GameOverTimeline::Flow::InactiveState final : public State {
  public:
    // 停止中は開始・取消だけを受け取り、時間や選択入力は進めない。
    Action Handle(Flow &flow, const Event &event) const override {
        flow.HandleControl(event);
        return Action::None;
    }
    Phase Kind() const override { return Phase::Inactive; }
};
class GameOverTimeline::Flow::FailureState final : public State {
  public:
    // 失敗の芝居が終わった時刻で、選択入力を受け付ける状態へ移る。
    Action Handle(Flow &flow, const Event &event) const override {
        if (flow.HandleControl(event)) {
            return Action::None;
        }
        if (event.kind == EventKind::Advance) {
            flow.elapsed_ += event.delta;
            if (flow.elapsed_ >= kMenuTime) {
                flow.Change(Phase::Menu);
            }
        }
        return Action::None;
    }
    Phase Kind() const override { return Phase::Failure; }
};
class GameOverTimeline::Flow::MenuState final : public State {
  public:
    // 選択待ちだけが決定を解釈し、再挑戦の幕開けかステージ選択への退出を選ぶ。
    Action Handle(Flow &flow, const Event &event) const override {
        if (flow.HandleControl(event)) {
            return Action::None;
        }
        if (event.kind == EventKind::Advance) {
            flow.elapsed_ += event.delta;
        }
        if (event.kind == EventKind::MoveSelection) {
            flow.selected_ = 1 - flow.selected_;
        }
        if (event.kind == EventKind::Confirm) {
            if (event.back || flow.selected_ == 1) {
                flow.Change(Phase::Leaving);
                return Action::StageSelect;
            }
            flow.revealElapsed_ = 0;
            flow.Change(Phase::Reveal);
            return Action::Retry;
        }
        return Action::None;
    }
    Phase Kind() const override { return Phase::Menu; }
};
class GameOverTimeline::Flow::RevealState final : public State {
  public:
    // 再挑戦の幕が開き切ったら通常プレイへ戻り、途中の二重決定は受け付けない。
    Action Handle(Flow &flow, const Event &event) const override {
        if (flow.HandleControl(event)) {
            return Action::None;
        }
        if (event.kind == EventKind::Advance) {
            flow.revealElapsed_ += event.delta;
            if (flow.revealElapsed_ >= kRevealTime) {
                flow.Change(Phase::Inactive);
            }
        }
        return Action::None;
    }
    Phase Kind() const override { return Phase::Reveal; }
};
class GameOverTimeline::Flow::LeavingState final : public State {
  public:
    // シーン退出待ちは入力を消費済みとして扱い、新しい開始・取消だけを受け付ける。
    Action Handle(Flow &flow, const Event &event) const override {
        flow.HandleControl(event);
        return Action::None;
    }
    Phase Kind() const override { return Phase::Leaving; }
};
// 状態オブジェクトを共有し、切替のたびに生成・破棄しない。
const GameOverTimeline::Flow::State &GameOverTimeline::Flow::StateFor(Phase phase) {
    static const InactiveState inactive;
    static const FailureState failure;
    static const MenuState menu;
    static const RevealState reveal;
    static const LeavingState leaving;
    switch (phase) {
    case Phase::Inactive:
        return inactive;
    case Phase::Failure:
        return failure;
    case Phase::Menu:
        return menu;
    case Phase::Reveal:
        return reveal;
    case Phase::Leaving:
        return leaving;
    }
    return inactive;
}
const GameOverTimeline::Flow::State &GameOverTimeline::Flow::Current() const {
    return state_ ? *state_ : StateFor(Phase::Inactive);
}
void GameOverTimeline::Flow::Change(Phase phase) {
    state_ = &StateFor(phase);
}
// どの状態からでも開始と取消を扱う。状態側がこの共通処理を呼んで遷移させる。
bool GameOverTimeline::Flow::HandleControl(const Event &event) {
    if (event.kind == EventKind::Begin) {
        elapsed_ = revealElapsed_ = 0;
        selected_ = 0;
        Change(Phase::Failure);
        return true;
    }
    if (event.kind == EventKind::Cancel) {
        Change(Phase::Inactive);
        return true;
    }
    return false;
}
void GameOverTimeline::Flow::Begin() {
    Current().Handle(*this, {EventKind::Begin});
}
void GameOverTimeline::Flow::Advance(float delta) {
    Current().Handle(*this, {EventKind::Advance, (std::max)(delta, 0.0f)});
}
void GameOverTimeline::Flow::MoveSelection() {
    Current().Handle(*this, {EventKind::MoveSelection});
}
GameOverTimeline::Action GameOverTimeline::Flow::Confirm(bool back) {
    return Current().Handle(*this, {EventKind::Confirm, 0, back});
}
void GameOverTimeline::Flow::Cancel() {
    Current().Handle(*this, {EventKind::Cancel});
}
bool GameOverTimeline::Flow::Active() const {
    return Current().Kind() != Phase::Inactive;
}
bool GameOverTimeline::Flow::Revealing() const {
    return Current().Kind() == Phase::Reveal;
}
bool GameOverTimeline::Flow::Leaving() const {
    return Current().Kind() == Phase::Leaving;
}
bool GameOverTimeline::Flow::MenuReady() const {
    return Current().Kind() == Phase::Menu;
}

// 文字ごとの表示開始時刻から、登場位置・回転・大きさを計算する。
GameOverTimeline::Letter GameOverTimeline::EvaluateLetter(float seconds, int index) {
    const float t =
        std::clamp((seconds - 2.1f - static_cast<float>(std::clamp(index, 0, 7)) * .055f) / .42f, 0.0f, 1.0f);
    const float ease = 1 - std::pow(1 - t, 3.0f);
    const float rebound = std::sin(t * 3.14159265359f) * (1 - t);
    return {Ease(t * 2), 35 * (1 - ease) - 18 * rebound, 1 + .5f * (1 - ease),
            (index % 2 == 0 ? -.15f : .15f) * (1 - ease)};
}
// 失敗演出の進み具合から、プレイヤーとカメラの姿勢を計算する。
GameOverTimeline::Pose GameOverTimeline::EvaluatePose(float seconds, bool fall, const Vector3 &initial,
                                                      const Vector3 &rotation, const Vector3 &cameraPosition,
                                                      const Vector3 &cameraRotation, float fov, float fallVelocity) {
    const auto frame = Evaluate(seconds);
    const float m = frame.motion, b = fall ? Ease(seconds / .65f) : frame.cameraBlend;
    const float yaw = cameraRotation.y;
    Vector3 position = initial, body = rotation;
    // 敵接触だけヒットストップを使う。落下は直前の速度と通常プレイと同じ重力を引き継ぐ。
    const float kick = fall ? 0.0f : 1.2f * m;
    position.x -= std::sin(yaw) * kick;
    position.z -= std::cos(yaw) * kick;
    if (fall) {
        // 円形暗転が閉じるまで動かし、メニュー裏で座標が増え続けることは避ける。
        const float time = std::clamp(seconds, 0.0f, 2.15f);
        const float velocity = std::isfinite(fallVelocity) ? (std::min)(fallVelocity, 0.0f) : -6.0f;
        position.y += velocity * time - 27.0f * time * time;
    } else {
        position.y += 1.4f * std::sin(m * 3.14159265359f) - 2 * Ease((seconds - .95f) / 1.0f);
    }
    body.z += fall ? 0.0f : .45f * std::sin(m * 3.14159265359f);
    body.x += fall ? 0.0f : -.35f * m;
    const float orbit = yaw + .15f * m;
    const Vector3 target{position.x, initial.y + (position.y - initial.y) * (fall ? .65f : 1.0f) + .4f, position.z};
    const Vector3 end{initial.x - std::sin(orbit) * 10, initial.y + 5, initial.z - std::cos(orbit) * 10};
    const float shakeTime = (std::max)(seconds - kHitStop, 0.0f);
    const float shake = fall || seconds <= kHitStop ? 0.0f : .055f * std::exp(-shakeTime * 9);
    const Vector3 cam{cameraPosition.x + (end.x - cameraPosition.x) * b + std::sin(shakeTime * 93) * shake,
                      cameraPosition.y + (end.y - cameraPosition.y) * b + std::sin(shakeTime * 77) * shake,
                      cameraPosition.z + (end.z - cameraPosition.z) * b};
    const Vector3 d{target.x - cam.x, target.y - cam.y, target.z - cam.z};
    const float pitch = -std::atan2(d.y, std::sqrt(d.x * d.x + d.z * d.z));
    const float aimYaw = std::atan2(d.x, d.z);
    const Vector3 aim{cameraRotation.x + (pitch - cameraRotation.x) * b,
                      cameraRotation.y + std::remainder(aimYaw - cameraRotation.y, 6.28318530718f) * b,
                      cameraRotation.z};
    return {position, body, cam, aim, fov + (.72f - fov) * b, 1 - .75f * Ease((seconds - .65f) / 1.4f)};
}
