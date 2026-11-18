// プレイヤー操作、ステージルール、画面遷移、BGM更新を担当する。
#include "GameRuntime.h"
#include "Goal.h"
#include "../Stage/RotationIsland.h"
#include <algorithm>
#include <cmath>
#include <random>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

// タイトル画面の案内を更新し、終了入力を受け付ける。
void GameRuntime::UpdateTitle() {
    titleTimer_ += 1.0f / 60.0f;

    if (menuExitKeyboardSprite_) {
        menuExitKeyboardSprite_->Update();
    }
    if (menuExitXboxSprite_) {
        menuExitXboxSprite_->Update();
    }

    if (input->TriggerKey(DIK_ESCAPE) || input->TriggerControllerButton(XINPUT_GAMEPAD_B)) {
        PostQuitMessage(0);
        return;
    }
}

// 選択状態を準備し、タイトルからステージ選択への遷移を要求する。
void GameRuntime::EnterStageSelectFromTitle() {
    if (stageSelect_) {
        stageSelect_->Initialize(object3dCommon.get(), input.get());
    }
    RequestSceneChange(SceneType::StageSelect);
}

// 操作・移動・ギミック・回転・失敗・ゴール判定を順に進める。
void GameRuntime::UpdateGamePlay() {
    const Matrix4x4 &lightVP = lightCamera_->GetViewProjectionMatrix();
    bool rotationControlInReach = false;
    if (player_) {
        const auto p = player_->GetPosition();
        for (const auto &r : stageMap_.GetRotationRegions()) {
            for (const auto &c : {r.control, r.alternate}) {
                if (c.x >= 0 && std::abs(p.x - c.x) < 1.8f && std::abs(p.z - c.z) < 1.8f &&
                    std::abs(p.y - c.y) < 2.0f) {
                    rotationControlInReach = true;
                }
            }
        }
    }
    EnsurePostProcessInitialized();
    postEffectShowcaseController_.UpdateGameplay(*input, postProcess_);

    if (isGoalReached_) {
        UpdateGoalCelebration();
        return;
    }

    // UI操作中はプレイヤー物理とゲーム用カメラを止め、現在の状態を保持する。
    if (isGamePaused_ || (blockInventoryUI_ && blockInventoryUI_->IsActive())) {
        return;
    }

    if (input->TriggerKey(DIK_C) || input->TriggerControllerButton(XINPUT_GAMEPAD_RIGHT_SHOULDER)) {
        useFirstPersonCamera_ = !useFirstPersonCamera_;
        if (useFirstPersonCamera_ && player_) {
            fpsCameraYaw_ = player_->GetFacingYaw();
            fpsCameraPitch_ = 0.0f;
        }
    }

    if (!useFirstPersonCamera_) {
        if (input->TriggerKey(DIK_V) || (input->TriggerControllerButton(XINPUT_GAMEPAD_Y) && !rotationControlInReach)) {
            gameplayCameraController_.TogglePlayerFollow(player_.get());
        }
        camera->SetFov(gameplayCameraController_.GetFov());
        gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(), player_.get());
        camera->Update();
    } else {
        camera->SetFov(0.9f);

        bool isGuiCaptured = input->PushKey(DIK_LMENU) || input->PushKey(DIK_RMENU);
#if defined(USE_IMGUI) && !defined(NDEBUG)

        isGuiCaptured = isGuiCaptured || ImGui::GetIO().WantCaptureMouse;
#endif
        const auto &mouse = input->GetMouseState();

        if (mouse.buttons[0] && !isGuiCaptured) {
            RECT rect;
            GetClientRect(winApp->GetHwnd(), &rect);
            float clientWidth = static_cast<float>(rect.right - rect.left);
            float clientHeight = static_cast<float>(rect.bottom - rect.top);
            if (clientWidth > 0.0f && clientHeight > 0.0f) {
                float screenScaleX = static_cast<float>(WinApp::kWindowWidth) / clientWidth;
                float screenScaleY = static_cast<float>(WinApp::kWindowHeight) / clientHeight;
                float scaledMouseX = static_cast<float>(mouse.posX) * screenScaleX;
                float scaledMouseY = static_cast<float>(mouse.posY) * screenScaleY;

                const float edgeRatio = 0.15f;
                const float rotateSpeed = 0.03f;
                float leftEdge = WinApp::kWindowWidth * edgeRatio;
                float rightEdge = WinApp::kWindowWidth * (1.0f - edgeRatio);
                float topEdge = WinApp::kWindowHeight * edgeRatio;
                float bottomEdge = WinApp::kWindowHeight * (1.0f - edgeRatio);

                if (scaledMouseX < leftEdge) {
                    fpsCameraYaw_ -= rotateSpeed;
                } else if (scaledMouseX > rightEdge) {
                    fpsCameraYaw_ += rotateSpeed;
                }

                if (scaledMouseY < topEdge) {
                    fpsCameraPitch_ -= rotateSpeed;
                } else if (scaledMouseY > bottomEdge) {
                    fpsCameraPitch_ += rotateSpeed;
                }
            }
        }

        const float keyRotateSpeed = 0.03f;
        if (input->PushKey(DIK_LEFT)) {
            fpsCameraYaw_ -= keyRotateSpeed;
        }
        if (input->PushKey(DIK_RIGHT)) {
            fpsCameraYaw_ += keyRotateSpeed;
        }
        if (input->PushKey(DIK_UP)) {
            fpsCameraPitch_ -= keyRotateSpeed;
        }
        if (input->PushKey(DIK_DOWN)) {
            fpsCameraPitch_ += keyRotateSpeed;
        }
        const GamePadState &gamePad = input->GetGamePadState();
        if (gamePad.connected) {
            fpsCameraYaw_ += gamePad.rightStickX * keyRotateSpeed * 1.5f;
            fpsCameraPitch_ -= gamePad.rightStickY * keyRotateSpeed * 1.5f;
        }
        fpsCameraPitch_ = std::clamp(fpsCameraPitch_, -1.4f, 1.4f);

        if (player_) {
            Vector3 playerPosition = player_->GetPosition();
            camera->SetPosition({playerPosition.x, playerPosition.y + 1.2f, playerPosition.z});
            camera->SetRotation({fpsCameraPitch_, fpsCameraYaw_, 0.0f});
        }
        camera->Update();
    }

    if (gameplayUIManager_) {
        gameplayUIManager_->UpdateCameraGuide(currentMode_ == AppMode::GamePlay, input.get(), winApp.get());
    }

    if (!stageMap_.GetRotationRegions().empty() && (!blockInventoryUI_ || !blockInventoryUI_->IsActive()) && player_ &&
        player_->IsGrounded() && (input->TriggerKey(DIK_F) || input->TriggerControllerButton(XINPUT_GAMEPAD_Y))) {
        const auto p = player_->GetPosition();
        for (const auto &r : stageMap_.GetRotationRegions()) {
            const auto nearControl = [&](const Int3 &c) {
                return c.x >= 0 && std::abs(p.x - c.x) < 1.8f && std::abs(p.z - c.z) < 1.8f &&
                       std::abs(p.y - c.y) < 2.0f;
            };
            if (nearControl(r.control) || nearControl(r.alternate)) {
                const bool reverse = input->PushKey(DIK_LSHIFT) ||
                                     (input->GetGamePadState().buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
                if (RotationIsland::Apply(stageMap_, p, reverse, r)) {
                    blockPlacementController_.RotateUndo(reverse, r);
                }
                break;
            }
        }
    }

    float fixedDeltaTime = 1.0f / 60.0f;
    totalTime_ += fixedDeltaTime;
    stageMap_.Update(fixedDeltaTime, player_ ? player_->GetPosition() : Vector3{0, 0, 0});
    stageRenderer_->UpdateEffect(stageMap_);

    if (player_) {
        float cameraYawForMovement = useFirstPersonCamera_ ? fpsCameraYaw_ : gameplayCameraController_.GetAngle();
        player_->SetStageCollisionEnabled(true);
        // PR1の自動撮影・検証は即復帰を維持し、対話プレイでだけ失敗演出へ進む。
        const bool deferFailure = !(comparisonActive_ || dioramaCapture_ || trialVerification_);
        player_->Update(input.get(), stageMap_, cameraYawForMovement, lightVP, dxCommon.get(), deferFailure, true);
        const bool contactFailure = player_->ConsumeFailure();
        if (deferFailure && (contactFailure || player_->GetPosition().y < -3.0f)) {
            BeginGameOver(!contactFailure);
            return;
        }
    }

    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->BuildFromStageMap(stageMap_);
        stageMap_.ClearRebuildFlag();
    }

    stageRespawnController_.Update(stageMap_, backupMap_, stageRenderer_.get(), player_.get(), &blockInventory_,
                                   &bubblePickupController_, &blockPlacementController_, &stageEditorController_,
                                   false);

    Vector3 pPos = player_ ? player_->GetPosition() : Vector3{};
    if (player_) {
        bubblePickupController_.Update(pPos);
    }

    // 全ての鍵の回収後だけスターを取得できる。シャボン玉は道具専用。
    // ゴール接触中のロック状態も保持し、未達成の理由をUIへ表示できるようにする。
    const bool isOnGoalPlatform =
        stageMap_.GetWeatherPresetName() != "Diorama" ||
        (player_ && player_->IsGrounded() && !player_->IsOnLadder() && player_->GetPosition().y >= 5.45f);
    const bool isTouchingGoal = player_ != nullptr && Goal::Check(pPos, {0.4f, 0.9f, 0.4f}, stageMap_);
    const bool hasCollectedAllPickups = player_ && player_->GetCollectedLevelKeys() >= objectiveKeyTotal_;
    isGoalBlocked_ = isTouchingGoal && !hasCollectedAllPickups;
    if (isTouchingGoal && isOnGoalPlatform && hasCollectedAllPickups && !isGoalReached_) {
        isGoalReached_ = true;
        isGoalBlocked_ = false;
        goalCelebrationTimer_ = 0.0f;
    }

    if ((input->TriggerKey(DIK_B) || input->TriggerControllerButton(XINPUT_GAMEPAD_LEFT_SHOULDER)) && player_ &&
        player_->IsGrounded()) {
        if (blockInventoryUI_) {
            blockInventoryUI_->ToggleOpen();
        }
    }

    if (isGoalReached_) {
        UpdateGoalCelebration();
    }
}

