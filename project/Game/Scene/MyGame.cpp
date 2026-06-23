// ==========================================================
//  MyGame.cpp
//  ゲーム全体の統括クラス実装
//
//  役割：全サブシステムの生成・接続・破棄、描画パスの制御、
//        AppMode に応じた画面遷移の管理。
//        各サブシステムの実装詳細は専用クラスに委譲する。
// ==========================================================
#include <filesystem>
#include <cmath>
#include <cstring>
#include <random>
#include "MyGame.h"
#include "EffectPresetStore.h"
#include "../Environment/WeatherPresetManager.h"
#include "Goal.h"
#include "ModelManager.h"
#include <memory>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

namespace {
const char* kEffectPresetPath = "Resources/presets/effect_presets.json";
const char* kStormPresetPath = "Resources/presets/storm_effect_presets.json";
const char* kStormShowcaseName = "Tempest Storm";

void CopyPresetName(std::array<char, 64>& buffer, const std::string& name) {
    buffer.fill('\0');
    strncpy_s(buffer.data(), buffer.size(), name.c_str(), _TRUNCATE);
}
}

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
    LoadEffectPresetNames();

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
    currentMode_           = AppMode::EffectShowcase;
    debugFlags_.showSkybox = false;
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

    if (currentMode_ == AppMode::EffectShowcase && !effectShowcasePresetNames_.empty()) {
        effectShowcaseSelectedIndex_ = 0;
        effectPreviewShowGPUParticleSphere_ = false;
    }

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
    terrainObject_->SetShininess(0.38f);
    terrainObject_->SetMetallic(0.08f);

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

        const bool leftEffectPresentation =
            (prevMode_ == AppMode::EffectPreview || prevMode_ == AppMode::EffectShowcase) &&
            (currentMode_ != AppMode::EffectPreview && currentMode_ != AppMode::EffectShowcase);
        if (leftEffectPresentation && particleManager) {
            particleManager->SetStormActive(false);
        }


        if (currentMode_ == AppMode::SkinningEditor) {
            // モデル正面に強制リセット
            camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
        } else if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
            camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
            effectShowcaseFirstPlay_ = true;
        }
        prevMode_ = currentMode_;
    }

    // --------------------------------------------------------
    // 2. ImGui の更新 (Debug ビルドのみ)
    //    BeginImGui() はフレームの先頭で必ず呼ぶこと
    // --------------------------------------------------------
    dxCommon->BeginImGui();
