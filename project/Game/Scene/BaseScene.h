#pragma once

#include "SceneUpdateContext.h"

class GameRuntime;

class BaseScene {
public:
    virtual ~BaseScene() = default;

    virtual void Initialize(GameRuntime& game) = 0;
    virtual void Update(GameRuntime& game, const SceneUpdateContext& context) = 0;
    virtual void Draw(GameRuntime& game) = 0;
    virtual void Finalize(GameRuntime& game) = 0;
};
