#include "StarGetPresentation.h"
#include <cstring>

namespace {
Vector3 Mix(const Vector3 &a, const Vector3 &b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
}

// 短い上昇和音を合成し、星を受け取る明るい響きを作る。
Sound::SoundData StarGetPresentation::MakeChime() {
    Sound::SoundData data{};
    data.wfex = {WAVE_FORMAT_PCM, 1, 44100, 88200, 2, 16, 0};
    const int count = 44100 * 2;
    data.buffer.resize(count * 2);
    const float notes[] = {523.25f, 659.25f, 783.99f, 1046.50f};
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i) / 44100;
        float sample = 0;
        for (int n = 0; n < 4; ++n) {
            const float age = t - .12f * n;
            if (age >= 0) {
                const float phase = age * notes[n] * 6.2831853f;
                sample += (std::sin(phase) + .18f * std::sin(phase * 2.01f)) * std::exp(-age * 3.8f) *
                          StarGet::Ease(age / .015f) * .18f;
            }
        }
        const int16_t value = static_cast<int16_t>(std::clamp(sample, -1.0f, 1.0f) * 27000);
        std::memcpy(data.buffer.data() + i * 2, &value, 2);
    }
    return data;
}

// 注視点からカメラへの経路を検査し、地形の手前へ収まる割合を返す。
float StarGetPresentation::ClearFraction(const StageMap &map, const Vector3 &target, const Vector3 &offset) {
    for (float t = .12f; t <= 1; t += .025f) {
        const auto p = Mix(target, {target.x + offset.x, target.y + offset.y, target.z + offset.z}, t);
        const auto *c = map.GetCell(static_cast<int>(std::floor(p.x + .5f)), static_cast<int>(std::floor(p.y + .5f)),
                                    static_cast<int>(std::floor(p.z + .5f)));
        if (c && c->isSolid && !c->isHidden && c->type != BlockType::MovingFloor) {
            return (std::max)(.25f, t - .07f);
        }
    }
    return 1;
}

// 開始前の姿勢を保存し、見通しの良いカメラ方向と専用モーションを準備する。
void StarGetPresentation::Begin(Object3dCommon *common, Player &player, const Camera &camera, const StageMap &map,
                                const Vector3 &source) {
    if (!object_) {
        model_ = Model::CreateFromOBJ(common->GetDxCommon(), "Resources/Models/star", "star.obj",
                                      common->GetTextureManager());
        object_ = std::make_unique<Object3d>();
        object_->Initialize(common);
        object_->SetModel(model_.get());
        object_->SetEnableLighting(false);
        object_->SetColor({1.0f, .78f, .24f, 1});
        object_->SetEmissive(.25f);
    }
    if (!soundReady_) {
        sound_.Initialize();
        chime_ = MakeChime();
        soundReady_ = true;
    }
    sound_.StopAllSoundEffects();
    sound_.SoundPlay(chime_, .8f);
    foot_ = player.GetPosition();
    rotation_ = player.GetRotation();
    source_ = source;
    cameraStart_ = camera.GetPosition();
    cameraRotation_ = camera.GetRotation();
    startFov_ = camera.GetFov();
    const float initial = std::atan2(cameraStart_.x - foot_.x, cameraStart_.z - foot_.z);
    float best = -1;
    for (const float offset : {0.0f, .7f, -.7f, 1.4f, -1.4f, 3.14159265f}) {
        const float angle = initial + offset;
        const float fraction = ClearFraction(map, {foot_.x, foot_.y + 1.15f, foot_.z},
                                             {std::sin(angle) * 6.0f, 2, std::cos(angle) * 6.0f});
        if (fraction > best) {
            best = fraction;
            yaw_ = angle;
        }
    }
    if (auto *skin = player.GetSkinnedObject()) {
        customAnimation_ = skin->IsPlayCustomAnimation();
        testAnimation_ = skin->IsPlayAnimation();
        motion_ = skin->GetModel()->GetActiveMotionIndex();
        skin->GetModel()->GenerateStarGetPreset();
        skin->SetPlayAnimation(false);
        skin->SetPlayCustomAnimation(false);
    }
    previous_ = 0;
    active_ = true;
    last_ = std::chrono::steady_clock::now();
}

// 非アクティブ復帰時の大きな時刻飛びを抑え、演出用の実時間差を返す。
float StarGetPresentation::DeltaTime() {
    const auto now = std::chrono::steady_clock::now();
    const float dt = std::clamp(std::chrono::duration<float>(now - last_).count(), 0.0f, .05f);
    last_ = now;
    return dt;
}

