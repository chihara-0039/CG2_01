#pragma once

#include "BaseScene.h"
#include "Camera.h"
#include "Model.h"
#include "Object3d.h"
#include "Sound.h"
#include "JournalResultTimeline.h"
#include <array>
#include <chrono>

#include <memory>
#include <string>
#include <vector>

// ゴール取得後に手帳へ打刻し、完了後は表紙も見返せるクリアシーン。
class GameClearScene final : public BaseScene {
  public:
    void Initialize(GameRuntime &game) override;
    void Update(GameRuntime &game, const SceneUpdateContext &context) override;
    void Draw(GameRuntime &game) override;
    void Finalize(GameRuntime &game) override;

  private:
    enum Part {
        Back,
        Pages,
        Paper,
        Cover,
        CoverArt,
        Inside,
        Ink,
        Shadow,
        Stamp,
        Count
    };
    std::array<std::unique_ptr<Model>, Count> models_;
    std::array<std::unique_ptr<Object3d>, Count> objects_;
    Camera camera_;
    Sound sound_;
    Sound::SoundData pageSound_{}, stampSound_{};
    static Sound::SoundData MakeSound(bool stamp);
    std::chrono::steady_clock::time_point lastUpdate_{};
    float elapsed_ = 0;
    JournalResult::Frame frame_{};
    JournalResult::Inspection inspection_{};
};
