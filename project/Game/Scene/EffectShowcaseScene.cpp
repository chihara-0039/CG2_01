#include "EffectShowcaseScene.h"
#include "GameRuntime.h"
// シーン入場時の共通サービスを準備する。
void EffectShowcaseScene::Initialize(GameRuntime &game) { game.OnSceneEntered(SceneType::EffectShowcase); }
// シーン固有の更新を行う。開発ツールの画面切替は外部のメニューコマンドで要求される。
void EffectShowcaseScene::Update(GameRuntime &game, const SceneUpdateContext &context) {
    (void)context;
    game.RunEffectShowcaseScene();
}
// シーン所有の表示を終了し、共通サービスへ退場を通知する。
void EffectShowcaseScene::Finalize(GameRuntime &game) { game.OnSceneExited(SceneType::EffectShowcase); }