// 星を頭上へ運び、決めポーズと穏やかなカメラ移動を同期する。
void StarGetPresentation::Update(float time, Player &player, Camera &camera, const StageMap &map,
                                 ParticleManager *particles, const Matrix4x4 &lightVP, bool moveCamera) {
    const auto f = StarGet::Evaluate(time);
    const float angle = yaw_ + .22f * StarGet::Ease(time / StarGet::kDuration);
    const Vector3 target{foot_.x, foot_.y + 1.15f, foot_.z};
    if (moveCamera) {
        const Vector3 offset{std::sin(angle) * 6.0f, 2, std::cos(angle) * 6.0f};
        const float fraction = ClearFraction(map, target, offset);
        const Vector3 position =
            Mix(cameraStart_,
                {target.x + offset.x * fraction, target.y + offset.y * fraction, target.z + offset.z * fraction},
                f.cameraBlend);
        const Vector3 aim{target.x - position.x, target.y - position.y, target.z - position.z};
        camera.SetPosition(position);
        const Vector3 desired{-std::atan2(aim.y, std::hypot(aim.x, aim.z)), std::atan2(aim.x, aim.z), 0};
        camera.SetRotation(
            {cameraRotation_.x + (desired.x - cameraRotation_.x) * f.cameraBlend,
             cameraRotation_.y + std::remainder(desired.y - cameraRotation_.y, 6.2831853f) * f.cameraBlend, 0});
        camera.SetFov(startFov_ + (.74f - startFov_) * f.cameraBlend);
        camera.Update();
    }
    player.SetPosition({foot_.x, foot_.y + f.hop, foot_.z});
    player.SetRotation(
        {0, rotation_.y + std::remainder(angle + 3.14159265f - rotation_.y, 6.2831853f) * f.cameraBlend, 0});
    if (auto *skin = player.GetSkinnedObject()) {
        skin->ApplyMotion(f.poseTime);
    }
    // 星を頭の真上から外し、掲げた右手の先へ置く。
    const float facing = player.GetRotation().y;
    const Vector3 trophy{foot_.x - .72f * std::cos(facing) - .10f * std::sin(facing), foot_.y + 1.65f + f.hop,
                         foot_.z + .72f * std::sin(facing) - .10f * std::cos(facing)};
    starPosition_ = Mix(source_, trophy, f.lift);
    object_->SetPosition(starPosition_);
    object_->SetRotation({.08f * std::sin(time * 3), angle + f.spin, .08f * std::sin(time * 4)});
    object_->SetScale({f.scale * .65f, f.scale * .65f, f.scale * .65f});
    if (particles && previous_ < .72f && time >= .72f) {
        particles->EmitAdventureEffect(ParticleManager::AdventureEffect::Star, starPosition_);
    }
    previous_ = time;
    sound_.CleanupFinishedSoundEffects();
    object_->SetCamera(camera.GetViewMatrix(), camera.GetProjectionMatrix());
    object_->Update(lightVP);
}

// 確定したカメラで取得用の星を描き、地形・プレイヤーとの前後判定を保つ。
void StarGetPresentation::Draw(const Camera &camera, const Matrix4x4 &lightVP) {
    if (active_ && object_) {
        object_->SetCamera(camera.GetViewMatrix(), camera.GetProjectionMatrix());
        object_->Update(lightVP);
        object_->Draw();
    }
}

// シーンを離れたら通常モーションへ戻し、次のステージへ決めポーズを持ち越さない。
void StarGetPresentation::Cancel(Player *player) {
    if (!active_) {
        return;
    }
    if (player) {
        player->SetPosition(foot_);
        player->SetRotation(rotation_);
        player->SetPresentationScale(1);
        if (auto *skin = player->GetSkinnedObject()) {
            skin->GetModel()->SetActiveMotionIndex(motion_);
            skin->GetModel()->ResetPose();
            skin->SetPlayAnimation(testAnimation_);
            skin->SetPlayCustomAnimation(customAnimation_);
        }
    }
    active_ = false;
}

// 終了待ち済みのGPU資源と、取得音の再生資源を解放する。
void StarGetPresentation::Finalize() {
    if (soundReady_) {
        sound_.StopAllSoundEffects();
        sound_.Finalize();
        soundReady_ = false;
    }
    object_.reset();
    model_.reset();
    active_ = false;
}
