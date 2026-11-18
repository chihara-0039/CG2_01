// アプリケーション資源の生成・破棄と、外部レベルの寿命を管理する。
#include "GameRuntime.h"
#include "LifecycleTrace.h"
#include "../Environment/WeatherPresetManager.h"
#include "ModelManager.h"
#include "externals/imgui/imgui.h"
#include <filesystem>
#include <memory>

// ゲーム全体を準備し、初期値と必要なリソースを設定する。
void GameRuntime::Initialize() {
    // Storm編集用のプリセット一覧は、UIが開かれる前にControllerへ読み込ませる。
    stormEffectEditor_.Initialize();
    WeatherPresetManager::GetInstance().LoadPresets();
    LoadEffectPresetNames();

    LifecycleTrace::Mark("startup-presets");
    winApp = std::make_unique<WinApp>();
    winApp->Initialize();

    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get());

    LifecycleTrace::Mark("startup-dx");
    input = std::make_unique<Input>();
    input->Initialize(winApp.get());

    srvManager = std::make_unique<SrvManager>();
    srvManager->Initialize(dxCommon.get(), SrvManager::kMaxSRVCount);

    textureManager = std::make_unique<TextureManager>();
    textureManager->Initialize(dxCommon.get(), srvManager.get());

    spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->SetTextureManager(textureManager.get());
    spriteCommon->Initialize(dxCommon.get());

    object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->SetTextureManager(textureManager.get());
    object3dCommon->Initialize(dxCommon.get());

    LifecycleTrace::Mark("startup-draw-pipelines");
    particleManager = std::make_unique<ParticleManager>();
    particleManager->Initialize(dxCommon.get(), textureManager.get());

    LifecycleTrace::Mark("startup-particles");
    stageSelect_ = std::make_unique<StageSelect>();
    stageSelect_->Initialize(object3dCommon.get(), input.get(), 0, false);

    LifecycleTrace::Mark("startup-previews");
    models.push_back(std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/block", "block.obj", textureManager.get())));
    models.push_back(std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/axis", "axis.obj", textureManager.get())));
    models.push_back(std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/player", "player.obj", textureManager.get())));

    blenderRuntimeLevel_.Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    const char *runtimeLevelPath = std::filesystem::exists("Resources/Levels/game_level.json")
                                       ? "Resources/Levels/game_level.json"
                                       : "Resources/Levels/sample_level.json";
    strncpy_s(blenderLevelPath_.data(), blenderLevelPath_.size(), runtimeLevelPath, _TRUNCATE);
    // 外部レベルはエディターの読込操作で読み、タイトル用に先読みしない。

    Object3d *debugFloor = CreateObject(models[0].get(), {-25.0f, 0.0f, 0.0f});
    debugFloor->SetScale({10.0f, 1.0f, 10.0f});
    debugFloor->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d *debugAxisA = CreateObject(models[1].get(), {-23.0f, 0.0f, 0.0f});
    debugAxisA->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d *debugAxisB = CreateObject(models[1].get(), {-27.0f, 0.0f, 0.0f});
    debugAxisB->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    uint32_t texHandle = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), texHandle);

    LifecycleTrace::Mark("startup-debug-level");
    bgmController_.Initialize();

    player_ = std::make_unique<Player>();
    const std::string playerWalkGltfPath = "Resources/Models/Work/human/walk.gltf";
    if (std::filesystem::exists(playerWalkGltfPath)) {
        player_->InitializeWithSkinnedGltf(object3dCommon.get(), dxCommon.get(), playerWalkGltfPath,
                                           textureManager.get());
    } else {
        player_->Initialize(object3dCommon.get(), models[2].get());
    }
    player_->SetPosition({0.0f, 1.5f, 0.0f});
    player_->SetExternalCollisionBoxes(nullptr);

    LifecycleTrace::Mark("startup-player-audio");
    camera = std::make_unique<Camera>();
#ifndef NDEBUG
    camera->SetAspectRatio(1280.0f / 720.0f);
#endif

    stageMap_.Initialize(100, 100, 100);

#ifdef DEVELOPMENT
    currentMode_ = AppMode::Title;
    // 確認用の DebugView では、従来のデバッグオブジェクトと地形を最初から表示する。
    debugFlags_.show3DObjects = true;
    debugFlags_.showTerrain = true;
    debugFlags_.showSkybox = true;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage1.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage1.txt");
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    }
#else
    currentMode_ = AppMode::Title;
    debugFlags_.showSkybox = true;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage01.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage01.txt");
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    }
#endif
    // 起動時はグリッドステージを表示するため、Level Editor用のSpawnで
    // PlayerStartを上書きしない。Blenderステージ開始時だけ適用する。

    if (currentMode_ == AppMode::EffectShowcase) {
        effectShowcaseController_.Reset();
        effectPreviewShowGPUParticleSphere_ = false;
    }

    LifecycleTrace::Mark("startup-stage-data");
    skydomeModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/skydome", "skydome.obj", textureManager.get()));
    skydomeObject_ = std::make_unique<Object3d>();
    skydomeObject_->Initialize(object3dCommon.get());
    skydomeObject_->SetModel(skydomeModel_.get());
    skydomeObject_->SetEnableLighting(false);
    skydomeObject_->SetScale({90.0f, 90.0f, 90.0f});

    skyboxTextureHandle_ = textureManager->LoadTexture("Resources/dds/rostock_laage_airport_4k.dds");
    object3dCommon->SetEnvironmentTextureHandle(skyboxTextureHandle_);
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(object3dCommon.get(), skyboxTextureHandle_);
    skybox_->SetScale({50.0f, 50.0f, 50.0f});

    LifecycleTrace::Mark("startup-sky");
    stageRenderer_ = std::make_unique<StageRenderer>();
    stageRenderer_->Initialize(object3dCommon.get());
    stageRenderer_->SetBlockScale({1.0f, 1.0f, 1.0f});
    // タイトルでは初期ステージの描画オブジェクトを作らず、使用する画面まで待つ。

    LifecycleTrace::Mark("startup-stage-renderer");
    mapCursor_ = std::make_unique<MapCursor>();
    mapCursor_->Initialize(object3dCommon.get());
    mapCursor_->SetIndex({0, 0, 0}, stageMap_);
    mapCursor_->SetScale({0.9f, 0.9f, 0.9f});

    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon.get(), textureManager.get());

    lightCamera_ = std::make_unique<LightCamera>();
    lightCamera_->Initialize();

    bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);

    gameplayUIManager_ = std::make_unique<GameplayUIManager>();
    gameplayUIManager_->Initialize(dxCommon.get(), textureManager.get(), spriteCommon.get(), object3dCommon.get());

    blockInventory_.Initialize(0);

    blockInventoryUI_ = std::make_unique<BlockInventoryUI>();
    blockInventoryUI_->Initialize(dxCommon.get(), spriteCommon.get(), textureManager.get(), &blockInventory_);

    LifecycleTrace::Mark("startup-gameplay-ui");
    tutorialSprite_ = std::make_unique<Sprite>();
    tutorialSprite_->Initialize(spriteCommon.get(), textureManager->LoadTexture("Resources/UI/adventure/play.png"));
    tutorialSprite_->SetPosition({24, 651});
    tutorialSprite_->SetSize({1000, 40});

    placementTutorialSprite_ = std::make_unique<Sprite>();
    placementTutorialSprite_->Initialize(spriteCommon.get(),
                                         textureManager->LoadTexture("Resources/UI/adventure/place.png"));
    placementTutorialSprite_->SetPosition({24, 651});
    placementTutorialSprite_->SetSize({1200, 44});

    inventoryTutorialSprite_ = std::make_unique<Sprite>();
    inventoryTutorialSprite_->Initialize(spriteCommon.get(),
                                         textureManager->LoadTexture("Resources/UI/adventure/inventory-help.png"));
    inventoryTutorialSprite_->SetPosition({20, 20});
    inventoryTutorialSprite_->SetSize({1000, 120});

    controllerTutorialSprite_ = std::make_unique<Sprite>();
    controllerTutorialSprite_->Initialize(spriteCommon.get(),
                                          textureManager->LoadTexture("Resources/UI/adventure/play-pad.png"));
    controllerTutorialSprite_->SetPosition({24, 651});
    controllerTutorialSprite_->SetSize({1000, 40});

    controllerPlacementTutorialSprite_ = std::make_unique<Sprite>();
    controllerPlacementTutorialSprite_->Initialize(spriteCommon.get(),
                                                   textureManager->LoadTexture("Resources/UI/adventure/place-pad.png"));
    controllerPlacementTutorialSprite_->SetPosition({24, 651});
    controllerPlacementTutorialSprite_->SetSize({1200, 44});

    controllerInventoryTutorialSprite_ = std::make_unique<Sprite>();
    controllerInventoryTutorialSprite_->Initialize(
        spriteCommon.get(), textureManager->LoadTexture("Resources/UI/adventure/inventory-help-pad.png"));
    controllerInventoryTutorialSprite_->SetPosition({20, 20});
    controllerInventoryTutorialSprite_->SetSize({1000, 120});

    // ゲーム開始時の目的と、ステージ選択時の操作をSpriteで案内する。
    objectiveGuideTexture_ = textureManager->LoadTexture("Resources/UI/adventure/objective.png");
    objectiveGuideSprite_ = std::make_unique<Sprite>();
    objectiveGuideSprite_->Initialize(spriteCommon.get(), objectiveGuideTexture_);
    objectiveGuideSprite_->SetPosition({320.0f, 165.0f});
    objectiveGuideSprite_->SetSize({640.0f, 72.0f});
    keyProgressSprite_ = std::make_unique<Sprite>();
    keyProgressSprite_->Initialize(spriteCommon.get(),
                                   textureManager->LoadTexture("Resources/UI/adventure/keys-0-0.png"));
    keyProgressSprite_->SetPosition({24, 24});
    keyProgressSprite_->SetSize({244, 76});

    // 選択中の島を、軽く弾む金色の星で示す。
    stageSelectAccentSprite_ = std::make_unique<Sprite>();
    stageSelectAccentSprite_->Initialize(spriteCommon.get(),
                                         textureManager->LoadTexture("Resources/UI/adventure/selection-star.png"));
    stageSelectAccentSprite_->SetAnchorPoint({.5f, .5f});
    stageSelectAccentSprite_->SetSize({40, 40});
    stageSelectAccentSprite_->SetPosition({28, 258});
    stageSelectAccentSprite_->Update();

    stageSelectGuideTexture_ = textureManager->LoadTexture("Resources/UI/adventure/select-0.png");
    stageSelectGuideSprite_ = std::make_unique<Sprite>();
    stageSelectGuideSprite_->Initialize(spriteCommon.get(), stageSelectGuideTexture_);
    stageSelectGuideSprite_->SetPosition({0, 0});
    stageSelectGuideSprite_->SetSize({1280, 720});

    stageSelectGuideXboxTexture_ = textureManager->LoadTexture("Resources/UI/adventure/select-0-pad.png");
    stageSelectGuideXboxSprite_ = std::make_unique<Sprite>();
    stageSelectGuideXboxSprite_->Initialize(spriteCommon.get(), stageSelectGuideXboxTexture_);
    stageSelectGuideXboxSprite_->SetPosition({0, 0});
    stageSelectGuideXboxSprite_->SetSize({1280, 720});

    // ゴール到達後にステージ選択へ戻る操作を案内する。
    clearGuideTexture_ = textureManager->LoadTexture("Resources/Models/Journal/guide.png");
    clearGuideSprite_ = std::make_unique<Sprite>();
    clearGuideSprite_->Initialize(spriteCommon.get(), clearGuideTexture_);
    clearGuideSprite_->SetPosition({0, 0});
    clearGuideSprite_->SetSize({1280, 720});

    clearGuideXboxTexture_ = textureManager->LoadTexture("Resources/Models/Journal/guide-pad.png");
    clearGuideXboxSprite_ = std::make_unique<Sprite>();
    clearGuideXboxSprite_->Initialize(spriteCommon.get(), clearGuideXboxTexture_);
    clearGuideXboxSprite_->SetPosition({0, 0});
    clearGuideXboxSprite_->SetSize({1280, 720});

    pauseKeyboardSprite_ = std::make_unique<Sprite>();
    pauseKeyboardSprite_->Initialize(spriteCommon.get(),
                                     textureManager->LoadTexture("Resources/UI/adventure/pause.png"));
    pauseKeyboardSprite_->SetPosition({0.0f, 0.0f});
    pauseKeyboardSprite_->SetSize({1280.0f, 720.0f});

    pauseXboxSprite_ = std::make_unique<Sprite>();
    pauseXboxSprite_->Initialize(spriteCommon.get(),
                                 textureManager->LoadTexture("Resources/UI/adventure/pause-pad.png"));
    pauseXboxSprite_->SetPosition({0.0f, 0.0f});
    pauseXboxSprite_->SetSize({1280.0f, 720.0f});

    starGetSprite_ = std::make_unique<Sprite>();
    starGetSprite_->Initialize(spriteCommon.get(), textureManager->LoadTexture("Resources/UI/adventure/star-get.png"));
    starGetSprite_->SetPosition({380.0f, 595.0f});
    starGetSprite_->SetSize({520.0f, 110.0f});

    goalDirectionSprite_ = std::make_unique<Sprite>();
    goalDirectionSprite_->Initialize(spriteCommon.get(),
                                     textureManager->LoadTexture("Resources/UI/adventure/compass.png"));
    goalDirectionSprite_->SetAnchorPoint({0.5f, 0.5f});
    goalDirectionSprite_->SetPosition({1220.0f, 620.0f});
    goalDirectionSprite_->SetSize({42.0f, 42.0f});

    menuExitKeyboardSprite_ = std::make_unique<Sprite>();
    menuExitKeyboardSprite_->Initialize(spriteCommon.get(),
                                        textureManager->LoadTexture("Resources/UI/adventure/title-controls.png"));
    menuExitKeyboardSprite_->SetPosition({24.0f, 651.0f});
    menuExitKeyboardSprite_->SetSize({1000.0f, 44.0f});

    menuExitXboxSprite_ = std::make_unique<Sprite>();
    menuExitXboxSprite_->Initialize(spriteCommon.get(),
                                    textureManager->LoadTexture("Resources/UI/adventure/title-controls-pad.png"));
    menuExitXboxSprite_->SetPosition({24.0f, 651.0f});
    menuExitXboxSprite_->SetSize({1000.0f, 44.0f});

    // 共通シーン遷移用の全画面暗転。白テクスチャへ黒色を乗算して使用する。
    sceneFadeSprite_ = std::make_unique<Sprite>();
    sceneFadeSprite_->Initialize(spriteCommon.get(), textureManager->LoadTexture("Resources/UI/inventory/white.png"));
    sceneFadeSprite_->SetPosition({0.0f, 0.0f});
    sceneFadeSprite_->SetSize({1280.0f, 720.0f});
    sceneFadeSprite_->SetColor({0.0f, 0.0f, 0.0f, 0.0f});
    sceneFadeSprite_->Update();

    recoveryTransition_ = std::make_unique<RecoveryTransition>();
    recoveryTransition_->Initialize(spriteCommon.get());
    gameOver_ = std::make_unique<GameOver>();
    gameOver_->Initialize(spriteCommon.get(), {textureManager->LoadTexture("Resources/UI/gameover/title.png"),
                                               textureManager->LoadTexture("Resources/UI/gameover/subtitle.png"),
                                               textureManager->LoadTexture("Resources/UI/gameover/retry.png"),
                                               textureManager->LoadTexture("Resources/UI/gameover/select.png"),
                                               textureManager->LoadTexture("Resources/UI/gameover/controls.png"),
                                               textureManager->LoadTexture("Resources/UI/gameover/background.png"),
                                               textureManager->LoadTexture("Resources/UI/gameover/sigil.png"),
                                               textureManager->LoadTexture("Resources/UI/gameover/selection.png"),
                                               textureManager->LoadTexture("Resources/UI/gameover/sparkle.png")});
    startIntro_ = std::make_unique<StartIntro>();
    startIntro_->Initialize(spriteCommon.get(), textureManager->LoadTexture("Resources/UI/arrival_cloud.png"),
                            textureManager->LoadTexture("Resources/UI/gameover/sparkle.png"), object3dCommon.get());

    LifecycleTrace::Mark("startup-menu-ui");
    gameplayCameraController_.Initialize();
    stageEditorController_.Initialize();

    sceneManager_ = SceneManager::GetInstance();
    sceneManager_->Initialize(&sceneFactory_, GetCurrentSceneType(), *this);
    LifecycleTrace::Mark("startup-scenes");
    InitializeCaptureTools();