// スター取得後のプレイヤー演出とカメラを進め、完了後にリザルトへ移る。
void GameRuntime::UpdateGoalCelebration() {
    if (!player_ || !camera) {
        return;
    }
    if (!starGetPresentation_.Active()) {
        starGetPresentation_.Begin(object3dCommon.get(), *player_, *camera, stageMap_,
                                   hasGoalGuideTarget_ ? goalGuideTarget_ : player_->GetPosition());
    }
    const float dt = starGetPresentation_.DeltaTime();
    if (!IsWindowInactive()) {
        goalCelebrationTimer_ += dt;
    }
    starGetPresentation_.Update(goalCelebrationTimer_, *player_, *camera, stageMap_, particleManager.get(),
                                lightCamera_->GetViewProjectionMatrix(), !dioramaCapture_);

    if (starGetSprite_) {
        const float alpha = StarGet::Evaluate(goalCelebrationTimer_).caption;
        starGetSprite_->SetColor({1, 1, 1, alpha});
        starGetSprite_->Update();
    }
    if (dioramaCapture_) {
        return; // Keep the wide shot and final pose until recording ends.
    }
    if (goalCelebrationTimer_ >= StarGet::kDuration) {
        RequestSceneChange(SceneType::GameClear);
    }
}

int GameRuntime::GetResultPageIndex() const {
    return rotationTrial_ ? rotationResultPage_ : stageSelect_ ? stageSelect_->GetSelectedIndex() : 0;
}
bool GameRuntime::IsResultAnimationVisible() {
    return sceneFadePhase_ == SceneFadePhase::None && !IsWindowInactive();
}

