#pragma once
#include "BaseScene.h"
class DebugViewScene final : public BaseScene {
 public:
 void Initialize(GameRuntime &game) override;
 void Update(GameRuntime &game, const SceneUpdateContext &context) override;
 void Draw(GameRuntime &) override {}
 void Finalize(GameRuntime &game) override;
};
