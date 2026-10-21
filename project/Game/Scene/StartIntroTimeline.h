#pragma once

// 開始演出の時間設計を描画・入力から分離する。
namespace StartIntroTimeline {
constexpr float kCountInterval = 1.0f;
constexpr float kGoTime = 3.0f * kCountInterval;
constexpr float kDuration = kGoTime + .65f;
struct Frame {
    int cue; // 0: 3, 1: 2, 2: 1, 3: GO!
    float scale;
    float alpha;
    float cameraBlend;
    float letterbox;
    bool blocksInput;
    bool finished;
};
Frame Evaluate(float seconds);
}
