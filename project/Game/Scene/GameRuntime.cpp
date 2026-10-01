#include <algorithm>
#include <cmath>
#include "GameRuntime.h"
#include "EffectPresetStore.h"
#include "../Environment/WeatherPresetManager.h"

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

void GameRuntime::Update() {


    HandleModeChange();


    BeginFrameImGui();


    input->Update();
    UpdateBlenderLevelFileWatch();
    if (input->TriggerKey(DIK_F3)) {
        debugFlags_.showCollisionBoxes = !debugFlags_.showCollisionBoxes;
    }
    UpdateSceneFade();
    UpdateSceneTransition();


    bool isGuiCaptured = IsGuiCapturingMouse();


    Vector3 lightDir = UpdateLightCameraForFrame();
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // 雷・ヒット発光の寿命は表示モードに関係なく毎フレーム減衰させる。
    // 以前はShowcase内だけで減衰していたため、通常ゲームでは次の雷まで残っていた。
    effectShowcaseController_.TickLight(1.0f / 60.0f);

    UpdateHitEffectShortcut();


    // インベントリやポーズを開いている間は、カメラの距離・角度を変更しない。
    const bool gameplayUiBlocksCamera =
        (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) &&
        (isGamePaused_ || (blockInventoryUI_ && blockInventoryUI_->IsActive()));
    if (!gameplayUiBlocksCamera) {
        UpdateSharedCameraControls(isGuiCaptured);
    }

    camera->Update();

    UpdateBackgroundObjects();


    UpdateParticleDebugVisibility();

    UpdateCurrentMode(lightVP, isGuiCaptured);

    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();


    UpdatePlayerCameraAndTransform(view, proj, lightVP);


    if (IsWindowInactive()) {
        return;
    }


    UpdateDebugAndEffectObjects(view, proj, lightVP);


    UpdateStagePresentation(view, proj, lightVP);
    UpdateRuntimeLevelObjects(view, proj, lightVP);


    UpdateWeatherParticles(view, proj);


    ApplySceneLighting(lightDir);


    UpdateClearColorForFrame();


    UpdateGameplayUserInterface();
}

void GameRuntime::HandleModeChange() {
    if (currentMode_ == prevMode_) {
        return;
    }

    // PostEffect表示で無効化したパーティクル表示を、天候を使うシーンへ持ち越さない。
    if (currentMode_ == AppMode::StageEditor ||
        currentMode_ == AppMode::GamePlay ||
        currentMode_ == AppMode::GamePlay_BlockPlace) {
        debugFlags_.showParticles = true;
    }
    if ((currentMode_ == AppMode::Title || currentMode_ == AppMode::StageSelect) && particleManager) {
        // 継続型の嵐は寿命の長い雨・風を持つため、停止だけでなく残存粒子も破棄する。
        weatherRuntimeController_.StopStorm(*particleManager);
        particleManager->GetWeatherEmitter().active = false;
        particleManager->GetAmbientCloudEmitter().active = false;
        particleManager->ClearParticles();
    }

    bgmController_.Update(
        currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);

    const bool leftEffectPresentation =
        (prevMode_ == AppMode::EffectPreview || prevMode_ == AppMode::EffectShowcase) &&
        (currentMode_ != AppMode::EffectPreview && currentMode_ != AppMode::EffectShowcase);
    if (leftEffectPresentation && particleManager) {
        particleManager->SetStormActive(false);
    }

    if (currentMode_ == AppMode::SkinningEditor) {
        EnsureSkinningEditorInitialized();
        EnsureTerrainInitialized();
        debugFlags_.showTerrain = true;
        terrainObject_->SetPosition({ 0.0f, -0.45f, 0.0f });
        terrainObject_->SetScale({ 0.22f, 0.22f, 0.22f });
        camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
    } else if (currentMode_ == AppMode::DebugView) {
        // DebugViewへ戻った時は、評価確認用の標準オブジェクトと地形を復帰させる。
        debugFlags_.show3DObjects = true;
        debugFlags_.showTerrain = true;
        EnsureTerrainInitialized();
    } else if (currentMode_ == AppMode::StageEditor) {
        // 旧左パネルを廃止して広がったシーンビューの中央へ、編集対象を再配置する。
        const Vector3 focus = player_ ? player_->GetPosition() : Vector3{ 8.0f, 1.0f, 8.0f };
        camera->ForceReset(focus, 18.0f, { 0.35f, 0.0f, 0.0f });
    } else if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        EnsureTerrainInitialized();
        camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
        effectShowcaseController_.Reset();
    } else if (currentMode_ == AppMode::PostEffectShowcase) {
        EnsurePostProcessInitialized();
        EnsureTerrainInitialized();
        camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 7.0f, { 0.35f, 0.0f, 0.0f });
    }

    prevMode_ = currentMode_;
}