#ifndef NDEBUG
    wchar_t openEditor[2]{};
    if (GetEnvironmentVariableW(L"CG2_OPEN_EDITOR", openEditor, 2) == 1 && openEditor[0] == L'1') {
        char rotationEditor[16]{};
        GetEnvironmentVariableA("CG2_ROTATION_COURSE", rotationEditor, sizeof(rotationEditor));
        const std::string course(rotationEditor);
        if (course == "bridge" || course == "relay") {
            rotationTrial_ = true;
            rotationResultPage_ = course == "bridge" ? 5 : 6;
            stageMap_.LoadFromFile("Resources/Stages/" +
                                   std::string(course == "bridge" ? "rotationBridge.txt" : "rotationRelay.txt"));
            stageRenderer_->BuildFromStageMap(stageMap_);
            backupMap_ = stageMap_;
            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
        }
        RequestSceneChange(SceneType::StageEditor);
    }
#endif
}

// モデルを持つ3Dオブジェクトを作り、初期位置を設定する。
Object3d *GameRuntime::CreateObject(Model *model, Vector3 initialPosition) {
    auto createdObject = std::make_unique<Object3d>();
    createdObject->Initialize(object3dCommon.get());
    createdObject->SetModel(model);
    createdObject->SetPosition(initialPosition);
    createdObject->SetRotation({1.57f, 0.0f, 0.0f});
    Object3d *createdObjectRawPtr = createdObject.get();
    objectList.push_back(std::move(createdObject));
    return createdObjectRawPtr;
}

