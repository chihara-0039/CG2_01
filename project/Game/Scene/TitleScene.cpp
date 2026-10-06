#include "TitleScene.h"

#include "GameRuntime.h"

#include <algorithm>
#include <cmath>

void TitleScene::Initialize(GameRuntime& game) {
    game.OnSceneEntered(SceneType::Title);
    game.SetTitleCloudsEnabled(true);

    Object3dCommon* object3dCommon = game.GetObject3dCommon();
    titleModel_ = Model::CreateFromOBJ(
        object3dCommon->GetDxCommon(), "Resources/Models/title", "title.obj",
        object3dCommon->GetTextureManager());
    titleObject_ = std::make_unique<Object3d>();
    titleObject_->Initialize(object3dCommon);
    titleObject_->SetModel(titleModel_.get());
    titleObject_->SetEnableLighting(true);
    titleObject_->SetColor({ 1.0f, 0.93f, 0.68f, 1.0f });
    titleObject_->SetShininess(0.72f);
    titleObject_->SetMetallic(0.16f);
    titleObject_->SetEmissive(0.06f);
    titleObject_->SetEnvironmentCoefficient(0.10f);
    titleObject_->SetScale({ 1.5f, 1.5f, 1.5f });

    // 同じ立体文字を少し奥へずらして描き、青空でも読めるドロップシャドウにする。
    titleShadowObject_ = std::make_unique<Object3d>();
    titleShadowObject_->Initialize(object3dCommon);
    titleShadowObject_->SetModel(titleModel_.get());
    titleShadowObject_->SetEnableLighting(false);
    titleShadowObject_->SetColor({ 0.055f, 0.09f, 0.20f, 1.0f });

    pressSpaceModel_ = Model::CreateFromOBJ(
        object3dCommon->GetDxCommon(), "Resources/UI/pressSpace", "pressSpace.obj",
        object3dCommon->GetTextureManager());
    pressSpaceObject_ = std::make_unique<Object3d>();
    pressSpaceObject_->Initialize(object3dCommon);
    pressSpaceObject_->SetModel(pressSpaceModel_.get());
    pressSpaceObject_->SetEnableLighting(false);
    pressSpaceObject_->SetColor({ 0.88f, 0.96f, 1.0f, 1.0f });

    decorationStarModel_ = Model::CreateFromOBJ(
        object3dCommon->GetDxCommon(), "Resources/Models/star", "star.obj",
        object3dCommon->GetTextureManager());

    // タイトル背景を横切る太陽と月。専用OBJと表面テクスチャを使用する。
    sunModel_ = Model::CreateFromOBJ(
        object3dCommon->GetDxCommon(), "Resources/Models/sun", "sun.obj",
        object3dCommon->GetTextureManager());
    moonModel_ = Model::CreateFromOBJ(
        object3dCommon->GetDxCommon(), "Resources/Models/moon", "moon.obj",
        object3dCommon->GetTextureManager());
    sunObject_ = std::make_unique<Object3d>();
    sunObject_->Initialize(object3dCommon);
    sunObject_->SetModel(sunModel_.get());
    sunObject_->SetEnableLighting(false);
    sunObject_->SetColor({ 1.0f, 0.62f, 0.08f, 1.0f });
    sunObject_->SetEmissive(8.5f);

    moonObject_ = std::make_unique<Object3d>();
    moonObject_->Initialize(object3dCommon);
    moonObject_->SetModel(moonModel_.get());
    moonObject_->SetEnableLighting(false);
    moonObject_->SetColor({ 0.42f, 0.66f, 1.0f, 1.0f });
    moonObject_->SetEmissive(3.6f);
    for (size_t i = 0; i < decorationStars_.size(); ++i) {
        decorationStars_[i] = std::make_unique<Object3d>();
        decorationStars_[i]->Initialize(object3dCommon);
        decorationStars_[i]->SetModel(decorationStarModel_.get());
        decorationStars_[i]->SetEnableLighting(true);
        decorationStars_[i]->SetColor(
            i % 2 == 0
            ? Vector4{ 1.0f, 0.90f, 0.48f, 1.0f }
            : Vector4{ 0.72f, 0.90f, 1.0f, 1.0f });
        decorationStars_[i]->SetShininess(0.85f);
        decorationStars_[i]->SetMetallic(0.18f);
        decorationStars_[i]->SetEmissive(2.2f);

        for (size_t trailIndex = 0; trailIndex < kTrailCount; ++trailIndex) {
            starTrails_[i][trailIndex] = std::make_unique<Object3d>();
            starTrails_[i][trailIndex]->Initialize(object3dCommon);
            starTrails_[i][trailIndex]->SetModel(decorationStarModel_.get());
            starTrails_[i][trailIndex]->SetEnableLighting(false);
            const float fade = 1.0f - static_cast<float>(trailIndex) /
                static_cast<float>(kTrailCount + 1);
            starTrails_[i][trailIndex]->SetColor(
                i % 2 == 0
                ? Vector4{ 1.0f * fade, 0.56f * fade, 0.10f * fade, 1.0f }
                : Vector4{ 0.10f * fade, 0.48f * fade, 1.0f * fade, 1.0f });
            starTrails_[i][trailIndex]->SetEmissive(1.7f - static_cast<float>(trailIndex) * 0.24f);
        }
    }

    // 飛び回る装飾星とは別に、夜だけ見える小さな遠景星を用意する。
    for (size_t i = 0; i < backgroundStars_.size(); ++i) {
        backgroundStars_[i] = std::make_unique<Object3d>();
        backgroundStars_[i]->Initialize(object3dCommon);
        backgroundStars_[i]->SetModel(decorationStarModel_.get());
        backgroundStars_[i]->SetEnableLighting(false);
        backgroundStars_[i]->SetColor(
            i % 3 == 0
            ? Vector4{ 0.62f, 0.78f, 1.0f, 1.0f }
            : Vector4{ 1.0f, 0.91f, 0.62f, 1.0f });
        backgroundStars_[i]->SetEmissive(2.4f);
        backgroundStars_[i]->SetScale({ 0.0f, 0.0f, 0.0f });
    }

    // タイトルが所定位置へ到着した瞬間だけ、交差する二本の星型フレアを走らせる。
    for (size_t i = 0; i < titleFlares_.size(); ++i) {
        titleFlares_[i] = std::make_unique<Object3d>();
        titleFlares_[i]->Initialize(object3dCommon);
        titleFlares_[i]->SetModel(decorationStarModel_.get());
        titleFlares_[i]->SetEnableLighting(false);
        titleFlares_[i]->SetColor(
            i == 0
            ? Vector4{ 1.0f, 0.72f, 0.16f, 1.0f }
            : Vector4{ 0.42f, 0.78f, 1.0f, 1.0f });
        titleFlares_[i]->SetEmissive(4.5f);
        titleFlares_[i]->SetScale({ 0.0f, 0.0f, 0.0f });
    }

    titlePosition_ = { 0.0f, -10.0f, 10.0f };
    titleRotation_ = {};
    spiralAngle_ = 0.0f;
    pressSpaceTimer_ = 0.0f;
    showPressSpace_ = false;
    startRequested_ = false;
    startAnimationTimer_ = 0.0f;
    idleTimer_ = 0.0f;
    environmentTimer_ = 0.0f;
    titleFlareTimer_ = 2.0f;
    titleArrivalFlareStarted_ = false;
    titleNightAmount_ = 0.0f;
    lastUpdateTime_ = std::chrono::steady_clock::now();
    camera_.SetPosition({ 0.0f, -2.0f, -20.0f });
    camera_.SetRotation({ 0.0f, 0.0f, 0.0f });
    camera_.Update();
}

