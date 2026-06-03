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
#include "../Environment/WeatherPresetManager.h"
#include "Goal.h"
#include "ModelManager.h"
#include <memory>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

// ==========================================================
//  MyGame::Initialize
//  全サブシステムを生成・初期化する
//
//  生成順序は依存関係の順を厳守すること:
//    WinApp → DirectXCommon → Input → TextureManager →
//    SpriteCommon → Object3dCommon → ParticleManager →
//    Scene群 → Model群 → Player → Camera → Stage → UI → ...
// ==========================================================
void MyGame::Initialize() {
    WeatherPresetManager::GetInstance().LoadPresets();

    // --------------------------------------------------------
    // 1. エンジン基盤システムの生成と初期化
    //    生成順は依存関係の順 (winApp → dxCommon → input → ...)
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
    
    

    stageSelect_ = std::make_unique<StageSelect>();
    stageSelect_->Initialize(object3dCommon.get(), input.get());

    
    

    // --------------------------------------------------------
    // 3. モデルのロード
    //    index 0: block  / index 1: axis  / index 2: player(OBJ)
    // --------------------------------------------------------
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/block",  "block.obj",  textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/axis",   "axis.obj",   textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/player", "player.obj", textureManager.get())));

    // --------------------------------------------------------
    // 4. DebugView 用の汎用オブジェクト生成
    //    ブロックとaxis モデルを DebugView 画面に配置する
    // --------------------------------------------------------
    Object3d* debugFloor = CreateObject(models[0].get(), { -25.0f, 0.0f, 0.0f });
    debugFloor->SetScale({ 10.0f, 1.0f, 10.0f });
    debugFloor->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d* debugAxisA = CreateObject(models[1].get(), { -23.0f, 0.0f, 0.0f });
    debugAxisA->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d* debugAxisB = CreateObject(models[1].get(), { -27.0f, 0.0f, 0.0f });
    debugAxisB->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    // --------------------------------------------------------
    // 5. テストスプライトの初期化
    //    uvChecker.png を使った UV 確認用スプライト
    // --------------------------------------------------------
    uint32_t texHandle = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), texHandle);

    // --------------------------------------------------------
    // 6. サウンドの初期化
    //    SPACE:WAV / M:MP4 / N:MP3 / UP-DOWN:MP3 音量調整
    // --------------------------------------------------------
    sound.Initialize();
    
    
    gameBgmData = sound.SoundLoadFile("Resources/Sound/gamePlay.mp3");
    

    // --------------------------------------------------------
    // 7. プレイヤーの生成と初期化
    // --------------------------------------------------------
    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon.get(), models[2].get());
    player_->SetPosition({ 0.0f, 1.5f, 0.0f });

    

    // --------------------------------------------------------
    // 8. メインカメラの初期化
    //    Debug ビルドでは ImGui パネル分だけビューポートが狭いため
    //    アスペクト比を手動で 1280/720 にセットする
    // --------------------------------------------------------
    camera = std::make_unique<Camera>();
#ifndef NDEBUG
    camera->SetAspectRatio(1280.0f / 720.0f);
#endif

    // --------------------------------------------------------
    // 9. ステージマップの初期化 (最大 100x100x100 グリッド)
    // --------------------------------------------------------
    stageMap_.Initialize(100, 100, 100);

    // --------------------------------------------------------
    // 10. ビルド設定による初期モードの分岐
    //     DEVELOPMENT: DebugView で起動 (エディタ開発用)
    //     NDEBUG(Release): Title で起動 (製品版)
    //     それ以外(Debug): Title で起動
    // --------------------------------------------------------
#ifdef DEVELOPMENT
    currentMode_           = AppMode::DebugView;
    debugFlags_.showSkybox = false;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage1.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage1.txt");
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    }
#elif defined(NDEBUG)
    currentMode_           = AppMode::StageSelect;
    debugFlags_.showSkybox = true;
    postProcess_.SetEnabled(false);
