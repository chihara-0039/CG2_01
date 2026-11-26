#include "SkinningEditorScene.h"
#include "GameRuntime.h"
// シーン入場時の共通サービスを準備する。
void SkinningEditorScene::Initialize(GameRuntime &game) { game.OnSceneEntered(SceneType::SkinningEditor); }
// シーン固有の更新を行う。開発ツールの画面切替は外部のメニューコマンドで要求される。
void SkinningEditorScene::Update(GameRuntime &game, const SceneUpdateContext &context) {
    (void)context;
    game.RunSkinningEditorScene(context);
}
// シーン所有の表示を終了し、共通サービスへ退場を通知する。
void SkinningEditorScene::Finalize(GameRuntime &game) { game.OnSceneExited(SceneType::SkinningEditor); }
