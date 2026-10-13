// GPUを使わない選択処理と、実際のSceneManagerのライフサイクルを検証する。
// ProbeSceneは描画・入力の代替であり、DirectXの実行確認ではない。
#include "StageSelection.h"
#include "SceneManager.h"
#include "SceneFactory.h"
#include "BaseScene.h"
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
