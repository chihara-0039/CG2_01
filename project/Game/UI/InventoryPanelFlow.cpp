#include "InventoryPanelFlow.h"
#include <cmath>

class InventoryPanelFlow::State {
  public:
    virtual ~State() = default;
    virtual void Handle(InventoryPanelFlow &flow, Event event) const = 0;
    virtual Phase Kind() const = 0;
};
class InventoryPanelFlow::ClosedState final : public State {
  public:
    // 閉じた状態での切替入力は、開く途中へ進める。
    void Handle(InventoryPanelFlow &flow, Event event) const override {
        if (flow.HandleClose(event)) {
            return;
        }
        if (event == Event::Toggle) {
            flow.Change(Phase::Opening);
        }
    }
    Phase Kind() const override { return Phase::Closed; }
};
class InventoryPanelFlow::OpeningState final : public State {
  public:
    // 到着時は開いた状態へ、途中の逆入力では現在位置から閉じる状態へ移る。
    void Handle(InventoryPanelFlow &flow, Event event) const override {
        if (flow.HandleClose(event)) {
            return;
        }
        if (event == Event::Toggle) {
            flow.Change(Phase::Closing);
        }
        if (event == Event::Advance) {
            flow.x_ += (flow.openedX_ - flow.x_) * .15f;
            if (std::abs(flow.x_ - flow.openedX_) < 1) {
                flow.x_ = flow.openedX_;
                flow.Change(Phase::Opened);
            }
        }
    }
    Phase Kind() const override { return Phase::Opening; }
};
class InventoryPanelFlow::OpenedState final : public State {
  public:
    // 開き切った状態では、切替入力で閉じる途中へ進める。
    void Handle(InventoryPanelFlow &flow, Event event) const override {
        if (flow.HandleClose(event)) {
            return;
        }
        if (event == Event::Toggle) {
            flow.Change(Phase::Closing);
        }
    }
    Phase Kind() const override { return Phase::Opened; }
};
class InventoryPanelFlow::ClosingState final : public State {
  public:
    // 閉じ切ったら非表示へ、途中の逆入力では現在位置から開き直す。
    void Handle(InventoryPanelFlow &flow, Event event) const override {
        if (flow.HandleClose(event)) {
            return;
        }
        if (event == Event::Toggle) {
            flow.Change(Phase::Opening);
        }
        if (event == Event::Advance) {
            flow.x_ += (flow.closedX_ - flow.x_) * .15f;
            if (std::abs(flow.x_ - flow.closedX_) < 1) {
                flow.x_ = flow.closedX_;
                flow.Change(Phase::Closed);
            }
        }
    }
    Phase Kind() const override { return Phase::Closing; }
};
// 振る舞いは共有するが、座標と遷移先は各Flowが保持する。
const InventoryPanelFlow::State &InventoryPanelFlow::StateFor(Phase phase) {
    static const ClosedState closed;
    static const OpeningState opening;
    static const OpenedState opened;
    static const ClosingState closing;
    switch (phase) {
    case Phase::Closed:
        return closed;
    case Phase::Opening:
        return opening;
    case Phase::Opened:
        return opened;
    case Phase::Closing:
        return closing;
    }
    return closed;
}
const InventoryPanelFlow::State &InventoryPanelFlow::Current() const {
    return state_ ? *state_ : StateFor(Phase::Closed);
}
void InventoryPanelFlow::Change(Phase phase) {
    state_ = &StateFor(phase);
}
// ショートカット選択やステージ初期化では、どの状態からでも即座に閉じる。
bool InventoryPanelFlow::HandleClose(Event event) {
    if (event != Event::Close) {
        return false;
    }
    x_ = closedX_;
    Change(Phase::Closed);
    return true;
}
// 不正な座標では既存の状態を保ち、有効な設定では閉じた位置から始める。
bool InventoryPanelFlow::Configure(float closedX, float openedX) {
    if (!std::isfinite(closedX) || !std::isfinite(openedX)) {
        return false;
    }
    closedX_ = closedX;
    openedX_ = openedX;
    Close();
    return true;
}
void InventoryPanelFlow::Toggle() {
    Current().Handle(*this, Event::Toggle);
}
void InventoryPanelFlow::Advance() {
    Current().Handle(*this, Event::Advance);
}
void InventoryPanelFlow::Close() {
    Current().Handle(*this, Event::Close);
}
InventoryPanelFlow::Phase InventoryPanelFlow::CurrentPhase() const {
    return Current().Kind();
}