void GameRuntime::BeginFrameImGui() {
    dxCommon->BeginImGui();
#ifndef NDEBUG
    UpdateImGui();
#endif
}

bool GameRuntime::IsGuiCapturingMouse() {
#ifndef NDEBUG
    return ImGui::GetIO().WantCaptureMouse;
#else
    return false;
#endif
}

Vector3 GameRuntime::UpdateLightCameraForFrame() {
    Vector3 lightDir = stageMap_.GetLightDirection();
    if (lightCamera_) {
        const Vector3 targetPos = player_ ? player_->GetPosition() : camera->GetPosition();
        lightCamera_->Update(lightDir, targetPos);
    }
    return lightDir;
}

void GameRuntime::UpdateHitEffectShortcut() {
    if (currentMode_ == AppMode::PostEffectShowcase) {
        return;
    }
    if (!input->TriggerKey(DIK_H) || !particleManager) {
        return;
    }

    Vector3 effectPos = effectPreviewPosition_;
    if (currentMode_ != AppMode::EffectPreview && currentMode_ != AppMode::EffectShowcase) {
        effectPos = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
        effectPos.y += 0.9f;
    }

    if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        if (IsCurrentEffectStorm()) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        } else {
            EmitEffectPreviewBurst();
        }
    } else {
        particleManager->EmitHitEffect(effectPos);
    }
}

void GameRuntime::UpdateSharedCameraControls(bool isGuiCaptured) {
    // 通常プレイとブロック配置中は同じ追従カメラを使用する。
    // 配置モードへ入った瞬間に Blender 形式の編集カメラへ切り替えると、
    // インベントリを開く前の視点が失われるため、ここでは更新しない。
    if (currentMode_ != AppMode::GamePlay &&
        currentMode_ != AppMode::GamePlay_BlockPlace &&
        currentMode_ != AppMode::GameClear &&
        currentMode_ != AppMode::DebugView) {
        const bool invertEditorOrbit = currentMode_ == AppMode::StageEditor;
        camera->UpdateBlenderStyle(
            input.get(), isGuiCaptured, winApp->GetHwnd(), invertEditorOrbit);
    }
}

void GameRuntime::UpdateBackgroundObjects() {
    if (skydomeObject_ && debugFlags_.showSkybox && !showSkyboxCubemap_) {
        skydomeObject_->SetPosition(camera->GetPosition());
        skydomeObject_->Update(Math::MakeIdentity4x4());
    }
    if (skybox_ && debugFlags_.showSkybox && showSkyboxCubemap_) {
        skybox_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skybox_->Update(camera->GetPosition());
    }
}

void GameRuntime::UpdateParticleDebugVisibility() {
    if (particleManager &&
        currentMode_ != AppMode::EffectPreview &&
        currentMode_ != AppMode::EffectShowcase &&
        currentMode_ != AppMode::PostEffectShowcase &&
        currentMode_ != AppMode::SkinningEditor) {
        particleManager->SetDrawGPUParticleSphere(false);
    }
}

void GameRuntime::UpdateCurrentMode(const Matrix4x4& lightVP, bool isGuiCaptured) {
    if (!sceneManager_) {
        return;
    }

    // 暗転中に遷移先を一度更新して、Spriteやカメラの初期値が露出するのを防ぐ。
    // 遷移元は更新せず、遷移先は黒画面の裏とフェードイン中に更新する。
    if (sceneFadePhase_ == SceneFadePhase::FadingOut) {
        return;
    }

    const SceneType requestedScene = GetCurrentSceneType();
    if (sceneManager_->GetCurrentSceneType() != requestedScene) {
        sceneManager_->ChangeScene(requestedScene, *this);
    }

    sceneManager_->Update(*this, SceneUpdateContext{ lightVP, isGuiCaptured });

    const SceneType sceneAfterUpdate = GetCurrentSceneType();
    if (sceneManager_->GetCurrentSceneType() != sceneAfterUpdate) {
        sceneManager_->ChangeScene(sceneAfterUpdate, *this);
    }
}

