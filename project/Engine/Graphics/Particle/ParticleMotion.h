#pragma once
#include "Particle.h"

// すべての粒の動きが共有する契約。寿命・衝突・描画の管理は含めない。
class ParticleMotion {
  public:
    virtual ~ParticleMotion() = default;
    virtual void Update(Particle &particle, float deltaTime) const = 0;
};
// 状態を持たない振る舞いを基底参照で返し、粒ごとのヒープ確保を避ける。
const ParticleMotion &SelectParticleMotion(const Particle &particle);
