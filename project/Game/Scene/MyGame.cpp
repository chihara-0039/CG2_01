// ==========================================================
//  MyGame.cpp
//  ゲーム全体の統括クラス実装
//
//  役割：全サブシステムの生成・接続・破棄、描画パスの制御、
//        AppMode に応じた画面遷移の管理。
//        各サブシステムの実装詳細は専用クラスに委譲する。
// ==========================================================
#include <filesystem>
#include "MyGame.h"
#include "Goal.h"
#include "ModelManager.h"
#include <memory>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

// ==========================================================
//  MyGame::Initialize
//  全サブシステムを生成・初期化する
// ==========================================================
void MyGame::Initialize() {

    // --------------------------------------------------------
    // 1. エンジン基盤システムの生成と初期化
    //    生成順は依存関係の順 (winApp -> dxCommon -> input -> ...)
    // --------------------------------------------------------
    winApp = std::make_unique<WinApp>();
    winApp->Initialize();

    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get());

    input = std::make_unique<Input>();
    input->Initialize(winApp.get());

    // TextureManager は SpriteCommon / Object3dCommon より先に作る
    textureManager = std::make_unique<TextureManager>();
    textureManager->Initialize(dxCommon.get());

    spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->SetTextureManager(textureManager.get());
    spriteCommon->Initialize(dxCommon.get());

    object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->SetTextureManager(textureManager.get());
    object3dCommon->Initialize(dxCommon.get());

    particleManager = std::make_unique<ParticleManager>();
    particleManager->Initialize(dxCommon.get(), textureManager.get());

    // --------------------------------------------------------
    // 2. シーン管理の初期化
    // --------------------------------------------------------
    titleScene_ = std::make_unique<TitleScene>();
    titleScene_->Initialize(object3dCommon.get(), input.get());

    stageSelect_ = std::make_unique<StageSelect>();
    stageSelect_->Initialize(object3dCommon.get(), input.get());

    gameClearScene_ = std::make_unique<GameClearScene>();
    gameClearScene_->Initialize(object3dCommon.get());

    // --------------------------------------------------------
    // 3. モデルのロード
    //    index 0: block  / index 1: axis  / index 2: player(OBJ)
    // --------------------------------------------------------
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/block",  "block.obj",  textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/axis",   "axis.obj",   textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/player", "player.obj", textureManager.get())));

    // --------------------------------------------------------
    // 4. DebugView 用の汎用オブジェクト生成
    // --------------------------------------------------------
    CreateObject(models[0].get(), { -25.0f, 0.0f, 0.0f })->SetScale({ 10.0f, 1.0f, 10.0f });
    CreateObject(models[1].get(), { -23.0f, 0.0f, 0.0f });
    CreateObject(models[1].get(), { -27.0f, 0.0f, 0.0f });

    // --------------------------------------------------------
    // 5. テストスプライトの初期化
    // --------------------------------------------------------
    uint32_t texHandle = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), texHandle);

    // --------------------------------------------------------
    // 6. サウンドの初期化
    //    SPACE:WAV / M:MP4 / N:MP3 / UP-DOWN:MP3音量
    // --------------------------------------------------------
    sound.Initialize();
    wavSoundData = sound.SoundLoadFile("Resources/Sound/Alarm01.wav");
    mp4SoundData = sound.SoundLoadFile("Resources/Sound/AlarmMovie.mp4");
    mp3SoundData = sound.SoundLoadFile("Resources/Sound/maou_bgm_neorock83.mp3");

    // --------------------------------------------------------
    // 7. プレイヤーの生成と初期化
    // --------------------------------------------------------
    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon.get(), models[2].get());
    player_->SetPosition({ 0.0f, 1.5f, 0.0f });

    // --------------------------------------------------------
    // 8. メインカメラの初期化
    // --------------------------------------------------------
    camera = std::make_unique<Camera>();
#ifndef NDEBUG
    camera->SetAspectRatio(1280.0f / 720.0f);
#endif

    // --------------------------------------------------------
    // 9. ステージマップの初期化
    // --------------------------------------------------------
    stageMap_.Initialize(100, 100, 100);

    // --------------------------------------------------------
    // 10. ビルド設定による初期モードの分岐
    // --------------------------------------------------------
#ifdef DEVELOPMENT
    currentMode_           = AppMode::DebugView;
    debugFlags_.showSkybox = false;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage1.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage1.txt");
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    }
#elif defined(NDEBUG)
    currentMode_           = AppMode::Title;
    debugFlags_.showSkybox = true;
    postProcess_.SetEnabled(false);