#else
    currentMode_           = AppMode::StageSelect;
    debugFlags_.showSkybox = true;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage01.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage01.txt");
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    }
#endif

    // --------------------------------------------------------
    // 11. スカイドーム・スカイボックスの初期化
    //     skydomeObject_ : 球体メッシュ上にテクスチャを貼った天球
    //     skybox_        : キューブマップを使った立方体型スカイボックス
    //     どちらを使うかは showSkyboxCubemap_ フラグで切り替える
    // --------------------------------------------------------
    skydomeModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/skydome", "skydome.obj", textureManager.get()));
    skydomeObject_ = std::make_unique<Object3d>();
    skydomeObject_->Initialize(object3dCommon.get());
    skydomeObject_->SetModel(skydomeModel_.get());
    skydomeObject_->SetEnableLighting(false); // 天球は自発光 (ライティング不要)
    skydomeObject_->SetScale({ 90.0f, 90.0f, 90.0f });

    skyboxTextureHandle_ = textureManager->LoadTexture("Resources/dds/rostock_laage_airport_4k.dds");
    object3dCommon->SetEnvironmentTextureHandle(skyboxTextureHandle_);
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(object3dCommon.get(), skyboxTextureHandle_);
    skybox_->SetScale({ 50.0f, 50.0f, 50.0f });

    // --------------------------------------------------------
    // 12. ステージレンダラー・マップカーソルの初期化
    //     stageRenderer_ : ステージマップの各ブロックを描画する
    //     mapCursor_     : エディタ・配置モードでブロック選択位置を表示する
    // --------------------------------------------------------
    stageRenderer_ = std::make_unique<StageRenderer>();
    stageRenderer_->Initialize(object3dCommon.get());
    stageRenderer_->SetBlockScale({ 1.0f, 1.0f, 1.0f });
    stageRenderer_->BuildFromStageMap(stageMap_);

    mapCursor_ = std::make_unique<MapCursor>();
    mapCursor_->Initialize(object3dCommon.get());
    mapCursor_->SetIndex({ 0, 0, 0 }, stageMap_);
    mapCursor_->SetScale({ 0.9f, 0.9f, 0.9f }); // ブロックより少し小さく表示

    // --------------------------------------------------------
    // 13. シャドウマップ・ライトカメラの初期化
    //     shadowMap_   : 影生成用の深度テクスチャ (4096x4096)
    //     lightCamera_ : ライト視点から見たビュー・プロジェクション行列を生成する
    // --------------------------------------------------------
    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon.get(), textureManager.get());

    lightCamera_ = std::make_unique<LightCamera>();
    lightCamera_->Initialize();

    // --------------------------------------------------------
    // 14. ブロック関連コントローラーの初期化
    //     bubblePickupController_    : バブル (コイン) の取得判定
    //     blockPlacementController_  : プレイヤーによるブロック設置
    // --------------------------------------------------------
    bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);

    // --------------------------------------------------------
    // 15. UI・インベントリの初期化
    //     gameplayUIManager_ : ゲームプレイ中のヘルスバー・ガイドUI等
    //     blockInventoryUI_  : Bキーで開くブロックインベントリ画面
    //     tutorialSprite_    : tutorial ステージ用の操作説明画像
    // --------------------------------------------------------
    gameplayUIManager_ = std::make_unique<GameplayUIManager>();
    gameplayUIManager_->Initialize(dxCommon.get(), textureManager.get(), spriteCommon.get(), object3dCommon.get());

    blockInventory_.Initialize(0);

    blockInventoryUI_ = std::make_unique<BlockInventoryUI>();
    blockInventoryUI_->Initialize(dxCommon.get(), spriteCommon.get(), textureManager.get(), &blockInventory_);

    // チュートリアル画像 (操作説明: 832x192px → 縮小して表示)
    tutorialSprite_ = std::make_unique<Sprite>();
    tutorialSprite_->Initialize(spriteCommon.get(),
        textureManager->LoadTexture("Resources/UI/tutorial/tutorial.png"));
    tutorialSprite_->SetPosition({ 20, 20 });
    tutorialSprite_->SetSize({ 554, 128 });

    // 配置チュートリアル画像 (1024x278px → 縮小して表示)
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
    // 18. 地形 (Terrain) オブジェクトの初期化
    //     DebugView での地形確認用。ゲームプレイには影響しない。
    // --------------------------------------------------------
    terrainModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/terrain", "terrain.obj", textureManager.get()));
    terrainObject_ = std::make_unique<Object3d>();
    terrainObject_->Initialize(object3dCommon.get());
    terrainObject_->SetModel(terrainModel_.get());
    terrainObject_->SetPosition({ 0.0f, 0.0f, 0.0f });
    terrainObject_->SetScale({ 1.0f, 1.0f, 1.0f });
    terrainObject_->SetRotation({ 0.0f, 0.0f, 0.0f });
    terrainObject_->SetEnvironmentCoefficient(terrainEnvironmentCoefficient_);

    // --------------------------------------------------------
    // 19. オフスクリーンレンダリング (PostProcessRenderer) の初期化
    //     RenderTexture にシーンを描き、ポストエフェクトを掛けてバックバッファに転送する
    // --------------------------------------------------------
    postProcess_.Initialize(dxCommon.get(), stageMap_.GetClearColor());
}