// 手帳演出の完了を待ち、ステージ選択またはタイトルへ戻る入力を受け付ける。
void GameRuntime::UpdateGameClear(bool celebrationReady) {
    gameClearTimer_ += 1.0f / 60.0f;

    // 手帳の打刻が終わった後、画面を離れるまで背景の花火を繰り返す。
    if (!celebrationReady) {
        return;
    }

    // 次の打ち上げまでの待ち時間を使い、決まった拍子や左右対称の繰り返しを避ける。
    gameClearFireworkTimer_ -= 1.0f / 60.0f;
    if (particleManager && (!gameClearCelebrationStarted_ || gameClearFireworkTimer_ <= 0)) {
        static std::mt19937 random(std::random_device{}());
        std::uniform_real_distribution<float> unit(0.0f, 1.0f);
        const float group = unit(random);
        const int bursts = group < .55f ? 1 : (group < .93f ? 2 : 3);
        for (int i = 0; i < bursts; ++i) {
            // 花火の中心は本の外側に置き、高さと奥行きは一発ごとにばらつかせる。
            const float side = unit(random) < .5f ? -1.0f : 1.0f;
            const float x = side * (6.8f + unit(random) * 3.0f);
            const float y = -5.3f + unit(random) * 6.1f;
            const float z = 2.8f + unit(random) * 2.4f;
            particleManager->EmitFireworkBurst({x, y, z});
        }
        gameClearFireworkTimer_ = .18f + unit(random) * .40f;
        gameClearCelebrationStarted_ = true;
    }

    if (clearGuideSprite_) {
        clearGuideSprite_->Update();
    }
    if (clearGuideXboxSprite_) {
        clearGuideXboxSprite_->Update();
    }
    if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_RETURN) ||
        input->TriggerControllerButton(XINPUT_GAMEPAD_A)) {
        ReturnToStageSelect();
    } else if (input->TriggerKey(DIK_ESCAPE) || input->TriggerControllerButton(XINPUT_GAMEPAD_B)) {
        RequestSceneChange(SceneType::Title);
    }
}