#else
    currentMode_           = AppMode::Title;
    debugFlags_.showSkybox = true;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage01.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage01.txt");
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    }
#endif

    // --------------------------------------------------------
    // 11. スカイドーム・スカイボックスの初期化
    // --------------------------------------------------------
    skydomeModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/skydome", "skydome.obj", textureManager.get()));
    skydomeObject_ = std::make_unique<Object3d>();
    skydomeObject_->Initialize(object3dCommon.get());
    skydomeObject_->SetModel(skydomeModel_.get());
    skydomeObject_->SetEnableLighting(false);
    skydomeObject_->SetScale({ 90.0f, 90.0f, 90.0f });

    skyboxTextureHandle_ = textureManager->LoadTexture("Resources/dds/rostock_laage_airport_4k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(object3dCommon.get(), skyboxTextureHandle_);
    skybox_->SetScale({ 50.0f, 50.0f, 50.0f });

    // --------------------------------------------------------
    // 12. ステージレンダラー・マップカーソルの初期化
    // --------------------------------------------------------
    stageRenderer_ = std::make_unique<StageRenderer>();
    stageRenderer_->Initialize(object3dCommon.get());
    stageRenderer_->SetBlockScale({ 1.0f, 1.0f, 1.0f });
    stageRenderer_->BuildFromStageMap(stageMap_);

    mapCursor_ = std::make_unique<MapCursor>();
    mapCursor_->Initialize(object3dCommon.get());
    mapCursor_->SetIndex({ 0, 0, 0 }, stageMap_);
    mapCursor_->SetScale({ 0.9f, 0.9f, 0.9f });

    // --------------------------------------------------------
    // 13. シャドウマップ・ライトカメラの初期化
    // --------------------------------------------------------
    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon.get(), textureManager.get());

    lightCamera_ = std::make_unique<LightCamera>();
    lightCamera_->Initialize();

    // --------------------------------------------------------
    // 14. ブロック関連コントローラーの初期化
    // --------------------------------------------------------
    bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);

    // --------------------------------------------------------
    // 15. UI・インベントリの初期化
    // --------------------------------------------------------
    gameplayUIManager_ = std::make_unique<GameplayUIManager>();
    gameplayUIManager_->Initialize(dxCommon.get(), textureManager.get(), spriteCommon.get(), object3dCommon.get());

    blockInventory_.Initialize(0);

    blockInventoryUI_ = std::make_unique<BlockInventoryUI>();
    blockInventoryUI_->Initialize(dxCommon.get(), spriteCommon.get(), textureManager.get(), &blockInventory_);

    // チュートリアル画像 (操作説明: 832x192px -> 縮小して表示)
    tutorialSprite_ = std::make_unique<Sprite>();
    tutorialSprite_->Initialize(spriteCommon.get(),
        textureManager->LoadTexture("Resources/UI/tutorial/tutorial.png"));
    tutorialSprite_->SetPosition({ 20, 20 });
    tutorialSprite_->SetSize({ 554, 128 });

    // 配置チュートリアル画像 (1024x278px -> 縮小して表示)
    placementTutorialSprite_ = std::make_unique<Sprite>();
    placementTutorialSprite_->Initialize(spriteCommon.get(),
        textureManager->LoadTexture("Resources/UI/tutorial/placement_tutorial.png"));
    placementTutorialSprite_->SetPosition({ 20, 20 });
    placementTutorialSprite_->SetSize({ 682, 185 });

    // --------------------------------------------------------
    // 16. カメラ・ステージエディタコントローラーの初期化
    // --------------------------------------------------------
    gameplayCameraController_.Initialize();
    stageEditorController_.Initialize();

    // --------------------------------------------------------
    // 17. スキニングエディタコントローラーの初期化
    //     SkinnedObject・グリッド線・モデルリストをここで構築する
    // --------------------------------------------------------
    skinningEditor_.Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());

    // --------------------------------------------------------
    // 18. 地形 (Terrain) の初期化
    // --------------------------------------------------------
    terrainModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/terrain", "terrain.obj", textureManager.get()));
    terrainObject_ = std::make_unique<Object3d>();
    terrainObject_->Initialize(object3dCommon.get());
    terrainObject_->SetModel(terrainModel_.get());
    terrainObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
    terrainObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    terrainObject_->SetRotation({ 0.0f, 0.0f, 0.0f });

    // --------------------------------------------------------
    // 19. オフスクリーンレンダリング (PostProcessRenderer) の初期化
    // --------------------------------------------------------
    postProcess_.Initialize(dxCommon.get(), stageMap_.GetClearColor());
}

// --------------------------------------------------------
//  ヘルパー：オブジェクトを生成して objectList に追加する
// --------------------------------------------------------
Object3d* MyGame::CreateObject(Model* model, Vector3 pos) {
    auto obj = std::make_unique<Object3d>();
    obj->Initialize(object3dCommon.get());
    obj->SetModel(model);
    obj->SetPosition(pos);
    obj->SetRotation({ 1.57f, 0.0f, 0.0f });
    Object3d* ptr = obj.get();
    objectList.push_back(std::move(obj));
    return ptr;
}