// --------------------------------------------------------
//  ヘルパー：モデルと位置を指定して Object3d を生成し objectList に追加する
//  戻り値の生ポインタは objectList が所有するため、
//  呼び出し元が delete してはいけない
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
//  毎フレームの更新処理
//
//  処理順:
//    1. モード切り替え検知 → 初期化
//    2. ImGui フレーム開始 (Debug のみ)
//    3. 入力更新・シーン遷移
//    4. ライトカメラ更新
//    5. Blender 風カメラ更新 (GamePlay 以外)
//    6. スカイドーム / スカイボックス追従
//    7. サウンドキー入力
//    8. AppMode 別の Update 呼び出し
//    9. カメラ最終更新
//   10. 各種サブシステム更新 (ステージ・UI・インベントリ)
// ==========================================================
void MyGame::Update() {

    // --------------------------------------------------------
    // 1. モード変化の検知 → SkinningEditor 移行時にカメラリセット
    // --------------------------------------------------------
    if (currentMode_ != prevMode_) {
        UpdateBGM();


        if (currentMode_ == AppMode::SkinningEditor) {
            // モデル正面に強制リセット
            camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
        }
        prevMode_ = currentMode_;
    }

    // --------------------------------------------------------
    // 2. ImGui の更新 (Debug ビルドのみ)
    //    BeginImGui() はフレームの先頭で必ず呼ぶこと
    // --------------------------------------------------------
#ifndef NDEBUG
    dxCommon->BeginImGui();
    UpdateImGui();
#endif

    // --------------------------------------------------------
    // 3. 入力の更新とシーン遷移 (ESC でタイトルに戻るなど)
    // --------------------------------------------------------
    input->Update();
    UpdateSceneTransition();

    // ImGui がマウスをキャプチャしている場合はゲーム側のクリック操作を無効化
    bool isGuiCaptured = false;
#ifndef NDEBUG
    isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
#endif

    // --------------------------------------------------------
    // 4. ライトカメラの更新
    //    ライト方向とプレイヤー位置からライト視点 VP 行列を生成する
    //    この lightVP はシャドウマップ生成と PSO への転送に使う
    // --------------------------------------------------------
    if (lightCamera_) {
        Vector3 targetPos = player_ ? player_->GetPosition() : camera->GetPosition();
        lightCamera_->Update({ 0.2f, -1.0f, 0.5f }, targetPos);
    }
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // --------------------------------------------------------
    // 5. カメラの更新
    //    GamePlay 中は GameplayCameraController が制御するため
    //    Blender 風操作はスキップする
    // --------------------------------------------------------
    if (currentMode_ != AppMode::GamePlay) {
        camera->UpdateBlenderStyle(input.get(), isGuiCaptured, winApp->GetHwnd());
    }

    // --------------------------------------------------------
    // 6. スカイドーム / スカイボックスの更新
    //    カメラ位置に追従させることで「無限遠にある」ように見せる
    //    showSkyboxCubemap_ フラグで天球メッシュ/キューブマップを切り替える
    // --------------------------------------------------------
    if (skydomeObject_ && debugFlags_.showSkybox && !showSkyboxCubemap_) {
        skydomeObject_->SetPosition(camera->GetPosition());
        skydomeObject_->Update(Math::MakeIdentity4x4());
    }
    if (skybox_ && debugFlags_.showSkybox && showSkyboxCubemap_) {
        skybox_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skybox_->Update(camera->GetPosition());
    }

    // --------------------------------------------------------
    // 7. サウンドの再生 (キーイベント)
    //    SPACE:WAV / M:MP4 / N:MP3 / UP-DOWN:MP3 音量調整
    // --------------------------------------------------------


   /* if (input->TriggerKey(DIK_P))     { sound.SoundPlay(wavSoundData, wavVolume); }
    if (input->TriggerKey(DIK_M))     { sound.SoundPlay(mp4SoundData, mp4Volume); }
    if (input->TriggerKey(DIK_N))     { sound.SoundPlay(mp3SoundData, mp3Volume); }
    if (input->TriggerKey(DIK_UP)) {
        mp3Volume = (mp3Volume + 0.1f < 1.0f) ? mp3Volume + 0.1f : 1.0f;
        OutputDebugStringA("[MyGame] mp3 volume up\n");
    }
    if (input->TriggerKey(DIK_DOWN)) {
        mp3Volume = (mp3Volume - 0.1f > 0.0f) ? mp3Volume - 0.1f : 0.0f;
        OutputDebugStringA("[MyGame] mp3 volume down\n");
    }*/

    // --------------------------------------------------------
    // 8. AppMode に応じた更新処理
    //    各 Update メソッドにモード固有のロジックを分離している
    // --------------------------------------------------------
    switch (currentMode_) {

    case AppMode::StageSelect:
        UpdateStageSelect();
        break;

    case AppMode::DebugView:
        UpdateDebugView();
        break;

    case AppMode::StageEditor:
        // StageEditorController に処理を全て委譲
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

    case AppMode::SkinningEditor:
        // SkinningEditorController に処理を全て委譲
        skinningEditor_.Update(dxCommon.get(), input.get(), camera.get(), lightVP, isGuiCaptured);
        break;
    }

    // --------------------------------------------------------
    // 9. カメラの最終更新 (View / Projection 行列を確定させる)
    // --------------------------------------------------------
    camera->Update();

    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();

    // プレイヤーにカメラ行列をセット
    // GamePlay 中は Player::Update() 内で行列更新するため UpdateTransform は呼ばない
    if (player_) {
        player_->SetCamera(view, proj);
        if (currentMode_ != AppMode::GamePlay) {
            player_->UpdateTransform(lightVP);
        }
    }

    // ウィンドウが最前面にない場合は以降の更新をスキップ
    if (GetActiveWindow() != winApp->GetHwnd()) {
        return;
    }

    // --------------------------------------------------------
    // 10. 3D オブジェクトの更新 (DebugView モード限定)
    // --------------------------------------------------------
    if (debugFlags_.show3DObjects && currentMode_ == AppMode::DebugView) {
        for (auto& obj : objectList) {
            if (obj) {
                obj->SetCamera(view, proj);
                obj->Update(lightVP);
            }
        }
        // 地形の更新 (DebugView + showTerrain フラグが ON のとき)
        if (terrainObject_ && debugFlags_.showTerrain) {
            terrainObject_->SetCamera(view, proj);
            terrainObject_->Update(lightVP);
        }
    }

    // --------------------------------------------------------
    // 11. ステージレンダラーの更新
    //     ブロックの定数バッファ転送・壁透明化 (カメラとプレイヤーの間)
    // --------------------------------------------------------
    if (stageRenderer_) {
        stageRenderer_->SetIsEditorMode(currentMode_ == AppMode::StageEditor);
        stageRenderer_->SetCamera(view, proj);
        stageRenderer_->Update(stageMap_, lightVP);
    }
    

    stageRenderer_->UpdateCloudTransparency(
        camera->GetPosition(),
        player_->GetPosition()
    );

    // --------------------------------------------------------
    // 12. マップカーソルの更新 (エディタ・配置モードのみ)
    // --------------------------------------------------------
    if (mapCursor_ && (currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        mapCursor_->SetCamera(view, proj);
        mapCursor_->Update(lightVP);
    }

    // --------------------------------------------------------
    // 13. スプライト・パーティクルの更新
    // --------------------------------------------------------
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        sprite->Update();
    }
    if (debugFlags_.showParticles) {
        // 天候プリセットの同期
        auto& wpMgr = WeatherPresetManager::GetInstance();
        WeatherPreset* currentPreset = wpMgr.GetPresetByName(stageMap_.GetWeatherPresetName());
        if (currentPreset) {
            auto& emitter = particleManager->GetWeatherEmitter();
            emitter.active = currentPreset->particleEnabled;
            if (emitter.active) {
                emitter.emitRate = currentPreset->emitRate;
                emitter.size = currentPreset->emitSize;
                emitter.velocity = currentPreset->velocity;
                emitter.velocityRandom = currentPreset->velocityRandom;
                emitter.particleSize = currentPreset->particleSize;
                emitter.particleLife = currentPreset->particleLife;
                emitter.color = currentPreset->particleColor;
                // パーティクルを上空から降らせるため、生成位置をプレイヤーの頭上(+15)に設定
                emitter.center = {0.0f, 15.0f, 0.0f}; 
                particleManager->SetTexture(textureManager->LoadTexture(currentPreset->particleTexture));
            }
        }
        
        particleManager->Update(1.0f / 60.0f, view, proj, player_ ? player_->GetPosition() : Vector3{0, 0, 0}, &stageMap_);
    }

    // --------------------------------------------------------
    // 14. ライト設定をステージマップから取得してエンジンに反映
    //     ステージごとに異なる光の色・方向・強度をサポートするための仕組み
    // --------------------------------------------------------
    Vector3 lightDir = stageMap_.GetLightDirection();
    lightCamera_->Update(lightDir, player_->GetPosition());
    object3dCommon->SetLightDirection(lightDir);
    object3dCommon->SetLightColor(Vector4(
        stageMap_.GetLightColor().x,
        stageMap_.GetLightColor().y,
        stageMap_.GetLightColor().z, 1.0f));
    object3dCommon->SetLightIntensity(stageMap_.GetLightIntensity());
    object3dCommon->SetCameraPosition(camera->GetPosition()); // スペキュラー計算用

    // クリアカラーをステージ設定と同期 (PostProcessRenderer のオフスクリーン背景色)
    postProcess_.SetClearColor(stageMap_.GetClearColor());

    // --------------------------------------------------------
    // 15. ゲームプレイ UI の更新
    // --------------------------------------------------------
    if (gameplayUIManager_) {
        gameplayUIManager_->Update(
            currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace,
            player_.get(), camera.get(), lightCamera_.get());
    }

    // --------------------------------------------------------
    // 16. インベントリ UI の更新 + ブロック配置モード移行
    //     ConsumeUseRequest() が true のとき GamePlay_BlockPlace に移行し、
    //     カーソルをプレイヤー位置に初期化する
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
//
//  3ペインの ImGui レイアウト:
//    左パネル (幅 320px)     : Information / Mode 切り替え / Camera
//    右パネル (幅 320px)     : StageEditor 操作 / SkinningEditor サイドパネル
//    下パネル (高さ 360px)   : Tools & Controls (モード別コンテンツ)
// ==========================================================
#ifndef NDEBUG
void MyGame::UpdateImGui() {
    ImGuiIO& io        = ImGui::GetIO();
    const float panelW = 320.0f;
    const float botH   = 360.0f;

    // ========================================================
    // 左パネル: Information
    // ========================================================
    ImGui::SetNextWindowPos( ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelW, io.DisplaySize.y - botH), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f);
    ImGui::Begin("Information", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::Text("FPS: %.1f (%.3f ms/f)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::SameLine(panelW - 60.0f);
    if (ImGui::Button("Exit", ImVec2(50, 20))) {
        PostQuitMessage(0);
    }
    ImGui::Separator();

    // AppMode の選択 (コンボボックス)
    if (ImGui::CollapsingHeader("Hierarchy / Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        int modeIndex = 0;
        switch (currentMode_) {
        case AppMode::DebugView:      modeIndex = 0; break;
        case AppMode::StageEditor:    modeIndex = 1; break;
        case AppMode::GamePlay:       modeIndex = 2; break;
        case AppMode::SkinningEditor: modeIndex = 3; break;
        default:                                     break;
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
        // デバッグ表示のオン/オフ切り替え
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
        // GamePlay 中はゲームプレイカメラに FOV を同期する
        if (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) {
            gameplayCameraController_.SetFov(*camera->GetFovPtr());
        }
    }

    if (ImGui::CollapsingHeader("StageMap Info")) {
        stageMap_.DrawImGui();
    }
    if (ImGui::CollapsingHeader("Cursor Info")) {
        mapCursor_->DrawImGui();
    }

    ImGui::End(); // 左パネルここまで

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
        if (isGoalReached_) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "GOAL REACHED!");
        } else {
            ImGui::Text("Status: Playing");
        }
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
        // DebugView
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Application Status ]");
        ImGui::Text("Scene: DEBUG VIEW");

        // --- Player Options ---
        if (ImGui::CollapsingHeader("Player Settings")) {
            ImGui::SliderFloat("Player Glow", &playerGlow_, 0.0f, 5.0f);
            if (player_) player_->SetGlow(playerGlow_);
        }

        if (ImGui::CollapsingHeader("Environment Map", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderFloat("Debug Objects", &debugObjectEnvironmentCoefficient_, 0.0f, 1.0f);
            ImGui::SliderFloat("Terrain", &terrainEnvironmentCoefficient_, 0.0f, 1.0f);
            ImGui::SliderFloat("Player", &playerEnvironmentCoefficient_, 0.0f, 1.0f);

            for (auto& obj : objectList) {
                if (obj) {
                    obj->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);
                }
            }
            if (terrainObject_) {
                terrainObject_->SetEnvironmentCoefficient(terrainEnvironmentCoefficient_);
            }
            if (player_) {
                player_->SetEnvironmentCoefficient(playerEnvironmentCoefficient_);
            }
        }
    }

    ImGui::End(); // 下パネルここまで

    // ========================================================
    // 右パネル (Skinning Editor) - SkinningEditor モード時のみ
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
//  3パスで描画を行う:
//    パス1: シャドウマップ生成 (ライト視点で深度のみ書き込む)
//    パス2: シーン描画 (オフスクリーンまたはバックバッファへ)
//    パス3: ImGui 出力 & SwapChain Present
// ==========================================================
void MyGame::Draw() {
    auto commandList = dxCommon->GetCommandList();

    // スカイドームのカラーをオフスクリーン設定と同期
    // skyboxLinkMode_ == 1 のとき、クリアカラーをスカイドームに乗算合成する
    if (skydomeObject_) {
        if (postProcess_.GetSkyboxLinkMode() == 1) {
            skydomeObject_->SetColor(postProcess_.GetClearColor());
        } else {
            skydomeObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // ========================================================
    // パス1: シャドウマップへの書き込み
    //   ・ShadowMap の DSV をクリアして書き込み準備
    //   ・影用 PSO (ピクセルシェーダーなし・深度のみ) で描画
    //   ・PostDraw() でリソースバリアを SRV に遷移
    // ========================================================
    shadowMap_->PreDraw(commandList);

    // 影用の RootSignature / PSO をバインドする
    commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    commandList->SetPipelineState(object3dCommon->GetShadowPipelineState());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& obj : objectList) {
        if (obj) { obj->DrawShadow(lightVP); }
    }
    if (player_) { player_->DrawShadow(lightVP); }
    if (currentMode_ == AppMode::SkinningEditor) { skinningEditor_.DrawShadow(lightVP); }
    if (stageRenderer_) { stageRenderer_->DrawShadow(lightVP); }

    shadowMap_->PostDraw(commandList); // DSV → SRV へバリア遷移

    // ========================================================
    // パス2: オフスクリーン or 直接バックバッファへ描画
    //   オフスクリーン有効時:
    //     BeginRender → RenderScene → EndRender → DrawToBackBuffer
    //   オフスクリーン無効時:
    //     dxCommon->PreDraw() → RenderScene (直接バックバッファ)
    // ========================================================
    if (postProcess_.IsEnabled()) {
        postProcess_.BeginRender(commandList, dxCommon.get()); // RenderTexture を RT にセット
        RenderScene(commandList, lightVP);
        postProcess_.EndRender(commandList);                   // RenderTexture を SRV に遷移
        dxCommon->PreDraw();                                   // バックバッファを RT にセット
        postProcess_.DrawToBackBuffer(commandList);            // 全画面コピー描画
    } else {
        // ビューポートとシザー矩形を設定
        // Debug 時は左 320px が ImGui パネルのため、描画領域をずらす
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
    // パス3: ImGui の描画コマンドを CommandList に追加して Present
    // ========================================================
#ifndef NDEBUG
    dxCommon->EndImGui();
#endif
    dxCommon->PostDraw(); // コマンドを GPU に送信し SwapChain を Present
}

// ==========================================================
//  MyGame::RenderScene
//  パス2 (シーン描画) の共通処理。オフスクリーンと通常パス両方から呼ばれる。
//
//  描画ルーティング:
//    Title / StageSelect / GameClear : 各シーンクラスに委譲
//    SkinningEditor                  : SkinningEditorController に委譲
//    その他 (DebugView/StageEditor/GamePlay) :
//      スカイボックス → ステージ → プレイヤー → カーソル → スプライト の順
// ==========================================================
void MyGame::RenderScene(ID3D12GraphicsCommandList* commandList, const Matrix4x4& lightVP) {
    // show3DObjects が OFF のときは何も描画しない (ImGui のみ表示)
    if (!debugFlags_.show3DObjects) {
        return;
    }

    // テクスチャ SRV ヒープをバインドし、シャドウマップ SRV をスロット 4 にセット
    ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
    object3dCommon->PreDraw(); // 通常描画用 RootSignature / PSO をバインド
    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());

    // ---- シーン別描画ルーティング ----
    // 全シーン共通でスカイボックスを描画（最背面）
    DrawSkybox(commandList);

    if (currentMode_ == AppMode::StageSelect) {
        if (stageSelect_) { stageSelect_->Draw(); }

    } else if (currentMode_ == AppMode::SkinningEditor) {
        skinningEditor_.Draw(object3dCommon.get(), camera.get());

    } else {
        // DebugView / StageEditor / GamePlay / GamePlay_BlockPlace の共通描画パス

        bool isGameMode = (currentMode_ == AppMode::StageEditor ||
                           currentMode_ == AppMode::GamePlay     ||
                           currentMode_ == AppMode::GamePlay_BlockPlace);

        // ゲーム系モードではステージブロックを描画する
        if (isGameMode && stageRenderer_) {
            stageRenderer_->Draw();             // 不透明ブロック
            stageRenderer_->DrawTransparent();  // 半透明ブロック (壁の透明化など)
            // DrawTransparent() の後は PSO が変わるため再バインドが必要
            object3dCommon->PreDraw();
            commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
        }

        if (currentMode_ == AppMode::GamePlay) {
            // プレイヤー本体の描画 (一人称時はスキップ)
            if (player_ && !useFirstPersonCamera_) {
                player_->Draw();
                // カメラとプレイヤーの間に壁がある場合はシルエット表示
                if (IsPlayerHiddenByWall()) {
                    object3dCommon->PreDrawPlayerHighlight(); // シルエット用 PSO に切り替え
                    player_->DrawHighlight();
                    object3dCommon->PreDraw(); // 通常 PSO に戻す
                    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
                }
            }
            // 3D UI (ドア・はしごのプロンプトなど)
            if (gameplayUIManager_) {
                gameplayUIManager_->Draw3DPrompts(
                    true, player_.get(), object3dCommon.get(), commandList, shadowMap_->GetSrvHandle());
            }
        }

        // エディタ・配置モードのカーソル描画
        if ((currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace) && mapCursor_) {
            mapCursor_->Draw();
        }

        // DebugView 限定の描画
        if (currentMode_ == AppMode::DebugView) {
            if (terrainObject_ && debugFlags_.showTerrain) { terrainObject_->Draw(); }
            for (auto& obj : objectList) {
                if (obj) { obj->Draw(); }
            }
            if (player_) { player_->Draw(); }
        }
    }

    // パーティクルの描画 (SRV ヒープを再バインドしてから描画)
    if (debugFlags_.showParticles) {
        ID3D12DescriptorHeap* ph[] = { textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, ph);
        particleManager->Draw();
    }

    // スプライトの描画 (DebugView モード + フラグ ON 時のみ)
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        spriteCommon->PreDraw();
        if (sprite) { sprite->Draw(); }
    }

    // ゲームプレイ UI のスプライト描画 (HP・カメラガイドなど)
    if (gameplayUIManager_) {
        gameplayUIManager_->DrawSprites(
            currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace,
            gameplayCameraController_.IsFollowPlayerMode());
    }

    // インベントリ UI の描画 (ゲームプレイ・配置モード中のみ)
    if (blockInventoryUI_ && (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        blockInventoryUI_->Draw();
    }

    // チュートリアルスプライトの描画
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
//  全サブシステムを解放する
//
//  順序注意: GPU がコマンドを処理し終えるまで待ってからリソースを解放する。
//  WaitForGpu() を呼ばずに解放すると GPU が使用中のリソースへ
//  アクセスしてクラッシュする。
// ==========================================================
void MyGame::Finalize() {
    // GPU の全コマンド処理が完了するまで待機 (最重要)
    if (dxCommon) { dxCommon->WaitForGpu(); }

    sound.Finalize();

#ifndef NDEBUG
    dxCommon->FinalizeImGui();
#endif

    // シーン・モデルの解放
    
    
    ModelManager::Finalize();
    objectList.clear();
    models.clear();

    // UI の解放 (Finalize() を持つものは先に呼ぶ)
    if (gameplayUIManager_) { gameplayUIManager_->Finalize(); }
    gameplayUIManager_.reset();
    if (blockInventoryUI_) { blockInventoryUI_->Finalize(); }
    blockInventoryUI_.reset();

    // ゲームオブジェクトの解放
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

    // エンジン基盤の解放 (生成の逆順)
    particleManager.reset();
    object3dCommon.reset();
    spriteCommon.reset();
    textureManager.reset();
    input.reset();
    dxCommon.reset();
    winApp.reset();
}

// ==========================================================
//  更新サブルーチン群
//  各 AppMode の Update 処理を分割して管理する
// ==========================================================

// --------------------------------------------------------
//  UpdateDebugView : DebugView モードの更新
//  SPACE キーでパーティクルを発生させてテスト確認できる
// --------------------------------------------------------
void MyGame::UpdateDebugView() {
    if (input->TriggerKey(DIK_SPACE)) {
        particleManager->Emit({ 0, 0, 0 }, 10);
    }
    // カーソル移動処理は StageEditorController に委譲
    stageEditorController_.HandleCursorInput(
        input.get(), stageMap_, mapCursor_.get(), lightCamera_.get(), camera.get());
}

// --------------------------------------------------------
//  UpdateGamePlay : ゲームプレイメインの更新
//
//  C キー: 三人称 ↔ 一人称カメラ切り替え
//  V キー: プレイヤー追従 ↔ 固定カメラ切り替え
// --------------------------------------------------------
void MyGame::UpdateGamePlay() {
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // C キーでカメラモード切り替え
    if (input->TriggerKey(DIK_C)) {
        useFirstPersonCamera_ = !useFirstPersonCamera_;
        if (useFirstPersonCamera_ && player_) {
            // 一人称に切り替えた瞬間、プレイヤーの向きをカメラ方向に合わせる
            fpsCameraYaw_   = player_->GetRotation().y;
            fpsCameraPitch_ = 0.0f;
        }
    }

    if (!useFirstPersonCamera_) {
        // === 三人称カメラ ===
        // V キーでプレイヤー追従 / 固定カメラを切り替える
        if (input->TriggerKey(DIK_V)) {
            bool cur = gameplayCameraController_.IsFollowPlayerMode();
            gameplayCameraController_.SetFollowPlayerMode(!cur);
            if (!cur && player_) {
                // 追従モードに切り替えたらカメラピボットをプレイヤー頭部に合わせる
                Vector3 pp  = player_->GetPosition();
                pp.y       += 0.8f;
                gameplayCameraController_.SetCameraPivot(pp);
            } else if (cur && stageSelect_) {
                // 固定モードに切り替えたらステージデフォルト位置にリセット
                gameplayCameraController_.ResetCamera(
                    camera.get(), player_.get(), stageMap_, stageSelect_->GetSelectedIndex());
            }
        }
        camera->SetFov(gameplayCameraController_.GetFov());
        gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(), player_.get());

    } else {
        // === 一人称カメラ (FPS) ===
        // 画面端へのマウス移動でカメラを回転させる
        camera->SetFov(0.9f); // FPS は少し狭い FOV が自然に見える

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

                const float er  = 0.15f; // 画面端の判定領域 (全体の 15%)
                const float spd = 0.03f; // 1フレームあたりの回転量 (ラジアン)
                float le = WinApp::kWindowWidth  * er;
                float re = WinApp::kWindowWidth  * (1.0f - er);
                float te = WinApp::kWindowHeight * er;
                float be = WinApp::kWindowHeight * (1.0f - er);

                // マウスが画面左端 → 左を向く / 右端 → 右を向く
                if (mx < le) {
                    fpsCameraYaw_ += spd;
                } else if (mx > re) {
                    fpsCameraYaw_ -= spd;
                }
                // マウスが画面上端 → 上を向く / 下端 → 下を向く
                if (my < te) {
                    fpsCameraPitch_ += spd;
                } else if (my > be) {
                    fpsCameraPitch_ -= spd;
                }
            }
        }

        // キーボードでもカメラ回転できるようにする
        const float ks = 0.03f;
        if (input->PushKey(DIK_LEFT))  { fpsCameraYaw_   += ks; }
        if (input->PushKey(DIK_RIGHT)) { fpsCameraYaw_   -= ks; }
        if (input->PushKey(DIK_UP))    { fpsCameraPitch_ += ks; }
        if (input->PushKey(DIK_DOWN))  { fpsCameraPitch_ -= ks; }
        // 仰角は ±80° (約1.4rad) に制限してひっくり返らないようにする
        fpsCameraPitch_ = std::clamp(fpsCameraPitch_, -1.4f, 1.4f);

        // カメラをプレイヤーの頭部に配置
        if (player_) {
            Vector3 pp = player_->GetPosition();
            camera->SetPosition({ pp.x, pp.y + 1.2f, pp.z });
            camera->SetRotation({ fpsCameraPitch_, fpsCameraYaw_, 0.0f });
        }
        camera->Update();
    }

    // カメラガイド UI (コントローラーアイコンなど) の更新
    if (gameplayUIManager_) {
        gameplayUIManager_->UpdateCameraGuide(
            currentMode_ == AppMode::GamePlay, input.get(), winApp.get());
    }

    // チュートリアルスプライトの更新 (tutorial ステージ + インベントリを閉じているとき)
    bool invOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();
    if (stageSelect_ && stageSelect_->GetSelectedFileName() == "tutorial.txt"
        && tutorialSprite_ && !invOpen) {
        tutorialSprite_->Update();
    }
    if ((currentMode_ == AppMode::GamePlay_BlockPlace || invOpen) && placementTutorialSprite_) {
        placementTutorialSprite_->Update();
    }

    // ステージマップの更新 (ギミックの時間経過・アニメーションなど)
    float dt = 1.0f / 60.0f; // 固定タイムステップ (60fps 前提)
    totalTime_ += dt;
    stageMap_.Update(dt, player_ ? player_->GetPosition() : Vector3{ 0,0,0 });
    stageRenderer_->UpdateEffect(stageMap_); // エフェクト (波紋・光など) の更新

    // プレイヤーの更新 (入力 → 物理 → 衝突判定)
    if (player_) {
        float camRot = useFirstPersonCamera_ ? fpsCameraYaw_ : gameplayCameraController_.GetAngle();
        player_->Update(input.get(), stageMap_, camRot, lightVP, dxCommon.get());
    }

    // マップの変化 (爆発・崩壊など) があればレンダラーを再構築
    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->BuildFromStageMap(stageMap_);
        stageMap_.ClearRebuildFlag();
    }

    // リスポーン・リセット処理 (落下・タイムアウトなど)
    stageRespawnController_.Update(
        stageMap_, backupMap_, stageRenderer_.get(), player_.get(),
        &blockInventory_, &bubblePickupController_,
        &blockPlacementController_, &stageEditorController_);

    // バブル (コイン) の取得判定
    Vector3 pPos = player_ ? player_->GetPosition() : Vector3{};
    if (player_) { bubblePickupController_.Update(pPos); }

    // ゴール判定 → ゴール到達フラグを立てる
    if (Goal::Check(pPos, { 0.4f, 0.9f, 0.4f }, stageMap_)) {
        isGoalReached_ = true;
    }

    // B キーでインベントリの開閉
    if (input->TriggerKey(DIK_B) && blockInventory_.HasBlock()) {
        if (blockInventoryUI_) { blockInventoryUI_->ToggleOpen(); }
    }

    // ゴール到達でクリア画面に遷移
    if (isGoalReached_) {
        // currentMode_ = AppMode::GameClear;
    }
}

