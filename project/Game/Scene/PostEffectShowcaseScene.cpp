#include "PostEffectShowcaseScene.h"
#include "GameRuntime.h"
// シーン入場時の共通サービスを準備する。
void PostEffectShowcaseScene::Initialize(GameRuntime &game) { game.OnSceneEntered(SceneType::PostEffectShowcase); }
// シーン固有の更新を行う。開発ツールの画面切替は外部のメニューコマンドで要求される。
void PostEffectShowcaseScene::Update(GameRuntime &game, const SceneUpdateContext &context) {
    (void)context;
    game.RunPostEffectShowcaseScene();
}
// シーン所有の表示を終了し、共通サービスへ退場を通知する。
void PostEffectShowcaseScene::Finalize(GameRuntime &game) { game.OnSceneExited(SceneType::PostEffectShowcase); }
