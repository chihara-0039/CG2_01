#include "StarGetTimeline.h"
#include "GamePlayScene.h"
#include "GameRuntime.h"
// シーン入場時の共通サービスを準備する。
void GamePlayScene::Initialize(GameRuntime &game) {
    game.OnSceneEntered(SceneType::GamePlay);
}
// シーン自身が終了条件を判断し、遷移を要求する。
void GamePlayScene::Update(GameRuntime &game, const SceneUpdateContext &) {
    if (game.RunGamePlayScene()) {
        game.PrepareReturnToStageSelect();
        game.RequestSceneChange(SceneType::StageSelect);
        return;
    }
    if (game.HasReachedGoal() && !game.IsDioramaCapture() && game.GoalCelebrationTime() >= StarGet::kDuration) {
        game.RequestSceneChange(SceneType::GameClear);
    }
}
// シーン所有の表示を終了し、共通サービスへ退場を通知する。
void GamePlayScene::Finalize(GameRuntime &game) {
    game.OnSceneExited(SceneType::GamePlay);
}
