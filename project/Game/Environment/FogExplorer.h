#pragma once
#include "MyMath.h"

// 通常の装飾霧とは独立した、探索ステージ用の視界制御。
// Mode 0/1/2を同じ霧密度で比較し、表現の差だけを検証できる。
class FogExplorer {
public:
    void Reset();
    bool TryScan();
    void Update(float seconds, const Vector3& player, float cameraYaw, bool paused);
    void SetMode(int mode);
    int Mode() const { return mode_; }
    float Cooldown() const { return cooldown_; }
    float Strength() const { return strength_; }
    float Radius() const { return radius_; }
    const Vector3& Origin() const { return origin_; }
    const Vector3& Direction() const { return direction_; }
    // HLSLと同じ距離マスク。CPUテストで連続性・範囲を確認する。
    float RevealAt(const Vector3& world) const;
    static float Smooth(float a, float b, float value);
private:
    int mode_ = 2;
    float age_ = 100.0f, cooldown_ = 0.0f, strength_ = 0.0f, radius_ = 0.0f;
    Vector3 origin_{}, direction_{0,0,1};
};