void GameRuntime::UpdatePlayerCameraAndTransform(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    if (!player_) {
        return;
    }

    player_->SetCamera(view, proj);
    if (currentMode_ == AppMode::DebugView) {
        // DebugView の見た目は StageMap と一致しないため、不可視のステージ壁を無効化する。
        player_->SetStageCollisionEnabled(false);
        player_->Update(input.get(), stageMap_, camera->GetRotation().y, lightVP, dxCommon.get());

        // 衝突判定後に確定した座標を追う。入力方向だけでカメラが先行する状態を防ぐ。
        camera->SetFov(gameplayCameraController_.GetFov());
        gameplayCameraController_.SetFollowPlayerMode(true);
        gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(), player_.get());
        camera->Update();
        player_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());

        // CG4 評価課題の装備は Animation モードではなく通常 DebugView で確認する。
        EnsureSkinningEditorInitialized();
        skinningEditor_.UpdateDebugViewAttachments(
            player_->GetSkinnedObject(), dxCommon.get(), camera.get(), lightVP, particleManager.get());
    } else if (currentMode_ == AppMode::GamePlay_BlockPlace) {
        // 配置中もプレイ時の追従カメラを継続し、切り替え前の距離と角度を保持する。
        // 右スティックの上下入力とトリガーによるズームもこの経路で処理する。
        player_->SetStageCollisionEnabled(true);
        player_->UpdateTransform(lightVP);
        camera->SetFov(gameplayCameraController_.GetFov());
        gameplayCameraController_.SetFollowPlayerMode(true);
        gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(), player_.get());
        camera->Update();
        player_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
    } else if (currentMode_ != AppMode::GamePlay) {
        player_->SetStageCollisionEnabled(true);
        player_->UpdateTransform(lightVP);
    }
}

bool GameRuntime::IsWindowInactive() {
    return GetActiveWindow() != winApp->GetHwnd();
}

void GameRuntime::UpdateDebugAndEffectObjects(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    if (debugFlags_.show3DObjects && currentMode_ == AppMode::DebugView) {
        for (auto& obj : objectList) {
            if (obj) {
                obj->SetCamera(view, proj);
                obj->Update(lightVP);
            }
        }
    }

    // Terrain は通常デバッグオブジェクトとは別の表示項目として更新する。
    // Show 3D Objectsを切っても、Show Terrainが有効なら地形だけを確認できる。
    if (debugFlags_.showTerrain && currentMode_ == AppMode::DebugView) {
        EnsureTerrainInitialized();
        // DebugView はプレイヤー確認が主目的なので、地形を足元へ寄せて扱いやすい縮尺にする。
        terrainObject_->SetPosition({ 0.0f, -0.45f, 0.0f });
        terrainObject_->SetScale({ 0.25f, 0.25f, 0.25f });
        terrainObject_->SetCamera(view, proj);
        terrainObject_->Update(lightVP);
    }

    if ((currentMode_ == AppMode::SkinningEditor ||
         currentMode_ == AppMode::EffectPreview ||
         currentMode_ == AppMode::EffectShowcase ||
         currentMode_ == AppMode::PostEffectShowcase)) {
        EnsureTerrainInitialized();
        terrainObject_->SetCamera(view, proj);
        terrainObject_->Update(lightVP);
    }
}