// ==========================================================
//  MyGame::Update
// ==========================================================
void MyGame::Update() {

    // --------------------------------------------------------
    // 1. モード変化の検知 -> SkinningEditor 移行時にカメラリセット
    // --------------------------------------------------------
    if (currentMode_ != prevMode_) {
        if (currentMode_ == AppMode::SkinningEditor) {
            camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
        }
        prevMode_ = currentMode_;
    }

    // --------------------------------------------------------
    // 2. ImGui の更新 (Debug ビルドのみ)
    // --------------------------------------------------------
#ifndef NDEBUG
    dxCommon->BeginImGui();
    UpdateImGui();
#endif

    // --------------------------------------------------------
    // 3. 入力の更新
    // --------------------------------------------------------
    input->Update();
    UpdateSceneTransition();

    bool isGuiCaptured = false;
#ifndef NDEBUG
    isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
#endif

    // --------------------------------------------------------
    // 4. ライトカメラの更新
    // --------------------------------------------------------
    if (lightCamera_) {
        Vector3 targetPos = player_ ? player_->GetPosition() : camera->GetPosition();
        lightCamera_->Update({ 0.2f, -1.0f, 0.5f }, targetPos);
    }
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // --------------------------------------------------------
    // 5. カメラの更新 (GamePlay 中は gameplayCameraController が担当)
    // --------------------------------------------------------
    if (currentMode_ != AppMode::GamePlay) {
        camera->UpdateBlenderStyle(input.get(), isGuiCaptured, winApp->GetHwnd());
    }

    // --------------------------------------------------------
    // 6. スカイドーム・スカイボックスの更新
    // --------------------------------------------------------
    if (skydomeObject_ && debugFlags_.showSkybox && !showSkyboxCubemap_) {
        skydomeObject_->SetPosition(camera->GetPosition());
        skydomeObject_->Update(Math::MakeIdentity4x4());
    }
    if (skybox_ && showSkyboxCubemap_) {
        skybox_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skybox_->SetPosition(camera->GetPosition());
        skybox_->Update();
    }

    // --------------------------------------------------------
    // 7. サウンドの再生 (キーイベント)
    // --------------------------------------------------------
    if (input->TriggerKey(DIK_SPACE)) { sound.SoundPlay(wavSoundData, wavVolume); }
    if (input->TriggerKey(DIK_M))     { sound.SoundPlay(mp4SoundData, mp4Volume); }
    if (input->TriggerKey(DIK_N))     { sound.SoundPlay(mp3SoundData, mp3Volume); }
    if (input->TriggerKey(DIK_UP)) {
        mp3Volume = (mp3Volume + 0.1f < 1.0f) ? mp3Volume + 0.1f : 1.0f;
        OutputDebugStringA("[MyGame] mp3 volume up\n");
    }
    if (input->TriggerKey(DIK_DOWN)) {
        mp3Volume = (mp3Volume - 0.1f > 0.0f) ? mp3Volume - 0.1f : 0.0f;
        OutputDebugStringA("[MyGame] mp3 volume down\n");
    }

    // --------------------------------------------------------
    // 8. AppMode に応じた更新処理
    // --------------------------------------------------------
    switch (currentMode_) {

    case AppMode::Title:
        UpdateTitle();
        break;

    case AppMode::StageSelect:
        UpdateStageSelect();
        break;

    case AppMode::DebugView:
        UpdateDebugView();
        break;

    case AppMode::StageEditor:
        stageEditorController_.Update(
            input.get(), stageMap_, stageRenderer_.get(),
            mapCursor_.get(), lightCamera_.get(), player_.get(), camera.get());
        break;

    case AppMode::GamePlay:
        UpdateGamePlay();
        break;

    case AppMode::GamePlay_BlockPlace:
        UpdateGamePlayBlockPlace();
        break;

    case AppMode::GameClear:
        if (input->TriggerKey(DIK_SPACE)) {
            if (!gameClearScene_->IsFinished()) {
                gameClearScene_->SkipAnimation();
            } else {
                stageSelect_->Initialize(object3dCommon.get(), input.get());
                gameClearScene_->Initialize(object3dCommon.get());
                isGoalReached_ = false;
                stageMap_.Clear();
                player_->Respawn();
                currentMode_ = AppMode::StageSelect;
            }
        }
        gameClearScene_->Update();
        break;

    case AppMode::SkinningEditor:
        skinningEditor_.Update(dxCommon.get(), input.get(), camera.get(), lightVP, isGuiCaptured);
        break;
    }

    // --------------------------------------------------------
    // 9. カメラの最終更新
    // --------------------------------------------------------
    camera->Update();

    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();

    // --------------------------------------------------------
    // 10. プレイヤーにカメラ行列をセット
    // --------------------------------------------------------
    if (player_) {
        player_->SetCamera(view, proj);
        if (currentMode_ != AppMode::GamePlay) {
            player_->UpdateTransform(lightVP);
        }
    }

    // ウィンドウが最前面にない場合はスキップ
    if (GetActiveWindow() != winApp->GetHwnd()) return;

    // --------------------------------------------------------
    // 11. 3D オブジェクトの更新 (DebugView のみ)
    // --------------------------------------------------------
    if (debugFlags_.show3DObjects && currentMode_ == AppMode::DebugView) {
        for (auto& obj : objectList) {
            if (obj) {
                obj->SetCamera(view, proj);
                obj->Update(lightVP);
            }
        }
        if (terrainObject_ && debugFlags_.showTerrain) {
            terrainObject_->SetCamera(view, proj);
            terrainObject_->Update(lightVP);
        }
    }

    // --------------------------------------------------------
    // 12. ステージレンダラーの更新
    // --------------------------------------------------------
    if (stageRenderer_) {
        stageRenderer_->SetIsEditorMode(currentMode_ == AppMode::StageEditor);
        stageRenderer_->SetCamera(view, proj);
        stageRenderer_->Update(stageMap_, lightVP);
    }
    if (stageRenderer_ && player_ && camera) {
        stageRenderer_->UpdateWallTransparency(camera->GetPosition(), player_->GetPosition());
    }

    // --------------------------------------------------------
    // 13. マップカーソルの更新 (エディタ・配置モードのみ)
    // --------------------------------------------------------
    if (stageRenderer_ && player_) {
        stageRenderer_->UpdateCloudTransparency(player_->GetPosition());
    }

	// マップカーソルの更新 (エディタモードまたは配置モードの時のみ更新)
    if (mapCursor_ && (currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        mapCursor_->SetCamera(view, proj);
        mapCursor_->Update(lightVP);
    }

    // --------------------------------------------------------
    // 14. スプライト・パーティクルの更新
    // --------------------------------------------------------
    if (debugFlags_.showSprite    && currentMode_ == AppMode::DebugView) { sprite->Update(); }
    if (debugFlags_.showParticles) { particleManager->Update(view, proj); }

    // --------------------------------------------------------
    // 15. ライト方向・色・強度をステージマップから取得して反映
    // --------------------------------------------------------
    Vector3 lightDir = stageMap_.GetLightDirection();
    lightCamera_->Update(lightDir, player_->GetPosition());
    object3dCommon->SetLightDirection(lightDir);
    object3dCommon->SetLightColor(Vector4(
        stageMap_.GetLightColor().x,
        stageMap_.GetLightColor().y,
        stageMap_.GetLightColor().z, 1.0f));
    object3dCommon->SetLightIntensity(stageMap_.GetLightIntensity());
    object3dCommon->SetCameraPosition(camera->GetPosition());

    // クリアカラーをステージ設定と同期
    postProcess_.SetClearColor(stageMap_.GetClearColor());

    // --------------------------------------------------------
    // 16. ゲームプレイ UI の更新
    // --------------------------------------------------------
    if (gameplayUIManager_) {
        gameplayUIManager_->Update(
            currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace,
            player_.get(), camera.get(), lightCamera_.get());
    }

    // --------------------------------------------------------
    // 17. インベントリ UI の更新 + 配置モード移行
    // --------------------------------------------------------
    if (blockInventoryUI_) {
        bool isPlayOrPlace = (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);
        blockInventoryUI_->Update(input.get(), winApp.get(), isPlayOrPlace, &stageMap_);

        if (blockInventoryUI_->ConsumeUseRequest()) {
            currentMode_ = AppMode::GamePlay_BlockPlace;
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
}

// ==========================================================
//  MyGame::UpdateImGui  [Debug ビルドのみ]
// ==========================================================
#ifndef NDEBUG
void MyGame::UpdateImGui() {
    ImGuiIO& io         = ImGui::GetIO();
    const float panelW  = 320.0f;
    const float botH    = 360.0f;

    // ========================================================
    // 左パネル: Information
    // ========================================================
    ImGui::SetNextWindowPos( ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, io.DisplaySize.y - botH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::Begin("Information", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::Text("FPS: %.1f (%.3f ms/f)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::SameLine(panelW - 60.0f);
    if (ImGui::Button("Exit", ImVec2(50, 20))) { PostQuitMessage(0); }
    ImGui::Separator();

    // AppMode の選択
    if (ImGui::CollapsingHeader("Hierarchy / Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        int modeIndex = 0;
        switch (currentMode_) {
        case AppMode::DebugView:      modeIndex = 0; break;
        case AppMode::StageEditor:    modeIndex = 1; break;
        case AppMode::GamePlay:       modeIndex = 2; break;
        case AppMode::SkinningEditor: modeIndex = 3; break;
        default: break;
        }
        const char* modeNames[] = { "DebugView", "StageEditor", "GamePlay", "SkinningEditor" };
        if (ImGui::Combo("App Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
            switch (modeIndex) {
            case 0: currentMode_ = AppMode::DebugView;      break;
            case 1: currentMode_ = AppMode::StageEditor;    break;
            case 2: currentMode_ = AppMode::GamePlay;       break;
            case 3:
                currentMode_ = AppMode::SkinningEditor;
                camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
                break;
            }
        }
        ImGui::Checkbox("Show 3D Objects",      &debugFlags_.show3DObjects);
        ImGui::Checkbox("Show Terrain",          &debugFlags_.showTerrain);
        ImGui::Checkbox("Show Skybox",           &debugFlags_.showSkybox);
        ImGui::Checkbox("Show Skybox (Cubemap)", &showSkyboxCubemap_);
        ImGui::Checkbox("Show Sprite",           &debugFlags_.showSprite);
        ImGui::Checkbox("Show Particles",        &debugFlags_.showParticles);
    }

    // オフスクリーン / ポストエフェクト設定
    postProcess_.DrawImGui();

    // カメラ設定
    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Use First-Person Camera", &useFirstPersonCamera_);
        if (useFirstPersonCamera_) {
            ImGui::SliderFloat("FPS Yaw",   &fpsCameraYaw_,   -6.28f, 6.28f);
            ImGui::SliderFloat("FPS Pitch", &fpsCameraPitch_, -1.4f,  1.4f);
        }
        camera->DrawImGui();
        if (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) {
            gameplayCameraController_.SetFov(*camera->GetFovPtr());
        }
    }

    if (ImGui::CollapsingHeader("StageMap Info")) { stageMap_.DrawImGui(); }
    if (ImGui::CollapsingHeader("Cursor Info"))   { mapCursor_->DrawImGui(); }

    ImGui::End();

    // ========================================================
    // 右パネル (Stage Editor) - StageEditorController に委譲
    // ========================================================
    stageEditorController_.DrawImGui(stageMap_, stageRenderer_.get(), mapCursor_.get(), player_.get());

    // ========================================================
    // 下パネル: Tools & Controls
    // ========================================================
    ImGui::SetNextWindowPos( ImVec2(0, io.DisplaySize.y - botH), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, botH),     ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::Begin("Tools & Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (currentMode_ == AppMode::SkinningEditor && skinningEditor_.HasPreviewObject()) {
        // タイムライン UI を SkinningEditorController に委譲
        skinningEditor_.DrawImGuiTimeline();

    } else if (currentMode_ == AppMode::GamePlay && player_) {
        ImGui::Columns(2, "GameplayColumns", false);

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Game Controls & Objective ]");
        ImGui::Text("A / D : Move Left / Right");
        ImGui::Text("SPACE : Jump");
        ImGui::Text("B     : Block Inventory");
        ImGui::Text("ESC   : Return to Stage Select");
        ImGui::Separator();
        ImGui::Text("Objective: Pick up bubbles and reach the Goal!");

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Player Debug ]");
        Vector3 pos = player_->GetPosition();
        ImGui::Text("Pos: X:%.2f Y:%.2f Z:%.2f", pos.x, pos.y, pos.z);
        if (isGoalReached_)
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "GOAL REACHED!");
        else
            ImGui::Text("Status: Playing");
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::StageEditor) {
        ImGui::Columns(2, "EditorColumns", false);

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Stage Editor Controls ]");
        ImGui::Text("W/A/S/D : Cursor Horizontal");
        ImGui::Text("Q/E     : Cursor Up/Down");
        ImGui::Text("ENTER   : Place Block");
        ImGui::Text("SPACE   : Erase Block");
        ImGui::Text("R       : Rotate Block");

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Stage Map Data ]");
        const Int3& cursor = mapCursor_->GetIndex();
        ImGui::Text("Cursor: X:%d Y:%d Z:%d", cursor.x, cursor.y, cursor.z);
        ImGui::Text("Block: %s (ID:%d)",
            BlockTypeToString(stageEditorController_.GetSelectedBlockType()),
            stageEditorController_.GetSelectedBlockType());
        ImGui::Text("Stock: %d", blockInventory_.GetBlockCount());
        ImGui::Columns(1);

    } else {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Application Status ]");
        if      (currentMode_ == AppMode::Title)     ImGui::Text("Scene: TITLE");
        else if (currentMode_ == AppMode::GameClear) ImGui::Text("Scene: CLEAR");
        else                                          ImGui::Text("Scene: DEBUG VIEW");
    }

    ImGui::End();

    // ========================================================
    // 右パネル (Skinning Editor) - SkinningEditor 時のみ
    // ========================================================
    if (currentMode_ == AppMode::SkinningEditor && skinningEditor_.HasPreviewObject()) {
        ImGui::SetNextWindowPos( ImVec2(io.DisplaySize.x - panelW, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelW, io.DisplaySize.y - botH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::Begin("Skinning Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        skinningEditor_.DrawImGuiSidePanel(camera.get(), player_.get(), models[2].get());
        ImGui::End();
    }
}
#endif // !NDEBUG

// ==========================================================
//  MyGame::Draw
// ==========================================================
void MyGame::Draw() {
    auto commandList = dxCommon->GetCommandList();

    // スカイドームのカラーをオフスクリーン設定と同期
    if (skydomeObject_) {
        if (postProcess_.GetSkyboxLinkMode() == 1)
            skydomeObject_->SetColor(postProcess_.GetClearColor());
        else
            skydomeObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
    }

    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // ========================================================
    // パス1: シャドウマップへの書き込み
    // ========================================================
    shadowMap_->PreDraw(commandList);

    commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    commandList->SetPipelineState(object3dCommon->GetShadowPipelineState());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& obj : objectList) { if (obj) obj->DrawShadow(lightVP); }
    if (player_) player_->DrawShadow(lightVP);
    if (currentMode_ == AppMode::SkinningEditor) skinningEditor_.DrawShadow(lightVP);
    if (stageRenderer_) stageRenderer_->DrawShadow(lightVP);

    shadowMap_->PostDraw(commandList);

    // ========================================================
    // パス2: オフスクリーン or 直接バックバッファへ描画
    // ========================================================
    if (postProcess_.IsEnabled()) {
        postProcess_.BeginRender(commandList, dxCommon.get());
        RenderScene(commandList, lightVP);
        postProcess_.EndRender(commandList);
        dxCommon->PreDraw();
        postProcess_.DrawToBackBuffer(commandList);
    } else {
#ifdef NDEBUG
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)WinApp::kWindowWidth, (float)WinApp::kWindowHeight, 0.0f, 1.0f };
        D3D12_RECT     scissor  = { 0, 0, WinApp::kWindowWidth, WinApp::kWindowHeight };
#else
        D3D12_VIEWPORT viewport = { 320.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
        D3D12_RECT     scissor  = { 320, 0, 1600, 720 };
#endif
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
        dxCommon->PreDraw();
        RenderScene(commandList, lightVP);
    }

    // ========================================================
    // パス3: ImGui 出力 & Present
    // ========================================================
#ifndef NDEBUG
    dxCommon->EndImGui();
#endif
    dxCommon->PostDraw();
}

// ==========================================================
//  MyGame::RenderScene
//  オフスクリーンと通常パスで共有するシーン描画
// ==========================================================
void MyGame::RenderScene(ID3D12GraphicsCommandList* commandList, const Matrix4x4& lightVP) {
    if (!debugFlags_.show3DObjects) return;

    ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
    object3dCommon->PreDraw();
    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());

    // ---- シーン別描画 ----
    if (currentMode_ == AppMode::Title) {
        if (titleScene_) titleScene_->Draw();

    } else if (currentMode_ == AppMode::StageSelect) {
        if (stageSelect_) stageSelect_->Draw();

    } else if (currentMode_ == AppMode::GameClear) {
        if (gameClearScene_) gameClearScene_->Draw();

    } else if (currentMode_ == AppMode::SkinningEditor) {
        DrawSkybox(commandList);
        skinningEditor_.Draw(object3dCommon.get(), camera.get());

    } else {
        // DebugView / StageEditor / GamePlay / GamePlay_BlockPlace
        DrawSkybox(commandList);

        bool isGameMode = (currentMode_ == AppMode::StageEditor ||
                           currentMode_ == AppMode::GamePlay     ||
                           currentMode_ == AppMode::GamePlay_BlockPlace);

        if (isGameMode && stageRenderer_) {
            stageRenderer_->Draw();
            stageRenderer_->DrawTransparent();
            object3dCommon->PreDraw();
            commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
        }

        if (currentMode_ == AppMode::GamePlay) {
            if (player_ && !useFirstPersonCamera_) {
                player_->Draw();
                if (IsPlayerHiddenByWall()) {
                    object3dCommon->PreDrawPlayerHighlight();
                    player_->DrawHighlight();
                    object3dCommon->PreDraw();
                    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
                }
            }
            if (gameplayUIManager_) {
                gameplayUIManager_->Draw3DPrompts(
                    true, player_.get(), object3dCommon.get(), commandList, shadowMap_->GetSrvHandle());
            }
        }

        if ((currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace) && mapCursor_) {
            mapCursor_->Draw();
        }

        if (currentMode_ == AppMode::DebugView) {
            if (terrainObject_ && debugFlags_.showTerrain) terrainObject_->Draw();
            for (auto& obj : objectList) { if (obj) obj->Draw(); }
            if (player_) player_->Draw();
        }
    }

    // パーティクルの描画
    if (debugFlags_.showParticles) {
        ID3D12DescriptorHeap* ph[] = { textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, ph);
        particleManager->Draw();
    }

    // スプライトの描画 (DebugView のみ)
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        spriteCommon->PreDraw();
        if (sprite) sprite->Draw();
    }

    // ゲームプレイ UI スプライト
    if (gameplayUIManager_) {
        gameplayUIManager_->DrawSprites(
            currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace,
            gameplayCameraController_.IsFollowPlayerMode());
    }

    // インベントリ UI
    if (blockInventoryUI_ && (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        blockInventoryUI_->Draw();
    }

    // チュートリアルスプライト
    bool invOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();
    if (currentMode_ == AppMode::GamePlay && !invOpen && stageSelect_) {
        if (stageSelect_->GetSelectedFileName() == "tutorial.txt" && tutorialSprite_) {
            spriteCommon->PreDraw();
            tutorialSprite_->Draw();
        }
    }
    if ((currentMode_ == AppMode::GamePlay_BlockPlace || invOpen) && placementTutorialSprite_) {
        spriteCommon->PreDraw();
        placementTutorialSprite_->Draw();
    }
}

// ==========================================================
//  MyGame::Finalize
// ==========================================================
void MyGame::Finalize() {
    if (dxCommon) dxCommon->WaitForGpu();

    sound.Finalize();

#ifndef NDEBUG
    dxCommon->FinalizeImGui();
#endif

    gameClearScene_.reset();
    titleScene_.reset();
    ModelManager::Finalize();
    objectList.clear();
    models.clear();

    if (gameplayUIManager_) gameplayUIManager_->Finalize();
    gameplayUIManager_.reset();
    if (blockInventoryUI_) blockInventoryUI_->Finalize();
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

// ==========================================================
//  更新サブルーチン
// ==========================================================

void MyGame::UpdateDebugView() {
    if (input->TriggerKey(DIK_SPACE)) {
        particleManager->Emit({ 0, 0, 0 }, 10);
    }
    stageEditorController_.HandleCursorInput(
        input.get(), stageMap_, mapCursor_.get(), lightCamera_.get(), camera.get());
}

void MyGame::UpdateGamePlay() {
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // C キーでカメラ切り替え
    if (input->TriggerKey(DIK_C)) {
        useFirstPersonCamera_ = !useFirstPersonCamera_;
        if (useFirstPersonCamera_ && player_) {
            fpsCameraYaw_   = player_->GetRotation().y;
            fpsCameraPitch_ = 0.0f;
        }
    }

    if (!useFirstPersonCamera_) {
        // 三人称カメラ
        if (input->TriggerKey(DIK_V)) {
            bool cur = gameplayCameraController_.IsFollowPlayerMode();
            gameplayCameraController_.SetFollowPlayerMode(!cur);
            if (!cur && player_) {
                Vector3 pp  = player_->GetPosition();
                pp.y       += 0.8f;
                gameplayCameraController_.SetCameraPivot(pp);
            } else if (cur && stageSelect_) {
                gameplayCameraController_.ResetCamera(
                    camera.get(), player_.get(), stageMap_, stageSelect_->GetSelectedIndex());
            }
        }
        camera->SetFov(gameplayCameraController_.GetFov());
        gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(), player_.get());

    } else {
        // 一人称カメラ
        camera->SetFov(0.9f);

        bool isGuiCaptured = false;
#ifndef NDEBUG
        isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
#endif
        const auto& mouse = input->GetMouseState();
        if (mouse.buttons[0] && !isGuiCaptured) {
            RECT rect;
            GetClientRect(winApp->GetHwnd(), &rect);
            float cw = static_cast<float>(rect.right  - rect.left);
            float ch = static_cast<float>(rect.bottom - rect.top);
            if (cw > 0.0f && ch > 0.0f) {
                float sx = static_cast<float>(WinApp::kWindowWidth)  / cw;
                float sy = static_cast<float>(WinApp::kWindowHeight) / ch;
                float mx = static_cast<float>(mouse.posX) * sx;
                float my = static_cast<float>(mouse.posY) * sy;

                const float er   = 0.15f;
                const float spd  = 0.03f;
                float le = WinApp::kWindowWidth  * er;
                float re = WinApp::kWindowWidth  * (1.0f - er);
                float te = WinApp::kWindowHeight * er;
                float be = WinApp::kWindowHeight * (1.0f - er);

                if      (mx < le) fpsCameraYaw_   += spd;
                else if (mx > re) fpsCameraYaw_   -= spd;
                if      (my < te) fpsCameraPitch_ += spd;
                else if (my > be) fpsCameraPitch_ -= spd;
            }
        }

        const float ks = 0.03f;
        if (input->PushKey(DIK_LEFT))  fpsCameraYaw_   += ks;
        if (input->PushKey(DIK_RIGHT)) fpsCameraYaw_   -= ks;
        if (input->PushKey(DIK_UP))    fpsCameraPitch_ += ks;
        if (input->PushKey(DIK_DOWN))  fpsCameraPitch_ -= ks;
        fpsCameraPitch_ = std::clamp(fpsCameraPitch_, -1.4f, 1.4f);

        if (player_) {
            Vector3 pp = player_->GetPosition();
            camera->SetPosition({ pp.x, pp.y + 1.2f, pp.z });
            camera->SetRotation({ fpsCameraPitch_, fpsCameraYaw_, 0.0f });
        }
        camera->Update();
    }

    // カメラガイド UI
    if (gameplayUIManager_) {
        gameplayUIManager_->UpdateCameraGuide(
            currentMode_ == AppMode::GamePlay, input.get(), winApp.get());
    }

    // チュートリアルスプライト更新
    bool invOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();
    if (stageSelect_ && stageSelect_->GetSelectedFileName() == "tutorial.txt"
        && tutorialSprite_ && !invOpen) {
        tutorialSprite_->Update();
    }
    if ((currentMode_ == AppMode::GamePlay_BlockPlace || invOpen) && placementTutorialSprite_) {
        placementTutorialSprite_->Update();
    }

    // ステージマップ更新
    float dt = 1.0f / 60.0f;
    totalTime_ += dt;
    stageMap_.Update(dt, player_ ? player_->GetPosition() : Vector3{ 0,0,0 });
    stageRenderer_->UpdateEffect(stageMap_);

    // プレイヤー更新
    if (player_) {
        float camRot = useFirstPersonCamera_ ? fpsCameraYaw_ : gameplayCameraController_.GetAngle();
        player_->Update(input.get(), stageMap_, camRot, lightVP, dxCommon.get());
    }

    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->BuildFromStageMap(stageMap_);
        stageMap_.ClearRebuildFlag();
    }

    stageRespawnController_.Update(
        stageMap_, backupMap_, stageRenderer_.get(), player_.get(),
        &blockInventory_, &bubblePickupController_,
        &blockPlacementController_, &stageEditorController_);

    Vector3 pPos = player_ ? player_->GetPosition() : Vector3{};
    if (player_) bubblePickupController_.Update(pPos);

    if (Goal::Check(pPos, { 0.4f, 0.9f, 0.4f }, stageMap_)) isGoalReached_ = true;

    if (input->TriggerKey(DIK_B) && blockInventory_.HasBlock()) {
        if (blockInventoryUI_) blockInventoryUI_->ToggleOpen();
    }

    if (isGoalReached_) currentMode_ = AppMode::GameClear;
}