// 配置中のカーソル・高さ・回転・確定・取り消しとカメラを更新する。
void GameRuntime::UpdateGamePlayBlockPlace() {
    blockPlacementController_.SetRotationIsland(rotationTrial_);
    EnsurePostProcessInitialized();
    postEffectShowcaseController_.UpdateGameplay(*input, postProcess_);

    if (input->TriggerKey(DIK_R) || input->TriggerControllerButton(XINPUT_GAMEPAD_X)) {
        placeRotationY_ += 1.5707963f;
        if (placeRotationY_ >= 6.0f) {
            placeRotationY_ = 0.0f;
        }
    }

    stageEditorController_.HandleCursorInput(input.get(), stageMap_, mapCursor_.get(), lightCamera_.get(),
                                             camera.get());

    // 入力処理後の最新カーソル座標を配置プレビューと確定処理に使用する。
    const Int3 cursor = mapCursor_->GetIndex();
    if (player_) {
        blockPlacementController_.SetPlayerPosition(player_->GetPosition());
    }

    BlockType selectedType = BlockType::Ground;
    int selectedCustomId = 0;
    if (blockInventoryUI_) {
        selectedType = blockInventoryUI_->GetSelectedBlockType();
        selectedCustomId = blockInventoryUI_->GetSelectedCustomId();
        blockPlacementController_.SetPlaceBlockType(selectedType);
        blockPlacementController_.SetPlaceCustomId(selectedCustomId);
    }

    if (stageRenderer_) {
        const auto plan = blockPlacementController_.Evaluate(cursor, placeRotationY_);
        if (std::chrono::steady_clock::now() >= placementFeedbackUntil_) {
            buildStatus_ = PlacementStatus(plan.reason);
        }
        stageRenderer_->SetPlacementPreview(stageMap_, cursor, selectedType, selectedCustomId, placeRotationY_,
                                            plan.Ready() ? 1 : 2);
    }

    bool mouseJustPressed = input->GetMouseState().buttons[0] && !placeMouseWasDown_;
    placeMouseWasDown_ = input->GetMouseState().buttons[0];
    bool mouseTrigger = false;
    if (mouseJustPressed && (!blockInventoryUI_ || !blockInventoryUI_->IsActive())) {
        mouseTrigger = true;
    }

    if (input->TriggerKey(DIK_RETURN) || input->TriggerControllerButton(XINPUT_GAMEPAD_A) || mouseTrigger) {
        if (blockPlacementController_.TryPlace(cursor, placeRotationY_)) {
            bool hasRest =
                (selectedType == BlockType::Ground) || blockInventory_.HasBlock(selectedType, selectedCustomId);
            if (!hasRest) {
                RequestSceneChange(SceneType::GamePlay);
                placeRotationY_ = 0.0f;
                if (stageRenderer_) {
                    stageRenderer_->ClearPlacementPreview();
                }
            }
        }
    }

    if (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_B) || input->TriggerControllerButton(XINPUT_GAMEPAD_B)) {
        RequestSceneChange(SceneType::GamePlay);
        placeRotationY_ = 0.0f;
        if (stageRenderer_) {
            stageRenderer_->ClearPlacementPreview();
        }
    }

    // 配置中のカメラは通常プレイと同じ追従カメラで更新する。
    // エディタ専用カメラを重ねて更新すると視点が跳ぶため、ここでは呼び出さない。
}

