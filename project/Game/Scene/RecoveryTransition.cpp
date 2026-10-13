#include "RecoveryTransition.h"
#include <cmath>

void RecoveryTransition::Initialize(SpriteCommon* common) {
    const auto make = [common]() {
        auto sprite = std::make_unique<Sprite>();
        sprite->Initialize(common, 0); // TextureManagerの標準白テクスチャ
        sprite->SetAnchorPoint({0.5f, 0.5f});
        sprite->SetPosition({640.0f, 360.0f});
        return sprite;
    };
    rim_ = make();
    curtain_ = make();
    for (auto& spark : sparks_) spark = make();
    Update(0.0f, false, SceneType::Title);
}

void RecoveryTransition::Update(float cover, bool opening, SceneType destination) {
    if (!curtain_) return;
    const auto frame = RecoveryTransitionMotion::Evaluate(cover, opening, destination);
    Vector4 accent = frame.accentColor;
    accent.w = frame.accentAlpha;
    rim_->SetSize({frame.size + 12.0f, frame.size + 12.0f});
    rim_->SetRotation(frame.rotation);
    rim_->SetColor(accent);
    rim_->Update();
    curtain_->SetSize({frame.size, frame.size});
    curtain_->SetRotation(frame.rotation);
    curtain_->SetColor({0.0f, 0.0f, 0.0f, 1.0f});
    curtain_->Update();

    // 幕の四隅に光を置き、回転の方向を読み取れるようにする。
    constexpr float kPi = 3.14159265f;
    for (size_t i = 0; i < sparks_.size(); ++i) {
        const float angle = frame.rotation + kPi * 0.25f + static_cast<float>(i) * kPi * 0.5f;
        const float radius = frame.size * 0.70710678f + 10.0f;
        sparks_[i]->SetPosition({640.0f + std::cos(angle) * radius,
                                360.0f + std::sin(angle) * radius});
        sparks_[i]->SetSize({18.0f, 18.0f});
        sparks_[i]->SetRotation(angle);
        sparks_[i]->SetColor(accent);
        sparks_[i]->Update();
    }
}

void RecoveryTransition::Draw() {
    if (!curtain_) return;
    rim_->Draw();
    curtain_->Draw();
    for (auto& spark : sparks_) spark->Draw();
}