// --------------------------------------------------------
//  UpdateGamePlayBlockPlace : ブロック配置モードの更新
//
//  R キー: ブロックの回転 (90° ずつ)
//  ENTER / 左クリック: ブロックを設置
//  ESC / B キー: キャンセルしてゲームプレイに戻る
// --------------------------------------------------------
void MyGame::UpdateGamePlayBlockPlace() {
    const Int3& cursor = mapCursor_->GetIndex();

    // R キーで配置回転を 90° ずつ更新
    if (input->TriggerKey(DIK_R)) {
        placeRotationY_ += 1.5707963f; // π/2 rad = 90°
        if (placeRotationY_ >= 6.0f) {
            placeRotationY_ = 0.0f; // 360° を超えたらリセット
        }
    }

    // カーソル移動処理 (StageEditorController に委譲)
    stageEditorController_.HandleCursorInput(
        input.get(), stageMap_, mapCursor_.get(), lightCamera_.get(), camera.get());

    // インベントリで選択されているブロックタイプを取得して配置コントローラーに渡す
    BlockType selectedType     = BlockType::Ground;
    int       selectedCustomId = 0;
    if (blockInventoryUI_) {
        selectedType     = blockInventoryUI_->GetSelectedBlockType();
        selectedCustomId = blockInventoryUI_->GetSelectedCustomId();
        blockPlacementController_.SetPlaceBlockType(selectedType);
        blockPlacementController_.SetPlaceCustomId(selectedCustomId);
    }

    // リアルタイムプレビューの更新 (カーソル位置に半透明ブロックを表示)
    if (stageRenderer_) {
        stageRenderer_->SetPlacementPreview(
            stageMap_, cursor, selectedType, selectedCustomId, placeRotationY_);
    }

    // マウス左クリックのエッジ検出
    static bool prevMouse0 = false;
    bool mouseJustPressed  = input->GetMouseState().buttons[0] && !prevMouse0;
    prevMouse0             = input->GetMouseState().buttons[0];
    bool mouseTrigger      = false;
    // インベントリが開いている間はゲーム側のクリックを無効化
    if (mouseJustPressed && (!blockInventoryUI_ || !blockInventoryUI_->IsActive())) {
        mouseTrigger = true;
    }

    // ブロックの設置確定 (ENTER または 左クリック)
    if (input->TriggerKey(DIK_RETURN) || mouseTrigger) {
        if (blockPlacementController_.TryPlace(cursor, placeRotationY_)) {
            // 在庫が 0 になったら自動でゲームプレイに戻る
            bool hasRest = (selectedType == BlockType::Ground)
                || blockInventory_.HasBlock(selectedType, selectedCustomId);
            if (!hasRest) {
                currentMode_    = AppMode::GamePlay;
                placeRotationY_ = 0.0f;
                if (stageRenderer_) { stageRenderer_->ClearPlacementPreview(); }
            }
        }
    }

    // ESC / B キーでキャンセルしてゲームプレイに戻る
    if (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_B)) {
        currentMode_    = AppMode::GamePlay;
        placeRotationY_ = 0.0f;
        if (stageRenderer_) { stageRenderer_->ClearPlacementPreview(); }
    }

    // カメラ操作 (StageEditorController に委譲)
    stageEditorController_.HandleCameraInput(input.get(), camera.get());
} 