// 選択入力を処理し、読み込みに成功したステージの開始状態を準備する。
void GameRuntime::UpdateStageSelect() {
    if (menuExitKeyboardSprite_) {
        menuExitKeyboardSprite_->Update();
    }
    if (menuExitXboxSprite_) {
        menuExitXboxSprite_->Update();
    }
    if (input->TriggerKey(DIK_ESCAPE) || input->TriggerControllerButton(XINPUT_GAMEPAD_B)) {
        // 選択画面の戻る操作はタイトルへ。ゲーム終了はタイトル側だけで行う。
        RequestSceneChange(SceneType::Title);
        return;
    }

    stageSelect_->Update();
    // 入力直後も選択先に追従し、実時間の緩い脈動で操作可能な場所を示す。
    if (stageSelectAccentSprite_) {
        static const auto started = std::chrono::steady_clock::now();
        const float time = std::chrono::duration<float>(std::chrono::steady_clock::now() - started).count();
        const float pulse = std::sin(std::fmod(time, 6.2831853f) * 3.0f);
        stageSelectAccentSprite_->SetPosition({28 + pulse * 3, 258.0f + stageSelect_->GetSelectedIndex() * 76});
        stageSelectAccentSprite_->SetSize({40 + pulse * 2, 40 + pulse * 2});
        stageSelectAccentSprite_->Update();
    }
    if (stageSelect_->IsFnished()) {
        std::string path = "Resources/Stages/" + stageSelect_->GetSelectedFileName();
        if (!std::filesystem::exists(path) || !stageMap_.LoadFromFile(path)) {
            stageSelect_->ReportLoadFailure();
            return;
        }
        {
            // グリッドステージ開始時はBlenderステージの衝突・Spawnを無効化する。
            blenderStageActive_ = false;
            if (player_) {
                player_->SetExternalCollisionBoxes(nullptr);
            }
            fogTrial_ = false;
            buildTrial_ = false;
            RefreshGoalGuideTarget();
            backupMap_ = stageMap_;
            stageRenderer_->BuildFromStageMap(stageMap_);

            playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());

            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
            gameplayCameraController_.ResetCamera(camera.get(), player_.get(), stageMap_,
                                                  stageSelect_->GetSelectedCameraPresetIndex());
            if (stageSelect_->GetSelectedFileName() == "dioramaRuins.txt") {
                gameplayCameraController_.ConfigureDiorama();
            }
            rotationTrial_ = false;
            showControlHelp_ = false;
            tutorialSprite_->SetTexture(textureManager->LoadTexture("Resources/UI/adventure/play.png"));
            controllerTutorialSprite_->SetTexture(textureManager->LoadTexture("Resources/UI/adventure/play-pad.png"));
            tutorialSprite_->SetPosition({24, 651});
            tutorialSprite_->SetSize({1000, 40});
            controllerTutorialSprite_->SetPosition({24, 651});
            controllerTutorialSprite_->SetSize({1000, 40});
            blockInventory_.Initialize(0);
            blockInventoryUI_->ResetForStage();
            placeRotationY_ = 0;
            placeMouseWasDown_ = input->GetMouseState().buttons[0];
            useFirstPersonCamera_ = false;
            isGamePaused_ = false;
            bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
            blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
            isGoalReached_ = false;
            isGoalBlocked_ = false;
            objectiveGuideStartedAt_ = std::chrono::steady_clock::now();
            objectiveGuideActive_ = true;
            const auto selectedFile = stageSelect_->GetSelectedFileName();
            const char *campaignFiles[] = {"beginnerTrail.txt", "mechanismGardens.txt", "skylineRelay.txt",
                                           "expeditionGrandTour.txt"};
            uint32_t guideTexture = objectiveGuideTexture_;
            for (int i = 0; i < 4; ++i) {
                if (selectedFile == campaignFiles[i]) {
                    guideTexture =
                        textureManager->LoadTexture("Resources/UI/campaign/objective-" + std::to_string(i) + ".png");
                    break;
                }
            }
            if (objectiveGuideSprite_) {
                objectiveGuideSprite_->SetTexture(guideTexture);
                objectiveGuideSprite_->SetSize({960.0f, 72.0f});
                objectiveGuideSprite_->SetPosition({24.0f, 24.0f});
            }
            startIntroPending_ = true;
        }
        RequestSceneChange(SceneType::GamePlay);
    }
}

