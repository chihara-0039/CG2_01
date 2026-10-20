#pragma once
#include "Sprite.h"
#include "RecoveryTransitionMotion.h"
#include <array>
#include <memory>

// この復旧版用の回転ワイプ。白テクスチャだけを使い、追加画像に依存しない。
class RecoveryTransition {
public:
    void Initialize(SpriteCommon* common);
    void Update(float cover, bool opening, SceneType destination);
    void Draw(); // SpriteCommon::PreDraw後に、既存フェードより上へ描く。
private:
    std::unique_ptr<Sprite> rim_;
    std::unique_ptr<Sprite> curtain_;
    std::array<std::unique_ptr<Sprite>, 4> sparks_;
};
