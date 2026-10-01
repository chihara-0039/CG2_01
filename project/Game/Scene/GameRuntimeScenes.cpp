// シーンの入退場、切り替え要求、各シーン更新関数への振り分けを管理する。
#include "GameRuntime.h"
#include <algorithm>
#include <cmath>

void GameRuntime::SetTitleCloudsEnabled(bool enabled) {
    if (!particleManager) {
        return;
    }

    auto clouds = particleManager->GetAmbientCloudEmitter();
    clouds.active = enabled;
    if (enabled) {
        // タイトルのカメラ前方かつ文字より上へ、明るい積雲の層を作る。
        clouds.center = { -20.0f, 0.0f, 24.0f };
        clouds.minimumHeight = -8.0f;
        clouds.heightRange = 13.1f;
        clouds.areaX = 18.0f;
        clouds.areaZ = 11.0f;
        clouds.emitRate = 1.35f;
        clouds.life = 34.0f;
        clouds.size = 1.28f;
        clouds.speed = 0.02f;
        clouds.depthParallax = true;
        // 青空との差が消えない白さと濃度。通常アルファ合成なので発光しすぎない。
        clouds.color = { 0.96f, 0.975f, 1.0f, 0.56f };
        // 起動直後から十分な雲が見えるように初回更新をプリウォームする。
        clouds.emitTimer = 12.0f;
    } else {
        clouds.emitTimer = 0.0f;
        clouds.depthParallax = false;
        particleManager->ClearParticles();
    }
    particleManager->SetAmbientCloudEmitter(clouds);
}

void GameRuntime::UpdateTitleDayNightCycle(float elapsedSeconds) {
    constexpr float kCycleSeconds = 300.0f;
    const float cycle = std::fmod(elapsedSeconds, kCycleSeconds) / kCycleSeconds;
    const float scaledCycle = cycle * 4.0f;
    const size_t keyIndex = (std::min)(static_cast<size_t>(scaledCycle), size_t{ 3 });
    float blend = scaledCycle - static_cast<float>(keyIndex);
    blend = blend * blend * (3.0f - 2.0f * blend);

    struct EnvironmentKey {
        Vector4 clear;
        Vector4 sky;
        Vector4 cloud;
        Vector3 lightColor;
        Vector3 lightDirection;
        float lightIntensity;
    };
    const EnvironmentKey keys[] = {
        { { 0.26f, 0.61f, 0.94f, 1.0f }, { 0.94f, 0.98f, 1.08f, 1.0f }, { 0.96f, 0.975f, 1.0f, 0.56f }, { 1.0f, 0.91f, 0.72f }, { -0.32f, -0.58f, -0.75f }, 1.18f },
        { { 0.88f, 0.30f, 0.14f, 1.0f }, { 1.10f, 0.48f, 0.24f, 1.0f }, { 1.0f, 0.58f, 0.40f, 0.48f }, { 1.0f, 0.48f, 0.20f }, { -0.8f, -0.22f, 0.3f }, 0.68f },
        { { 0.012f, 0.022f, 0.085f, 1.0f }, { 0.10f, 0.16f, 0.34f, 1.0f }, { 0.20f, 0.30f, 0.52f, 0.30f }, { 0.28f, 0.45f, 0.92f }, { 0.16f, -0.72f, 0.48f }, 0.26f },
        { { 0.44f, 0.18f, 0.32f, 1.0f }, { 0.66f, 0.34f, 0.56f, 1.0f }, { 0.72f, 0.52f, 0.75f, 0.40f }, { 1.0f, 0.62f, 0.54f }, { 0.72f, -0.30f, 0.24f }, 0.62f },
        { { 0.26f, 0.61f, 0.94f, 1.0f }, { 0.94f, 0.98f, 1.08f, 1.0f }, { 0.96f, 0.975f, 1.0f, 0.56f }, { 1.0f, 0.91f, 0.72f }, { -0.32f, -0.58f, -0.75f }, 1.18f }
    };
    const auto lerp3 = [blend](const Vector3& a, const Vector3& b) {
        return Vector3{ a.x + (b.x - a.x) * blend, a.y + (b.y - a.y) * blend, a.z + (b.z - a.z) * blend };
    };
    const auto lerp4 = [blend](const Vector4& a, const Vector4& b) {
        return Vector4{ a.x + (b.x - a.x) * blend, a.y + (b.y - a.y) * blend,
            a.z + (b.z - a.z) * blend, a.w + (b.w - a.w) * blend };
    };
    const EnvironmentKey& from = keys[keyIndex];
    const EnvironmentKey& to = keys[keyIndex + 1];
    stageMap_.SetClearColor(lerp4(from.clear, to.clear));
    stageMap_.SetLightColor(lerp3(from.lightColor, to.lightColor));
    stageMap_.SetLightDirection(lerp3(from.lightDirection, to.lightDirection));
    stageMap_.SetLightIntensity(from.lightIntensity + (to.lightIntensity - from.lightIntensity) * blend);
    gameplaySkyTint_ = lerp4(from.sky, to.sky);
    if (particleManager) {
        particleManager->SetAmbientCloudColor(lerp4(from.cloud, to.cloud));
    }
    titleDayNightActive_ = true;
}

