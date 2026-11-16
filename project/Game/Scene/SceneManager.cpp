#include "SceneManager.h"
#include "BaseScene.h"
#include "SceneFactory.h"

#include <cassert>

SceneManager *SceneManager::GetInstance() {
    static SceneManager instance;
    return &instance;
}

// 現在のシーンを準備し、初期値と必要なリソースを設定する。
void SceneManager::Initialize(const SceneFactory *sceneFactory, SceneType initialScene, GameRuntime &game) {
    assert(sceneFactory);
    sceneFactory_ = sceneFactory;
    ChangeScene(initialScene, game);
}

// 現在のシーンに、そのフレームの更新処理を委譲する。
void SceneManager::Update(GameRuntime &game, const SceneUpdateContext &context) {
    if (currentScene_) {
        currentScene_->Update(game, context);
    }
}

// 現在のシーンの表示対象を選び、描画コマンドを記録する。
void SceneManager::Draw(GameRuntime &game) {
    if (currentScene_) {
        currentScene_->Draw(game);
    }
}

// 現在のシーンに背景だけの描画を委譲する。
void SceneManager::DrawBackground(GameRuntime &game) {
    if (currentScene_) {
        currentScene_->DrawBackground(game);
    }
}

// 現在のシーンが保持するオブジェクトとリソースを解放する。
void SceneManager::Finalize(GameRuntime &game) {
    if (currentScene_) {
        currentScene_->Finalize(game);
        currentScene_.reset();
    }
}

// 現在のシーンを終了し、次のシーンの初期化へ切り替える。
void SceneManager::ChangeScene(SceneType nextScene, GameRuntime &game) {
    assert(sceneFactory_);

    if (currentScene_ && currentSceneType_ == nextScene) {
        return;
    }

    if (currentScene_) {
        currentScene_->Finalize(game);
        currentScene_.reset();
    }

    currentSceneType_ = nextScene;
    currentScene_ = sceneFactory_->CreateScene(nextScene);
    assert(currentScene_);
    currentScene_->Initialize(game);
}
