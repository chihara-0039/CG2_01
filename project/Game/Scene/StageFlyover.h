#pragma once
#include "MyMath.h"

// ステージ俯瞰→開始位置への着地。描画APIから分け、経路の終端を検証できるようにする。
namespace StageFlyover {
constexpr float kDuration = 3.2f;
struct Pose { Vector3 position; Vector3 rotation; float fov; };
Pose Evaluate(float seconds, const Vector3& center, float radius, const Pose& destination);
}