void GameRuntime::SyncTitleCamera(const Vector3& position, const Vector3& rotation) {
    if (!camera) {
        return;
    }
    camera->SetPosition(position);
    camera->SetRotation(rotation);
    camera->Update();
}

void GameRuntime::OnSceneEntered(SceneType sceneType) {
    // 画面効果はシーン固有の状態として扱い、前のシーンから持ち越さない。
    // Dissolveの内部タイマーも同時に止めるため、Controller経由で初期化する。
    EnsurePostProcessInitialized();
    postEffectShowcaseController_.Reset(postProcess_);
    postProcess_.SetEnabled(true);
    // 通常シーンの標準画作りとしてBloomを使う。専用展示シーンだけは
    // キー入力で各ポストエフェクトを単独確認できる従来仕様を優先する。
    postProcess_.SetPostEffectMode(
        sceneType == SceneType::PostEffectShowcase ? 0 : 12);

    switch (sceneType) {
    case SceneType::Title:
        currentMode_ = AppMode::Title;
        titleTimer_ = 0.0f;
        stageMap_.SetClearColor({ 0.26f, 0.61f, 0.94f, 1.0f });
        break;
    case SceneType::StageSelect:
        currentMode_ = AppMode::StageSelect;
        break;
    case SceneType::GameClear:
        currentMode_ = AppMode::GameClear;
        gameClearTimer_ = 0.0f;
        gameClearFireworkTimer_ = 0.0f;
        gameClearCelebrationStarted_ = false;
        // クリア文字と花火が同じ座標系になるよう、ランタイム側も専用カメラへ揃える。
        if (camera) {
            camera->SetPosition({ 0.0f, 2.0f, -20.0f });
            camera->SetRotation({ 0.25f, 0.0f, 0.0f });
            camera->Update();
        }
        break;
    case SceneType::DebugView:
        currentMode_ = AppMode::DebugView;
        break;
    case SceneType::StageEditor:
        currentMode_ = AppMode::StageEditor;
        break;
    case SceneType::GamePlay:
        currentMode_ = AppMode::GamePlay;
        break;
    case SceneType::GamePlayBlockPlace:
        currentMode_ = AppMode::GamePlay_BlockPlace;
        break;
    case SceneType::SkinningEditor:
        currentMode_ = AppMode::SkinningEditor;
        break;
    case SceneType::EffectPreview:
        currentMode_ = AppMode::EffectPreview;
        break;
    case SceneType::EffectShowcase:
        currentMode_ = AppMode::EffectShowcase;
        break;
    case SceneType::PostEffectShowcase:
        currentMode_ = AppMode::PostEffectShowcase;
        break;
    }

    HandleModeChange();

    // 天候を使わないシーンでは、Emitterを止めるだけでなく既に生成済みの
    // 雨・雪・雲も破棄する。これにより遷移タイミングに依存した残留を防ぐ。
    const bool usesStageWeather =
        sceneType == SceneType::StageEditor ||
        sceneType == SceneType::GamePlay ||
        sceneType == SceneType::GamePlayBlockPlace;
    const bool ownsEffectParticles =
        sceneType == SceneType::EffectPreview ||
        sceneType == SceneType::EffectShowcase;
    if (!usesStageWeather && !ownsEffectParticles && particleManager) {
        weatherRuntimeController_.StopStorm(*particleManager);
        particleManager->SetWeatherEmitterActive(false);
        particleManager->SetAmbientCloudEmitterActive(false);
        particleManager->ClearParticles();
    }
}

