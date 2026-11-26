#include "GamePlayBlockPlaceScene.h"
#include "GameRuntime.h"
// シーン入場時の共通サービスを準備する。
void GamePlayBlockPlaceScene::Initialize(GameRuntime &game) {
    game.OnSceneEntered(SceneType::GamePlayBlockPlace);
}
// 配置完了・在庫切れ・取消入力は、このシーンがプレイへの復帰条件として判断する。
void GamePlayBlockPlaceScene::Update(GameRuntime &game, const SceneUpdateContext &) {
    const bool empty = game.RunGamePlayBlockPlaceScene();
    const auto *input = game.GetInput();
    if (empty || input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_B) ||
        input->TriggerControllerButton(XINPUT_GAMEPAD_B)) {
        game.FinishBlockPlacement();
        game.RequestSceneChange(SceneType::GamePlay);
    }
}
// シーン所有の表示を終了し、共通サービスへ退場を通知する。
void GamePlayBlockPlaceScene::Finalize(GameRuntime &game) {
    game.OnSceneExited(SceneType::GamePlayBlockPlace);
}
