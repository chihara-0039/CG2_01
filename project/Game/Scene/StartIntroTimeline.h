#pragma once

// 開始演出の時間設計を描画・入力から分離する。
namespace StartIntroTimeline {
constexpr float kReadyDuration = 1.0f;
constexpr float kCountInterval = 1.0f;
constexpr float kGoTime = kReadyDuration + 3.0f * kCountInterval;
constexpr float kDuration = kGoTime + .65f;
struct Frame {
    int cue; // 0: READY, 1: 3, 2: 2, 3: 1, 4: GO!
    float scale;
    float alpha;
    float cameraBlend;
    float letterbox;
    bool blocksInput;
    bool finished;
};
Frame Evaluate(float seconds);
}
