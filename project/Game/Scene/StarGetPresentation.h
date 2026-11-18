#pragma once
#include "Object3d.h"
#include "Model.h"
#include "Player.h"
#include "Camera.h"
#include "Sound.h"
#include "StageMap.h"
#include "ParticleManager.h"
#include "StarGetTimeline.h"
#include <chrono>
#include <memory>

// 取得用の星・カメラ・モーション・効果音を、通常プレイの状態から分離する。
class StarGetPresentation {
  public:
    void Begin(Object3dCommon *common, Player &player, const Camera &camera, const StageMap &map,
               const Vector3 &source);
    float DeltaTime();
    void Update(float time, Player &player, Camera &camera, const StageMap &map, ParticleManager *particles,
                const Matrix4x4 &lightVP, bool moveCamera);
    void Draw(const Camera &camera, const Matrix4x4 &lightVP);
    void Cancel(Player *player);
    void Finalize();
    bool Active() const { return active_; }
    Vector3 LightPosition() const { return starPosition_; }

  private:
    std::unique_ptr<Model> model_;
    std::unique_ptr<Object3d> object_;
    Sound sound_;
    Sound::SoundData chime_{};
    static Sound::SoundData MakeChime();
    static float ClearFraction(const StageMap &map, const Vector3 &target, const Vector3 &offset);
    Vector3 foot_{}, rotation_{}, source_{}, cameraStart_{}, cameraRotation_{}, starPosition_{};
    float yaw_ = 0, startFov_ = 0, previous_ = 0;
    bool active_ = false, soundReady_ = false, customAnimation_ = false, testAnimation_ = false;
    int motion_ = -1;
    std::chrono::steady_clock::time_point last_{};
};