void GameRuntime::UpdateStagePresentation(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    // ステージエディターで外部レベルを確認している間は、グリッドステージを重ねない。
    const bool showNativeStage =
        !(currentMode_ == AppMode::StageEditor && blenderStageActive_);

    if (stageRenderer_ && showNativeStage) {
        stageRenderer_->SetIsEditorMode(currentMode_ == AppMode::StageEditor);
        stageRenderer_->SetCamera(view, proj);
        stageRenderer_->Update(stageMap_, lightVP);
    }

    if (stageRenderer_ && player_ && showNativeStage) {
        stageRenderer_->UpdateCloudTransparency(
            camera->GetPosition(),
            player_->GetPosition()
        );
    }

    if (mapCursor_ && showNativeStage &&
        (currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        mapCursor_->SetCamera(view, proj);
        mapCursor_->Update(lightVP);
    }
}

void GameRuntime::UpdateWeatherParticles(const Matrix4x4& view, const Matrix4x4& proj) {
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        sprite->Update();
    }
    if (!debugFlags_.showParticles || !particleManager || !textureManager) {
        return;
    }

    const bool usesStageWeather =
        currentMode_ == AppMode::StageEditor ||
        currentMode_ == AppMode::GamePlay ||
        currentMode_ == AppMode::GamePlay_BlockPlace;

    bool lightningFlashed = false;
    if (usesStageWeather) {
        if (WeatherPreset* preset =
            WeatherPresetManager::GetInstance().GetPresetByName(stageMap_.GetWeatherPresetName())) {
            // プリセット編集内容をゲーム内の環境光と背景色へ即時反映する。
            stageMap_.SetClearColor(preset->clearColor);
            stageMap_.SetLightIntensity(preset->lightIntensity);
            stageMap_.SetLightColor(preset->lightColor);
            stageMap_.SetLightDirection(preset->lightDirection);
        }
        const Vector3 focusPosition = player_ ? player_->GetPosition() : Vector3{0.0f, 0.0f, 0.0f};
        WeatherRuntimeController::UpdateContext context{
            *particleManager,
            *textureManager,
            stageMap_,
            view,
            proj,
            focusPosition,
            stageMap_.GetWeatherPresetName(),
            false,
            [this](const std::string& name) { return LoadStormPreset(name); }
        };
        lightningFlashed = weatherRuntimeController_.Update(context);

        // スカイドームを使う実ゲーム中だけ、天候プリセットの環境要素を
        // 昼 -> 夕方 -> 夜 -> 朝へゆっくり連続補間する。
        const WeatherPreset* activePreset =
            WeatherPresetManager::GetInstance().GetPresetByName(stageMap_.GetWeatherPresetName());
        const bool isGameplayScene =
            currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace;
        gameplayDayNightActive_ = isGameplayScene && activePreset && activePreset->stormPreset.empty();
        if (gameplayDayNightActive_) {
            if (!isGamePaused_) {
                gameplayDayNightTimer_ += (1.0f / 60.0f) * environmentTimeScale_;
            }
            constexpr float kCycleSeconds = 300.0f;
            const float cycle = std::fmod(gameplayDayNightTimer_, kCycleSeconds) / kCycleSeconds;
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
                { { 0.26f, 0.61f, 0.94f, 1.0f }, { 0.94f, 0.98f, 1.08f, 1.0f }, { 0.90f, 0.95f, 1.0f, 0.20f }, { 1.0f, 0.98f, 0.92f }, { 0.5f, -1.0f, 0.5f }, 1.08f },
                { { 0.88f, 0.30f, 0.14f, 1.0f }, { 1.10f, 0.48f, 0.24f, 1.0f }, { 1.0f, 0.58f, 0.40f, 0.24f }, { 1.0f, 0.48f, 0.20f }, { -0.8f, -0.22f, 0.3f }, 0.68f },
                { { 0.012f, 0.022f, 0.085f, 1.0f }, { 0.10f, 0.16f, 0.34f, 1.0f }, { 0.16f, 0.24f, 0.46f, 0.17f }, { 0.30f, 0.46f, 0.88f }, { 0.2f, -0.72f, -0.4f }, 0.24f },
                { { 0.44f, 0.18f, 0.32f, 1.0f }, { 0.66f, 0.34f, 0.56f, 1.0f }, { 0.72f, 0.52f, 0.76f, 0.21f }, { 1.0f, 0.62f, 0.54f }, { 0.75f, -0.30f, 0.2f }, 0.58f },
                { { 0.26f, 0.61f, 0.94f, 1.0f }, { 0.94f, 0.98f, 1.08f, 1.0f }, { 0.90f, 0.95f, 1.0f, 0.20f }, { 1.0f, 0.98f, 0.92f }, { 0.5f, -1.0f, 0.5f }, 1.08f }
            };
            const auto lerp3 = [blend](const Vector3& a, const Vector3& b) {
                return Vector3{
                    a.x + (b.x - a.x) * blend,
                    a.y + (b.y - a.y) * blend,
                    a.z + (b.z - a.z) * blend };
            };
            const auto lerp4 = [blend](const Vector4& a, const Vector4& b) {
                return Vector4{
                    a.x + (b.x - a.x) * blend,
                    a.y + (b.y - a.y) * blend,
                    a.z + (b.z - a.z) * blend,
                    a.w + (b.w - a.w) * blend };
            };
            const EnvironmentKey& from = keys[keyIndex];
            const EnvironmentKey& to = keys[keyIndex + 1];
            stageMap_.SetClearColor(lerp4(from.clear, to.clear));
            stageMap_.SetLightColor(lerp3(from.lightColor, to.lightColor));
            stageMap_.SetLightDirection(lerp3(from.lightDirection, to.lightDirection));
            stageMap_.SetLightIntensity(
                from.lightIntensity + (to.lightIntensity - from.lightIntensity) * blend);
            gameplaySkyTint_ = lerp4(from.sky, to.sky);
            particleManager->GetAmbientCloudEmitter().color = lerp4(from.cloud, to.cloud);
        }
    } else {
        gameplayDayNightActive_ = false;
        // エフェクト編集系ではステージ天候を上書きせず、発生済みエフェクトだけ更新する。
        particleManager->GetWeatherEmitter().active = false;
        // タイトルだけは専用の明るい雲パーティクルを継続する。
        if (currentMode_ != AppMode::Title) {
            particleManager->GetAmbientCloudEmitter().active = false;
        }
        particleManager->Update(
            1.0f / 60.0f, view, proj,
            player_ ? player_->GetPosition() : Vector3{0.0f, 0.0f, 0.0f},
            &stageMap_);
        lightningFlashed = particleManager->ConsumeStormLightningFlash();
    }

    if (lightningFlashed) {
        effectShowcaseController_.NotifyImpact();
    }
}
void GameRuntime::ApplySceneLighting(const Vector3& lightDir) {
    object3dCommon->SetLightDirection(lightDir);
    object3dCommon->SetLightColor(Vector4(
        stageMap_.GetLightColor().x,
        stageMap_.GetLightColor().y,
        stageMap_.GetLightColor().z, 1.0f));
    const bool isEffectPresentation = currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase;
    object3dCommon->SetLightIntensity(isEffectPresentation ? 0.18f : stageMap_.GetLightIntensity());
    object3dCommon->SetCameraPosition(camera->GetPosition());
    object3dCommon->ClearPointLights();

    const bool isPlayerScene =
        currentMode_ == AppMode::DebugView ||
        currentMode_ == AppMode::StageEditor ||
        currentMode_ == AppMode::GamePlay ||
        currentMode_ == AppMode::GamePlay_BlockPlace;
    if (player_ && isPlayerScene) {
        // プレイヤー本体の発光と、周囲を照らすライトを常時維持する。
        player_->SetGlow(playerGlow_);
        Vector3 playerLightPosition = player_->GetPosition();
        playerLightPosition.y += 0.8f;
        // DebugViewでは発光スライダーを周囲光にも連動させる。
        // ゲームプレイ中の常時プレイヤーライトは従来どおり維持する。
        const float pointLightIntensity =
            currentMode_ == AppMode::DebugView
            ? playerLightIntensity_ * std::clamp(playerGlow_ / 5.0f, 0.0f, 1.0f)
            : playerLightIntensity_;
        if (pointLightIntensity > 0.001f) {
            object3dCommon->AddPointLight(
                playerLightPosition, pointLightIntensity, playerLightColor_, 12.0f);
        }
    }

    if (isEffectPresentation) {
        const bool isStorm = IsCurrentEffectStorm();
        const float remaining = effectShowcaseController_.GetLightRatio();
        const float lightEnvelope = remaining * remaining;
        const Vector4 sourceColor = isStorm && particleManager
            ? particleManager->GetStormSettings().lightningColor
            : effectPreviewHitSettings_.lightningCount > 0
            ? effectPreviewHitSettings_.lightningColor
            : effectPreviewHitSettings_.coreColor;
        const Vector4 lightColor = {
            std::clamp(sourceColor.x, 0.0f, 1.0f),
            std::clamp(sourceColor.y, 0.0f, 1.0f),
            std::clamp(sourceColor.z, 0.0f, 1.0f),
            1.0f
        };
        const float lightIntensity = isStorm && particleManager
            ? particleManager->GetStormSettings().pointLightPower * lightEnvelope
            : (1.8f + effectPreviewHitSettings_.brightness * 2.8f) * lightEnvelope;
        const Vector3 lightPosition = isStorm && particleManager
            ? particleManager->GetStormLightningPosition()
            : effectPreviewPosition_;
        object3dCommon->AddPointLight(lightPosition, lightIntensity, lightColor, 18.0f);
    } else if (weatherRuntimeController_.IsStormActive() && particleManager) {
        // 嵐の雷光はプレイヤーライトを消さず、追加ライトとして重ねる。
        const float lightningEnvelope = effectShowcaseController_.GetLightRatio();
        if (lightningEnvelope > 0.0f) {
            const auto& storm = particleManager->GetStormSettings();
            object3dCommon->AddPointLight(
                particleManager->GetStormLightningPosition(),
                storm.pointLightPower * lightningEnvelope * lightningEnvelope,
                storm.lightningColor,
                24.0f);
        }
    }
}