// 外部レベルに開始位置があれば、プレイヤーの復帰地点へ反映する。
bool GameRuntime::ApplyRuntimePlayerSpawn() {
    if (!player_ || !blenderRuntimeLevel_.HasPlayerSpawn()) {
        return false;
    }

    const Vector3 &spawn = blenderRuntimeLevel_.GetPlayerSpawn();
    player_->SetPosition(spawn);
    player_->SetRespawnPosition(spawn);
    return true;
}

// 外部レベルを読み、開始位置・描画・当たり判定を切り替える。
bool GameRuntime::LoadBlenderStage(bool beginPlay) {
    const std::string levelPath = blenderLevelPath_.data();
    if (!blenderRuntimeLevel_.Load(levelPath)) {
        return false;
    }
    SyncBlenderLevelWriteTime();

    if (beginPlay) {
        blenderStageActive_ = true;

        stageMap_.Clear();
        hasGoalGuideTarget_ = false;
        backupMap_ = stageMap_;
        if (stageRenderer_) {
            stageRenderer_->BuildFromStageMap(stageMap_);
        }

        player_->SetExternalCollisionBoxes(&blenderRuntimeLevel_.GetCollisionBoxes());
        ApplyRuntimePlayerSpawn();
        objectiveGuideStartedAt_ = std::chrono::steady_clock::now();
        objectiveGuideActive_ = true;
        RequestSceneChange(SceneType::GamePlay);
    } else if (currentMode_ == AppMode::StageEditor) {
        // 外部ツールで作成したレベルを、シーン遷移せずステージエディター内へ表示する。
        // 読み込んだBOXコライダーとスポーン地点も同時に反映し、見た目だけでなく
        // 実際のゲーム用レベルとして確認できる状態にする。
        blenderStageActive_ = true;
        player_->SetExternalCollisionBoxes(&blenderRuntimeLevel_.GetCollisionBoxes());
        ApplyRuntimePlayerSpawn();
        camera->ForceReset(player_->GetPosition(), 18.0f, {0.35f, 0.0f, 0.0f});
    } else if (blenderStageActive_) {
        player_->SetExternalCollisionBoxes(&blenderRuntimeLevel_.GetCollisionBoxes());
    }
    return true;
}