void MyGame::UpdateGamePlayBlockPlace() {
    const Int3& cursor = mapCursor_->GetIndex();

    if (input->TriggerKey(DIK_R)) {
        placeRotationY_ += 1.5707963f;
        if (placeRotationY_ >= 6.0f) placeRotationY_ = 0.0f;
    }

    stageEditorController_.HandleCursorInput(
        input.get(), stageMap_, mapCursor_.get(), lightCamera_.get(), camera.get());

    BlockType selectedType     = BlockType::Ground;
    int       selectedCustomId = 0;
    if (blockInventoryUI_) {
        selectedType     = blockInventoryUI_->GetSelectedBlockType();
        selectedCustomId = blockInventoryUI_->GetSelectedCustomId();
        blockPlacementController_.SetPlaceBlockType(selectedType);
        blockPlacementController_.SetPlaceCustomId(selectedCustomId);
    }

    if (stageRenderer_) {
        stageRenderer_->SetPlacementPreview(
            stageMap_, cursor, selectedType, selectedCustomId, placeRotationY_);
    }

    static bool prevMouse0    = false;
    bool mouseJustPressed     = input->GetMouseState().buttons[0] && !prevMouse0;
    prevMouse0                = input->GetMouseState().buttons[0];
    bool mouseTrigger         = false;
    if (mouseJustPressed && (!blockInventoryUI_ || !blockInventoryUI_->IsActive())) {
        mouseTrigger = true;
    }

    if (input->TriggerKey(DIK_RETURN) || mouseTrigger) {
        if (blockPlacementController_.TryPlace(cursor, placeRotationY_)) {
            bool hasRest = (selectedType == BlockType::Ground)
                || blockInventory_.HasBlock(selectedType, selectedCustomId);
            if (!hasRest) {
                currentMode_    = AppMode::GamePlay;
                placeRotationY_ = 0.0f;
                if (stageRenderer_) stageRenderer_->ClearPlacementPreview();
            }
        }
    }

    if (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_B)) {
        currentMode_    = AppMode::GamePlay;
        placeRotationY_ = 0.0f;
        if (stageRenderer_) stageRenderer_->ClearPlacementPreview();
    }

    stageEditorController_.HandleCameraInput(input.get(), camera.get());
}

