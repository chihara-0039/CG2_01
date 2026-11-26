#include "DebugViewScene.h"
#include "GameRuntime.h"
// シーン入場時の共通サービスを準備する。
void DebugViewScene::Initialize(GameRuntime &game) { game.OnSceneEntered(SceneType::DebugView); }
// シーン固有の更新を行う。開発ツールの画面切替は外部のメニューコマンドで要求される。
void DebugViewScene::Update(GameRuntime &game, const SceneUpdateContext &context) {
    (void)context;
    game.RunDebugViewScene();
}
// シーン所有の表示を終了し、共通サービスへ退場を通知する。
void DebugViewScene::Finalize(GameRuntime &game) { game.OnSceneExited(SceneType::DebugView); }
