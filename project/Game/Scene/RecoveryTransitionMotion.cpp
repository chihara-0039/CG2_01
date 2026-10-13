#include "RecoveryTransitionMotion.h"
#include <algorithm>
#include <cmath>

bool RecoveryTransitionMotion::IsContestScene(SceneType scene) {
    return scene == SceneType::Title || scene == SceneType::StageSelect ||
        scene == SceneType::GamePlay || scene == SceneType::GameClear;
}

RecoveryTransitionMotion::Frame RecoveryTransitionMotion::Evaluate(
    float cover, bool opening, SceneType destination) {
    cover = std::clamp(cover, 0.0f, 1.0f);
    const float eased = cover * cover * (3.0f - 2.0f * cover);
    constexpr float kPi = 3.14159265f;
    // 2080pxの正方形なら、どの回転角でも1280x720の四隅を完全に隠せる。
    // 閉じる時と開く時に同じ向きへ回し続け、黒画面の裏で次の一周へ接続する。
    const float direction = destination == SceneType::Title ? -1.0f : 1.0f;
    const float angle = direction * kPi * 0.5f * (opening ? 2.0f - eased : eased);
    Vector4 color = {0.38f, 0.82f, 1.0f, 1.0f};
    if (destination == SceneType::GamePlay) color = {0.30f, 1.0f, 0.82f, 1.0f};
    if (destination == SceneType::GameClear) color = {1.0f, 0.82f, 0.30f, 1.0f};
    const float alpha = cover <= 0.0f || cover >= 1.0f ? 0.0f : std::sin(kPi * cover);
    return {2080.0f * eased, angle, alpha, color};
}