// ステージからゴールと鍵の総数を調べ、クリア条件と案内先を更新する。
void GameRuntime::RefreshGoalGuideTarget() {
    hasGoalGuideTarget_ = false;
    objectiveKeyTotal_ = 0;
    for (int y = 0; y < stageMap_.GetHeight(); ++y) {
        for (int z = 0; z < stageMap_.GetDepth(); ++z) {
            for (int x = 0; x < stageMap_.GetWidth(); ++x) {
                const MapCell *cell = stageMap_.GetCell(x, y, z);
                if (cell && cell->type == BlockType::Key) {
                    ++objectiveKeyTotal_;
                }
                if (cell && cell->type == BlockType::Goal && !hasGoalGuideTarget_) {
                    goalGuideTarget_ = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
                    hasGoalGuideTarget_ = true;
                }
            }
        }
    }
}

// 現在のモードと入力状態から、必要なシーン切り替えを要求する。
void GameRuntime::UpdateSceneTransition() {
    if (gameOver_ && gameOver_->IsActive()) {
        return;
    }
    if (sceneFadePhase_ != SceneFadePhase::None) {
        return;
    }

    if (startIntro_ && startIntro_->BlocksInput()) {
        // 演出中のESC/Bはポーズへ入れず、ステージ選択へ戻す。
        if (input->TriggerKey(DIK_ESCAPE) || input->TriggerControllerButton(XINPUT_GAMEPAD_B)) {
            if (player_) {
                player_->SetPosition(startIntro_->GetSpawnPosition());
                player_->SetRotation(startIntro_->GetSpawnRotation());
            }
            startIntro_->Cancel(camera.get());
            ReturnToStageSelect();
        }
        return;
    }

    const bool isGameplayMode = currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace;
    if (!isGameplayMode) {
        isGamePaused_ = false;
        return;
    }

    // ブロック配置モードではBを配置キャンセル専用にする。
    if (currentMode_ == AppMode::GamePlay_BlockPlace) {
        return;
    }

    // インベントリ中のBはインベントリ側だけで処理する。
    if (blockInventoryUI_ && blockInventoryUI_->IsActive()) {
        return;
    }

    if (input->TriggerKey(DIK_ESCAPE) || input->TriggerControllerButton(XINPUT_GAMEPAD_START)) {
        isGamePaused_ = !isGamePaused_;
        return;
    }

    if (isGamePaused_) {
        if (input->TriggerKey(DIK_T) || input->TriggerControllerButton(XINPUT_GAMEPAD_X)) {
            isGamePaused_ = false;
            RequestSceneChange(SceneType::Title);
            return;
        }
        if (input->TriggerKey(DIK_RETURN) || input->TriggerControllerButton(XINPUT_GAMEPAD_Y)) {
            ReturnToStageSelect();
            return;
        }
        if (input->TriggerControllerButton(XINPUT_GAMEPAD_A) || input->TriggerControllerButton(XINPUT_GAMEPAD_B)) {
            isGamePaused_ = false;
        }
    }
}