#ifndef NDEBUG
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

    if (input->TriggerKey(DIK_H) && particleManager) {
        Vector3 effectPos = effectPreviewPosition_;
        if (currentMode_ != AppMode::EffectPreview && currentMode_ != AppMode::EffectShowcase) {
            effectPos = player_ ? player_->GetPosition() : Vector3{ 0.0f, 0.0f, 0.0f };
            effectPos.y += 0.9f;
        }
        if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
            const bool showcaseStorm = currentMode_ == AppMode::EffectShowcase &&
                effectShowcaseSelectedIndex_ >= 0 &&
                effectShowcaseSelectedIndex_ < static_cast<int>(effectShowcasePresetNames_.size()) &&
                std::find(stormPresetNames_.begin(), stormPresetNames_.end(),
                    effectShowcasePresetNames_[effectShowcaseSelectedIndex_]) != stormPresetNames_.end();
            if ((currentMode_ == AppMode::EffectPreview && effectPreviewStormMode_) || showcaseStorm) {
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
    if (particleManager && currentMode_ != AppMode::EffectPreview && currentMode_ != AppMode::EffectShowcase) {
        particleManager->SetDrawGPUParticleSphere(true);
    }

    switch (currentMode_) {

    case AppMode::StageSelect:
        UpdateStageSelect();
        break;

    case AppMode::DebugView:
        UpdateDebugView();
        break;

    case AppMode::EffectPreview:
        UpdateEffectPreview();
        break;

    case AppMode::EffectShowcase:
        UpdateEffectShowcase();
        DrawEffectShowcaseImGui();
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
    if ((currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) && terrainObject_) {
        terrainObject_->SetCamera(view, proj);
        terrainObject_->Update(lightVP);
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
        if ((currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) &&
            particleManager->ConsumeStormLightningFlash()) {
            effectShowcaseLightTimer_ = kEffectShowcaseLightDuration_;
        }
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
    const bool isEffectPresentation = currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase;
    object3dCommon->SetLightIntensity(isEffectPresentation ? 0.18f : stageMap_.GetLightIntensity());
    object3dCommon->SetCameraPosition(camera->GetPosition()); // スペキュラー計算用

    if (isEffectPresentation) {
        const bool showcaseStorm = currentMode_ == AppMode::EffectShowcase &&
            effectShowcaseSelectedIndex_ >= 0 &&
            effectShowcaseSelectedIndex_ < static_cast<int>(effectShowcasePresetNames_.size()) &&
            std::find(stormPresetNames_.begin(), stormPresetNames_.end(),
                effectShowcasePresetNames_[effectShowcaseSelectedIndex_]) != stormPresetNames_.end();
        const bool isStorm = (currentMode_ == AppMode::EffectPreview && effectPreviewStormMode_) || showcaseStorm;
        const float remaining = std::clamp(
            effectShowcaseLightTimer_ / kEffectShowcaseLightDuration_, 0.0f, 1.0f);
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
        object3dCommon->SetPointLight(lightPosition, lightIntensity, lightColor);
    } else {
        object3dCommon->SetPointLight({ 0.0f, 0.0f, 0.0f }, 0.0f, { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // クリアカラーをステージ設定と同期 (PostProcessRenderer のオフスクリーン背景色)
    postProcess_.SetClearColor(stageMap_.GetClearColor());
    const bool stormBackdrop =
        (currentMode_ == AppMode::EffectPreview && effectPreviewStormMode_) ||
        (currentMode_ == AppMode::EffectShowcase &&
         effectShowcaseSelectedIndex_ >= 0 &&
         effectShowcaseSelectedIndex_ < static_cast<int>(effectShowcasePresetNames_.size()) &&
         std::find(stormPresetNames_.begin(), stormPresetNames_.end(),
             effectShowcasePresetNames_[effectShowcaseSelectedIndex_]) != stormPresetNames_.end());
    if (stormBackdrop) {
        dxCommon->SetClearColor(0.012f, 0.018f, 0.045f, 1.0f);
    } else {
        const Vector4& clear = stageMap_.GetClearColor();
        dxCommon->SetClearColor(clear.x, clear.y, clear.z, clear.w);
    }

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
void MyGame::DrawStormEffectEditorImGui() {
    if (!particleManager) return;
    auto& storm = particleManager->GetStormSettings();

    ImGui::TextColored(ImVec4(0.48f, 0.70f, 1.0f, 1.0f), "[ Storm Editor ]");
    if (ImGui::Button("Restart Storm", ImVec2(-1, 26))) {
        particleManager->SetStormActive(false);
        particleManager->ClearParticles();
        particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
    }

    if (ImGui::CollapsingHeader("Dark Clouds", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Cloud Area X", &storm.cloudAreaX, 0.0f, 15.0f);
        ImGui::SliderFloat("Cloud Area Z", &storm.cloudAreaZ, 0.0f, 12.0f);
        ImGui::SliderFloat("Cloud Height", &storm.cloudHeight, 1.5f, 10.0f);
        ImGui::SliderFloat("Cloud Emit Rate", &storm.cloudEmitRate, 0.5f, 20.0f);
        ImGui::SliderFloat("Cloud Life", &storm.cloudLife, 1.0f, 12.0f);
        ImGui::SliderFloat("Cloud Size", &storm.cloudSize, 0.2f, 3.0f);
        ImGui::ColorEdit4("Cloud Color", &storm.cloudColor.x);
        ImGui::Checkbox("Random Cloud Position", &storm.randomizeCloudPosition);
        ImGui::Checkbox("Random Cloud Size", &storm.randomizeCloudSize);
    }
    if (ImGui::CollapsingHeader("Rain", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Rain Area X", &storm.rainAreaX, 0.0f, 15.0f);
        ImGui::SliderFloat("Rain Area Z", &storm.rainAreaZ, 0.0f, 12.0f);
        ImGui::SliderFloat("Rain Emit Rate", &storm.rainEmitRate, 1.0f, 180.0f);
        ImGui::SliderFloat("Rain Speed", &storm.rainSpeed, 0.1f, 3.0f);
        ImGui::SliderFloat("Rain Length", &storm.rainLength, 0.2f, 3.0f);
        ImGui::ColorEdit4("Rain Color", &storm.rainColor.x);
        ImGui::Checkbox("Random Rain Position", &storm.randomizeRainPosition);
        ImGui::Checkbox("Random Rain Speed", &storm.randomizeRainSpeed);
    }
    if (ImGui::CollapsingHeader("Wind", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Wind Emit Rate", &storm.windEmitRate, 0.5f, 40.0f);
        ImGui::SliderFloat("Wind Speed", &storm.windSpeed, 0.1f, 3.0f);
        ImGui::SliderFloat("Wind Length", &storm.windLength, 0.2f, 4.0f);
        ImGui::ColorEdit4("Wind Color", &storm.windColor.x);
    }
    if (ImGui::CollapsingHeader("Lightning", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Lightning Area X", &storm.lightningAreaX, 0.0f, 12.0f);
        ImGui::SliderFloat("Lightning Area Z", &storm.lightningAreaZ, 0.0f, 10.0f);
        ImGui::SliderFloat("Lightning Frequency", &storm.lightningFrequency, 0.1f, 5.0f, "%.2fx");
        ImGui::SliderFloat("Interval Min", &storm.lightningIntervalMin, 0.15f, 5.0f);
        ImGui::SliderFloat("Interval Max", &storm.lightningIntervalMax, 0.2f, 8.0f);
        ImGui::SliderFloat("Strike Size", &storm.lightningStrikeSize, 0.25f, 3.0f, "%.2fx");
        ImGui::Checkbox("Random Strike Size", &storm.randomizeLightningSize);
        ImGui::SliderInt("Simultaneous Strikes", &storm.lightningSimultaneousCount, 1, 8);
        ImGui::SliderFloat("Simultaneous Spread", &storm.lightningSimultaneousSpread, 0.0f, 8.0f);
        ImGui::SliderInt("Burst Count", &storm.lightningBurstCount, 1, 12);
        ImGui::SliderFloat("Burst Interval", &storm.lightningBurstInterval, 0.02f, 0.8f, "%.2f sec");
        ImGui::Checkbox("Random Burst Count", &storm.randomizeLightningBurstCount);
        ImGui::Checkbox("Random Lightning Position", &storm.randomizeLightningPosition);
        ImGui::Checkbox("Random Lightning Interval", &storm.randomizeLightningInterval);
        ImGui::Checkbox("Random Lightning Direction", &storm.randomizeLightningDirection);
        ImGui::SliderInt("Bolt Count", &storm.lightningCount, 1, 12);
        ImGui::SliderInt("Segments", &storm.lightningSegments, 2, 16);
        ImGui::SliderFloat("Bolt Length", &storm.lightningLength, 1.0f, 10.0f);
        ImGui::SliderFloat("Bolt Spread", &storm.lightningSpread, 0.0f, 3.0f);
        ImGui::SliderFloat("Bolt Power", &storm.lightningPower, 0.1f, 3.0f);
        ImGui::SliderFloat("Bolt Width", &storm.lightningWidth, 0.1f, 4.0f);
        ImGui::SliderFloat("Glow Width", &storm.lightningGlowWidth, 1.0f, 8.0f);
        ImGui::SliderFloat("Glow Opacity", &storm.lightningGlowOpacity, 0.0f, 1.0f);
        ImGui::SliderInt("Branch Count", &storm.lightningBranchCount, 0, 12);
        ImGui::Checkbox("Random Branch Count", &storm.randomizeLightningBranchCount);
        ImGui::SliderFloat("Branch Length", &storm.lightningBranchLength, 0.05f, 1.0f);
        ImGui::SliderFloat("Branch Spread", &storm.lightningBranchSpread, 0.0f, 1.57f);
        ImGui::SliderFloat("Branch Width", &storm.lightningBranchWidth, 0.1f, 1.5f);
        ImGui::ColorEdit4("Lightning Color", &storm.lightningColor.x);
        ImGui::ColorEdit4("Lightning Glow", &storm.lightningGlowColor.x);
        ImGui::SliderFloat("Ground Light Power", &storm.pointLightPower, 0.0f, 24.0f);
    }

    if (ImGui::CollapsingHeader("Storm Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Include in Showcase", &stormPresetIncludeInShowcase_);
        ImGui::InputText("Storm Preset Name", stormPresetNameBuffer_.data(), stormPresetNameBuffer_.size());
        if (ImGui::Button("Save Storm Preset", ImVec2(-1, 24))) {
            SaveStormPreset(stormPresetNameBuffer_.data());
        }
        const char* selected = stormPresetSelectedIndex_ >= 0 && stormPresetSelectedIndex_ < static_cast<int>(stormPresetNames_.size())
            ? stormPresetNames_[stormPresetSelectedIndex_].c_str() : "Select storm preset";
        if (ImGui::BeginCombo("Saved Storms", selected)) {
            for (int i = 0; i < static_cast<int>(stormPresetNames_.size()); ++i) {
                const bool isSelected = i == stormPresetSelectedIndex_;
                if (ImGui::Selectable(stormPresetNames_[i].c_str(), isSelected)) {
                    stormPresetSelectedIndex_ = i;
                    CopyPresetName(stormPresetNameBuffer_, stormPresetNames_[i]);
                }
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Load Storm Preset", ImVec2(-1, 24)) && stormPresetSelectedIndex_ >= 0) {
            LoadStormPreset(stormPresetNames_[stormPresetSelectedIndex_]);
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        }
        if (ImGui::Button("Reset Storm Defaults", ImVec2(-1, 24))) {
            storm = ParticleManager::StormEffectSettings{};
        }
        ImGui::TextWrapped("%s", stormPresetStatus_.c_str());
    }
}

void MyGame::DrawEffectPreviewEditorImGui() {
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Editor ]");
    int effectType = effectPreviewStormMode_ ? 1 : 0;
    const char* effectTypes[] = { "Hit Effect", "Tempest Storm" };
    if (ImGui::Combo("Effect Type", &effectType, effectTypes, IM_ARRAYSIZE(effectTypes))) {
        effectPreviewStormMode_ = effectType == 1;
        if (particleManager) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            if (effectPreviewStormMode_) {
                particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
            }
        }
    }
    if (effectPreviewStormMode_) {
        ImGui::TextColored(ImVec4(0.52f, 0.72f, 1.0f, 1.0f), "Persistent preview: clouds / wind / rain / lightning");
        DrawStormEffectEditorImGui();
        return;
    }
    ImGui::Text("SPACE / H : Trigger");
    ImGui::Checkbox("Auto Trigger", &effectPreviewAutoPlay_);
    ImGui::Checkbox("Show GPU Sphere", &effectPreviewShowGPUParticleSphere_);
    ImGui::SliderFloat("Interval", &effectPreviewInterval_, 0.2f, 3.0f);

    if (ImGui::Button(effectPreviewStormMode_ ? "Restart Tempest Storm" : "Trigger Saber Hit", ImVec2(-1, 24)) && particleManager) {
        if (effectPreviewStormMode_) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        } else {
            EmitEffectPreviewBurst();
        }
    }
    if (ImGui::Button("Clear Particles", ImVec2(-1, 24)) && particleManager) {
        particleManager->ClearParticles();
    }

    if (ImGui::CollapsingHeader("Core Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderFloat("Size", &effectPreviewHitSettings_.size, 0.2f, 3.0f);
        ImGui::SliderFloat("Brightness", &effectPreviewHitSettings_.brightness, 0.1f, 2.5f);
        ImGui::SliderFloat("Life Scale", &effectPreviewHitSettings_.lifeScale, 0.2f, 3.0f);
        ImGui::SliderFloat("Slash Angle", &effectPreviewHitSettings_.slashAngle, -3.14f, 3.14f);
        ImGui::SliderFloat("Slash Spread", &effectPreviewHitSettings_.slashSpread, 0.2f, 3.14f);
        ImGui::Checkbox("Mirror Slash", &effectPreviewMirrorSlash_);
        ImGui::SliderInt("Burst Count", &effectPreviewBurstCount_, 1, 8);
        ImGui::SliderFloat("Burst Radius", &effectPreviewBurstRadius_, 0.0f, 2.0f);
    }

    if (ImGui::CollapsingHeader("Detail", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::SliderInt("Slash Count", &effectPreviewHitSettings_.slashCount, 1, 32);
        ImGui::SliderInt("Spark Count", &effectPreviewHitSettings_.sparkCount, 0, 160);
        ImGui::SliderFloat("Spark Speed", &effectPreviewHitSettings_.sparkSpeed, 0.1f, 3.0f);
        ImGui::SliderFloat("Spark Length", &effectPreviewHitSettings_.sparkLength, 0.1f, 3.0f);
        ImGui::SliderFloat("Scatter Radius", &effectPreviewHitSettings_.scatterRadius, 0.0f, 3.0f);
        ImGui::SliderFloat("Blue Ratio", &effectPreviewHitSettings_.blueRatio, 0.0f, 1.0f);
        ImGui::SliderFloat("Ring Power", &effectPreviewHitSettings_.ringPower, 0.0f, 3.0f);
        ImGui::SliderFloat("Core Power", &effectPreviewHitSettings_.corePower, 0.0f, 3.0f);
        ImGui::SliderFloat("Cross Power", &effectPreviewHitSettings_.crossPower, 0.0f, 3.0f);
        ImGui::SliderFloat("Pillar Power", &effectPreviewHitSettings_.pillarPower, 0.0f, 3.0f);
        ImGui::SliderInt("Main Bolt Count", &effectPreviewHitSettings_.lightningCount, 0, 12);
        ImGui::SliderInt("Lightning Segments", &effectPreviewHitSettings_.lightningSegments, 2, 8);
        ImGui::SliderFloat("Lightning Length", &effectPreviewHitSettings_.lightningLength, 0.1f, 4.0f);
        ImGui::SliderFloat("Lightning Spread", &effectPreviewHitSettings_.lightningSpread, 0.0f, 3.0f);
        ImGui::SliderFloat("Lightning Power", &effectPreviewHitSettings_.lightningPower, 0.0f, 3.0f);
        ImGui::SliderFloat("Main Bolt Width", &effectPreviewHitSettings_.lightningWidth, 0.1f, 4.0f);
        ImGui::SliderFloat("Glow Width", &effectPreviewHitSettings_.lightningGlowWidth, 1.0f, 8.0f);
        ImGui::SliderFloat("Glow Opacity", &effectPreviewHitSettings_.lightningGlowOpacity, 0.0f, 1.0f);
        ImGui::SliderInt("Branch Count", &effectPreviewHitSettings_.lightningBranchCount, 0, 12);
        ImGui::SliderFloat("Branch Length", &effectPreviewHitSettings_.lightningBranchLength, 0.05f, 1.0f);
        ImGui::SliderFloat("Branch Spread", &effectPreviewHitSettings_.lightningBranchSpread, 0.0f, 1.57f);
        ImGui::SliderFloat("Branch Width", &effectPreviewHitSettings_.lightningBranchWidth, 0.1f, 1.0f);
        const char* lightningModes[] = { "Radial", "Slash Forward", "Slash Axis", "Custom" };
        ImGui::Combo("Lightning Mode", &effectPreviewHitSettings_.lightningMode, lightningModes, 4);
        if (effectPreviewHitSettings_.lightningMode == 3) {
            ImGui::SliderFloat("Lightning Direction", &effectPreviewHitSettings_.lightningDirection, -3.14f, 3.14f);
        }
        if (effectPreviewHitSettings_.lightningMode != 0) {
            ImGui::SliderFloat("Direction Spread", &effectPreviewHitSettings_.lightningDirectionSpread, 0.0f, 1.57f);
        }
    }

    if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::ColorEdit4("Core Color", &effectPreviewHitSettings_.coreColor.x);
        ImGui::ColorEdit4("Slash Color", &effectPreviewHitSettings_.slashColor.x);
        ImGui::ColorEdit4("Spark Primary", &effectPreviewHitSettings_.sparkColor.x);
        ImGui::ColorEdit4("Spark Secondary", &effectPreviewHitSettings_.sparkSecondaryColor.x);
        ImGui::ColorEdit4("Ring Color", &effectPreviewHitSettings_.ringColor.x);
        ImGui::ColorEdit4("Cross Color", &effectPreviewHitSettings_.crossColor.x);
        ImGui::ColorEdit4("Pillar Color", &effectPreviewHitSettings_.pillarColor.x);
        ImGui::ColorEdit4("Lightning Color", &effectPreviewHitSettings_.lightningColor.x);
        ImGui::ColorEdit4("Lightning Glow", &effectPreviewHitSettings_.lightningGlowColor.x);
    }

    if (ImGui::CollapsingHeader("Randomization", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Random Position", &effectPreviewHitSettings_.randomizePosition);
        ImGui::Checkbox("Random Direction", &effectPreviewHitSettings_.randomizeDirection);
        ImGui::Checkbox("Random Angle", &effectPreviewHitSettings_.randomizeAngle);
        if (effectPreviewHitSettings_.randomizeAngle) {
            ImGui::SliderFloat("Angle Random Range", &effectPreviewHitSettings_.angleRandomRange, 0.0f, 3.14f);
        }
        ImGui::Checkbox("Random Scale", &effectPreviewHitSettings_.randomizeScale);
        ImGui::Checkbox("Random Lifetime", &effectPreviewHitSettings_.randomizeLifetime);
        ImGui::Checkbox("Random Color", &effectPreviewHitSettings_.randomizeColor);
    }

    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::Button("Saber Impact", ImVec2(140, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.25f;
            effectPreviewHitSettings_.brightness = 1.55f;
            effectPreviewHitSettings_.slashCount = 18;
            effectPreviewHitSettings_.sparkCount = 84;
            effectPreviewHitSettings_.ringPower = 1.35f;
            effectPreviewHitSettings_.corePower = 1.15f;
            effectPreviewHitSettings_.crossPower = 1.3f;
            effectPreviewHitSettings_.pillarPower = 0.8f;
            effectPreviewBurstCount_ = 2;
            effectPreviewBurstRadius_ = 0.18f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Blue Flash", ImVec2(120, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.brightness = 1.9f;
            effectPreviewHitSettings_.lifeScale = 0.85f;
            effectPreviewHitSettings_.slashSpread = 2.2f;
            effectPreviewHitSettings_.sparkCount = 112;
            effectPreviewHitSettings_.blueRatio = 0.95f;
            effectPreviewHitSettings_.corePower = 1.6f;
            effectPreviewHitSettings_.crossPower = 1.8f;
            effectPreviewHitSettings_.pillarPower = 1.2f;
            effectPreviewHitSettings_.slashColor = { 0.42f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkColor = { 0.42f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.crossColor = { 0.55f, 0.82f, 1.0f, 1.0f };
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        if (ImGui::Button("Spark Burst", ImVec2(140, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 0.9f;
            effectPreviewHitSettings_.brightness = 1.35f;
            effectPreviewHitSettings_.lifeScale = 1.45f;
            effectPreviewHitSettings_.slashCount = 7;
            effectPreviewHitSettings_.sparkCount = 150;
            effectPreviewHitSettings_.sparkSpeed = 2.15f;
            effectPreviewHitSettings_.sparkLength = 1.6f;
            effectPreviewHitSettings_.scatterRadius = 1.6f;
            effectPreviewHitSettings_.blueRatio = 0.25f;
            effectPreviewHitSettings_.ringPower = 0.65f;
            effectPreviewHitSettings_.corePower = 0.8f;
            effectPreviewHitSettings_.crossPower = 0.55f;
            effectPreviewHitSettings_.pillarPower = 0.2f;
            effectPreviewBurstCount_ = 3;
            effectPreviewBurstRadius_ = 0.35f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Heavy Hit", ImVec2(120, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.85f;
            effectPreviewHitSettings_.brightness = 1.25f;
            effectPreviewHitSettings_.lifeScale = 2.0f;
            effectPreviewHitSettings_.slashSpread = 1.05f;
            effectPreviewHitSettings_.slashCount = 13;
            effectPreviewHitSettings_.sparkCount = 44;
            effectPreviewHitSettings_.ringPower = 2.2f;
            effectPreviewHitSettings_.corePower = 1.35f;
            effectPreviewHitSettings_.crossPower = 0.85f;
            effectPreviewHitSettings_.pillarPower = 1.65f;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        if (ImGui::Button("Cinematic Finisher", ImVec2(-1, 28))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 2.15f;
            effectPreviewHitSettings_.brightness = 2.15f;
            effectPreviewHitSettings_.lifeScale = 1.55f;
            effectPreviewHitSettings_.slashAngle = -0.42f;
            effectPreviewHitSettings_.slashSpread = 2.75f;
            effectPreviewHitSettings_.slashCount = 30;
            effectPreviewHitSettings_.sparkCount = 160;
            effectPreviewHitSettings_.sparkSpeed = 2.45f;
            effectPreviewHitSettings_.sparkLength = 2.2f;
            effectPreviewHitSettings_.scatterRadius = 1.35f;
            effectPreviewHitSettings_.blueRatio = 0.72f;
            effectPreviewHitSettings_.ringPower = 2.8f;
            effectPreviewHitSettings_.corePower = 2.25f;
            effectPreviewHitSettings_.crossPower = 2.65f;
            effectPreviewHitSettings_.pillarPower = 1.45f;
            effectPreviewHitSettings_.slashColor = { 0.38f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkColor = { 0.38f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkSecondaryColor = { 1.0f, 0.48f, 0.10f, 1.0f };
            effectPreviewHitSettings_.ringColor = { 0.38f, 0.78f, 1.0f, 1.0f };
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 4;
            effectPreviewBurstRadius_ = 0.28f;
            EmitEffectPreviewBurst();
        }
        if (ImGui::Button("Lightning Slash", ImVec2(-1, 28))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.55f;
            effectPreviewHitSettings_.brightness = 2.05f;
            effectPreviewHitSettings_.lifeScale = 0.95f;
            effectPreviewHitSettings_.slashAngle = -0.28f;
            effectPreviewHitSettings_.slashSpread = 0.55f;
            effectPreviewHitSettings_.slashCount = 1;
            effectPreviewHitSettings_.sparkCount = 92;
            effectPreviewHitSettings_.sparkSpeed = 1.85f;
            effectPreviewHitSettings_.sparkLength = 1.75f;
            effectPreviewHitSettings_.scatterRadius = 1.05f;
            effectPreviewHitSettings_.blueRatio = 0.88f;
            effectPreviewHitSettings_.ringPower = 1.25f;
            effectPreviewHitSettings_.corePower = 1.55f;
            effectPreviewHitSettings_.crossPower = 1.35f;
            effectPreviewHitSettings_.pillarPower = 0.45f;
            effectPreviewHitSettings_.lightningCount = 3;
            effectPreviewHitSettings_.lightningSegments = 5;
            effectPreviewHitSettings_.lightningLength = 2.15f;
            effectPreviewHitSettings_.lightningSpread = 1.55f;
            effectPreviewHitSettings_.lightningPower = 1.55f;
            effectPreviewHitSettings_.lightningWidth = 2.2f;
            effectPreviewHitSettings_.lightningGlowWidth = 3.6f;
            effectPreviewHitSettings_.lightningGlowOpacity = 0.28f;
            effectPreviewHitSettings_.lightningBranchCount = 4;
            effectPreviewHitSettings_.lightningBranchLength = 0.38f;
            effectPreviewHitSettings_.lightningBranchSpread = 0.72f;
            effectPreviewHitSettings_.lightningBranchWidth = 0.38f;
            effectPreviewHitSettings_.lightningMode = 2;
            effectPreviewHitSettings_.lightningDirectionSpread = 0.28f;
            effectPreviewHitSettings_.lightningColor = { 0.40f, 0.86f, 1.0f, 1.0f };
            effectPreviewHitSettings_.lightningGlowColor = { 0.24f, 0.28f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkColor = { 0.40f, 0.86f, 1.0f, 1.0f };
            effectPreviewHitSettings_.sparkSecondaryColor = { 1.0f, 0.86f, 0.40f, 1.0f };
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 2;
            effectPreviewBurstRadius_ = 0.16f;
            EmitEffectPreviewBurst();
        }
        if (ImGui::Button("Shock Ring", ImVec2(140, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.35f;
            effectPreviewHitSettings_.brightness = 1.75f;
            effectPreviewHitSettings_.lifeScale = 1.35f;
            effectPreviewHitSettings_.slashCount = 1;
            effectPreviewHitSettings_.slashSpread = 0.2f;
            effectPreviewHitSettings_.sparkCount = 32;
            effectPreviewHitSettings_.sparkSpeed = 0.75f;
            effectPreviewHitSettings_.sparkLength = 0.9f;
            effectPreviewHitSettings_.scatterRadius = 0.45f;
            effectPreviewHitSettings_.ringPower = 3.0f;
            effectPreviewHitSettings_.corePower = 1.25f;
            effectPreviewHitSettings_.crossPower = 0.15f;
            effectPreviewHitSettings_.pillarPower = 0.15f;
            effectPreviewHitSettings_.lightningCount = 6;
            effectPreviewHitSettings_.lightningSegments = 6;
            effectPreviewHitSettings_.lightningLength = 0.8f;
            effectPreviewHitSettings_.lightningSpread = 2.4f;
            effectPreviewHitSettings_.lightningPower = 0.75f;
            effectPreviewHitSettings_.lightningMode = 0;
            effectPreviewHitSettings_.lightningColor = { 0.52f, 0.92f, 1.0f, 1.0f };
            effectPreviewHitSettings_.ringColor = { 0.52f, 0.92f, 1.0f, 1.0f };
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
            EmitEffectPreviewBurst();
        }
        ImGui::SameLine();
        if (ImGui::Button("Thin Cut", ImVec2(120, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.2f;
            effectPreviewHitSettings_.brightness = 2.2f;
            effectPreviewHitSettings_.lifeScale = 0.62f;
            effectPreviewHitSettings_.slashAngle = -0.64f;
            effectPreviewHitSettings_.slashSpread = 0.2f;
            effectPreviewHitSettings_.slashCount = 1;
            effectPreviewHitSettings_.sparkCount = 24;
            effectPreviewHitSettings_.sparkSpeed = 1.25f;
            effectPreviewHitSettings_.sparkLength = 1.25f;
            effectPreviewHitSettings_.scatterRadius = 0.25f;
            effectPreviewHitSettings_.blueRatio = 1.0f;
            effectPreviewHitSettings_.ringPower = 0.35f;
            effectPreviewHitSettings_.corePower = 0.75f;
            effectPreviewHitSettings_.crossPower = 0.25f;
            effectPreviewHitSettings_.pillarPower = 0.0f;
            effectPreviewHitSettings_.lightningCount = 2;
            effectPreviewHitSettings_.lightningSegments = 3;
            effectPreviewHitSettings_.lightningLength = 1.15f;
            effectPreviewHitSettings_.lightningSpread = 0.55f;
            effectPreviewHitSettings_.lightningPower = 0.55f;
            effectPreviewHitSettings_.lightningMode = 1;
            effectPreviewHitSettings_.lightningDirectionSpread = 0.12f;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
            EmitEffectPreviewBurst();
        }
    }

    if (ImGui::CollapsingHeader("Saved Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Include in Showcase", &effectPresetIncludeInShowcase_);
        ImGui::InputText("Preset Name", effectPresetNameBuffer_.data(), effectPresetNameBuffer_.size());
        if (ImGui::Button("Save Preset", ImVec2(135, 24))) {
            SaveEffectPreset(effectPresetNameBuffer_.data());
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh", ImVec2(100, 24))) {
            LoadEffectPresetNames();
        }

        const char* selectedPresetName = effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())
            ? effectPresetNames_[effectPresetSelectedIndex_].c_str()
            : "Select saved preset";
        if (ImGui::BeginCombo("Saved", selectedPresetName)) {
            for (int i = 0; i < static_cast<int>(effectPresetNames_.size()); ++i) {
                bool selected = effectPresetSelectedIndex_ == i;
                if (ImGui::Selectable(effectPresetNames_[i].c_str(), selected)) {
                    effectPresetSelectedIndex_ = i;
                    CopyPresetName(effectPresetNameBuffer_, effectPresetNames_[i]);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Load Selected", ImVec2(135, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                LoadEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                effectPresetStatus_ = "Preset: nothing selected";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Over", ImVec2(100, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                SaveEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                SaveEffectPreset(effectPresetNameBuffer_.data());
            }
        }
        ImGui::TextWrapped("%s", effectPresetStatus_.c_str());
    }

    if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Position", &effectPreviewPosition_.x, 0.05f, -20.0f, 20.0f);
        if (ImGui::Button("Focus Camera", ImVec2(-1, 24))) {
            camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
        }
        if (ImGui::Button("Reset Tuning", ImVec2(-1, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
    }
}

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

    const bool isStageToolMode = (currentMode_ == AppMode::StageEditor ||
                                  currentMode_ == AppMode::GamePlay_BlockPlace);

    // AppMode の選択 (コンボボックス)
    if (ImGui::CollapsingHeader("Hierarchy / Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        int modeIndex = 0;
        switch (currentMode_) {
        case AppMode::DebugView:      modeIndex = 0; break;
        case AppMode::StageEditor:    modeIndex = 1; break;
        case AppMode::GamePlay:       modeIndex = 2; break;
        case AppMode::SkinningEditor: modeIndex = 3; break;
        case AppMode::EffectPreview:  modeIndex = 4; break;
        case AppMode::EffectShowcase: modeIndex = 5; break;
        default:                                     break;
        }
        const char* modeNames[] = { "DebugView", "StageEditor", "GamePlay", "SkinningEditor", "EffectPreview", "EffectShowcase" };
        if (ImGui::Combo("App Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
            switch (modeIndex) {
            case 0: currentMode_ = AppMode::DebugView;      break;
            case 1: currentMode_ = AppMode::StageEditor;    break;
            case 2: currentMode_ = AppMode::GamePlay;       break;
            case 3:
                currentMode_ = AppMode::SkinningEditor;
                camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
                break;
            case 4:
                currentMode_ = AppMode::EffectPreview;
                camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
                break;
            case 5:
                currentMode_ = AppMode::EffectShowcase;
                camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
                break;
            }
        }
        if (currentMode_ == AppMode::EffectPreview) {
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Effect only viewport");
            ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);
        } else {
            // デバッグ表示のオン/オフ切り替え
            ImGui::Checkbox("Show 3D Objects",      &debugFlags_.show3DObjects);
            ImGui::Checkbox("Show Terrain",          &debugFlags_.showTerrain);
            ImGui::Checkbox("Show Skybox",           &debugFlags_.showSkybox);
            ImGui::Checkbox("Show Skybox (Cubemap)", &showSkyboxCubemap_);
            if (currentMode_ == AppMode::DebugView) {
                ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
            }
            ImGui::Checkbox("Show Particles",        &debugFlags_.showParticles);
        }
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

    if (isStageToolMode) {
        if (ImGui::CollapsingHeader("StageMap Info")) {
            stageMap_.DrawImGui();
        }
        if (ImGui::CollapsingHeader("Cursor Info")) {
            mapCursor_->DrawImGui();
        }
    }

    ImGui::End(); // 左パネルここまで

    // ========================================================
    // 右パネル (Stage Editor) - StageEditorController に委譲
    // ========================================================
    if (isStageToolMode) {
        stageEditorController_.DrawImGui(stageMap_, stageRenderer_.get(), mapCursor_.get(), player_.get());
    }

    if (currentMode_ == AppMode::EffectPreview) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelW, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelW, io.DisplaySize.y - botH), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f);
        ImGui::Begin("Effect Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        DrawEffectPreviewEditorImGui();
        ImGui::End();
    }

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

    } else if (currentMode_ == AppMode::EffectPreview) {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Preview ]");
        ImGui::Text("Effect Type: %s", effectPreviewStormMode_ ? "Tempest Storm" : "Hit Effect");
        ImGui::Text("Editor controls are on the right panel.");
        ImGui::Text("SPACE / H : Trigger Saber Hit");
        ImGui::Text("Saved preset: %s", effectPresetNameBuffer_.data());
        ImGui::Text("GPU Sphere: %s", effectPreviewShowGPUParticleSphere_ ? "ON" : "OFF");
        if (ImGui::Button("Trigger Saber Hit", ImVec2(180, 24)) && particleManager) {
            EmitEffectPreviewBurst();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Particles", ImVec2(140, 24)) && particleManager) {
            particleManager->ClearParticles();
        }

    } else if (false && currentMode_ == AppMode::EffectPreview) {
        ImGui::Columns(2, "EffectPreviewColumns", false);
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Preview ]");
        ImGui::Text("SPACE / H : Trigger Saber Hit");
        ImGui::Checkbox("Auto Trigger", &effectPreviewAutoPlay_);
        ImGui::Checkbox("Show GPU Particle Sphere", &effectPreviewShowGPUParticleSphere_);
        ImGui::SliderFloat("Interval", &effectPreviewInterval_, 0.2f, 3.0f);
        if (ImGui::Button("Trigger Saber Hit", ImVec2(180, 24)) && particleManager) {
            EmitEffectPreviewBurst();
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Particles", ImVec2(140, 24)) && particleManager) {
            particleManager->ClearParticles();
        }
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.55f, 0.9f, 1.0f, 1.0f), "[ Hit Effect Tuning ]");
        ImGui::SliderFloat("Size", &effectPreviewHitSettings_.size, 0.2f, 3.0f);
        ImGui::SliderFloat("Brightness", &effectPreviewHitSettings_.brightness, 0.1f, 2.5f);
        ImGui::SliderFloat("Life Scale", &effectPreviewHitSettings_.lifeScale, 0.2f, 3.0f);
        ImGui::SliderFloat("Slash Angle", &effectPreviewHitSettings_.slashAngle, -3.14f, 3.14f);
        ImGui::SliderFloat("Slash Spread", &effectPreviewHitSettings_.slashSpread, 0.2f, 3.14f);
        ImGui::Checkbox("Mirror Slash", &effectPreviewMirrorSlash_);
        ImGui::SliderInt("Burst Count", &effectPreviewBurstCount_, 1, 8);
        ImGui::SliderFloat("Burst Radius", &effectPreviewBurstRadius_, 0.0f, 2.0f);
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.35f, 1.0f), "[ Presets ]");
        if (ImGui::Button("Saber Impact", ImVec2(120, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.25f;
            effectPreviewHitSettings_.brightness = 1.55f;
            effectPreviewHitSettings_.slashCount = 18;
            effectPreviewHitSettings_.sparkCount = 84;
            effectPreviewHitSettings_.ringPower = 1.35f;
            effectPreviewHitSettings_.corePower = 1.15f;
            effectPreviewHitSettings_.crossPower = 1.3f;
            effectPreviewHitSettings_.pillarPower = 0.8f;
            effectPreviewBurstCount_ = 2;
            effectPreviewBurstRadius_ = 0.18f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Blue Flash", ImVec2(110, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.brightness = 1.9f;
            effectPreviewHitSettings_.lifeScale = 0.85f;
            effectPreviewHitSettings_.slashSpread = 2.2f;
            effectPreviewHitSettings_.sparkCount = 112;
            effectPreviewHitSettings_.blueRatio = 0.95f;
            effectPreviewHitSettings_.corePower = 1.6f;
            effectPreviewHitSettings_.crossPower = 1.8f;
            effectPreviewHitSettings_.pillarPower = 1.2f;
            effectPreviewHitSettings_.coolColor = { 0.42f, 0.78f, 1.0f, 1.0f };
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        if (ImGui::Button("Spark Burst", ImVec2(120, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 0.9f;
            effectPreviewHitSettings_.brightness = 1.35f;
            effectPreviewHitSettings_.lifeScale = 1.45f;
            effectPreviewHitSettings_.slashCount = 7;
            effectPreviewHitSettings_.sparkCount = 150;
            effectPreviewHitSettings_.sparkSpeed = 2.15f;
            effectPreviewHitSettings_.sparkLength = 1.6f;
            effectPreviewHitSettings_.scatterRadius = 1.6f;
            effectPreviewHitSettings_.blueRatio = 0.25f;
            effectPreviewHitSettings_.ringPower = 0.65f;
            effectPreviewHitSettings_.corePower = 0.8f;
            effectPreviewHitSettings_.crossPower = 0.55f;
            effectPreviewHitSettings_.pillarPower = 0.2f;
            effectPreviewBurstCount_ = 3;
            effectPreviewBurstRadius_ = 0.35f;
        }
        ImGui::SameLine();
        if (ImGui::Button("Heavy Hit", ImVec2(110, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 1.85f;
            effectPreviewHitSettings_.brightness = 1.25f;
            effectPreviewHitSettings_.lifeScale = 2.0f;
            effectPreviewHitSettings_.slashSpread = 1.05f;
            effectPreviewHitSettings_.slashCount = 13;
            effectPreviewHitSettings_.sparkCount = 44;
            effectPreviewHitSettings_.ringPower = 2.2f;
            effectPreviewHitSettings_.corePower = 1.35f;
            effectPreviewHitSettings_.crossPower = 0.85f;
            effectPreviewHitSettings_.pillarPower = 1.65f;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        if (ImGui::Button("Cinematic Finisher", ImVec2(240, 28))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewHitSettings_.size = 2.15f;
            effectPreviewHitSettings_.brightness = 2.15f;
            effectPreviewHitSettings_.lifeScale = 1.55f;
            effectPreviewHitSettings_.slashAngle = -0.42f;
            effectPreviewHitSettings_.slashSpread = 2.75f;
            effectPreviewHitSettings_.slashCount = 30;
            effectPreviewHitSettings_.sparkCount = 160;
            effectPreviewHitSettings_.sparkSpeed = 2.45f;
            effectPreviewHitSettings_.sparkLength = 2.2f;
            effectPreviewHitSettings_.scatterRadius = 1.35f;
            effectPreviewHitSettings_.blueRatio = 0.72f;
            effectPreviewHitSettings_.ringPower = 2.8f;
            effectPreviewHitSettings_.corePower = 2.25f;
            effectPreviewHitSettings_.crossPower = 2.65f;
            effectPreviewHitSettings_.pillarPower = 1.45f;
            effectPreviewHitSettings_.coolColor = { 0.38f, 0.78f, 1.0f, 1.0f };
            effectPreviewHitSettings_.warmColor = { 1.0f, 0.48f, 0.10f, 1.0f };
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 4;
            effectPreviewBurstRadius_ = 0.28f;
            EmitEffectPreviewBurst();
        }
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.50f, 1.0f, 0.55f, 1.0f), "[ Saved Presets ]");
        ImGui::InputText("Preset Name", effectPresetNameBuffer_.data(), effectPresetNameBuffer_.size());
        if (ImGui::Button("Save Preset", ImVec2(120, 24))) {
            SaveEffectPreset(effectPresetNameBuffer_.data());
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh List", ImVec2(120, 24))) {
            LoadEffectPresetNames();
        }

        const char* selectedPresetName = effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())
            ? effectPresetNames_[effectPresetSelectedIndex_].c_str()
            : "Select saved preset";
        if (ImGui::BeginCombo("Saved Presets", selectedPresetName)) {
            for (int i = 0; i < static_cast<int>(effectPresetNames_.size()); ++i) {
                bool selected = effectPresetSelectedIndex_ == i;
                if (ImGui::Selectable(effectPresetNames_[i].c_str(), selected)) {
                    effectPresetSelectedIndex_ = i;
                    CopyPresetName(effectPresetNameBuffer_, effectPresetNames_[i]);
                }
                if (selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Button("Load Selected", ImVec2(120, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                LoadEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                effectPresetStatus_ = "Preset: nothing selected";
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Over", ImVec2(120, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                SaveEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                SaveEffectPreset(effectPresetNameBuffer_.data());
            }
        }
        ImGui::TextUnformatted(effectPresetStatus_.c_str());

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.25f, 1.0f), "[ Spawn Transform ]");
        ImGui::DragFloat3("Position", &effectPreviewPosition_.x, 0.05f, -20.0f, 20.0f);
        if (ImGui::Button("Focus Camera", ImVec2(160, 24))) {
            camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
        }
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "[ Detail ]");
        ImGui::SliderInt("Slash Count", &effectPreviewHitSettings_.slashCount, 3, 32);
        ImGui::SliderInt("Spark Count", &effectPreviewHitSettings_.sparkCount, 0, 160);
        ImGui::SliderFloat("Spark Speed", &effectPreviewHitSettings_.sparkSpeed, 0.1f, 3.0f);
        ImGui::SliderFloat("Spark Length", &effectPreviewHitSettings_.sparkLength, 0.1f, 3.0f);
        ImGui::SliderFloat("Scatter Radius", &effectPreviewHitSettings_.scatterRadius, 0.0f, 3.0f);
        ImGui::SliderFloat("Blue Ratio", &effectPreviewHitSettings_.blueRatio, 0.0f, 1.0f);
        ImGui::SliderFloat("Ring Power", &effectPreviewHitSettings_.ringPower, 0.0f, 3.0f);
        ImGui::SliderFloat("Core Power", &effectPreviewHitSettings_.corePower, 0.0f, 3.0f);
        ImGui::SliderFloat("Cross Power", &effectPreviewHitSettings_.crossPower, 0.0f, 3.0f);
        ImGui::SliderFloat("Pillar Power", &effectPreviewHitSettings_.pillarPower, 0.0f, 3.0f);
        ImGui::ColorEdit3("Cool Color", &effectPreviewHitSettings_.coolColor.x);
        ImGui::ColorEdit3("Warm Color", &effectPreviewHitSettings_.warmColor.x);
        if (ImGui::Button("Reset Tuning", ImVec2(160, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        ImGui::Text("Particles are forced visible in this mode.");
        ImGui::Columns(1);

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

    } else if (currentMode_ == AppMode::GamePlay_BlockPlace) {
        ImGui::Columns(2, "BlockPlaceColumns", false);

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Block Placement ]");
        ImGui::Text("W/A/S/D : Move Cursor");
        ImGui::Text("Q/E     : Cursor Up/Down");
        ImGui::Text("ENTER   : Place Block");
        ImGui::Text("R       : Rotate Block");
        ImGui::Text("ESC / B : Cancel");

        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Placement State ]");
        const Int3& cursor = mapCursor_->GetIndex();
        ImGui::Text("Cursor: X:%d Y:%d Z:%d", cursor.x, cursor.y, cursor.z);
        if (blockInventoryUI_) {
            ImGui::Text("Selected: %s", BlockTypeToString(blockInventoryUI_->GetSelectedBlockType()));
        }
        ImGui::Text("Rotation Y: %.1f deg", placeRotationY_ * 57.29578f);
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::EffectShowcase) {
        const int presetCount = static_cast<int>(effectShowcasePresetNames_.size());
        const int safeIndex = presetCount > 0
            ? std::clamp(effectShowcaseSelectedIndex_, 0, presetCount - 1)
            : 0;
        const char* presetName = presetCount > 0
            ? effectShowcasePresetNames_[safeIndex].c_str()
            : "No showcase presets";

        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Showcase ]");
        ImGui::Text("Preset: %02d / %02d  %s", presetCount > 0 ? safeIndex + 1 : 0, presetCount, presetName);
        ImGui::Text("LEFT / RIGHT : Select    SPACE : Replay    A : Auto Play [%s]    TAB : Game",
            effectShowcaseAutoPlay_ ? "ON" : "OFF");
        ImGui::Text("MMB Drag : Orbit    Shift + MMB : Pan    Mouse Wheel : Zoom");

    } else if (currentMode_ == AppMode::StageSelect) {
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "[ Stage Select ]");
        ImGui::Text("Choose a stage from the center view.");
        ImGui::Text("Only stage selection UI is active in this mode.");

    } else if (currentMode_ == AppMode::DebugView) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Debug View ]");
        ImGui::Text("General object, terrain, sprite, and particle checks.");

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

    } else {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Application Status ]");
        ImGui::Text("No mode-specific tools.");
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
        dxCommon->PreDraw(false);                              // バックバッファを RT にセット
        postProcess_.DrawToBackBuffer(commandList, camera->GetProjectionMatrix()); // 全画面コピー描画
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
    dxCommon->EndImGui();
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
    if (!debugFlags_.show3DObjects && currentMode_ != AppMode::EffectPreview && currentMode_ != AppMode::EffectShowcase) {
        return;
    }

    // テクスチャ SRV ヒープをバインドし、シャドウマップ SRV をスロット 4 にセット
    ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
    object3dCommon->PreDraw(); // 通常描画用 RootSignature / PSO をバインド
    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());

    if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        if (terrainObject_) {
            terrainObject_->Draw();
        }
        if (debugFlags_.showParticles) {
            ID3D12DescriptorHeap* ph[] = { textureManager->GetSrvHeap() };
            commandList->SetDescriptorHeaps(1, ph);
            particleManager->Draw();
        }
        return;
    }

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
                           currentMode_ == AppMode::GamePlay_BlockPlace ||
                           currentMode_ == AppMode::EffectPreview);

        // ゲーム系モードではステージブロックを描画する
        if (isGameMode && stageRenderer_) {
            stageRenderer_->Draw();             // 不透明ブロック
            stageRenderer_->DrawTransparent();  // 半透明ブロック (壁の透明化など)
            // DrawTransparent() の後は PSO が変わるため再バインドが必要
            object3dCommon->PreDraw();
            commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
        }

        if (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::EffectPreview) {
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
            if (currentMode_ == AppMode::GamePlay && gameplayUIManager_) {
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

    dxCommon->FinalizeImGui();

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

void MyGame::LoadStormPresetNames() {
    const EffectPresetStore store;
    const auto result = store.LoadStormPresetNames(kStormPresetPath, kStormShowcaseName);
    stormPresetNames_ = result.all;
    stormShowcasePresetNames_ = result.showcase;
    stormPresetSelectedIndex_ = -1;
    if (!result.status.empty()) {
        stormPresetStatus_ = result.status;
    }
}

bool MyGame::SaveStormPreset(const std::string& name) {
    if (name.empty() || !particleManager) {
        stormPresetStatus_ = "Storm preset: invalid name";
        return false;
    }

    EffectPresetStore::StormPreset preset;
    preset.settings = particleManager->GetStormSettings();
    preset.includeInShowcase = stormPresetIncludeInShowcase_;

    const EffectPresetStore store;
    if (!store.SaveStormPreset(kStormPresetPath, name, preset, stormPresetStatus_)) {
        return false;
    }

    LoadEffectPresetNames();
    for (int i = 0; i < static_cast<int>(stormPresetNames_.size()); ++i) {
        if (stormPresetNames_[i] == name) {
            stormPresetSelectedIndex_ = i;
            break;
        }
    }
    CopyPresetName(stormPresetNameBuffer_, name);
    return true;
}

bool MyGame::LoadStormPreset(const std::string& name) {
    if (!particleManager) return false;

    EffectPresetStore::StormPreset preset;
    preset.settings = particleManager->GetStormSettings();
    preset.includeInShowcase = stormPresetIncludeInShowcase_;

    const EffectPresetStore store;
    if (!store.LoadStormPreset(kStormPresetPath, kStormShowcaseName, name, preset, stormPresetStatus_)) {
        return false;
    }

    particleManager->GetStormSettings() = preset.settings;
    stormPresetIncludeInShowcase_ = preset.includeInShowcase;
    CopyPresetName(stormPresetNameBuffer_, name);
    return true;
}

void MyGame::LoadEffectPresetNames() {
    LoadStormPresetNames();

    const EffectPresetStore store;
    const auto result = store.LoadHitPresetNames(kEffectPresetPath);
    effectPresetNames_ = result.all;
    effectShowcasePresetNames_ = stormShowcasePresetNames_;
    effectShowcasePresetNames_.insert(effectShowcasePresetNames_.end(), result.showcase.begin(), result.showcase.end());
    effectPresetSelectedIndex_ = -1;
    effectPresetStatus_ = result.status;
}

bool MyGame::SaveEffectPreset(const std::string& name) {
    EffectPresetStore::HitPreset preset;
    preset.settings = effectPreviewHitSettings_;
    preset.showGpuSphere = effectPreviewShowGPUParticleSphere_;
    preset.mirrorSlash = effectPreviewMirrorSlash_;
    preset.burstCount = effectPreviewBurstCount_;
    preset.burstRadius = effectPreviewBurstRadius_;
    preset.includeInShowcase = effectPresetIncludeInShowcase_;

    const EffectPresetStore store;
    if (!store.SaveHitPreset(kEffectPresetPath, name, preset, effectPresetStatus_)) {
        return false;
    }

    LoadEffectPresetNames();
    for (int i = 0; i < static_cast<int>(effectPresetNames_.size()); ++i) {
        if (effectPresetNames_[i] == name) {
            effectPresetSelectedIndex_ = i;
            break;
        }
    }
    return true;
}

bool MyGame::LoadEffectPreset(const std::string& name) {
    EffectPresetStore::HitPreset preset;
    preset.settings = effectPreviewHitSettings_;
    preset.showGpuSphere = effectPreviewShowGPUParticleSphere_;
    preset.mirrorSlash = effectPreviewMirrorSlash_;
    preset.burstCount = effectPreviewBurstCount_;
    preset.burstRadius = effectPreviewBurstRadius_;
    preset.includeInShowcase = effectPresetIncludeInShowcase_;

    const EffectPresetStore store;
    if (!store.LoadHitPreset(kEffectPresetPath, name, preset, effectPresetStatus_)) {
        return false;
    }

    effectPreviewHitSettings_ = preset.settings;
    effectPreviewShowGPUParticleSphere_ = preset.showGpuSphere;
    effectPreviewMirrorSlash_ = preset.mirrorSlash;
    effectPreviewBurstCount_ = preset.burstCount;
    effectPreviewBurstRadius_ = preset.burstRadius;
    effectPresetIncludeInShowcase_ = preset.includeInShowcase;

    if (currentMode_ == AppMode::EffectPreview) {
        effectPreviewStormMode_ = false;
        if (particleManager) {
            particleManager->SetStormActive(false);
        }
    }

    CopyPresetName(effectPresetNameBuffer_, name);
    return true;
}

void MyGame::EmitEffectPreviewBurst() {
    if (!particleManager) {
        return;
    }

    if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        effectShowcaseLightTimer_ = kEffectShowcaseLightDuration_;
    }

    ParticleManager::HitEffectSettings settings = effectPreviewHitSettings_;
    if (effectPreviewMirrorSlash_) {
        settings.slashAngle = -settings.slashAngle;
    }

    const int burstCount = effectPreviewBurstCount_ < 1 ? 1 : effectPreviewBurstCount_;
    constexpr float kPi = 3.14159265f;
    static std::mt19937 randomEngine(std::random_device{}());
    for (int i = 0; i < burstCount; ++i) {
        Vector3 pos = effectPreviewPosition_;
        if (burstCount > 1 && effectPreviewBurstRadius_ > 0.0f) {
            const float angle = (static_cast<float>(i) / static_cast<float>(burstCount)) * kPi * 2.0f;
            pos.x += std::cos(angle) * effectPreviewBurstRadius_;
            pos.y += std::sin(angle * 1.7f) * effectPreviewBurstRadius_ * 0.35f;
            pos.z += std::sin(angle) * effectPreviewBurstRadius_;
        }

        ParticleManager::HitEffectSettings burstSettings = settings;
        if (burstSettings.randomizeAngle && burstSettings.angleRandomRange > 0.0f) {
            std::uniform_real_distribution<float> angleOffset(-burstSettings.angleRandomRange, burstSettings.angleRandomRange);
            const float randomAngle = angleOffset(randomEngine);
            burstSettings.slashAngle += randomAngle;
            burstSettings.lightningDirection += randomAngle;
        }
        const float burstT = burstCount <= 1 ? 0.0f : static_cast<float>(i) / static_cast<float>(burstCount - 1);
        burstSettings.size *= 1.0f - 0.12f * burstT;
        burstSettings.brightness *= 1.0f - 0.10f * burstT;
        burstSettings.slashAngle += (static_cast<float>(i) - static_cast<float>(burstCount - 1) * 0.5f) * 0.22f;
        burstSettings.slashSpread += 0.18f * burstT;
        burstSettings.sparkSpeed *= 1.0f + 0.20f * burstT;
        burstSettings.ringPower *= 1.0f + 0.18f * burstT;
        particleManager->EmitHitEffect(pos, burstSettings);
    }
}

void MyGame::UpdateEffectPreview() {
    debugFlags_.showParticles = true;
    effectShowcaseLightTimer_ = (std::max)(0.0f, effectShowcaseLightTimer_ - 1.0f / 60.0f);
    if (particleManager) {
        particleManager->SetDrawGPUParticleSphere(effectPreviewStormMode_ ? false : effectPreviewShowGPUParticleSphere_);
        if (effectPreviewStormMode_ && !particleManager->IsStormActive()) {
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        } else if (!effectPreviewStormMode_ && particleManager->IsStormActive()) {
            particleManager->SetStormActive(false);
        }
    }

    if (input->TriggerKey(DIK_SPACE) && particleManager) {
        if (effectPreviewStormMode_) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        } else {
            EmitEffectPreviewBurst();
        }
    }

    if (!effectPreviewStormMode_ && effectPreviewAutoPlay_ && particleManager) {
        constexpr float kDeltaTime = 1.0f / 60.0f;
        effectPreviewTimer_ += kDeltaTime;
        if (effectPreviewTimer_ >= effectPreviewInterval_) {
            effectPreviewTimer_ = 0.0f;
            EmitEffectPreviewBurst();
        }
    } else {
        effectPreviewTimer_ = 0.0f;
    }
}

void MyGame::UpdateEffectShowcase() {
    debugFlags_.showParticles = true;
    debugFlags_.showSkybox = false;
    if (particleManager) {
        particleManager->SetDrawGPUParticleSphere(false);
    }
    effectShowcaseLightTimer_ = (std::max)(0.0f, effectShowcaseLightTimer_ - 1.0f / 60.0f);

    const int presetCount = static_cast<int>(effectShowcasePresetNames_.size());
    auto isStormIndex = [&](int index) {
        if (index < 0 || index >= presetCount) return false;
        return std::find(stormPresetNames_.begin(), stormPresetNames_.end(), effectShowcasePresetNames_[index]) != stormPresetNames_.end();
    };
    auto selectPreset = [&](int index, bool play) {
        if (presetCount <= 0) {
            return;
        }
        effectShowcaseSelectedIndex_ = (index % presetCount + presetCount) % presetCount;
        effectPreviewShowGPUParticleSphere_ = false;
        effectShowcaseTimer_ = 0.0f;
        if (particleManager) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
        }
        if (isStormIndex(effectShowcaseSelectedIndex_)) {
            LoadStormPreset(effectShowcasePresetNames_[effectShowcaseSelectedIndex_]);
            if (particleManager) {
                particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
            }
        } else {
            LoadEffectPreset(effectShowcasePresetNames_[effectShowcaseSelectedIndex_]);
        }
        if (play && !isStormIndex(effectShowcaseSelectedIndex_)) {
            EmitEffectPreviewBurst();
        }
    };

    if (effectShowcaseFirstPlay_) {
        effectShowcaseFirstPlay_ = false;
        selectPreset(effectShowcaseSelectedIndex_, true);
    }
    if (input->TriggerKey(DIK_LEFT)) {
        selectPreset(effectShowcaseSelectedIndex_ - 1, true);
    }
    if (input->TriggerKey(DIK_RIGHT)) {
        selectPreset(effectShowcaseSelectedIndex_ + 1, true);
    }
    if (input->TriggerKey(DIK_SPACE)) {
        if (particleManager) {
            particleManager->ClearParticles();
        }
        if (isStormIndex(effectShowcaseSelectedIndex_)) {
            if (particleManager) {
                particleManager->SetStormActive(false);
                particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
            }
        } else {
            EmitEffectPreviewBurst();
        }
        effectShowcaseTimer_ = 0.0f;
    }
    if (input->TriggerKey(DIK_A)) {
        effectShowcaseAutoPlay_ = !effectShowcaseAutoPlay_;
        effectShowcaseTimer_ = 0.0f;
    }
    if (input->TriggerKey(DIK_R)) {
        LoadEffectPresetNames();
        effectShowcaseSelectedIndex_ = 0;
        effectShowcaseFirstPlay_ = true;
        return;
    }
    if (input->TriggerKey(DIK_TAB)) {
        if (particleManager) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
        }
        stageSelect_->Initialize(object3dCommon.get(), input.get());
        currentMode_ = AppMode::StageSelect;
        return;
    }

    if (effectShowcaseAutoPlay_ && presetCount > 0) {
        effectShowcaseTimer_ += 1.0f / 60.0f;
        const float displayInterval = isStormIndex(effectShowcaseSelectedIndex_)
            ? 10.0f
            : effectShowcaseInterval_;
        if (effectShowcaseTimer_ >= displayInterval) {
            selectPreset(effectShowcaseSelectedIndex_ + 1, true);
        }
    }
}

void MyGame::DrawEffectShowcaseImGui() {
    const ImGuiIO& io = ImGui::GetIO();
    const int presetCount = static_cast<int>(effectShowcasePresetNames_.size());
    const int safeIndex = presetCount > 0
        ? std::clamp(effectShowcaseSelectedIndex_, 0, presetCount - 1)
        : 0;
    const char* presetName = presetCount > 0
        ? effectShowcasePresetNames_[safeIndex].c_str()
        : "No showcase presets";

#ifdef NDEBUG
    const float headerX = 24.0f;
    const float headerWidth = io.DisplaySize.x - 48.0f;
#else
    // Keep the editor's 320 px information panel intact.
    const float headerX = 344.0f;
    const float headerWidth = io.DisplaySize.x - headerX - 24.0f;
#endif
    ImGui::SetNextWindowPos(ImVec2(headerX, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(headerWidth, 94.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    ImGui::Begin("Effect Showcase Header", nullptr, flags);
    ImGui::SetWindowFontScale(1.45f);
    ImGui::TextColored(ImVec4(0.42f, 0.86f, 1.0f, 1.0f), "EFFECT SHOWCASE");
    ImGui::SetWindowFontScale(1.15f);
    if (presetCount > 0) {
        ImGui::Text("%02d / %02d    %s", safeIndex + 1, presetCount, presetName);
    } else {
        ImGui::TextUnformatted(presetName);
    }
    ImGui::End();

#ifdef NDEBUG
    // Debug builds use the existing Tools & Controls panel for this guide.
    ImGui::SetNextWindowPos(ImVec2(24.0f, io.DisplaySize.y - 72.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x - 48.0f, 48.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("Effect Showcase Controls", nullptr, flags);
    ImGui::Text("LEFT / RIGHT : Select     SPACE : Replay     A : Auto Play [%s]     TAB : Game",
        effectShowcaseAutoPlay_ ? "ON" : "OFF");
    ImGui::SameLine();
    ImGui::TextUnformatted("     MMB : Orbit     Shift+MMB : Pan     Wheel : Zoom");
    ImGui::End();
#endif
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










