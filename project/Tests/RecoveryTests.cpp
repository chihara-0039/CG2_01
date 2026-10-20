// GPUを使わない選択処理と、実際のSceneManagerのライフサイクルを検証する。
// ProbeSceneは描画・入力の代替であり、DirectXの実行確認ではない。
#include "StageSelection.h"
#include "SceneManager.h"
#include "SceneFactory.h"
#include "BaseScene.h"
#include "RecoveryTransitionMotion.h"
#include "StartIntroTimeline.h"
#include <cmath>
#include <cassert>
#include <iostream>
#include <vector>

class GameRuntime {
public:
    std::vector<SceneType> entered;
    std::vector<SceneType> exited;
    int updates = 0;
    int draws = 0;
};

class ProbeScene final : public BaseScene {
public:
    explicit ProbeScene(SceneType type) : type_(type) {}
    void Initialize(GameRuntime& game) override { game.entered.push_back(type_); }
    void Update(GameRuntime& game, const SceneUpdateContext&) override { ++game.updates; }
    void Draw(GameRuntime& game) override { ++game.draws; }
    void Finalize(GameRuntime& game) override { game.exited.push_back(type_); }
private:
    SceneType type_;
};

std::unique_ptr<BaseScene> SceneFactory::CreateScene(SceneType type) const {
    return std::make_unique<ProbeScene>(type);
}

