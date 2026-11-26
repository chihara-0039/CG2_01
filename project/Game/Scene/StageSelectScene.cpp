#include "StageSelectScene.h"
#include "GameRuntime.h"
// シーン入場時の共通サービスを準備する。
void StageSelectScene::Initialize(GameRuntime &game) {
    game.OnSceneEntered(SceneType::StageSelect);
}
// シーン自身が終了条件を判断し、遷移を要求する。
void StageSelectScene::Update(GameRuntime &game, const SceneUpdateContext &) {
    const auto *input = game.GetInput();
    if (input->TriggerKey(DIK_ESCAPE) || input->TriggerControllerButton(XINPUT_GAMEPAD_B)) {
        game.RequestSceneChange(SceneType::Title);
        return;
    }
    if (game.RunStageSelectScene()) {
        game.RequestSceneChange(SceneType::GamePlay);
    }
}
// シーン所有の表示を終了し、共通サービスへ退場を通知する。
void StageSelectScene::Finalize(GameRuntime &game) {
    game.OnSceneExited(SceneType::StageSelect);
}