void GameRuntime::UpdateClearColorForFrame() {
    if (postProcessInitialized_) {
        postProcess_.SetClearColor(stageMap_.GetClearColor());
    }
    const bool stormBackdrop = IsCurrentEffectStorm();
    if (stormBackdrop) {
        dxCommon->SetClearColor(0.012f, 0.018f, 0.045f, 1.0f);
    } else {
        const Vector4& clear = stageMap_.GetClearColor();
        dxCommon->SetClearColor(clear.x, clear.y, clear.z, clear.w);
    }
}

void GameRuntime::UpdateGameplayUserInterface() {
    if (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) {
        if (objectiveGuideSprite_) {
            objectiveGuideSprite_->Update();
        }
    }
    if (currentMode_ == AppMode::GamePlay && player_ && hasGoalGuideTarget_ &&
        !isGoalReached_ && goalDirectionSprite_) {
        const Vector3 playerPosition = player_->GetPosition();
        const float directionX = goalGuideTarget_.x - playerPosition.x;
        const float directionZ = goalGuideTarget_.z - playerPosition.z;
        const float worldAngle = std::atan2(directionX, directionZ);
        const float cameraYaw = camera ? camera->GetRotation().y : 0.0f;
        goalDirectionSprite_->SetRotation(worldAngle - cameraYaw);
        goalDirectionSprite_->Update();
    }
    if (currentMode_ == AppMode::StageSelect) {
        if (stageSelectGuideSprite_) {
            stageSelectGuideSprite_->Update();
        }
        if (stageSelectGuideXboxSprite_) {
            stageSelectGuideXboxSprite_->Update();
        }
    }

    if (gameplayUIManager_) {
        gameplayUIManager_->Update(
            currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace,
            player_.get(), camera.get(), lightCamera_.get());
    }

    if (isGamePaused_) {
        if (pauseKeyboardSprite_) {
            pauseKeyboardSprite_->Update();
        }
        if (pauseXboxSprite_) {
            pauseXboxSprite_->Update();
        }
        return;
    }

    if (blockInventoryUI_) {
        bool isPlayOrPlace = (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);
        blockInventoryUI_->Update(input.get(), winApp.get(), isPlayOrPlace, &stageMap_);

        if (blockInventoryUI_->ConsumeUseRequest()) {
            RequestSceneChange(SceneType::GamePlayBlockPlace);
            Vector3 pPos = player_ ? player_->GetPosition() : Vector3{ 0,0,0 };
            mapCursor_->SetIndex({
                static_cast<int>(std::floor(pPos.x + 0.5f)),
                static_cast<int>(std::floor(pPos.y)),
                static_cast<int>(std::floor(pPos.z + 0.5f))
            }, stageMap_);
            blockPlacementController_.SetPlaceBlockType(blockInventoryUI_->GetSelectedBlockType());
            blockPlacementController_.SetPlaceCustomId(blockInventoryUI_->GetSelectedCustomId());
        }
    }

    // 通常プレイ、インベントリ、配置モードのどの更新経路でも
    // 対応する操作ガイドのSprite定数バッファを更新する。
    const bool isInventoryOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();
    if (currentMode_ == AppMode::GamePlay && !isInventoryOpen) {
        if (tutorialSprite_) {
            tutorialSprite_->Update();
        }
        if (controllerTutorialSprite_) {
            controllerTutorialSprite_->Update();
        }
    }
    if (isInventoryOpen) {
        if (inventoryTutorialSprite_) {
            inventoryTutorialSprite_->Update();
        }
        if (controllerInventoryTutorialSprite_) {
            controllerInventoryTutorialSprite_->Update();
        }
    } else if (currentMode_ == AppMode::GamePlay_BlockPlace) {
        if (placementTutorialSprite_) {
            placementTutorialSprite_->Update();
        }
        if (controllerPlacementTutorialSprite_) {
            controllerPlacementTutorialSprite_->Update();
        }
    }
}

void GameRuntime::UpdateDebugView() {
    if (input->TriggerKey(DIK_SPACE)) {
        particleManager->Emit({ 0, 0, 0 }, 10);
    }

    // DebugView では StageEditor のカーソル操作を呼ばない。
    // 同じ WASD 入力で編集カーソルとカメラまで動き、プレイヤーから離れる原因になるため。
}