void GameRuntime::OnSceneExited(SceneType sceneType) {
    if (sceneType == SceneType::Title) {
        titleDayNightActive_ = false;
    }
    if ((sceneType == SceneType::EffectPreview || sceneType == SceneType::EffectShowcase) && particleManager) {
        particleManager->SetStormActive(false);
    }
}

void GameRuntime::RequestSceneChange(SceneType sceneType) {
    const SceneType currentScene = sceneManager_
        ? sceneManager_->GetCurrentSceneType()
        : GetCurrentSceneType();
    if (currentScene == sceneType || sceneFadePhase_ != SceneFadePhase::None) {
        return;
    }

    const bool currentIsGameplaySubMode =
        currentScene == SceneType::GamePlay || currentScene == SceneType::GamePlayBlockPlace;
    const bool nextIsGameplaySubMode =
        sceneType == SceneType::GamePlay || sceneType == SceneType::GamePlayBlockPlace;
    if (currentIsGameplaySubMode && nextIsGameplaySubMode) {
        ApplySceneTypeToMode(sceneType);
        return;
    }

    pendingSceneType_ = sceneType;
    sceneFadePhase_ = SceneFadePhase::FadingOut;
    sceneFadeAlpha_ = 0.0f;
    sceneFadeHoldRemaining_ = 0.0f;
    sceneFadeLastUpdate_ = std::chrono::steady_clock::now();
}

void GameRuntime::ApplySceneTypeToMode(SceneType sceneType) {
    switch (sceneType) {
    case SceneType::Title: currentMode_ = AppMode::Title; break;
    case SceneType::StageSelect: currentMode_ = AppMode::StageSelect; break;
    case SceneType::GameClear: currentMode_ = AppMode::GameClear; break;
    case SceneType::DebugView: currentMode_ = AppMode::DebugView; break;
    case SceneType::StageEditor: currentMode_ = AppMode::StageEditor; break;
    case SceneType::GamePlay: currentMode_ = AppMode::GamePlay; break;
    case SceneType::GamePlayBlockPlace: currentMode_ = AppMode::GamePlay_BlockPlace; break;
    case SceneType::SkinningEditor: currentMode_ = AppMode::SkinningEditor; break;
    case SceneType::EffectPreview: currentMode_ = AppMode::EffectPreview; break;
    case SceneType::EffectShowcase: currentMode_ = AppMode::EffectShowcase; break;
    case SceneType::PostEffectShowcase: currentMode_ = AppMode::PostEffectShowcase; break;
    }
}