void MyGame::UpdateTitle() {
    if (titleScene_) {
        titleScene_->Update();
        if (titleScene_->IsFinished()) currentMode_ = AppMode::StageSelect;
    }
}

void MyGame::UpdateStageSelect() {
    stageSelect_->Update();
    if (stageSelect_->IsFnished()) {
        std::string path = "Resources/Stages/" + stageSelect_->GetSelectedFileName();
        if (std::filesystem::exists(path)) {
            stageMap_.LoadFromFile(path);
            backupMap_ = stageMap_;
            stageRenderer_->BuildFromStageMap(stageMap_);
            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
            gameplayCameraController_.ResetCamera(
                camera.get(), player_.get(), stageMap_, stageSelect_->GetSelectedIndex());
            blockInventory_.Initialize(0);
        }
        currentMode_ = AppMode::GamePlay;
    }
}

void MyGame::UpdateSceneTransition() {
    if ((currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)
        && input->TriggerKey(DIK_ESCAPE)) {
        stageMap_ = backupMap_;
        stageRenderer_->BuildFromStageMap(stageMap_);
        bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        stageSelect_->Initialize(object3dCommon.get(), input.get());
        isGoalReached_ = false;
        if (player_) player_->Respawn();
        currentMode_ = AppMode::StageSelect;
    }
}

