#include "ParticleMotion.h"
#include "AdventureParticlePath.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace {
// 共通の直線移動。単位は既存の1/60秒基準を維持する。
void AdvanceLinear(Particle &p, float deltaTime) {
    p.transform.translate.x += p.velocity.x * deltaTime * 60.0f;
    p.transform.translate.y += p.velocity.y * deltaTime * 60.0f;
    p.transform.translate.z += p.velocity.z * deltaTime * 60.0f;
}
// Linear用の移動・拡縮を、共通の更新契約から実行する。
class LinearMotion final : public ParticleMotion {
  public:
    // 速度から座標を積分する。
    void Update(Particle &p, float deltaTime) const override { AdvanceLinear(p, deltaTime); }
};
// Accent用の移動・拡縮を、共通の更新契約から実行する。
class AccentMotion final : public ParticleMotion {
  public:
    // 発生位置と経過時間から演出粒の移動・縮小・回転を求める。
    void Update(Particle &p, float /*deltaTime*/) const override {

        const float age = p.lifeTime, t = age / p.maxTime;
        p.transform.translate = {p.origin.x + p.velocity.x * age, p.origin.y + p.velocity.y * age - .25f * age * age,
                                 p.origin.z + p.velocity.z * age};
        const float size = (.65f + .5f * std::sin(t * 3.14159265f)) * (1 - .65f * t);
        p.transform.scale = {p.baseScale.x * size, p.baseScale.y * size, 1};
        p.transform.rotate.z = p.phase + age * .7f;
    }
};
// Dust用の移動・拡縮を、共通の更新契約から実行する。
class DustMotion final : public ParticleMotion {
  public:
    // 減衰する移動と拡大で土煙の広がりを求める。
    void Update(Particle &p, float /*deltaTime*/) const override {

        const float age = p.lifeTime;
        const float spread = (1.0f - std::exp(-age * 4.0f)) / 4.0f;
        p.transform.translate = {p.origin.x + p.velocity.x * spread, p.origin.y + p.velocity.y * spread + age * .12f,
                                 p.origin.z + p.velocity.z * spread};
        const float size = .65f + 1.65f * age / p.maxTime;
        p.transform.scale = {p.baseScale.x * size, p.baseScale.y * size, 1};
        p.transform.rotate.z = p.phase + age * .35f;
    }
};
// Adventure用の移動・拡縮を、共通の更新契約から実行する。
class AdventureMotion final : public ParticleMotion {
  public:
    // 回収・取消・組立の軌跡とリング・柱の拡縮を求める。
    void Update(Particle &p, float deltaTime) const override {
        AdvanceLinear(p, deltaTime);

        const float t = std::clamp(p.lifeTime / p.maxTime, 0.0f, 1.0f);
        const bool inward = p.adventure == static_cast<int>(AdventureEffectKind::Collect) ||
                            p.adventure == static_cast<int>(AdventureEffectKind::Undo);
        if (inward && p.type == Particle::Type::Adventure && p.shape != 6) {
            p.transform.translate = p.adventure == static_cast<int>(AdventureEffectKind::Undo)
                                        ? AdventureParticlePath::Undo(p.origin, p.target, t, p.phase)
                                        : AdventureParticlePath::Sample(p.origin, p.target, t, p.phase);
        } else if (p.type == Particle::Type::Adventure && p.shape == 5) {
            p.transform.translate = AdventureParticlePath::Assemble(p.origin, p.target, t);
        } else if (p.type == Particle::Type::Adventure) {
            const float age = p.lifeTime;
            p.transform.translate = {p.origin.x + p.velocity.x * age * 60,
                                     p.origin.y + p.velocity.y * age * 60 - .35f * age * age,
                                     p.origin.z + p.velocity.z * age * 60};
        }
        float scale = .65f + .8f * std::sin(t * 3.14159265f);
        if (p.type == Particle::Type::Ring) {
            scale = inward ? 1.2f - t : .2f + 1.6f * t;
        }
        if (p.shape == 6) {
            // 割れたシャボン玉は外へ広がる輪だけを短く残す。
            scale = .6f + t * 1.8f;
        } else if (p.shape == 5 || (inward && p.type == Particle::Type::Adventure)) {
            scale *= 1.0f - .85f * t;
        }
        p.transform.scale = {p.baseScale.x * scale, p.baseScale.y * scale, p.baseScale.z * scale};
        if (p.type == Particle::Type::Cylinder) {
            const float rise = std::clamp(t / .35f, 0.0f, 1.0f);
            p.transform.scale.y = p.baseScale.y * (.05f + rise);
        }
        if (p.type == Particle::Type::Adventure && (p.shape == 3 || p.shape == 5)) {
            p.transform.rotate.z = p.phase + p.lifeTime * 1.3f;
        }
    }
};
// Firework用の移動・拡縮を、共通の更新契約から実行する。
class FireworkMotion final : public ParticleMotion {
  public:
    // 火花の移動に重力と寿命による縮小を加える。
    void Update(Particle &p, float deltaTime) const override {
        AdvanceLinear(p, deltaTime);

        // 炸裂後の火花を徐々に落下させ、花火らしい放物線を作る。
        p.velocity.y -= 0.0012f * deltaTime * 60.0f;
        const float remaining = 1.0f - p.lifeTime / p.maxTime;
        const float scale = 0.035f + 0.075f * remaining;
        p.transform.scale = {scale, scale, 1.0f};
    }
};
}
// 種類と振る舞いの対応だけを選ぶ。各粒の状態はParticle側にあり、実装は共有できる。
const ParticleMotion &SelectParticleMotion(const Particle &p) {
    static const LinearMotion linear;
    static const AccentMotion accent;
    static const DustMotion dust;
    static const AdventureMotion adventure;
    static const FireworkMotion firework;
    if (p.adventure != 0) {
        return adventure;
    }
    static const auto table = [] {
        std::array<const ParticleMotion *, static_cast<size_t>(Particle::Type::StormWind) + 1> result{};
        result.fill(&linear);
        result[static_cast<size_t>(Particle::Type::Firework)] = &firework;
        result[static_cast<size_t>(Particle::Type::Dust)] = &dust;
        result[static_cast<size_t>(Particle::Type::Accent)] = &accent;
        return result;
    }();
    const auto index = static_cast<size_t>(p.type);
    return index < table.size() ? *table[index] : linear;
}