void GameRuntime::UpdateSceneFade() {
    if (sceneFadePhase_ == SceneFadePhase::None || !sceneFadeSprite_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    const float deltaTime = (std::clamp)(
        std::chrono::duration<float>(now - sceneFadeLastUpdate_).count(),
        0.0f,
        0.1f);
    sceneFadeLastUpdate_ = now;

    constexpr float kFadeSpeed = 3.0f;
    if (sceneFadePhase_ == SceneFadePhase::FadingOut) {
        sceneFadeAlpha_ = (std::min)(1.0f, sceneFadeAlpha_ + kFadeSpeed * deltaTime);
        if (sceneFadeAlpha_ >= 1.0f) {
            if (sceneManager_) {
                sceneManager_->ChangeScene(pendingSceneType_, *this);
            } else {
                ApplySceneTypeToMode(pendingSceneType_);
            }
            // 遷移先の最初の更新・描画を完全な黒の裏で済ませる。
            // 読み込み直後の未更新Spriteが一瞬大きく表示されるのを防止する。
            sceneFadePhase_ = SceneFadePhase::HoldingBlack;
            sceneFadeHoldRemaining_ = 0.12f;
        }
    } else if (sceneFadePhase_ == SceneFadePhase::HoldingBlack) {
        sceneFadeAlpha_ = 1.0f;
        sceneFadeHoldRemaining_ -= deltaTime;
        if (sceneFadeHoldRemaining_ <= 0.0f) {
            sceneFadePhase_ = SceneFadePhase::FadingIn;
        }
    } else {
        sceneFadeAlpha_ = (std::max)(0.0f, sceneFadeAlpha_ - kFadeSpeed * deltaTime);
        if (sceneFadeAlpha_ <= 0.0f) {
            sceneFadePhase_ = SceneFadePhase::None;
        }
    }

    sceneFadeSprite_->SetColor({ 0.0f, 0.0f, 0.0f, sceneFadeAlpha_ });
    sceneFadeSprite_->Update();
}

SceneType GameRuntime::GetCurrentSceneType() const {
    if (currentMode_ == AppMode::Title) {
        return SceneType::Title;
    }
    if (currentMode_ == AppMode::StageSelect) {
        return SceneType::StageSelect;
    }
    if (currentMode_ == AppMode::GameClear) {
        return SceneType::GameClear;
    }
    if (currentMode_ == AppMode::DebugView) {
        return SceneType::DebugView;
    }
    if (currentMode_ == AppMode::StageEditor) {
        return SceneType::StageEditor;
    }
    if (currentMode_ == AppMode::GamePlay) {
        return SceneType::GamePlay;
    }
    if (currentMode_ == AppMode::GamePlay_BlockPlace) {
        return SceneType::GamePlayBlockPlace;
    }
    if (currentMode_ == AppMode::SkinningEditor) {
        return SceneType::SkinningEditor;
    }
    if (currentMode_ == AppMode::EffectPreview) {
        return SceneType::EffectPreview;
    }
    if (currentMode_ == AppMode::EffectShowcase) {
        return SceneType::EffectShowcase;
    }
    if (currentMode_ == AppMode::PostEffectShowcase) {
        return SceneType::PostEffectShowcase;
    }

    return SceneType::DebugView;
}

void GameRuntime::RunTitleScene() {
    UpdateTitle();
}

void GameRuntime::RunStageSelectScene() {
    UpdateStageSelect();
}

void GameRuntime::RunGameClearScene(bool celebrationReady) {
    UpdateGameClear(celebrationReady);
}

void GameRuntime::RunDebugViewScene() {
    UpdateDebugView();
}

void GameRuntime::RunStageEditorScene() {
    stageEditorController_.Update(
        input.get(), stageMap_, stageRenderer_.get(),
        mapCursor_.get(), lightCamera_.get(), player_.get(), camera.get());
}

void GameRuntime::RunGamePlayScene() {
    UpdateGamePlay();
}

void GameRuntime::RunGamePlayBlockPlaceScene() {
    UpdateGamePlayBlockPlace();
}

void GameRuntime::RunSkinningEditorScene(const SceneUpdateContext& context) {
    EnsureSkinningEditorInitialized();
    skinningEditor_.Update(
        dxCommon.get(), input.get(), camera.get(),
        context.lightViewProjection, context.isGuiCaptured, particleManager.get());

    if (skinningEditor_.ConsumePlayRequest()) {
        strncpy_s(blenderLevelPath_.data(), blenderLevelPath_.size(),
            skinningEditor_.GetSceneFilePath(), _TRUNCATE);
        LoadBlenderStage(true);
    }
}

void GameRuntime::RunEffectPreviewScene() {
    UpdateEffectPreview();
}

void GameRuntime::RunEffectShowcaseScene() {
    UpdateEffectShowcase();
    DrawEffectShowcaseImGui();
}

void GameRuntime::RunPostEffectShowcaseScene() {
    UpdatePostEffectShowcase();
    DrawPostEffectShowcaseImGui();
}