// 選択画面の状態を準備し、プレイ中の演出を終えて戻る。
void GameRuntime::ReturnToStageSelect() {
    const int selected = stageSelect_ ? stageSelect_->GetSelectedIndex() : 0;
    blockInventory_.Initialize(0);
    if (blockInventoryUI_) {
        blockInventoryUI_->ResetForStage();
    }
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    placeRotationY_ = 0;
    placeMouseWasDown_ = false;
    useFirstPersonCamera_ = false;
    stageMap_ = backupMap_;
    if (stageRenderer_) {
        stageRenderer_->BuildFromStageMap(stageMap_);
    }

    bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    if (stageSelect_) {
        stageSelect_->Initialize(object3dCommon.get(), input.get(), selected);
    }
    isGoalReached_ = false;
    isGoalBlocked_ = false;
    isGamePaused_ = false;
    if (player_) {
        player_->Respawn();
    }
    RequestSceneChange(SceneType::StageSelect);
}

// 失敗時の操作を止め、プレイヤーとカメラの再挑戦演出を開始する。
void GameRuntime::BeginGameOver(bool fall) {
    if (!player_ || !gameOver_ || gameOver_->IsActive()) {
        return;
    }
    if (startIntro_) {
        startIntro_->Cancel(camera.get());
    }
    isGamePaused_ = false;
    bgmController_.Update(false);
    if (!fall && particleManager) {
        // 失敗時の一発だけ。主人公を覆わない小さな星色の破片と衝撃波に絞る。
        ParticleManager::HitEffectSettings hit{};
        hit.size = .55f;
        hit.brightness = .65f;
        hit.lifeScale = .7f;
        hit.slashCount = 0;
        hit.sparkCount = 14;
        hit.blueRatio = 0;
        hit.corePower = .25f;
        hit.crossPower = 0;
        hit.pillarPower = 0;
        hit.ringPower = .35f;
        hit.sparkColor = {.7f, .9f, 1, 1};
        hit.sparkSecondaryColor = {.85f, .7f, 1, 1};
        hit.coreColor = {.85f, .95f, 1, 1};
        hit.ringColor = {.55f, .8f, 1, 1};
        particleManager->EmitHitEffect(player_->GetPosition(), hit);
    }
    gameOver_->Begin(camera.get(), player_->GetPosition(), player_->GetRotation(), fall);
}
