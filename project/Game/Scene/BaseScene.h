#pragma once

#include "SceneUpdateContext.h"

class MyGame;
class SceneManager;

class BaseScene {
public:
    virtual ~BaseScene() = default;

    void SetSceneManager(SceneManager* sceneManager) { sceneManager_ = sceneManager; }

    virtual void Initialize(MyGame& game) = 0;
    virtual void Update(MyGame& game, const SceneUpdateContext& context) = 0;
    virtual void Draw(MyGame& game) = 0;
    virtual void Finalize(MyGame& game) = 0;

protected:
    SceneManager* sceneManager_ = nullptr;
};