void TitleScene::Update(GameRuntime& game, const SceneUpdateContext& context) {
    (void)context;

    const auto now = std::chrono::steady_clock::now();
    const float deltaTime = (std::clamp)(
        std::chrono::duration<float>(now - lastUpdateTime_).count(), 0.0f, 0.1f);
    lastUpdateTime_ = now;
    idleTimer_ += deltaTime;
    environmentTimer_ += deltaTime * game.GetEnvironmentTimeScale();
    game.UpdateTitleDayNightCycle(environmentTimer_);

    constexpr float kTitleDayNightCycleSeconds = 300.0f;
    const float titleCycle = std::fmod(environmentTimer_, kTitleDayNightCycleSeconds) /
        kTitleDayNightCycleSeconds * 4.0f;
    const size_t titleCycleIndex = (std::min)(static_cast<size_t>(titleCycle), size_t{ 3 });
    float titleCycleBlend = titleCycle - static_cast<float>(titleCycleIndex);
    titleCycleBlend = titleCycleBlend * titleCycleBlend * (3.0f - 2.0f * titleCycleBlend);
    constexpr float kNightKeys[] = { 0.0f, 0.25f, 1.0f, 0.42f, 0.0f };
    titleNightAmount_ = kNightKeys[titleCycleIndex] +
        (kNightKeys[titleCycleIndex + 1] - kNightKeys[titleCycleIndex]) * titleCycleBlend;

    // cycle=0 を昼の頂点とし、太陽と月を半周ずらして同じ空の軌道へ載せる。
    constexpr float kPi = 3.14159265f;
    constexpr float kTwoPi = kPi * 2.0f;
    const float dayProgress = std::fmod(environmentTimer_, kTitleDayNightCycleSeconds) /
        kTitleDayNightCycleSeconds;
    const float sunAngle = kPi * 0.5f + dayProgress * kTwoPi;
    const float moonAngle = sunAngle + kPi;
    const auto updateCelestial = [&](Object3d* object, float angle, float baseScale) {
        if (!object) {
            return;
        }
        const float altitude = std::sin(angle);
        // 表示開始の縮小区間を雲海の下に隠し、地平線では実寸で昇らせる。
        // 縮小は雲海の底より下で行い、昇降中は天体の大きさを維持する。
        const float visibility = std::clamp((altitude + 0.95f) / 0.15f, 0.0f, 1.0f);
        const float easedVisibility = visibility * visibility * (3.0f - 2.0f * visibility);
        const Vector3 position = {
            std::cos(angle) * 22.0f,
            -8.5f + altitude * 19.0f,
            64.0f
        };
        const float scale = baseScale * easedVisibility;
        object->SetPosition(position);
        object->SetRotation({ 0.0f, -angle * 0.18f, angle * 0.08f });
        object->SetScale({ scale, scale, scale });
        object->SetCamera(camera_.GetViewMatrix(), camera_.GetProjectionMatrix());
        object->Update(Math::MakeIdentity4x4());
    };

    if (titleArrivalFlareStarted_) {
        titleFlareTimer_ += deltaTime;
    }

    if (!startRequested_ && spiralAngle_ < 6.2831853f) {
        spiralAngle_ += 3.0f * deltaTime;
        constexpr float kTitleStopY = -2.0f;
        if (titlePosition_.y < kTitleStopY) {
            titlePosition_.y += 6.0f * deltaTime;
        }
        if (titlePosition_.y > kTitleStopY) {
            titlePosition_.y = kTitleStopY;
        }
        const float calculatedRadius = 2.0f * (1.0f - (titlePosition_.y + 10.0f) / 8.0f);
        const float radius = calculatedRadius > 0.0f ? calculatedRadius : 0.0f;
        titlePosition_.x = std::cos(spiralAngle_) * radius;
        titlePosition_.z = 10.0f + std::sin(spiralAngle_) * radius;
    }
    if (!startRequested_) {
        showPressSpace_ = titlePosition_.y >= -2.0f;
        // 上昇中だけ回転し、到着時には正面へ滑らかに戻して固定する。
        const float arrivalProgress = std::clamp((titlePosition_.y + 10.0f) / 8.0f, 0.0f, 1.0f);
        titleRotation_.y = showPressSpace_ ? 0.0f : std::sin(arrivalProgress * 3.14159265f) * 0.7f;
        if (showPressSpace_ && !titleArrivalFlareStarted_) {
            titleArrivalFlareStarted_ = true;
            titleFlareTimer_ = 0.0f;
        }
        pressSpaceTimer_ += 3.0f * deltaTime;

        Input* input = game.GetInput();
        if (showPressSpace_ && input &&
            (input->TriggerKey(DIK_SPACE) ||
             input->TriggerControllerButton(XINPUT_GAMEPAD_A))) {
            startRequested_ = true;
            startAnimationTimer_ = 0.0f;
            showPressSpace_ = false;
        }
    } else {
        startAnimationTimer_ += deltaTime;
        titleRotation_.y += (2.5f + startAnimationTimer_ * 14.0f) * deltaTime;
        titleRotation_.z = std::sin(startAnimationTimer_ * 11.0f) * 0.08f;

        if (startAnimationTimer_ >= 0.72f) {
            game.EnterStageSelectFromTitle();
        }
    }

    // 雲海を真横から眺める水平視点。上下の揺れと俯角は付けない。
    camera_.SetPosition({
        std::sin(idleTimer_ * 0.32f) * 0.28f,
        -2.0f,
        -20.0f
    });
    camera_.SetRotation({ 0.0f, 0.0f, 0.0f });
    camera_.Update();
    // 雲パーティクルとタイトルモデルが同じ視点から見えるよう、ランタイム側も同期する。
    game.SyncTitleCamera(camera_.GetPosition(), camera_.GetRotation());

    const Matrix4x4& view = camera_.GetViewMatrix();
    const Matrix4x4& projection = camera_.GetProjectionMatrix();
    updateCelestial(sunObject_.get(), sunAngle, 4.3f);
    updateCelestial(moonObject_.get(), moonAngle, 3.44f);
    if (sunObject_) {
        sunObject_->SetEmissive(7.4f + std::sin(idleTimer_ * 1.7f) * 1.1f);
    }
    if (moonObject_) {
        moonObject_->SetEmissive(3.2f + std::sin(idleTimer_ * 0.8f) * 0.4f);
    }
    Vector3 animatedTitlePosition = titlePosition_;
    Vector3 titleScale = { 1.5f, 1.5f, 1.5f };
    if (!startRequested_ && showPressSpace_) {
        animatedTitlePosition.y += std::sin(idleTimer_ * 1.8f) * 0.12f;
        titleRotation_.z = 0.0f;
        // 大きく見た目を変えず、静止画に見えない程度の呼吸を加える。
        const float breathingScale = 1.5f * (1.0f + std::sin(idleTimer_ * 1.35f) * 0.018f);
        titleScale = { breathingScale, breathingScale, breathingScale };
    } else if (startRequested_) {
        const float normalized = (std::min)(startAnimationTimer_ / 0.72f, 1.0f);
        if (normalized < 0.22f) {
            const float pop = normalized / 0.22f;
            const float scale = 1.5f + 0.35f * std::sin(pop * 3.14159265f);
            titleScale = { scale, scale, scale };
        } else {
            const float retreat = (normalized - 0.22f) / 0.78f;
            const float easedRetreat = retreat * retreat * retreat;
            const float scale = 1.5f * (1.0f - easedRetreat * 0.92f);
            titleScale = { scale, scale, scale };
            animatedTitlePosition.z += easedRetreat * 25.0f;
        }
    }
    titleObject_->SetPosition(animatedTitlePosition);
    titleObject_->SetRotation(titleRotation_);
    titleObject_->SetScale(titleScale);
    titleObject_->SetEmissive(
        0.045f + (std::sin(idleTimer_ * 1.35f) * 0.5f + 0.5f) * 0.035f);
    titleObject_->SetCamera(view, projection);
    titleObject_->Update(Math::MakeIdentity4x4());

    Vector3 shadowPosition = animatedTitlePosition;
    shadowPosition.x += 0.20f;
    shadowPosition.y -= 0.18f;
    shadowPosition.z += 0.34f;
    titleShadowObject_->SetPosition(shadowPosition);
    titleShadowObject_->SetRotation(titleRotation_);
    titleShadowObject_->SetScale({
        titleScale.x * 1.015f,
        titleScale.y * 1.015f,
        titleScale.z * 1.015f
    });
    titleShadowObject_->SetCamera(view, projection);
    titleShadowObject_->Update(Math::MakeIdentity4x4());

    const float transitionProgress = startRequested_
        ? (std::min)(startAnimationTimer_ / 0.72f, 1.0f)
        : 0.0f;
    for (size_t i = 0; i < decorationStars_.size(); ++i) {
        const float orbitSpeed = 0.24f + static_cast<float>(i) * 0.012f;
        const float phase = idleTimer_ * orbitSpeed +
            static_cast<float>(i) * (6.2831853f / static_cast<float>(decorationStars_.size()));
        const float radius = 5.8f + static_cast<float>(i % 3) * 0.65f + transitionProgress * 4.5f;
        Vector3 starPosition = {
            std::cos(phase) * radius,
            -1.8f + std::sin(phase * 1.7f) * 2.5f,
            11.8f + std::sin(phase) * 1.4f + transitionProgress * 8.0f
        };
        const float baseScale = 0.28f + static_cast<float>(i % 3) * 0.09f;
        const float starScale = baseScale * (1.0f - transitionProgress * 0.72f) *
            (1.0f + std::sin(idleTimer_ * 2.0f + static_cast<float>(i)) * 0.10f);
        decorationStars_[i]->SetPosition(starPosition);
        decorationStars_[i]->SetRotation({
            phase * 0.55f,
            -phase * 0.8f,
            phase * 0.35f
        });
        decorationStars_[i]->SetScale({ starScale, starScale, starScale });
        decorationStars_[i]->SetEmissive(
            1.8f + (std::sin(idleTimer_ * 2.8f + static_cast<float>(i) * 0.9f) * 0.5f + 0.5f) * 1.8f);
        decorationStars_[i]->SetCamera(view, projection);
        decorationStars_[i]->Update(Math::MakeIdentity4x4());

        // 現在位置より少し前の軌道を小さな星で再現し、彗星のような残光にする。
        for (size_t trailIndex = 0; trailIndex < kTrailCount; ++trailIndex) {
            const float lagSeconds = 0.32f * static_cast<float>(trailIndex + 1);
            const float trailPhase = phase - orbitSpeed * lagSeconds;
            const Vector3 trailPosition = {
                std::cos(trailPhase) * radius,
                -1.8f + std::sin(trailPhase * 1.7f) * 2.5f,
                11.8f + std::sin(trailPhase) * 1.4f + transitionProgress * 8.0f
            };
            const float trailFade = 1.0f - static_cast<float>(trailIndex + 1) /
                static_cast<float>(kTrailCount + 1);
            const float trailScale = starScale * trailFade * 0.62f;
            auto& trail = starTrails_[i][trailIndex];
            trail->SetPosition(trailPosition);
            trail->SetRotation({ trailPhase * 0.55f, -trailPhase * 0.8f, trailPhase * 0.35f });
            trail->SetScale({ trailScale, trailScale, trailScale });
            trail->SetCamera(view, projection);
            trail->Update(Math::MakeIdentity4x4());
        }
    }

    for (size_t i = 0; i < backgroundStars_.size(); ++i) {
        // 整数ハッシュ風の配置で毎起動時に同じ、しかし規則的に見えない星空にする。
        const float xSeed = std::fmod(static_cast<float>(i * 37 + 11), 101.0f) / 100.0f;
        const float ySeed = std::fmod(static_cast<float>(i * 53 + 7), 97.0f) / 96.0f;
        const float zSeed = std::fmod(static_cast<float>(i * 29 + 19), 89.0f) / 88.0f;
        const float twinkle = 0.72f +
            (std::sin(idleTimer_ * (1.2f + static_cast<float>(i % 5) * 0.18f) +
                static_cast<float>(i) * 1.73f) * 0.5f + 0.5f) * 0.28f;
        const float baseScale = 0.085f + static_cast<float>(i % 6) * 0.021f;
        // 薄明時にも完全な点以下にならないよう、出現量は平方根で立ち上げる。
        const float visibility = std::sqrt((std::max)(titleNightAmount_, 0.0f));
        const float visibleScale = baseScale * visibility * twinkle;
        backgroundStars_[i]->SetPosition({
            -10.2f + xSeed * 20.4f,
            -6.1f + ySeed * 12.6f,
            14.0f + zSeed * 10.5f
        });
        backgroundStars_[i]->SetRotation({ 0.0f, 0.0f, idleTimer_ * 0.12f + static_cast<float>(i) });
        backgroundStars_[i]->SetScale({ visibleScale, visibleScale, visibleScale });
        backgroundStars_[i]->SetEmissive(2.0f + titleNightAmount_ * 3.0f * twinkle);
        backgroundStars_[i]->SetCamera(view, projection);
        backgroundStars_[i]->Update(Math::MakeIdentity4x4());
    }

    // 到着直後に素早く開き、余韻を残して縮むタイトルフレア。
    const float flareLife = 1.15f;
    const float flareT = (std::min)(titleFlareTimer_ / flareLife, 1.0f);
    const float flareEnvelope = flareT < 0.18f
        ? flareT / 0.18f
        : (1.0f - flareT) * (1.0f - flareT);
    for (size_t i = 0; i < titleFlares_.size(); ++i) {
        const float length = flareEnvelope * (i == 0 ? 4.8f : 3.6f);
        titleFlares_[i]->SetPosition({ animatedTitlePosition.x, animatedTitlePosition.y, animatedTitlePosition.z + 0.65f });
        titleFlares_[i]->SetRotation({ 0.0f, 0.0f, idleTimer_ * (i == 0 ? 0.7f : -0.9f) + static_cast<float>(i) * 0.78f });
        titleFlares_[i]->SetScale({ length, length * 0.13f, length * 0.10f });
        titleFlares_[i]->SetEmissive(3.2f + flareEnvelope * 5.8f);
        titleFlares_[i]->SetCamera(view, projection);
        titleFlares_[i]->Update(Math::MakeIdentity4x4());
    }

    const float pulse = 1.0f + std::sin(pressSpaceTimer_) * 0.06f;
    pressSpaceObject_->SetPosition({ 0.0f, -5.5f, 10.0f });
    pressSpaceObject_->SetScale({ pulse, pulse, pulse });
    pressSpaceObject_->SetCamera(view, projection);
    pressSpaceObject_->Update(Math::MakeIdentity4x4());
    game.RunTitleScene();
}

