#pragma once
#include "Sprite.h"
#include "Sound.h"
#include "Camera.h"
#include "StartIntroTimeline.h"
#include <array>
#include <memory>
#include <chrono>

// ステージ開始専用。文字・音・シネマスコープ・カメラの寄りを同期する。
class StartIntro {
public:
    void Initialize(SpriteCommon* common);
    void Begin(Camera* camera);
    void Update(Camera* camera, bool visible);
    void Cancel(Camera* camera);
    void Finalize();
    void Draw();
    bool IsActive() const { return active_; }
    bool BlocksInput() const { return active_ && frame_.blocksInput; }
    bool ConsumeGo();
private:
    void UpdateSprites();
    Sound::SoundData MakeTone(bool go);
    std::array<std::unique_ptr<Sprite>, 96> pixels_;
    std::array<std::unique_ptr<Sprite>, 2> bars_;
    std::array<std::unique_ptr<Sprite>, 4> frameLines_;
    std::unique_ptr<Sprite> backdrop_;
    size_t pixelCount_ = 0;
    Sound sound_;
    Sound::SoundData tick_{};
    Sound::SoundData go_{};
    StartIntroTimeline::Frame frame_{};
    std::chrono::steady_clock::time_point lastUpdate_{};
    float elapsed_ = 0;
    float cameraFov_ = .55f;
    int lastCue_ = -1;
    bool active_ = false;
    bool goPending_ = false;
};
