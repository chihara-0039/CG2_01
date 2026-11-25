#pragma once

// 道具袋の開閉だけを所有するState Pattern。選択・在庫・描画はUI側へ残す。
class InventoryPanelFlow {
  public:
    enum class Phase {
        Closed,
        Opening,
        Opened,
        Closing
    };
    bool Configure(float closedX, float openedX);
    void Toggle();
    void Advance();
    void Close();
    Phase CurrentPhase() const;
    float X() const { return x_; }

  private:
    enum class Event {
        Toggle,
        Advance,
        Close
    };
    class State;
    class ClosedState;
    class OpeningState;
    class OpenedState;
    class ClosingState;
    const State &Current() const;
    static const State &StateFor(Phase phase);
    void Change(Phase phase);
    bool HandleClose(Event event);
    const State *state_ = nullptr;
    float x_ = 0, closedX_ = 0, openedX_ = 0;
};
