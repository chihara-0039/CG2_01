#pragma once
#include "Sprite.h"
#include "Sound.h"
#include "Camera.h"
#include "StageFlyover.h"
#include <array>
#include <memory>
#include <chrono>

// ステージ到着専用。降下・追従視点・雲抜け・着地音を一つの時計で同期する。
class StartIntro {
public:
    void Initialize(SpriteCommon* common, uint32_t cloudTexture);
    void Begin(Camera* camera, const Vector3& stageCenter, float stageRadius, const Vector3& spawnPosition);
    void Update(Camera* camera, bool visible);
    void Cancel(Camera* camera);
    void Finalize();
    void Draw();
    bool IsActive() const { return active_; }
    bool IsTouring() const { return active_; }
    bool BlocksInput() const { return active_ && !frame_.landed; }
    const Vector3& GetPlayerPosition() const { return frame_.playerPosition; }
    const Vector3& GetSpawnPosition() const { return spawn_; }
    bool ConsumeGo();
private:
    void UpdateSprites();
    Sound::SoundData MakeTone(bool landing);
    std::array<std::unique_ptr<Sprite>,2> bars_;
    std::array<std::unique_ptr<Sprite>,8> clouds_;
    Sound sound_;
    Sound::SoundData wind_{};
    Sound::SoundData landing_{};
    StageArrival::Frame frame_{};
    std::chrono::steady_clock::time_point lastUpdate_{};
    float elapsed_ = 0;
    float cameraFarClip_ = 100;
    Vector3 stageCenter_{};
    Vector3 spawn_{};
    float stageRadius_ = 3;
    StageFlyover::Pose gameCamera_{};
    bool active_ = false;
    bool windStarted_ = false;
    bool goPending_ = false;
};
