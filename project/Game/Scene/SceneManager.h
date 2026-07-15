#pragma once

#include <memory>
#include "BaseScene.h"
#include "SceneType.h"
#include "SceneUpdateContext.h"

class MyGame;
class SceneFactory;

class SceneManager {
public:
    static SceneManager* GetInstance();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    void Initialize(const SceneFactory* sceneFactory, SceneType initialScene, MyGame& game);
    void Update(MyGame& game, const SceneUpdateContext& context);
    void Draw(MyGame& game);
    void Finalize(MyGame& game);
    void ChangeScene(SceneType nextScene, MyGame& game);

    SceneType GetCurrentSceneType() const { return currentSceneType_; }

private:
    SceneManager() = default;
    ~SceneManager() = default;

    const SceneFactory* sceneFactory_ = nullptr;
    std::unique_ptr<BaseScene> currentScene_;
    SceneType currentSceneType_ = SceneType::DebugView;
};