// --------------------------------------------------------
//  UpdateStageSelect : ステージ選択画面の更新
//  ステージを選んだらマップをロードしてゲームプレイに移行
// --------------------------------------------------------
void MyGame::UpdateStageSelect() {
    stageSelect_->Update();
    if (stageSelect_->IsFnished()) {
        std::string path = "Resources/Stages/" + stageSelect_->GetSelectedFileName();
        if (std::filesystem::exists(path)) {
            stageMap_.LoadFromFile(path);
            backupMap_ = stageMap_; // リスポーン / ESC でこのバックアップに戻す
            stageRenderer_->BuildFromStageMap(stageMap_);

            playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());

            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
            gameplayCameraController_.ResetCamera(
                camera.get(), player_.get(), stageMap_, stageSelect_->GetSelectedIndex());
            blockInventory_.Initialize(0); // インベントリをリセット
        }
        currentMode_ = AppMode::GamePlay;
    }
}

// --------------------------------------------------------
//  UpdateSceneTransition : ESC によるシーン遷移を処理する
//  GamePlay / GamePlay_BlockPlace 中に ESC を押すと
//  マップをバックアップから復元してステージ選択に戻る
// --------------------------------------------------------
void MyGame::UpdateSceneTransition() {
    if ((currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)
        && input->TriggerKey(DIK_ESCAPE)) {
        stageMap_ = backupMap_; // バックアップから復元 (ブロック設置も元に戻る)
        stageRenderer_->BuildFromStageMap(stageMap_);
        // コントローラーをリセットしてステージ選択に戻る
        bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        stageSelect_->Initialize(object3dCommon.get(), input.get());
        isGoalReached_ = false;
        if (player_) { player_->Respawn(); }
        currentMode_ = AppMode::StageSelect;
    }
}

