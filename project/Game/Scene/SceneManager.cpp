#include "SceneManager.h"
#include "BaseScene.h"
#include "SceneFactory.h"

#include <cassert>

void SceneManager::Initialize(const SceneFactory* sceneFactory, SceneType initialScene, MyGame& game) {
    assert(sceneFactory);
    sceneFactory_ = sceneFactory;
    ChangeScene(initialScene, game);
}

void SceneManager::Update(MyGame& game, const SceneUpdateContext& context) {
    if (currentScene_) {
        currentScene_->Update(game, context);
    }
}

void SceneManager::Draw(MyGame& game) {
    if (currentScene_) {
        currentScene_->Draw(game);
    }
}

void SceneManager::Finalize(MyGame& game) {
    if (currentScene_) {
        currentScene_->Finalize(game);
        currentScene_.reset();
    }
}

void SceneManager::ChangeScene(SceneType nextScene, MyGame& game) {
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