// ==========================================================
//  描画ヘルパー
// ==========================================================

void MyGame::DrawSkybox(ID3D12GraphicsCommandList* commandList) {
    if (showSkyboxCubemap_ && skybox_) {
        skybox_->Draw();
        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
    } else if (debugFlags_.showSkybox && skydomeObject_) {
        skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skydomeObject_->Draw();
    }
}

bool MyGame::IsPlayerHiddenByWall() const {
    if (!player_ || !camera) return false;

    Vector3 camPos    = camera->GetPosition();
    Vector3 playerPos = player_->GetPosition();
    playerPos.y      += 0.8f;

    Vector3 diff = {
        playerPos.x - camPos.x,
        playerPos.y - camPos.y,
        playerPos.z - camPos.z
    };
    float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    if (len <= 0.001f) return false;

    Vector3 dir = { diff.x / len, diff.y / len, diff.z / len };

    for (float t = 0.8f; t < len - 1.0f; t += 0.8f) {
        Vector3 cp = { camPos.x + dir.x * t, camPos.y + dir.y * t, camPos.z + dir.z * t };
        const MapCell* cell = stageMap_.GetCell(
            static_cast<int>(std::floor(cp.x + 0.5f)),
            static_cast<int>(std::floor(cp.y)),
            static_cast<int>(std::floor(cp.z + 0.5f)));
        if (cell && cell->isSolid) return true;
    }
    return false;
}
