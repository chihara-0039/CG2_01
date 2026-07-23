// アプリケーション資源の生成・破棄と、外部レベルの寿命を管理する。
#include "GameRuntime.h"
#include "../Environment/WeatherPresetManager.h"
#include "ModelManager.h"
#include <filesystem>
#include <memory>

void GameRuntime::Initialize() {
    // Storm編集用のプリセット一覧は、UIが開かれる前にControllerへ読み込ませる。
    stormEffectEditor_.Initialize();
    WeatherPresetManager::GetInstance().LoadPresets();
    LoadEffectPresetNames();

    winApp = std::make_unique<WinApp>();
    winApp->Initialize();

    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get());

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

    particleManager = std::make_unique<ParticleManager>();
    particleManager->Initialize(dxCommon.get(), textureManager.get());


    stageSelect_ = std::make_unique<StageSelect>();
    stageSelect_->Initialize(object3dCommon.get(), input.get());

    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/block",  "block.obj",  textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/axis",   "axis.obj",   textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/player", "player.obj", textureManager.get())));

    blenderRuntimeLevel_.Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    const char* runtimeLevelPath = std::filesystem::exists("Resources/Levels/game_level.json")
        ? "Resources/Levels/game_level.json"
        : "Resources/Levels/sample_level.json";
    blenderRuntimeLevel_.Load(runtimeLevelPath);


    Object3d* debugFloor = CreateObject(models[0].get(), { -25.0f, 0.0f, 0.0f });
    debugFloor->SetScale({ 10.0f, 1.0f, 10.0f });
    debugFloor->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d* debugAxisA = CreateObject(models[1].get(), { -23.0f, 0.0f, 0.0f });
    debugAxisA->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d* debugAxisB = CreateObject(models[1].get(), { -27.0f, 0.0f, 0.0f });
    debugAxisB->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    uint32_t texHandle = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), texHandle);


    bgmController_.Initialize();
    
    
    
    player_ = std::make_unique<Player>();
    const std::string playerWalkGltfPath = "Resources/Models/Work/human/walk.gltf";
    if (std::filesystem::exists(playerWalkGltfPath)) {
        player_->InitializeWithSkinnedGltf(
            object3dCommon.get(),
            dxCommon.get(),
            playerWalkGltfPath,
            textureManager.get());
    } else {
        player_->Initialize(object3dCommon.get(), models[2].get());
    }
    player_->SetPosition({ 0.0f, 1.5f, 0.0f });
    player_->SetExternalCollisionBoxes(nullptr);

    
    camera = std::make_unique<Camera>();
#ifndef NDEBUG
    camera->SetAspectRatio(1280.0f / 720.0f);
#endif


    stageMap_.Initialize(100, 100, 100);

#ifdef DEVELOPMENT
    currentMode_           = AppMode::DebugView;
    debugFlags_.showSkybox = false;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage1.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage1.txt");
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    }
#else
    currentMode_           = AppMode::StageSelect;
    debugFlags_.showSkybox = true;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage01.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage01.txt");
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    }
#endif
    ApplyRuntimePlayerSpawn();

    if (currentMode_ == AppMode::EffectShowcase) {
        effectShowcaseController_.Reset();
        effectPreviewShowGPUParticleSphere_ = false;
    }

    skydomeModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/skydome", "skydome.obj", textureManager.get()));
    skydomeObject_ = std::make_unique<Object3d>();
    skydomeObject_->Initialize(object3dCommon.get());
    skydomeObject_->SetModel(skydomeModel_.get());
    skydomeObject_->SetEnableLighting(false);
    skydomeObject_->SetScale({ 90.0f, 90.0f, 90.0f });

    skyboxTextureHandle_ = textureManager->LoadTexture("Resources/dds/rostock_laage_airport_4k.dds");
    object3dCommon->SetEnvironmentTextureHandle(skyboxTextureHandle_);
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(object3dCommon.get(), skyboxTextureHandle_);
    skybox_->SetScale({ 50.0f, 50.0f, 50.0f });

    stageRenderer_ = std::make_unique<StageRenderer>();
    stageRenderer_->Initialize(object3dCommon.get());
    stageRenderer_->SetBlockScale({ 1.0f, 1.0f, 1.0f });
    stageRenderer_->BuildFromStageMap(stageMap_);

    mapCursor_ = std::make_unique<MapCursor>();
    mapCursor_->Initialize(object3dCommon.get());
    mapCursor_->SetIndex({ 0, 0, 0 }, stageMap_);
    mapCursor_->SetScale({ 0.9f, 0.9f, 0.9f });

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


    tutorialSprite_ = std::make_unique<Sprite>();
    tutorialSprite_->Initialize(spriteCommon.get(),
        textureManager->LoadTexture("Resources/UI/tutorial/tutorial.png"));
    tutorialSprite_->SetPosition({ 20, 20 });
    tutorialSprite_->SetSize({ 554, 128 });


    placementTutorialSprite_ = std::make_unique<Sprite>();
    placementTutorialSprite_->Initialize(spriteCommon.get(),
        textureManager->LoadTexture("Resources/UI/tutorial/placement_tutorial.png"));
    placementTutorialSprite_->SetPosition({ 20, 20 });
    placementTutorialSprite_->SetSize({ 682, 185 });


    gameplayCameraController_.Initialize();
    stageEditorController_.Initialize();

    sceneManager_ = SceneManager::GetInstance();
    sceneManager_->Initialize(&sceneFactory_, GetCurrentSceneType(), *this);
}



