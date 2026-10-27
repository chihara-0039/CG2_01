#pragma once
#include "MyMath.h"

// ステージ俯瞰→開始位置への着地。描画APIから分け、経路の終端を検証できるようにする。
namespace StageFlyover {
constexpr float kOrbitDuration = 4.8f;
constexpr float kLandingDuration = 1.2f;
constexpr float kDuration = kOrbitDuration + kLandingDuration;
struct Pose { Vector3 position; Vector3 rotation; float fov; };
Pose Evaluate(float seconds, const Vector3& center, float radius, const Pose& destination);
}

// 到着演出は物理移動と分離し、着地時にスポーン位置へ正確に戻す。
namespace StageArrival {
constexpr float kFlightDuration = 4.8f;
constexpr float kFadeDuration = .45f;
struct Frame {
    StageFlyover::Pose camera;
    Vector3 playerPosition;
    Vector3 playerRotation;
    float cloudAlpha;
    float letterbox;
    bool landed;
    bool finished;
};
Frame Evaluate(float seconds, const Vector3& center, float radius,
    const Vector3& spawn, const StageFlyover::Pose& destination, const Vector3& spawnRotation = {});
}