void MyGame::UpdateBGM() {
    BgmType nextBgmType = BgmType::None;

    switch (currentMode_) {

    case AppMode::GamePlay:
    case AppMode::GamePlay_BlockPlace:
        nextBgmType = BgmType::Game;
        break;

    default:
        nextBgmType = BgmType::None;
        break;
    }

    if (currentBgmType_ == nextBgmType) {
        return;
    }

    sound.BGMStop();
    currentBgmType_ = nextBgmType;

    switch (currentBgmType_) {

    case BgmType::Game:
        sound.BGMPlay(gameBgmData, bgmVolume_);
        break;

    case BgmType::None:
    default:
        break;
    }
}

// ==========================================================
//  描画ヘルパー
// ==========================================================

// --------------------------------------------------------
//  DrawSkybox : スカイドームまたはスカイボックスを描画する
//  showSkyboxCubemap_ フラグで使うものを切り替える
//  描画後は PreDraw() + SRV バインドを再実行して
//  後続の描画が壊れないようにする
// --------------------------------------------------------
void MyGame::DrawSkybox(ID3D12GraphicsCommandList* commandList) {
    if (debugFlags_.showSkybox && showSkyboxCubemap_ && skybox_) {
        skybox_->Draw();
        // スカイボックス描画後に PSO / ヒープが変わるため再バインド
        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
    } else if (debugFlags_.showSkybox && skydomeObject_) {
        skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skydomeObject_->Draw();
    }
}

