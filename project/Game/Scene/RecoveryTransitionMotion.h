#pragma once
#include "MyMath.h"
#include "SceneType.h"

// 復旧版の画面遷移だけに使う、GPUに依存しない演出パラメータ。
namespace RecoveryTransitionMotion {
struct Frame {
    float size;
    float rotation;
    float accentAlpha;
    Vector4 accentColor;
};
bool IsContestScene(SceneType scene);
Frame Evaluate(float cover, bool opening, SceneType destination);
}
