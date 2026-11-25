#pragma once
#include "../Engine/Graphics/Particle/ParticleMotion.h"
#include "../Game/Stage/StageCellBehavior.h"
#include "../Game/Scene/BaseScene.h"
#include <cassert>
#include <cmath>
#include <memory>
#include <type_traits>
#include <iostream>

namespace PolymorphismChecks {
// 所有を基底型へ渡した場合にも派生側の破棄が実行されることを確認する。
class DestructionProbe final : public ParticleMotion {
  public:
    explicit DestructionProbe(bool &destroyed) : destroyed_(destroyed) {}
    ~DestructionProbe() override { destroyed_ = true; }
    void Update(Particle &particle, float deltaTime) const override { particle.transform.translate.x += deltaTime; }

  private:
    bool &destroyed_;
};

// 実際の派生クラスを基底参照・ポインタから呼び、種類別の振る舞いと状態の独立性を確認する。
inline void Run() {
    static_assert(std::is_abstract_v<ParticleMotion> && std::has_virtual_destructor_v<ParticleMotion>);
    static_assert(std::is_abstract_v<StageCellBehavior> && std::has_virtual_destructor_v<StageCellBehavior>);
    static_assert(std::has_virtual_destructor_v<BaseScene>);
    bool destroyed = false;
    {
        std::unique_ptr<ParticleMotion> owned = std::make_unique<DestructionProbe>(destroyed);
        Particle p{};
        owned->Update(p, .25f);
        assert(p.transform.translate.x == .25f);
    }
    assert(destroyed);

    // 同じ更新口から、直線・煙・演出粒・花火・回収等の異なる動きを呼ぶ。
    for (int fps : {30, 60, 144}) {
        const float dt = 1.0f / fps;
        Particle p{};
        p.maxTime = 1;
        p.lifeTime = .5f;
        p.velocity = {.1f, .2f, -.1f};
        p.baseScale = {.2f, .3f, 1};
        const ParticleMotion &linear = SelectParticleMotion(p);
        linear.Update(p, dt);
        assert(std::abs(p.transform.translate.x - .1f * dt * 60) < .001f);
        p.type = Particle::Type::Dust;
        SelectParticleMotion(p).Update(p, dt);
        const float spread = (1 - std::exp(-2.0f)) / 4;
        assert(std::abs(p.transform.translate.x - .1f * spread) < .001f);
        assert(p.transform.scale.x > p.baseScale.x);
        p.type = Particle::Type::Accent;
        SelectParticleMotion(p).Update(p, dt);
        assert(std::abs(p.transform.translate.x - .05f) < .001f);
        assert(std::abs(p.transform.translate.y - (.1f - .25f * .5f * .5f)) < .001f);
        p.type = Particle::Type::Firework;
        const float vy = p.velocity.y;
        SelectParticleMotion(p).Update(p, dt);
        assert(std::abs(p.velocity.y - (vy - .0012f * dt * 60)) < .001f);
        assert(std::abs(p.transform.scale.x - .0725f) < .001f);
        for (const auto kind : {AdventureEffectKind::Collect, AdventureEffectKind::Undo}) {
            p.type = Particle::Type::Adventure;
            p.adventure = static_cast<int>(kind);
            p.shape = 3;
            p.origin = {3, 2, -1};
            p.target = {-2, 4, 5};
            p.lifeTime = 1;
            SelectParticleMotion(p).Update(p, dt);
            assert(std::abs(p.transform.translate.x - p.target.x) < .001f &&
                   std::abs(p.transform.translate.y - p.target.y) < .001f &&
                   std::abs(p.transform.translate.z - p.target.z) < .001f);
        }
        p.adventure = static_cast<int>(AdventureEffectKind::Place);
        p.shape = 5;
        SelectParticleMotion(p).Update(p, dt);
        assert(std::abs(p.transform.translate.z - p.target.z) < .001f);
        p.type = Particle::Type::Ring;
        p.shape = 0;
        p.lifeTime = .5f;
        SelectParticleMotion(p).Update(p, dt);
        assert(std::abs(p.transform.scale.x - p.baseScale.x) < .001f);
        p.type = Particle::Type::Cylinder;
        SelectParticleMotion(p).Update(p, dt);
        assert(std::abs(p.transform.scale.y - p.baseScale.y * 1.05f) < .001f);
    }

    std::array<std::vector<int>, 10> orders;
    orders[1] = {1, 4};
    StageCellContext context{.1f, .5f, {0, 0, 0}, {4, 0, 0}, orders};
    assert(!FindStageCellBehavior(BlockType::Ground) && !FindStageCellBehavior(static_cast<BlockType>(999)));
    const StageCellBehavior *walker = FindStageCellBehavior(BlockType::EnemyWalker);
    assert(walker && walker == FindStageCellBehavior(BlockType::EnemyWalker));
    MapCell first{}, second{};
    walker->Update(first, context);
    walker->Update(second, context);
    assert(first.currentOffsetX == second.currentOffsetX && first.moveTimer == second.moveTimer);
    assert(std::abs(first.currentOffsetX - std::sin(.2f) * 2) < .001f);
    FindStageCellBehavior(BlockType::EnemyFlyer)->Update(second, context);
    assert(second.currentOffsetX == 0 && second.currentOffsetY > 0);
    MapCell chaser{};
    FindStageCellBehavior(BlockType::EnemyChaser)->Update(chaser, context);
    assert(std::abs(chaser.currentOffsetX - .12f) < .001f);
    context.playerPosition = {20, 0, 0};
    FindStageCellBehavior(BlockType::EnemyChaser)->Update(chaser, context);
    assert(std::abs(chaser.currentOffsetX - .02f) < .001f);
    MapCell floor{};
    floor.moveOffset = {4, 2, -3};
    FindStageCellBehavior(BlockType::MovingFloor)->Update(floor, context);
    assert(std::abs(floor.currentOffsetX - 4 * (std::sin(.1f) + 1) / 2) < .001f);
    assert(floor.deltaOffsetX == floor.currentOffsetX);
    const float oldX = floor.currentOffsetX;
    FindStageCellBehavior(BlockType::MovingFloor)->Update(floor, context);
    assert(std::abs(floor.deltaOffsetX - (floor.currentOffsetX - oldX)) < .001f);
    MapCell timed{};
    timed.variant = 11;
    FindStageCellBehavior(BlockType::TimedBlock)->Update(timed, context);
    assert(timed.isSolid);
    timed.variant = 14;
    FindStageCellBehavior(BlockType::TimedBlock)->Update(timed, context);
    assert(!timed.isSolid);
    context.elapsedTime = 1.3f;
    FindStageCellBehavior(BlockType::TimedBlock)->Update(timed, context);
    assert(timed.isSolid);
    MapCell crumbling{};
    crumbling.isCrumbling = true;
    context.deltaTime = 1;
    FindStageCellBehavior(BlockType::CrumblingFloor)->Update(crumbling, context);
    assert(crumbling.isHidden && !crumbling.isSolid);
    context.deltaTime = 2;
    FindStageCellBehavior(BlockType::CrumblingFloor)->Update(crumbling, context);
    assert(!crumbling.isHidden && crumbling.isSolid && crumbling.crumbleTimer == 0);
    std::cout << "PASS: base dispatch for particle/cell behaviors, shared state isolation and virtual destruction\n";
}
}