Object3d* GameRuntime::CreateObject(Model* model, Vector3 initialPosition) {
    auto createdObject = std::make_unique<Object3d>();
    createdObject->Initialize(object3dCommon.get());
    createdObject->SetModel(model);
    createdObject->SetPosition(initialPosition);
    createdObject->SetRotation({ 1.57f, 0.0f, 0.0f });
    Object3d* createdObjectRawPtr = createdObject.get();
    objectList.push_back(std::move(createdObject));
    return createdObjectRawPtr;
}

bool GameRuntime::ApplyRuntimePlayerSpawn() {
    if (!player_ || !blenderRuntimeLevel_.HasPlayerSpawn()) {
        return false;
    }

    const Vector3& spawn = blenderRuntimeLevel_.GetPlayerSpawn();
    player_->SetPosition(spawn);
    player_->SetRespawnPosition(spawn);
    return true;
}

bool GameRuntime::LoadBlenderStage(bool beginPlay) {
    const std::string levelPath = blenderLevelPath_.data();
    if (!blenderRuntimeLevel_.Load(levelPath)) {
        return false;
    }

    if (beginPlay) {
        blenderStageActive_ = true;

        stageMap_.Clear();
        backupMap_ = stageMap_;
        if (stageRenderer_) {
            stageRenderer_->BuildFromStageMap(stageMap_);
        }

        player_->SetExternalCollisionBoxes(&blenderRuntimeLevel_.GetCollisionBoxes());
        ApplyRuntimePlayerSpawn();
        RequestSceneChange(SceneType::GamePlay);
    } else if (blenderStageActive_) {
        player_->SetExternalCollisionBoxes(&blenderRuntimeLevel_.GetCollisionBoxes());
        ApplyRuntimePlayerSpawn();
    }
    return true;
}

bool GameRuntime::IsRuntimeLevelVisible() const {
    return blenderStageActive_ && (currentMode_ == AppMode::StageEditor ||
        currentMode_ == AppMode::GamePlay ||
        currentMode_ == AppMode::GamePlay_BlockPlace);
}

void GameRuntime::UpdateRuntimeLevelObjects(
    const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    if (!IsRuntimeLevelVisible()) {
        return;
    }
    blenderRuntimeLevel_.Update(view, proj, lightVP);
}

void GameRuntime::DrawRuntimeLevelObjects() {
    if (!IsRuntimeLevelVisible()) {
        return;
    }
    blenderRuntimeLevel_.Draw();
}

void GameRuntime::DrawRuntimeLevelShadows(const Matrix4x4& lightVP) {
    if (!IsRuntimeLevelVisible()) {
        return;
    }
    blenderRuntimeLevel_.DrawShadow(lightVP);
}

void GameRuntime::EnsureSkinningEditorInitialized() {
    if (skinningEditorInitialized_) {
        return;
    }
    skinningEditor_.Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    skinningEditorInitialized_ = true;
}

void GameRuntime::EnsureTerrainInitialized() {
    if (terrainObject_) {
        return;
    }

    terrainModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/terrain", "terrain.obj", textureManager.get()));
    terrainObject_ = std::make_unique<Object3d>();
    terrainObject_->Initialize(object3dCommon.get());
    terrainObject_->SetModel(terrainModel_.get());
    terrainObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
    terrainObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    terrainObject_->SetRotation({ 0.0f, 0.0f, 0.0f });
    terrainObject_->SetEnvironmentCoefficient(terrainEnvironmentCoefficient_);
    terrainObject_->SetShininess(0.38f);
    terrainObject_->SetMetallic(0.08f);
}

void GameRuntime::EnsurePostProcessInitialized() {
    if (postProcessInitialized_) {
        return;
    }
    postProcess_.Initialize(dxCommon.get(), stageMap_.GetClearColor());
    postProcessInitialized_ = true;
}


void GameRuntime::Finalize() {

    if (dxCommon) {
        dxCommon->WaitForGpu();
    }

    if (sceneManager_) {
        sceneManager_->Finalize(*this);
        sceneManager_ = nullptr;
    }

    bgmController_.Finalize();

    dxCommon->FinalizeImGui();


    ModelManager::Finalize();
    objectList.clear();
    models.clear();

    if (gameplayUIManager_) {
        gameplayUIManager_->Finalize();
    }
    gameplayUIManager_.reset();
    if (blockInventoryUI_) {
        blockInventoryUI_->Finalize();
    }
    blockInventoryUI_.reset();

    player_.reset();
    terrainObject_.reset();
    terrainModel_.reset();
    skydomeObject_.reset();
    skydomeModel_.reset();
    sprite.reset();
    stageRenderer_.reset();
    mapCursor_.reset();
    camera.reset();
    shadowMap_.reset();
    lightCamera_.reset();


    particleManager.reset();
    object3dCommon.reset();
    spriteCommon.reset();
    textureManager.reset();
    input.reset();
    dxCommon.reset();
    winApp.reset();
}