void TitleScene::Draw(GameRuntime& game) {
    // タイトル専用の暖かなキーライト。文字の前面と側面へ明確な差を作る。
    if (Object3dCommon* common = game.GetObject3dCommon()) {
        common->SetCameraPosition(camera_.GetPosition());
        common->ClearPointLights();
        // 漂う星そのものを光源にして、タイトル表面へ色付きの光を落とす。
        // 星7灯とロゴの走査光1灯で、Object3dCommonの8灯制限内に収める。
        for (size_t i = 0; i < decorationStars_.size(); ++i) {
            if (!decorationStars_[i]) {
                continue;
            }
            const float pulse =
                std::sin(idleTimer_ * 2.8f + static_cast<float>(i) * 0.9f) * 0.5f + 0.5f;
            const Vector4 lightColor = i % 2 == 0
                ? Vector4{ 1.0f, 0.62f, 0.12f, 1.0f }
                : Vector4{ 0.12f, 0.55f, 1.0f, 1.0f };
            common->AddPointLight(
                decorationStars_[i]->GetPosition(),
                3.4f + pulse * 3.2f,
                lightColor,
                7.0f + pulse * 3.0f);
        }
        if (!startRequested_ && showPressSpace_ && titleObject_) {
            // 左端で現れ、文字面を横切って右端で消える柔らかなハイライト。
            const float sweep = std::fmod(idleTimer_, 5.2f) / 5.2f;
            const float sweepEnvelope = std::sin(sweep * 3.14159265f);
            Vector3 sweepPosition = titleObject_->GetPosition();
            sweepPosition.x += -5.4f + sweep * 10.8f;
            sweepPosition.y += 0.35f;
            sweepPosition.z -= 1.4f;
            common->AddPointLight(
                sweepPosition,
                4.6f * sweepEnvelope * sweepEnvelope,
                { 1.0f, 0.82f, 0.42f, 1.0f },
                3.1f);
        }
    }
    for (auto& star : backgroundStars_) {
        if (star && titleNightAmount_ > 0.01f) {
            star->Draw();
        }
    }
    for (auto& trails : starTrails_) {
        for (auto& trail : trails) {
            if (trail) {
                trail->Draw();
            }
        }
    }
    if (titleFlareTimer_ < 1.15f) {
        for (auto& flare : titleFlares_) {
            if (flare) {
                flare->Draw();
            }
        }
    }
    for (auto& star : decorationStars_) {
        if (star) {
            star->Draw();
        }
    }
    if (titleShadowObject_) {
        titleShadowObject_->Draw();
    }
    if (titleObject_) {
        titleObject_->Draw();
    }
    if (showPressSpace_ && pressSpaceObject_) {
        pressSpaceObject_->Draw();
    }
}

void TitleScene::DrawBackground(GameRuntime& game) {
    (void)game;
    if (sunObject_) {
        sunObject_->Draw();
    }
    if (moonObject_) {
        moonObject_->Draw();
    }
}

void TitleScene::Finalize(GameRuntime& game) {
    game.SetTitleCloudsEnabled(false);
    game.OnSceneExited(SceneType::Title);
    for (auto& star : decorationStars_) {
        star.reset();
    }
    for (auto& star : backgroundStars_) {
        star.reset();
    }
    for (auto& trails : starTrails_) {
        for (auto& trail : trails) {
            trail.reset();
        }
    }
    for (auto& flare : titleFlares_) {
        flare.reset();
    }
    decorationStarModel_.reset();
    moonObject_.reset();
    sunObject_.reset();
    moonModel_.reset();
    sunModel_.reset();
    pressSpaceObject_.reset();
    pressSpaceModel_.reset();
    titleShadowObject_.reset();
    titleObject_.reset();
    titleModel_.reset();
}