// --------------------------------------------------------
//  IsPlayerHiddenByWall : カメラとプレイヤーの間に壁がある場合 true を返す
//  レイを 0.8m 間隔でサンプリングして isSolid なブロックに当たれば遮蔽と判定する
//  戻り値が true のとき PreDrawPlayerHighlight() → DrawHighlight() でシルエット表示する
// --------------------------------------------------------
bool MyGame::IsPlayerHiddenByWall() const {
    if (!player_ || !camera) { return false; }

    Vector3 camPos    = camera->GetPosition();
    Vector3 playerPos = player_->GetPosition();
    playerPos.y      += 0.8f; // プレイヤーの胴体中央くらいに調整

    // カメラからプレイヤーへの方向ベクトルを正規化する
    Vector3 diff = {
        playerPos.x - camPos.x,
        playerPos.y - camPos.y,
        playerPos.z - camPos.z
    };
    float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    if (len <= 0.001f) { return false; }

    Vector3 dir = { diff.x / len, diff.y / len, diff.z / len };

    // レイを 0.8m 間隔でサンプリング (カメラの直近 0.8m と
    // プレイヤー直前 1.0m はスキップして誤判定を防ぐ)
    for (float t = 0.8f; t < len - 1.0f; t += 0.8f) {
        Vector3 cp = { camPos.x + dir.x * t, camPos.y + dir.y * t, camPos.z + dir.z * t };
        const MapCell* cell = stageMap_.GetCell(
            static_cast<int>(std::floor(cp.x + 0.5f)),
            static_cast<int>(std::floor(cp.y)),
            static_cast<int>(std::floor(cp.z + 0.5f)));
        if (cell && cell->isSolid) { return true; }
    }
    return false;
}










