#pragma once
#include "../Game/Scene/GameOverTimeline.h"
#include "../Game/Scene/SceneFadeFlow.h"
#include "../Game/UI/InventoryPanelFlow.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>

namespace StatePatternChecks {
// 失敗演出・メニュー・再挑戦の順序と、各状態で受け付けない入力を確認する。
inline void CheckFailureFlow() {
    using namespace GameOverTimeline;
    for (int fps : {30, 60, 144}) {
        Flow flow;
        assert(!flow.Active());
        flow.Begin();
        assert(flow.Active() && !flow.MenuReady());
        flow.Advance(-1);
        assert(flow.Elapsed() == 0);
        for (int i = 0; i < fps * 4 && !flow.MenuReady(); ++i) {
            assert(flow.Confirm(false) == Action::None);
            flow.MoveSelection();
            assert(flow.Selected() == 0);
            flow.Advance(1.0f / fps);
        }
        assert(flow.MenuReady());
        Flow separate = flow;
        assert(flow.Confirm(false) == Action::Retry && flow.Revealing());
        assert(flow.Confirm(false) == Action::None && flow.Confirm(true) == Action::None);
        separate.MoveSelection();
        assert(separate.MenuReady() && separate.Selected() == 1 && flow.Selected() == 0);
        assert(separate.Confirm(false) == Action::StageSelect && separate.Leaving());
        flow.Advance(kRevealTime - .001f);
        assert(flow.Active());
        flow.Advance(.002f);
        assert(!flow.Active());
        flow.Begin();
        flow.Advance(kMenuTime);
        assert(flow.MenuReady() && flow.Confirm(true) == Action::StageSelect);
        flow.Cancel();
        assert(!flow.Active());
        flow.Begin();
        assert(flow.Elapsed() == 0 && flow.RevealElapsed() == 0 && flow.Selected() == 0);
    }
}

// フェードの各状態は一フレームに一つだけ進み、読込要求は黒到達時の一回だけになる。
inline void CheckSceneFade() {
    using Phase = SceneFadeFlow::Phase;
    for (int fps : {30, 60, 144}) {
        for (bool recovery : {false, true}) {
            SceneFadeFlow flow;
            assert(flow.CurrentPhase() == Phase::None && flow.Alpha() == 0);
            flow.Begin(recovery);
            float previous = 0;
            int covered = 0, holdFrames = 0, closingFrames = 0, openingFrames = 0, ticks = 0;
            while (flow.CurrentPhase() != Phase::None && ticks++ < fps * 4) {
                const auto before = flow.CurrentPhase();
                const float old = flow.Alpha();
                flow.Begin(!recovery); // 進行中の再予約では向きや設定を変えない。
                assert(flow.CurrentPhase() == before && flow.Alpha() == old);
                const bool load = flow.Advance(1.0f / fps);
                covered += load;
                if (before == Phase::FadingOut) {
                    ++closingFrames;
                    assert(flow.Alpha() >= previous);
                } else if (before == Phase::HoldingBlack) {
                    assert(flow.Alpha() == 1);
                    ++holdFrames;
                } else {
                    ++openingFrames;
                    assert(flow.Alpha() <= previous);
                }
                if (load) {
                    assert(flow.CurrentPhase() == Phase::HoldingBlack && flow.Alpha() == 1);
                }
                assert(flow.Alpha() >= 0 && flow.Alpha() <= 1);
                previous = flow.Alpha();
            }
            assert(ticks < fps * 4 && covered == 1 && holdFrames >= 1 && flow.Alpha() == 0);
            const float step = 1.0f / fps;
            assert(std::abs(closingFrames * step - (recovery ? .55f : 1.0f / 3)) <= step + .0001f);
            assert(std::abs(openingFrames * step - (recovery ? .65f : 1.0f / 3)) <= step + .0001f);
            assert(holdFrames * step >= .12f - .0001f && holdFrames * step <= .12f + step + .0001f);
            flow.Begin(recovery);
            flow.Advance(-1);
            flow.Advance(std::numeric_limits<float>::quiet_NaN());
            assert(flow.Alpha() == 0);
            auto copy = flow;
            copy.Advance(.1f);
            assert(copy.Alpha() > 0 && flow.Alpha() == 0);
            copy.Reset();
            assert(copy.CurrentPhase() == Phase::None && copy.Alpha() == 0);
        }
    }
}

// パネルの開閉と途中の折返しを、座標の飛びや在庫UIなしで確認する。
inline void CheckInventoryPanel() {
    using Phase = InventoryPanelFlow::Phase;
    InventoryPanelFlow flow;
    assert(flow.Configure(1235, 960));
    assert(flow.CurrentPhase() == Phase::Closed && flow.X() == 1235);
    flow.Toggle();
    flow.Advance();
    assert(flow.CurrentPhase() == Phase::Opening && std::abs(flow.X() - 1193.75f) < .001f);
    const float turning = flow.X();
    flow.Toggle();
    assert(flow.CurrentPhase() == Phase::Closing && flow.X() == turning);
    flow.Advance();
    assert(flow.X() > turning);
    flow.Toggle();
    for (int i = 0; i < 100 && flow.CurrentPhase() != Phase::Opened; ++i) {
        flow.Advance();
    }
    assert(flow.CurrentPhase() == Phase::Opened && flow.X() == 960);
    auto separate = flow;
    separate.Toggle();
    separate.Advance();
    assert(flow.X() == 960 && separate.X() > 960);
    flow.Toggle();
    for (int i = 0; i < 100 && flow.CurrentPhase() != Phase::Closed; ++i) {
        flow.Advance();
    }
    assert(flow.CurrentPhase() == Phase::Closed && flow.X() == 1235);
    flow.Toggle();
    flow.Close();
    assert(flow.CurrentPhase() == Phase::Closed && flow.X() == 1235);
    assert(!flow.Configure(std::numeric_limits<float>::infinity(), 0) && flow.X() == 1235);
}

// 実装した3系統の状態クラス経由で、実ゲームと同じ公開操作を検証する。
inline void Run() {
    CheckFailureFlow();
    CheckSceneFade();
    CheckInventoryPanel();
    std::cout << "PASS: state-owned failure/retry, scene fade and reversible inventory transitions\n";
}
}