// 外部レベルの更新時刻を記録し、次の変更検出の基準にする。
void GameRuntime::SyncBlenderLevelWriteTime() {
    std::error_code error;
    const std::filesystem::path levelPath(blenderLevelPath_.data());
    if (!std::filesystem::exists(levelPath, error) || error) {
        blenderLevelWatchInitialized_ = false;
        return;
    }

    blenderLevelWriteTime_ = std::filesystem::last_write_time(levelPath, error);
    if (!error) {
        pendingBlenderLevelWriteTime_ = blenderLevelWriteTime_;
        blenderLevelWatchInitialized_ = true;
        blenderLevelReloadPending_ = false;
        blenderLevelChangeStableTime_ = 0.0f;
    }
}

// 外部レベルの更新時刻を監視し、変更時に再読み込みする。
void GameRuntime::UpdateBlenderLevelFileWatch() {
    // エディターから明示的に読み込んだ外部レベルだけを監視する。
    // 初期パスを常時監視すると、F5操作や別ステージ編集中に意図せず混在してしまう。
    if (!blenderStageActive_) {
        return;
    }

    const std::filesystem::path levelPath(blenderLevelPath_.data());
    std::error_code error;
    if (!std::filesystem::exists(levelPath, error) || error) {
        return;
    }

    const auto currentWriteTime = std::filesystem::last_write_time(levelPath, error);
    if (error) {
        return;
    }

    if (!blenderLevelWatchInitialized_) {
        blenderLevelWriteTime_ = currentWriteTime;
        pendingBlenderLevelWriteTime_ = currentWriteTime;
        blenderLevelWatchInitialized_ = true;
        return;
    }

    if (currentWriteTime != blenderLevelWriteTime_) {
        if (!blenderLevelReloadPending_ || currentWriteTime != pendingBlenderLevelWriteTime_) {
            pendingBlenderLevelWriteTime_ = currentWriteTime;
            blenderLevelChangeStableTime_ = 0.0f;
        }
        blenderLevelReloadPending_ = true;
        blenderLevelChangeStableTime_ += 1.0f / 60.0f;
    }

    // BlenderがJSONを書き終える前に開かないよう、更新時刻が安定してから確認を出す。
    if (blenderLevelReloadPending_ && blenderLevelChangeStableTime_ >= 0.5f && !blenderReloadDialogOpen_) {
        ImGui::OpenPopup("Blenderレベルの外部変更");
        blenderReloadDialogOpen_ = true;
    }

    bool keepDialogOpen = true;
    if (ImGui::BeginPopupModal("Blenderレベルの外部変更", &keepDialogOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Blenderレベルファイルの変更を検出しました。");
        ImGui::Text("File: %s", blenderLevelPath_.data());
        ImGui::Separator();
        ImGui::TextUnformatted("変更内容をエンジンへ再読み込みしますか？");

        if (ImGui::Button("再読み込み", ImVec2(150.0f, 0.0f))) {
            const bool runtimeReloaded = LoadBlenderStage(false);
            const bool editorReloaded = currentMode_ != AppMode::SkinningEditor ||
                                        skinningEditor_.ReloadExternalLevel(blenderLevelPath_.data());
            if (runtimeReloaded && editorReloaded) {
                blenderLevelReloadPending_ = false;
            }
            blenderReloadDialogOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("今回は無視", ImVec2(150.0f, 0.0f))) {
            blenderLevelWriteTime_ = pendingBlenderLevelWriteTime_;
            blenderLevelReloadPending_ = false;
            blenderLevelChangeStableTime_ = 0.0f;
            blenderReloadDialogOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (!keepDialogOpen) {
        blenderLevelWriteTime_ = pendingBlenderLevelWriteTime_;
        blenderLevelReloadPending_ = false;
        blenderLevelChangeStableTime_ = 0.0f;
        blenderReloadDialogOpen_ = false;
    }
}

bool GameRuntime::IsRuntimeLevelVisible() const {
    return blenderStageActive_ && (currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay ||
                                   currentMode_ == AppMode::GamePlay_BlockPlace);
}

// 外部レベルのオブジェクトへ、カメラと光源の変換を渡す。
void GameRuntime::UpdateRuntimeLevelObjects(const Matrix4x4 &view, const Matrix4x4 &proj, const Matrix4x4 &lightVP) {
    if (!IsRuntimeLevelVisible()) {
        return;
    }
    blenderRuntimeLevel_.Update(view, proj, lightVP);
}

// 表示対象の外部レベルのオブジェクトを描画する。
void GameRuntime::DrawRuntimeLevelObjects() {
    if (!IsRuntimeLevelVisible()) {
        return;
    }
    blenderRuntimeLevel_.Draw();
}

// 外部レベルの形状を、影の深度マップへ描画する。
void GameRuntime::DrawRuntimeLevelShadows(const Matrix4x4 &lightVP) {
    if (!IsRuntimeLevelVisible()) {
        return;
    }
    blenderRuntimeLevel_.DrawShadow(lightVP);
}

// スキニングエディタを、最初に必要になった時だけ初期化する。
void GameRuntime::EnsureSkinningEditorInitialized() {
    if (skinningEditorInitialized_) {
        return;
    }
    skinningEditor_.Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    skinningEditorInitialized_ = true;
}

// 地形表示に必要なリソースを、初回利用時に作成する。
void GameRuntime::EnsureTerrainInitialized() {
    if (terrainObject_) {
        return;
    }

    terrainModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/terrain", "terrain.obj", textureManager.get()));
    terrainObject_ = std::make_unique<Object3d>();
    terrainObject_->Initialize(object3dCommon.get());
    terrainObject_->SetModel(terrainModel_.get());
    terrainObject_->SetPosition({0.0f, 0.0f, 0.0f});
    terrainObject_->SetScale({1.0f, 1.0f, 1.0f});
    terrainObject_->SetRotation({0.0f, 0.0f, 0.0f});
    terrainObject_->SetEnvironmentCoefficient(terrainEnvironmentCoefficient_);
    terrainObject_->SetShininess(0.38f);
    terrainObject_->SetMetallic(0.08f);
}

// ポストエフェクト用の描画先を、初回利用時に作成する。
void GameRuntime::EnsurePostProcessInitialized() {
    if (postProcessInitialized_) {
        return;
    }
    postProcess_.Initialize(dxCommon.get(), stageMap_.GetClearColor());
    postProcessInitialized_ = true;
}

// ゲーム全体が保持するオブジェクトとリソースを解放する。
void GameRuntime::Finalize() {
    // 保存やGPU資源の解放を待つ前に画面を閉じ、カーソルを通常へ戻す。
    if (winApp) {
        SetCursor(LoadCursor(nullptr, IDC_ARROW));
        ShowWindow(winApp->GetHwnd(), SW_HIDE);
    }
    LifecycleTrace::Mark("window-hidden");
    if (dxCommon) {
        dxCommon->Recorder().Finish();
    }

    if (dxCommon) {
        dxCommon->WaitForGpu();
    }

    LifecycleTrace::Mark("shutdown-gpu-idle");
    if (sceneManager_) {
        sceneManager_->Finalize(*this);
        sceneManager_ = nullptr;
    }

    LifecycleTrace::Mark("shutdown-scenes");
    bgmController_.Finalize();
    starGetPresentation_.Finalize();

    LifecycleTrace::Mark("shutdown-audio");
    dxCommon->FinalizeImGui();

    ModelManager::Finalize();
    objectList.clear();
    models.clear();

    LifecycleTrace::Mark("shutdown-models");
    if (gameplayUIManager_) {
        gameplayUIManager_->Finalize();
    }
    gameplayUIManager_.reset();
    if (blockInventoryUI_) {
        blockInventoryUI_->Finalize();
    }
    blockInventoryUI_.reset();

    LifecycleTrace::Mark("shutdown-ui");
    player_.reset();
    terrainObject_.reset();
    terrainModel_.reset();
    skydomeObject_.reset();
    skydomeModel_.reset();
    sprite.reset();
    sceneFadeSprite_.reset();
    recoveryTransition_.reset();
    if (gameOver_) {
        gameOver_->Finalize();
    }
    gameOver_.reset();
    if (startIntro_) {
        startIntro_->Finalize();
    }
    startIntro_.reset();
    stageRenderer_.reset();
    mapCursor_.reset();
    camera.reset();
    shadowMap_.reset();
    lightCamera_.reset();

    LifecycleTrace::Mark("shutdown-world");
    particleManager.reset();
    object3dCommon.reset();
    spriteCommon.reset();
    LifecycleTrace::Mark("shutdown-pipelines");
    textureManager.reset();
    input.reset();
    LifecycleTrace::Mark("shutdown-textures-input");
    dxCommon.reset();
    LifecycleTrace::Mark("shutdown-dx");
    winApp.reset();
}