int main() {
    const float cueTimes[] = {0, 1, 2, 3, 4};
    for (int cue = 0; cue < 5; ++cue) {
        const auto frame = StartIntroTimeline::Evaluate(cueTimes[cue]);
        assert(frame.cue == cue);
        assert(frame.blocksInput == (cue < 4));
        assert(!frame.finished);
        if (cue > 0) {
            assert(cueTimes[cue]-cueTimes[cue-1] == 1.0f);
            assert(StartIntroTimeline::Evaluate(cueTimes[cue]-.001f).cue == cue-1);
        }
    }
    assert(StartIntroTimeline::Evaluate(-5).cue == 0);
    assert(StartIntroTimeline::Evaluate(StartIntroTimeline::kGoTime-.001f).blocksInput);
    assert(!StartIntroTimeline::Evaluate(StartIntroTimeline::kGoTime).blocksInput);
    const auto finalIntro = StartIntroTimeline::Evaluate(StartIntroTimeline::kDuration);
    assert(finalIntro.finished && finalIntro.alpha == 0 && finalIntro.letterbox == 0);
    assert(finalIntro.cameraBlend == 1);
    // 30/60/144fpsのサンプルでも切替は同じ実時間の拍で決まる。
    for (float step : {1.0f/30, 1.0f/60, 1.0f/144, .15f}) {
        int previousCue = 0;
        for (float time = 0; time < StartIntroTimeline::kDuration; time += step) {
            const auto frame = StartIntroTimeline::Evaluate(time);
            if (frame.cue != previousCue) {
                assert(frame.cue == previousCue+1);
                assert(time >= cueTimes[frame.cue]);
                assert(time < cueTimes[frame.cue]+step+.0001f);
                previousCue = frame.cue;
            }
        }
        assert(previousCue == 4);
    }
    for (int sample = 0; sample <= 500; ++sample) {
        const auto frame = StartIntroTimeline::Evaluate(sample*.01f);
        assert(frame.cue >= 0 && frame.cue <= 4);
        assert(frame.alpha >= 0 && frame.alpha <= 1);
        assert(frame.cameraBlend >= 0 && frame.cameraBlend <= 1);
        assert(frame.scale >= 1 && frame.scale < 1.6f);
    }
    std::cout << "Start intro: cue boundaries, input release, 501 timing samples passed\n";
    // 閉幕時の画面被覆と、黒画面の裏で装飾光が消えることを数値で確認する。
    for (SceneType target : {SceneType::Title, SceneType::StageSelect, SceneType::GamePlay, SceneType::GameClear}) {
        assert(RecoveryTransitionMotion::IsContestScene(target));
        float previousSize = 0.0f;
        for (int step = 0; step <= 100; ++step) {
            const auto frame = RecoveryTransitionMotion::Evaluate(step / 100.0f, false, target);
            assert(frame.size >= previousSize);
            assert(frame.accentAlpha >= 0.0f && frame.accentAlpha <= 1.0f);
            previousSize = frame.size;
        }
        const auto closed = RecoveryTransitionMotion::Evaluate(1.0f, false, target);
        const auto opening = RecoveryTransitionMotion::Evaluate(1.0f, true, target);
        assert(closed.rotation == opening.rotation);
        assert(closed.accentAlpha == 0.0f);
        for (float x : {-640.0f, 640.0f}) {
            for (float y : {-360.0f, 360.0f}) {
                const float localX = x * std::cos(closed.rotation) + y * std::sin(closed.rotation);
                const float localY = -x * std::sin(closed.rotation) + y * std::cos(closed.rotation);
                assert(std::abs(localX) < closed.size / 2 && std::abs(localY) < closed.size / 2);
            }
        }
        assert(RecoveryTransitionMotion::Evaluate(-1.0f, true, target).size == 0.0f);
        assert(RecoveryTransitionMotion::Evaluate(2.0f, false, target).size == closed.size);
    }
    assert(!RecoveryTransitionMotion::IsContestScene(SceneType::StageEditor));
    assert(!RecoveryTransitionMotion::IsContestScene(SceneType::GamePlayBlockPlace));
    std::cout << "Recovery wipe: four routes, coverage, continuity and 404 motion samples passed\n";
    using StageSelection::Direction;
    assert(StageSelection::Move(0, Direction::Up, 6) == 4);
    assert(StageSelection::Move(0, Direction::Down, 6) == 5);
    assert(StageSelection::Move(0, Direction::Left, 6) == 3);
    assert(StageSelection::Move(0, Direction::Right, 6) == 1);
    assert(StageSelection::Move(0, Direction::Up, 1) == 0);
    assert(StageSelection::Move(-1, Direction::Right, 6) == 1);
    assert(StageSelection::Move(6, Direction::Down, 6) == 2);
    assert(StageSelection::Move(0, Direction::Left, 8) == 7);
    assert(StageSelection::Move(7, Direction::Right, 8) == 0);
    int cases = 0;
    for (int count = 1; count <= 12; ++count) {
        for (int current = -2; current <= count+2; ++current) {
            for (int direction = 0; direction < 4; ++direction) {
                const int next = StageSelection::Move(current, static_cast<Direction>(direction), count);
                assert(next >= 0 && next < count);
                ++cases;
            }
        }
    }
    // 六面すべてが到達可能であることも確認する。
    bool reached[6] = {true,false,false,false,false,false};
    for (int round = 0; round < 6; ++round) {
        for (int current = 0; current < 6; ++current) {
            if (!reached[current]) continue;
            for (int direction = 0; direction < 4; ++direction) {
                reached[StageSelection::Move(current, static_cast<Direction>(direction), 6)] = true;
            }
        }
    }
    for (bool value : reached) assert(value);
    assert(StageSelection::Move(5, Direction::Right, 0) == 0);

    const std::vector<SceneType> route = {
        SceneType::Title, SceneType::StageSelect, SceneType::GamePlay,
        SceneType::GameClear, SceneType::Title, SceneType::StageSelect,
        SceneType::Title, SceneType::StageSelect, SceneType::GamePlay,
        SceneType::StageSelect
    };
    GameRuntime game;
    SceneFactory factory;
    SceneManager* manager = SceneManager::GetInstance();
    manager->Initialize(&factory, route.front(), game);
    const Matrix4x4 light{};
    SceneUpdateContext context{light};
    for (size_t index = 0; index < route.size(); ++index) {
        manager->ChangeScene(route[index], game);
        assert(manager->GetCurrentSceneType() == route[index]);
        assert(game.entered.size() == index+1); // 同じシーンは初期化し直さない
        manager->Update(game, context);
        manager->Draw(game);
    }
    manager->Finalize(game);
    assert(game.entered == route);
    assert(game.exited == route);
    assert(game.updates == static_cast<int>(route.size()));
    assert(game.draws == static_cast<int>(route.size()));
    manager->Finalize(game); // 二重終了でFinalizeを重ねない
    assert(game.exited == route);
    std::cout << "Stage selection: " << cases << " range cases; six faces reachable\n"
              << "SceneManager: " << route.size() << " scene entries/exits passed (probe scenes)\n";
}
