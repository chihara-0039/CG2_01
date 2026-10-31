#pragma once
#include "Camera.h"
#include "Sprite.h"
#include "Sound.h"
#include "Input.h"
#include "GameOverTimeline.h"
#include <array>
#include <chrono>
#include <memory>

// 通常ゲーム専用。死亡地点を保存して物理から分離し、選択待ちと再開幕まで管理する。
class GameOver {
public:
    using Action=GameOverTimeline::Action;
    void Initialize(SpriteCommon* common, const std::array<uint32_t,9>& textures);
    void Begin(Camera* camera, const Vector3& position, const Vector3& rotation, bool fall);
    Action Update(Camera* camera, const Input& input, bool visible);
    void Draw();
    void Cancel(Camera* camera);
    void Finalize();
    bool IsActive() const { return flow_.Active(); }
    bool IsShowingFailure() const { return flow_.Active() && !flow_.Revealing(); }
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetRotation() const { return rotation_; }
    float GetPlayerScale() const { return playerScale_; }
private:
    void UpdateSprites();
    Sound::SoundData MakeSound(int kind);
    // 8pxの水平帯で円の外側だけを覆う。円形の穴を滑らかに閉じる。
    std::array<std::unique_ptr<Sprite>,180> iris_;
    std::array<std::unique_ptr<Sprite>,5> labels_;
    std::array<std::unique_ptr<Sprite>,8> letters_;
    std::array<std::unique_ptr<Sprite>,24> sparks_;
    std::unique_ptr<Sprite> backdrop_, sigil_;
    std::unique_ptr<Sprite> flash_;
    std::unique_ptr<Sprite> selection_;
    Sound sound_;
    Sound::SoundData failureSound_{}, titleSound_{}, cursorSound_{}, confirmSound_{};
    std::chrono::steady_clock::time_point lastUpdate_{};
    Vector3 initialPosition_{}, initialRotation_{}, position_{}, rotation_{};
    Vector3 cameraPosition_{}, cameraRotation_{};
    float cameraFov_= .55f;
    GameOverTimeline::Flow flow_;
    GameOverTimeline::Frame frame_{};
    bool fall_=false, titleSoundPlayed_=false;
    float selectionY_=425;
    float playerScale_=1;
};
