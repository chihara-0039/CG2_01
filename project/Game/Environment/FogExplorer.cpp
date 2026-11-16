#include "FogExplorer.h"
#include <algorithm>
#include <cmath>

float FogExplorer::Smooth(float a, float b, float value) {
    const float t = std::clamp((value - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3 - 2 * t);
}
// 探索記録とスキャンの状態を消し、開始時の霧へ戻す。
void FogExplorer::Reset() {
    mode_ = 2;
    age_ = 100;
    cooldown_ = 0;
    strength_ = 0;
    radius_ = 0;
}
void FogExplorer::SetMode(int mode) {
    mode_ = std::clamp(mode, 0, 2);
}
// 発動条件を満たす場合だけ探索スキャンを開始する。
bool FogExplorer::TryScan() {
    if (mode_ != 2 || cooldown_ > 0) {
        return false;
    }
    age_ = 0;
    cooldown_ = 6;
    return true;
}
// 移動と経過時間に合わせて、探索の進み具合と表示用状態を更新する。
void FogExplorer::Update(float seconds, const Vector3 &player, float cameraYaw, bool paused) {
    origin_ = player;
    direction_ = {std::sin(cameraYaw), 0, std::cos(cameraYaw)};
    if (!paused) {
        const float dt = std::clamp(seconds, 0.0f, .1f);
        age_ += dt;
        cooldown_ = (std::max)(0.0f, cooldown_ - dt);
    }
    // 0.7秒で展開し、約2秒見せてから1秒で戻す。瞬間的な視界切替を避ける。
    strength_ = Smooth(0, .35f, age_) * (1 - Smooth(2.5f, 3.5f, age_));
    radius_ = 36 * Smooth(0, .9f, age_);
}
// 指定地点の周囲を探索済みにし、新しく晴れた霧の量を返す。
float FogExplorer::RevealAt(const Vector3 &world) const {
    if (!mode_) {
        return 0;
    }
    const float x = world.x - origin_.x, z = world.z - origin_.z;
    const float radial = std::sqrt(x * x + z * z);
    const float local = 1 - Smooth(4, 8, radial);
    if (mode_ == 1) {
        return local;
    }
    const float along = std::clamp(x * direction_.x + z * direction_.z, 0.0f, 28.0f);
    const float dx = x - direction_.x * along, dz = z - direction_.z * along;
    const float corridor = 1 - Smooth(4, 8, std::sqrt(dx * dx + dz * dz));
    const float wave = 1 - Smooth(radius_ - 2, radius_ + 2, radial);
    return (std::max)(local, corridor * wave * strength_);
}
