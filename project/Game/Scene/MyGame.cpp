//  MyGame.cpp
//  ゲーム全体の初期化、更新、終了処理をまとめる司令塔クラスの実装。
//  描画やゲームプレイ固有の処理は専用クラスへ委譲する。
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>
#include <iterator>
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

struct EditorLayout {
    float leftPanelWidth = 320.0f;
    float rightPanelWidth = 320.0f;
    float bottomPanelHeight = 360.0f;
    float mainPanelHeight = 720.0f;
};

struct PostEffectHotKey {
    BYTE key;
    int mode;
    const char* keyLabel;
    const char* effectName;
};

constexpr PostEffectHotKey kPostEffectHotKeys[] = {
    { DIK_1, 1, "1", "Grayscale" },
    { DIK_2, 3, "2", "Vignetting" },
    { DIK_3, 6, "3", "GaussianFilter / Smoothing" },
    { DIK_4, 4, "4", "BoxFilter 3x3" },
    { DIK_5, 5, "5", "BoxFilter 5x5" },
    { DIK_6, 7, "6", "LuminanceBasedOutline" },
    { DIK_7, 8, "7", "DepthBasedOutline" },
    { DIK_8, 9, "8", "RadialBlur" },
    { DIK_9, 10, "9", "Dissolve" },
    { DIK_0, 11, "0", "Random" },
};

const char* GetPostEffectShowcaseName(int mode) {
    for (const PostEffectHotKey& hotKey : kPostEffectHotKeys) {
        if (hotKey.mode == mode) {
            return hotKey.effectName;
        }
    }
    switch (mode) {
    case 2: return "Sepia";
    default: return "Normal";
    }
}

void CopyPresetName(std::array<char, 64>& buffer, const std::string& name) {
    buffer.fill('\0');
    strncpy_s(buffer.data(), buffer.size(), name.c_str(), _TRUNCATE);
}

EditorLayout MakeEditorLayout(const ImVec2& displaySize) {
    EditorLayout layout;
    const float width = (std::max)(displaySize.x, 1.0f);
    const float height = (std::max)(displaySize.y, 1.0f);

    float sidePanel = std::clamp(width * 0.18f, 300.0f, 380.0f);
    if (width < 1360.0f) {
        sidePanel = std::clamp(width * 0.22f, 260.0f, 320.0f);
    }

    const float minimumViewportWidth = 560.0f;
    if (width - sidePanel * 2.0f < minimumViewportWidth) {
        sidePanel = (std::max)(220.0f, (width - minimumViewportWidth) * 0.5f);
    }

    float bottomPanel = std::clamp(height * 0.32f, 280.0f, 420.0f);
    if (height < 820.0f) {
        bottomPanel = std::clamp(height * 0.28f, 220.0f, 320.0f);
    }

    layout.leftPanelWidth = sidePanel;
    layout.rightPanelWidth = sidePanel;
    layout.bottomPanelHeight = bottomPanel;
    layout.mainPanelHeight = (std::max)(220.0f, height - bottomPanel);
    return layout;
}
}

//  MyGame::Initialize
//  エンジン基盤、シーン、モデル、UI、ステージ関連の初期化を行う。

void MyGame::Initialize() {
    WeatherPresetManager::GetInstance().LoadPresets();
    LoadEffectPresetNames();

    // ウィンドウ、DirectX、入力などエンジン基盤を先に用意する。
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


    stageSelect_ = std::make_unique<StageSelect>();
    stageSelect_->Initialize(object3dCommon.get(), input.get());

    // デバッグ表示やプレイヤーで共有する基本モデルを読み込む。
    // index 0: block / index 1: axis / index 2: player(OBJ)
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/block",  "block.obj",  textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/axis",   "axis.obj",   textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/player", "player.obj", textureManager.get())));


    Object3d* debugFloor = CreateObject(models[0].get(), { -25.0f, 0.0f, 0.0f });
    debugFloor->SetScale({ 10.0f, 1.0f, 10.0f });
    debugFloor->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d* debugAxisA = CreateObject(models[1].get(), { -23.0f, 0.0f, 0.0f });
    debugAxisA->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    Object3d* debugAxisB = CreateObject(models[1].get(), { -27.0f, 0.0f, 0.0f });
    debugAxisB->SetEnvironmentCoefficient(debugObjectEnvironmentCoefficient_);

    // DebugView 用の確認スプライト。
    uint32_t texHandle = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), texHandle);


    sound.Initialize();
    
    
    gameBgmData = sound.SoundLoadFile("Resources/Sound/gamePlay.mp3");
    
    // プレイヤー本体とメインカメラを生成する。
    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon.get(), models[2].get());
    player_->SetPosition({ 0.0f, 1.5f, 0.0f });

    
    camera = std::make_unique<Camera>();
#ifndef NDEBUG
    camera->SetAspectRatio(1280.0f / 720.0f);
#endif


    stageMap_.Initialize(100, 100, 100);

    // ビルド設定に応じて初期モードと初期ステージを切り替える。
#ifdef DEVELOPMENT
    currentMode_           = AppMode::DebugView;
    debugFlags_.showSkybox = false;
    postProcess_.SetEnabled(false);
    if (std::filesystem::exists("Resources/Stages/stage1.txt")) {
        stageMap_.LoadFromFile("Resources/Stages/stage1.txt");
        playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());
    }
#elif defined(NDEBUG)
    currentMode_           = AppMode::PostEffectShowcase;
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

    // 背景用のスカイドームと環境マップ用のスカイボックスを準備する。
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

    // ステージ描画とブロック選択カーソルを初期化する。
    stageRenderer_ = std::make_unique<StageRenderer>();
    stageRenderer_->Initialize(object3dCommon.get());
    stageRenderer_->SetBlockScale({ 1.0f, 1.0f, 1.0f });
    stageRenderer_->BuildFromStageMap(stageMap_);

    mapCursor_ = std::make_unique<MapCursor>();
    mapCursor_->Initialize(object3dCommon.get());
    mapCursor_->SetIndex({ 0, 0, 0 }, stageMap_);
    mapCursor_->SetScale({ 0.9f, 0.9f, 0.9f });

    // シャドウマップ用のライト視点を作る。
    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon.get(), textureManager.get());

    lightCamera_ = std::make_unique<LightCamera>();
    lightCamera_->Initialize();

    // ステージ上の収集物とブロック配置を管理するコントローラー。
    bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
    blockPlacementController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);


    gameplayUIManager_ = std::make_unique<GameplayUIManager>();
    gameplayUIManager_->Initialize(dxCommon.get(), textureManager.get(), spriteCommon.get(), object3dCommon.get());

    blockInventory_.Initialize(0);

    // ゲームプレイ中に使う UI とチュートリアル画像。
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

    // SkinningEditor / Terrain / PostProcess は重いので、必要なモードへ入った時に初期化する。
    sceneManager_ = std::make_unique<SceneManager>();
    sceneManager_->Initialize(&sceneFactory_, GetCurrentSceneType(), *this);
}


//  生成した Object3d は objectList が所有する。

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

void MyGame::EnsureSkinningEditorInitialized() {
    if (skinningEditorInitialized_) {
        return;
    }
    skinningEditor_.Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    skinningEditorInitialized_ = true;
}

void MyGame::EnsureTerrainInitialized() {
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

void MyGame::EnsurePostProcessInitialized() {
    if (postProcessInitialized_) {
        return;
    }
    postProcess_.Initialize(dxCommon.get(), stageMap_.GetClearColor());
    postProcessInitialized_ = true;
}

void MyGame::OnSceneEntered(SceneType sceneType) {
    switch (sceneType) {
    case SceneType::StageSelect:
        currentMode_ = AppMode::StageSelect;
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
}

void MyGame::OnSceneExited(SceneType sceneType) {
    if ((sceneType == SceneType::EffectPreview || sceneType == SceneType::EffectShowcase) && particleManager) {
        particleManager->SetStormActive(false);
    }
}

void MyGame::RequestSceneChange(SceneType sceneType) {
    if (sceneType == SceneType::StageSelect) {
        currentMode_ = AppMode::StageSelect;
    } else if (sceneType == SceneType::DebugView) {
        currentMode_ = AppMode::DebugView;
    } else if (sceneType == SceneType::StageEditor) {
        currentMode_ = AppMode::StageEditor;
    } else if (sceneType == SceneType::GamePlay) {
        currentMode_ = AppMode::GamePlay;
    } else if (sceneType == SceneType::GamePlayBlockPlace) {
        currentMode_ = AppMode::GamePlay_BlockPlace;
    } else if (sceneType == SceneType::SkinningEditor) {
        currentMode_ = AppMode::SkinningEditor;
    } else if (sceneType == SceneType::EffectPreview) {
        currentMode_ = AppMode::EffectPreview;
    } else if (sceneType == SceneType::EffectShowcase) {
        currentMode_ = AppMode::EffectShowcase;
    } else if (sceneType == SceneType::PostEffectShowcase) {
        currentMode_ = AppMode::PostEffectShowcase;
    }
}

SceneType MyGame::GetCurrentSceneType() const {
    if (currentMode_ == AppMode::StageSelect) {
        return SceneType::StageSelect;
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

void MyGame::RunStageSelectScene() {
    UpdateStageSelect();
}

void MyGame::RunDebugViewScene() {
    UpdateDebugView();
}

void MyGame::RunStageEditorScene() {
    stageEditorController_.Update(
        input.get(), stageMap_, stageRenderer_.get(),
        mapCursor_.get(), lightCamera_.get(), player_.get(), camera.get());
}

void MyGame::RunGamePlayScene() {
    UpdateGamePlay();
}

void MyGame::RunGamePlayBlockPlaceScene() {
    UpdateGamePlayBlockPlace();
}

void MyGame::RunSkinningEditorScene(const SceneUpdateContext& context) {
    EnsureSkinningEditorInitialized();
    skinningEditor_.Update(
        dxCommon.get(), input.get(), camera.get(),
        context.lightViewProjection, context.isGuiCaptured, particleManager.get());
}

void MyGame::RunEffectPreviewScene() {
    UpdateEffectPreview();
}

void MyGame::RunEffectShowcaseScene() {
    UpdateEffectShowcase();
    DrawEffectShowcaseImGui();
}

void MyGame::RunPostEffectShowcaseScene() {
    UpdatePostEffectShowcase();
    DrawPostEffectShowcaseImGui();
}

//  MyGame::Update
//  モード遷移、入力、カメラ、ライト、UI などフレーム単位の共通更新を行う。


void MyGame::Update() {


    // モードが変わった瞬間だけ、BGM やカメラ初期位置を切り替える。
    HandleModeChange();


    // Debug ビルドでは各モード更新より前に ImGui を組み立てる。
    BeginFrameImGui();


    // 入力を取り込み、ゲームプレイ中の ESC 遷移を先に処理する。
    input->Update();
    UpdateSceneTransition();


    // ImGui のフレーム情報と表示サイズを取得する。
    bool isGuiCaptured = IsGuiCapturingMouse();


    // ステージ設定のライト方向を使って、シャドウ用のライトカメラを更新する。
    Vector3 lightDir = UpdateLightCameraForFrame();
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // H キーは現在のモードに合わせてヒットエフェクトを再生する。
    UpdateHitEffectShortcut();


    // GamePlay mode has its own camera handling in UpdateGamePlay().
    UpdateSharedCameraControls(isGuiCaptured);


    // 背景オブジェクトは常にカメラ位置へ追従させる。
    UpdateBackgroundObjects();


    // EffectPreview/Showcase 以外では GPU パーティクル確認球を表示可能に戻す。
    UpdateParticleDebugVisibility();

    // AppMode ごとの本体処理はサブルーチンや専用クラスへ委譲する。
    UpdateCurrentMode(lightVP, isGuiCaptured);


    // ここで最終的なカメラ行列を確定し、描画対象へ共有する。
    camera->Update();

    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();


    UpdatePlayerCameraAndTransform(view, proj, lightVP);


    // ウィンドウが非アクティブなら、ゲーム側の重い更新を止める。
    if (IsWindowInactive()) {
        return;
    }


    // DebugView と Effect 表示では補助 3D オブジェクトも更新する。
    UpdateDebugAndEffectObjects(view, proj, lightVP);


    // ステージメッシュ、壁透過、カーソルなどステージ周りの表示を更新する。
    UpdateStagePresentation(view, proj, lightVP);


    // 天候プリセットに合わせてパーティクル設定を同期する。
    UpdateWeatherParticles(view, proj);


    // ステージライトとエフェクト用ポイントライトを描画共通へ反映する。
    ApplySceneLighting(lightDir);


    // 背景色はステージ設定を基本に、嵐演出中だけ暗い色へ寄せる。
    UpdateClearColorForFrame();


    // ゲームプレイ UI とインベントリ UI を更新する。
    UpdateGameplayUserInterface();
}

void MyGame::HandleModeChange() {
    if (currentMode_ == prevMode_) {
        return;
    }

    UpdateBGM();

    const bool leftEffectPresentation =
        (prevMode_ == AppMode::EffectPreview || prevMode_ == AppMode::EffectShowcase) &&
        (currentMode_ != AppMode::EffectPreview && currentMode_ != AppMode::EffectShowcase);
    if (leftEffectPresentation && particleManager) {
        particleManager->SetStormActive(false);
    }

    if (currentMode_ == AppMode::SkinningEditor) {
        EnsureSkinningEditorInitialized();
        camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
    } else if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        EnsureTerrainInitialized();
        camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
        effectShowcaseFirstPlay_ = true;
    } else if (currentMode_ == AppMode::PostEffectShowcase) {
        EnsurePostProcessInitialized();
        EnsureTerrainInitialized();
        camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 7.0f, { 0.35f, 0.0f, 0.0f });
    }

    prevMode_ = currentMode_;
}

void MyGame::BeginFrameImGui() {
    dxCommon->BeginImGui();
#ifndef NDEBUG
    UpdateImGui();
#endif
}

bool MyGame::IsGuiCapturingMouse() {
#ifndef NDEBUG
    return ImGui::GetIO().WantCaptureMouse;
#else
    return false;
#endif
}

Vector3 MyGame::UpdateLightCameraForFrame() {
    Vector3 lightDir = stageMap_.GetLightDirection();
    if (lightCamera_) {
        const Vector3 targetPos = player_ ? player_->GetPosition() : camera->GetPosition();
        lightCamera_->Update(lightDir, targetPos);
    }
    return lightDir;
}

void MyGame::UpdateHitEffectShortcut() {
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

void MyGame::UpdateSharedCameraControls(bool isGuiCaptured) {
    if (currentMode_ != AppMode::GamePlay) {
        camera->UpdateBlenderStyle(input.get(), isGuiCaptured, winApp->GetHwnd());
    }
}

void MyGame::UpdateBackgroundObjects() {
    if (skydomeObject_ && debugFlags_.showSkybox && !showSkyboxCubemap_) {
        skydomeObject_->SetPosition(camera->GetPosition());
        skydomeObject_->Update(Math::MakeIdentity4x4());
    }
    if (skybox_ && debugFlags_.showSkybox && showSkyboxCubemap_) {
        skybox_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skybox_->Update(camera->GetPosition());
    }
}

void MyGame::UpdateParticleDebugVisibility() {
    if (particleManager &&
        currentMode_ != AppMode::EffectPreview &&
        currentMode_ != AppMode::EffectShowcase &&
        currentMode_ != AppMode::PostEffectShowcase) {
        particleManager->SetDrawGPUParticleSphere(true);
    }
}

void MyGame::UpdateCurrentMode(const Matrix4x4& lightVP, bool isGuiCaptured) {
    if (!sceneManager_) {
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

void MyGame::UpdatePlayerCameraAndTransform(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    if (!player_) {
        return;
    }

    player_->SetCamera(view, proj);
    if (currentMode_ != AppMode::GamePlay) {
        player_->UpdateTransform(lightVP);
    }
}

bool MyGame::IsWindowInactive() {
    return GetActiveWindow() != winApp->GetHwnd();
}

void MyGame::UpdateDebugAndEffectObjects(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    if (debugFlags_.show3DObjects && currentMode_ == AppMode::DebugView) {
        for (auto& obj : objectList) {
            if (obj) {
                obj->SetCamera(view, proj);
                obj->Update(lightVP);
            }
        }

        if (debugFlags_.showTerrain) {
            EnsureTerrainInitialized();
            terrainObject_->SetCamera(view, proj);
            terrainObject_->Update(lightVP);
        }
    }

    if ((currentMode_ == AppMode::EffectPreview ||
         currentMode_ == AppMode::EffectShowcase ||
         currentMode_ == AppMode::PostEffectShowcase)) {
        EnsureTerrainInitialized();
        terrainObject_->SetCamera(view, proj);
        terrainObject_->Update(lightVP);
    }
}

void MyGame::UpdateStagePresentation(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP) {
    if (stageRenderer_) {
        stageRenderer_->SetIsEditorMode(currentMode_ == AppMode::StageEditor);
        stageRenderer_->SetCamera(view, proj);
        stageRenderer_->Update(stageMap_, lightVP);
    }

    if (stageRenderer_ && player_) {
        stageRenderer_->UpdateCloudTransparency(
            camera->GetPosition(),
            player_->GetPosition()
        );
    }

    if (mapCursor_ && (currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        mapCursor_->SetCamera(view, proj);
        mapCursor_->Update(lightVP);
    }
}

void MyGame::UpdateWeatherParticles(const Matrix4x4& view, const Matrix4x4& proj) {
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        sprite->Update();
    }

    if (!debugFlags_.showParticles) {
        return;
    }

    auto& wpMgr = WeatherPresetManager::GetInstance();
    const std::string& weatherPresetName = stageMap_.GetWeatherPresetName();
    WeatherPreset* currentPreset = wpMgr.GetPresetByName(weatherPresetName);
    if (currentPreset) {
        auto& emitter = particleManager->GetWeatherEmitter();
        emitter.active = currentPreset->particleEnabled;
        if (emitter.active) {
            if (cachedWeatherPresetName_ != weatherPresetName ||
                cachedWeatherParticleTexturePath_ != currentPreset->particleTexture) {
                cachedWeatherPresetName_ = weatherPresetName;
                cachedWeatherParticleTexturePath_ = currentPreset->particleTexture;
                cachedWeatherParticleTexture_ = textureManager->LoadTexture(currentPreset->particleTexture);
                if (cachedWeatherParticleTexture_ != 0) {
                    particleManager->SetTexture(cachedWeatherParticleTexture_);
                }
            }
            emitter.emitRate = currentPreset->emitRate;
            emitter.size = currentPreset->emitSize;
            emitter.velocity = currentPreset->velocity;
            emitter.velocityRandom = currentPreset->velocityRandom;
            emitter.particleSize = currentPreset->particleSize;
            emitter.particleLife = currentPreset->particleLife;
            emitter.color = currentPreset->particleColor;

            emitter.center = { 0.0f, 15.0f, 0.0f };
        }
    }

    particleManager->Update(1.0f / 60.0f, view, proj, player_ ? player_->GetPosition() : Vector3{ 0, 0, 0 }, &stageMap_);
    if ((currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) &&
        particleManager->ConsumeStormLightningFlash()) {
        effectShowcaseLightTimer_ = kEffectShowcaseLightDuration_;
    }
}

void MyGame::ApplySceneLighting(const Vector3& lightDir) {
    object3dCommon->SetLightDirection(lightDir);
    object3dCommon->SetLightColor(Vector4(
        stageMap_.GetLightColor().x,
        stageMap_.GetLightColor().y,
        stageMap_.GetLightColor().z, 1.0f));
    const bool isEffectPresentation = currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase;
    object3dCommon->SetLightIntensity(isEffectPresentation ? 0.18f : stageMap_.GetLightIntensity());
    object3dCommon->SetCameraPosition(camera->GetPosition());

    if (isEffectPresentation) {
        const bool isStorm = IsCurrentEffectStorm();
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
}

void MyGame::UpdateClearColorForFrame() {
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

void MyGame::UpdateGameplayUserInterface() {
    if (gameplayUIManager_) {
        gameplayUIManager_->Update(
            currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace,
            player_.get(), camera.get(), lightCamera_.get());
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
}

//  MyGame::UpdateImGui [Debug ビルドのみ]


#ifndef NDEBUG
void MyGame::DrawStormEffectEditorImGui() {
    if (!particleManager) return;
    auto& storm = particleManager->GetStormSettings();

    // ImGui の UI 要素を表示・更新する。
    ImGui::TextColored(ImVec4(0.48f, 0.70f, 1.0f, 1.0f), "[ Storm Editor ]");
    // ImGui ボタン「Restart Storm」を表示し、押されたら処理する。
    if (ImGui::Button("Restart Storm", ImVec2(-1, 26))) {
        particleManager->SetStormActive(false);
        particleManager->ClearParticles();
        particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
    }

    // ImGui セクション「Dark Clouds」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Dark Clouds", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui スライダー「Cloud Area X」で小数値を調整する。
        ImGui::SliderFloat("Cloud Area X", &storm.cloudAreaX, 0.0f, 15.0f);
        // ImGui スライダー「Cloud Area Z」で小数値を調整する。
        ImGui::SliderFloat("Cloud Area Z", &storm.cloudAreaZ, 0.0f, 12.0f);
        // ImGui スライダー「Cloud Height」で小数値を調整する。
        ImGui::SliderFloat("Cloud Height", &storm.cloudHeight, 1.5f, 10.0f);
        // ImGui スライダー「Cloud Emit Rate」で小数値を調整する。
        ImGui::SliderFloat("Cloud Emit Rate", &storm.cloudEmitRate, 0.5f, 20.0f);
        // ImGui スライダー「Cloud Life」で小数値を調整する。
        ImGui::SliderFloat("Cloud Life", &storm.cloudLife, 1.0f, 12.0f);
        // ImGui スライダー「Cloud Size」で小数値を調整する。
        ImGui::SliderFloat("Cloud Size", &storm.cloudSize, 0.2f, 3.0f);
        // ImGui カラー編集「Cloud Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Cloud Color", &storm.cloudColor.x);
        // ImGui チェックボックス「Random Cloud Position」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Cloud Position", &storm.randomizeCloudPosition);
        // ImGui チェックボックス「Random Cloud Size」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Cloud Size", &storm.randomizeCloudSize);
    }
    // ImGui セクション「Rain」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Rain", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui スライダー「Rain Area X」で小数値を調整する。
        ImGui::SliderFloat("Rain Area X", &storm.rainAreaX, 0.0f, 15.0f);
        // ImGui スライダー「Rain Area Z」で小数値を調整する。
        ImGui::SliderFloat("Rain Area Z", &storm.rainAreaZ, 0.0f, 12.0f);
        // ImGui スライダー「Rain Emit Rate」で小数値を調整する。
        ImGui::SliderFloat("Rain Emit Rate", &storm.rainEmitRate, 1.0f, 180.0f);
        // ImGui スライダー「Rain Speed」で小数値を調整する。
        ImGui::SliderFloat("Rain Speed", &storm.rainSpeed, 0.1f, 3.0f);
        // ImGui スライダー「Rain Length」で小数値を調整する。
        ImGui::SliderFloat("Rain Length", &storm.rainLength, 0.2f, 3.0f);
        // ImGui カラー編集「Rain Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Rain Color", &storm.rainColor.x);
        // ImGui チェックボックス「Random Rain Position」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Rain Position", &storm.randomizeRainPosition);
        // ImGui チェックボックス「Random Rain Speed」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Rain Speed", &storm.randomizeRainSpeed);
    }
    // ImGui セクション「Wind」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Wind", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui スライダー「Wind Emit Rate」で小数値を調整する。
        ImGui::SliderFloat("Wind Emit Rate", &storm.windEmitRate, 0.5f, 40.0f);
        // ImGui スライダー「Wind Speed」で小数値を調整する。
        ImGui::SliderFloat("Wind Speed", &storm.windSpeed, 0.1f, 3.0f);
        // ImGui スライダー「Wind Length」で小数値を調整する。
        ImGui::SliderFloat("Wind Length", &storm.windLength, 0.2f, 4.0f);
        // ImGui カラー編集「Wind Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Wind Color", &storm.windColor.x);
    }
    // ImGui セクション「Lightning」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Lightning", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui スライダー「Lightning Area X」で小数値を調整する。
        ImGui::SliderFloat("Lightning Area X", &storm.lightningAreaX, 0.0f, 12.0f);
        // ImGui スライダー「Lightning Area Z」で小数値を調整する。
        ImGui::SliderFloat("Lightning Area Z", &storm.lightningAreaZ, 0.0f, 10.0f);
        // ImGui スライダー「Lightning Frequency」で小数値を調整する。
        ImGui::SliderFloat("Lightning Frequency", &storm.lightningFrequency, 0.1f, 5.0f, "%.2fx");
        // ImGui スライダー「Interval Min」で小数値を調整する。
        ImGui::SliderFloat("Interval Min", &storm.lightningIntervalMin, 0.15f, 5.0f);
        // ImGui スライダー「Interval Max」で小数値を調整する。
        ImGui::SliderFloat("Interval Max", &storm.lightningIntervalMax, 0.2f, 8.0f);
        // ImGui スライダー「Strike Size」で小数値を調整する。
        ImGui::SliderFloat("Strike Size", &storm.lightningStrikeSize, 0.25f, 3.0f, "%.2fx");
        // ImGui チェックボックス「Random Strike Size」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Strike Size", &storm.randomizeLightningSize);
        // ImGui スライダー「Simultaneous Strikes」で整数値を調整する。
        ImGui::SliderInt("Simultaneous Strikes", &storm.lightningSimultaneousCount, 1, 8);
        // ImGui スライダー「Simultaneous Spread」で小数値を調整する。
        ImGui::SliderFloat("Simultaneous Spread", &storm.lightningSimultaneousSpread, 0.0f, 8.0f);
        // ImGui スライダー「Burst Count」で整数値を調整する。
        ImGui::SliderInt("Burst Count", &storm.lightningBurstCount, 1, 12);
        // ImGui スライダー「Burst Interval」で小数値を調整する。
        ImGui::SliderFloat("Burst Interval", &storm.lightningBurstInterval, 0.02f, 0.8f, "%.2f sec");
        // ImGui チェックボックス「Random Burst Count」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Burst Count", &storm.randomizeLightningBurstCount);
        // ImGui チェックボックス「Random Lightning Position」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Lightning Position", &storm.randomizeLightningPosition);
        // ImGui チェックボックス「Random Lightning Interval」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Lightning Interval", &storm.randomizeLightningInterval);
        // ImGui チェックボックス「Random Lightning Direction」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Lightning Direction", &storm.randomizeLightningDirection);
        // ImGui スライダー「Bolt Count」で整数値を調整する。
        ImGui::SliderInt("Bolt Count", &storm.lightningCount, 1, 12);
        // ImGui スライダー「Segments」で整数値を調整する。
        ImGui::SliderInt("Segments", &storm.lightningSegments, 2, 16);
        // ImGui スライダー「Bolt Length」で小数値を調整する。
        ImGui::SliderFloat("Bolt Length", &storm.lightningLength, 1.0f, 10.0f);
        // ImGui スライダー「Bolt Spread」で小数値を調整する。
        ImGui::SliderFloat("Bolt Spread", &storm.lightningSpread, 0.0f, 3.0f);
        // ImGui スライダー「Bolt Power」で小数値を調整する。
        ImGui::SliderFloat("Bolt Power", &storm.lightningPower, 0.1f, 3.0f);
        // ImGui スライダー「Bolt Width」で小数値を調整する。
        ImGui::SliderFloat("Bolt Width", &storm.lightningWidth, 0.1f, 4.0f);
        // ImGui スライダー「Glow Width」で小数値を調整する。
        ImGui::SliderFloat("Glow Width", &storm.lightningGlowWidth, 1.0f, 8.0f);
        // ImGui スライダー「Glow Opacity」で小数値を調整する。
        ImGui::SliderFloat("Glow Opacity", &storm.lightningGlowOpacity, 0.0f, 1.0f);
        // ImGui スライダー「Branch Count」で整数値を調整する。
        ImGui::SliderInt("Branch Count", &storm.lightningBranchCount, 0, 12);
        // ImGui チェックボックス「Random Branch Count」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Branch Count", &storm.randomizeLightningBranchCount);
        // ImGui スライダー「Branch Length」で小数値を調整する。
        ImGui::SliderFloat("Branch Length", &storm.lightningBranchLength, 0.05f, 1.0f);
        // ImGui スライダー「Branch Spread」で小数値を調整する。
        ImGui::SliderFloat("Branch Spread", &storm.lightningBranchSpread, 0.0f, 1.57f);
        // ImGui スライダー「Branch Width」で小数値を調整する。
        ImGui::SliderFloat("Branch Width", &storm.lightningBranchWidth, 0.1f, 1.5f);
        // ImGui カラー編集「Lightning Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Lightning Color", &storm.lightningColor.x);
        // ImGui カラー編集「Lightning Glow」で RGBA 色を調整する。
        ImGui::ColorEdit4("Lightning Glow", &storm.lightningGlowColor.x);
        // ImGui スライダー「Ground Light Power」で小数値を調整する。
        ImGui::SliderFloat("Ground Light Power", &storm.pointLightPower, 0.0f, 24.0f);
    }

    // ImGui セクション「Storm Presets」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Storm Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui チェックボックス「Include in Showcase」で ON/OFF を切り替える。
        ImGui::Checkbox("Include in Showcase", &stormPresetIncludeInShowcase_);
        // ImGui 入力欄「Storm Preset Name」で名前や文字列を編集する。
        ImGui::InputText("Storm Preset Name", stormPresetNameBuffer_.data(), stormPresetNameBuffer_.size());
        // ImGui ボタン「Save Storm Preset」を表示し、押されたら処理する。
        if (ImGui::Button("Save Storm Preset", ImVec2(-1, 24))) {
            SaveStormPreset(stormPresetNameBuffer_.data());
        }
        const char* selected = stormPresetSelectedIndex_ >= 0 && stormPresetSelectedIndex_ < static_cast<int>(stormPresetNames_.size())
            ? stormPresetNames_[stormPresetSelectedIndex_].c_str() : "Select storm preset";
        // ImGui コンボ「Saved Storms」の選択リストを開始する。
        if (ImGui::BeginCombo("Saved Storms", selected)) {
            for (int i = 0; i < static_cast<int>(stormPresetNames_.size()); ++i) {
                const bool isSelected = i == stormPresetSelectedIndex_;
                // ImGui の選択項目を表示し、選ばれたら選択状態を更新する。
                if (ImGui::Selectable(stormPresetNames_[i].c_str(), isSelected)) {
                    stormPresetSelectedIndex_ = i;
                    CopyPresetName(stormPresetNameBuffer_, stormPresetNames_[i]);
                }
                // 現在選択中の ImGui 項目へ既定フォーカスを当てる。
                if (isSelected) ImGui::SetItemDefaultFocus();
            }
            // ImGui コンボの選択リストを閉じる。
            ImGui::EndCombo();
        }
        // ImGui ボタン「Load Storm Preset」を表示し、押されたら処理する。
        if (ImGui::Button("Load Storm Preset", ImVec2(-1, 24)) && stormPresetSelectedIndex_ >= 0) {
            LoadStormPreset(stormPresetNames_[stormPresetSelectedIndex_]);
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        }
        // ImGui ボタン「Reset Storm Defaults」を表示し、押されたら処理する。
        if (ImGui::Button("Reset Storm Defaults", ImVec2(-1, 24))) {
            storm = ParticleManager::StormEffectSettings{};
        }
        // ImGui に折り返し付きのステータス文字列を表示する。
        ImGui::TextWrapped("%s", stormPresetStatus_.c_str());
    }
}

void MyGame::DrawEffectPreviewEditorImGui() {
    // ImGui の UI 要素を表示・更新する。
    ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Editor ]");
    int effectType = effectPreviewStormMode_ ? 1 : 0;
    const char* effectTypes[] = { "Hit Effect", "Tempest Storm" };
    // ImGui コンボ「Effect Type」で候補から選択する。
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
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.52f, 0.72f, 1.0f, 1.0f), "Persistent preview: clouds / wind / rain / lightning");
        DrawStormEffectEditorImGui();
        return;
    }
    // ImGui テキスト「SPACE / H : Trigger」を表示する。
    ImGui::Text("SPACE / H : Trigger");
    // ImGui チェックボックス「Auto Trigger」で ON/OFF を切り替える。
    ImGui::Checkbox("Auto Trigger", &effectPreviewAutoPlay_);
    // ImGui チェックボックス「Show GPU Sphere」で ON/OFF を切り替える。
    ImGui::Checkbox("Show GPU Sphere", &effectPreviewShowGPUParticleSphere_);
    // ImGui スライダー「Interval」で小数値を調整する。
    ImGui::SliderFloat("Interval", &effectPreviewInterval_, 0.2f, 3.0f);

    // ImGui ボタンを表示し、押されたら処理する。
    if (ImGui::Button(effectPreviewStormMode_ ? "Restart Tempest Storm" : "Trigger Saber Hit", ImVec2(-1, 24)) && particleManager) {
        if (effectPreviewStormMode_) {
            particleManager->SetStormActive(false);
            particleManager->ClearParticles();
            particleManager->SetStormActive(true, { effectPreviewPosition_.x, 0.0f, effectPreviewPosition_.z });
        } else {
            EmitEffectPreviewBurst();
        }
    }
    // ImGui ボタン「Clear Particles」を表示し、押されたら処理する。
    if (ImGui::Button("Clear Particles", ImVec2(-1, 24)) && particleManager) {
        particleManager->ClearParticles();
    }

    // ImGui セクション「Core Shape」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Core Shape", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui スライダー「Size」で小数値を調整する。
        ImGui::SliderFloat("Size", &effectPreviewHitSettings_.size, 0.2f, 3.0f);
        // ImGui スライダー「Brightness」で小数値を調整する。
        ImGui::SliderFloat("Brightness", &effectPreviewHitSettings_.brightness, 0.1f, 2.5f);
        // ImGui スライダー「Life Scale」で小数値を調整する。
        ImGui::SliderFloat("Life Scale", &effectPreviewHitSettings_.lifeScale, 0.2f, 3.0f);
        // ImGui スライダー「Slash Angle」で小数値を調整する。
        ImGui::SliderFloat("Slash Angle", &effectPreviewHitSettings_.slashAngle, -3.14f, 3.14f);
        // ImGui スライダー「Slash Spread」で小数値を調整する。
        ImGui::SliderFloat("Slash Spread", &effectPreviewHitSettings_.slashSpread, 0.2f, 3.14f);
        // ImGui チェックボックス「Mirror Slash」で ON/OFF を切り替える。
        ImGui::Checkbox("Mirror Slash", &effectPreviewMirrorSlash_);
        // ImGui スライダー「Burst Count」で整数値を調整する。
        ImGui::SliderInt("Burst Count", &effectPreviewBurstCount_, 1, 8);
        // ImGui スライダー「Burst Radius」で小数値を調整する。
        ImGui::SliderFloat("Burst Radius", &effectPreviewBurstRadius_, 0.0f, 2.0f);
    }

    // ImGui セクション「Detail」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Detail", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui スライダー「Slash Count」で整数値を調整する。
        ImGui::SliderInt("Slash Count", &effectPreviewHitSettings_.slashCount, 1, 32);
        // ImGui スライダー「Spark Count」で整数値を調整する。
        ImGui::SliderInt("Spark Count", &effectPreviewHitSettings_.sparkCount, 0, 160);
        // ImGui スライダー「Spark Speed」で小数値を調整する。
        ImGui::SliderFloat("Spark Speed", &effectPreviewHitSettings_.sparkSpeed, 0.1f, 3.0f);
        // ImGui スライダー「Spark Length」で小数値を調整する。
        ImGui::SliderFloat("Spark Length", &effectPreviewHitSettings_.sparkLength, 0.1f, 3.0f);
        // ImGui スライダー「Scatter Radius」で小数値を調整する。
        ImGui::SliderFloat("Scatter Radius", &effectPreviewHitSettings_.scatterRadius, 0.0f, 3.0f);
        // ImGui スライダー「Blue Ratio」で小数値を調整する。
        ImGui::SliderFloat("Blue Ratio", &effectPreviewHitSettings_.blueRatio, 0.0f, 1.0f);
        // ImGui スライダー「Ring Power」で小数値を調整する。
        ImGui::SliderFloat("Ring Power", &effectPreviewHitSettings_.ringPower, 0.0f, 3.0f);
        // ImGui スライダー「Core Power」で小数値を調整する。
        ImGui::SliderFloat("Core Power", &effectPreviewHitSettings_.corePower, 0.0f, 3.0f);
        // ImGui スライダー「Cross Power」で小数値を調整する。
        ImGui::SliderFloat("Cross Power", &effectPreviewHitSettings_.crossPower, 0.0f, 3.0f);
        // ImGui スライダー「Pillar Power」で小数値を調整する。
        ImGui::SliderFloat("Pillar Power", &effectPreviewHitSettings_.pillarPower, 0.0f, 3.0f);
        // ImGui スライダー「Main Bolt Count」で整数値を調整する。
        ImGui::SliderInt("Main Bolt Count", &effectPreviewHitSettings_.lightningCount, 0, 12);
        // ImGui スライダー「Lightning Segments」で整数値を調整する。
        ImGui::SliderInt("Lightning Segments", &effectPreviewHitSettings_.lightningSegments, 2, 8);
        // ImGui スライダー「Lightning Length」で小数値を調整する。
        ImGui::SliderFloat("Lightning Length", &effectPreviewHitSettings_.lightningLength, 0.1f, 4.0f);
        // ImGui スライダー「Lightning Spread」で小数値を調整する。
        ImGui::SliderFloat("Lightning Spread", &effectPreviewHitSettings_.lightningSpread, 0.0f, 3.0f);
        // ImGui スライダー「Lightning Power」で小数値を調整する。
        ImGui::SliderFloat("Lightning Power", &effectPreviewHitSettings_.lightningPower, 0.0f, 3.0f);
        // ImGui スライダー「Main Bolt Width」で小数値を調整する。
        ImGui::SliderFloat("Main Bolt Width", &effectPreviewHitSettings_.lightningWidth, 0.1f, 4.0f);
        // ImGui スライダー「Glow Width」で小数値を調整する。
        ImGui::SliderFloat("Glow Width", &effectPreviewHitSettings_.lightningGlowWidth, 1.0f, 8.0f);
        // ImGui スライダー「Glow Opacity」で小数値を調整する。
        ImGui::SliderFloat("Glow Opacity", &effectPreviewHitSettings_.lightningGlowOpacity, 0.0f, 1.0f);
        // ImGui スライダー「Branch Count」で整数値を調整する。
        ImGui::SliderInt("Branch Count", &effectPreviewHitSettings_.lightningBranchCount, 0, 12);
        // ImGui スライダー「Branch Length」で小数値を調整する。
        ImGui::SliderFloat("Branch Length", &effectPreviewHitSettings_.lightningBranchLength, 0.05f, 1.0f);
        // ImGui スライダー「Branch Spread」で小数値を調整する。
        ImGui::SliderFloat("Branch Spread", &effectPreviewHitSettings_.lightningBranchSpread, 0.0f, 1.57f);
        // ImGui スライダー「Branch Width」で小数値を調整する。
        ImGui::SliderFloat("Branch Width", &effectPreviewHitSettings_.lightningBranchWidth, 0.1f, 1.0f);
        const char* lightningModes[] = { "Radial", "Slash Forward", "Slash Axis", "Custom" };
        // ImGui コンボ「Lightning Mode」で候補から選択する。
        ImGui::Combo("Lightning Mode", &effectPreviewHitSettings_.lightningMode, lightningModes, 4);
        if (effectPreviewHitSettings_.lightningMode == 3) {
            // ImGui スライダー「Lightning Direction」で小数値を調整する。
            ImGui::SliderFloat("Lightning Direction", &effectPreviewHitSettings_.lightningDirection, -3.14f, 3.14f);
        }
        if (effectPreviewHitSettings_.lightningMode != 0) {
            // ImGui スライダー「Direction Spread」で小数値を調整する。
            ImGui::SliderFloat("Direction Spread", &effectPreviewHitSettings_.lightningDirectionSpread, 0.0f, 1.57f);
        }
    }

    // ImGui セクション「Colors」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Colors", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui カラー編集「Core Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Core Color", &effectPreviewHitSettings_.coreColor.x);
        // ImGui カラー編集「Slash Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Slash Color", &effectPreviewHitSettings_.slashColor.x);
        // ImGui カラー編集「Spark Primary」で RGBA 色を調整する。
        ImGui::ColorEdit4("Spark Primary", &effectPreviewHitSettings_.sparkColor.x);
        // ImGui カラー編集「Spark Secondary」で RGBA 色を調整する。
        ImGui::ColorEdit4("Spark Secondary", &effectPreviewHitSettings_.sparkSecondaryColor.x);
        // ImGui カラー編集「Ring Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Ring Color", &effectPreviewHitSettings_.ringColor.x);
        // ImGui カラー編集「Cross Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Cross Color", &effectPreviewHitSettings_.crossColor.x);
        // ImGui カラー編集「Pillar Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Pillar Color", &effectPreviewHitSettings_.pillarColor.x);
        // ImGui カラー編集「Lightning Color」で RGBA 色を調整する。
        ImGui::ColorEdit4("Lightning Color", &effectPreviewHitSettings_.lightningColor.x);
        // ImGui カラー編集「Lightning Glow」で RGBA 色を調整する。
        ImGui::ColorEdit4("Lightning Glow", &effectPreviewHitSettings_.lightningGlowColor.x);
    }

    // ImGui セクション「Randomization」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Randomization", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui チェックボックス「Random Position」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Position", &effectPreviewHitSettings_.randomizePosition);
        // ImGui チェックボックス「Random Direction」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Direction", &effectPreviewHitSettings_.randomizeDirection);
        // ImGui チェックボックス「Random Angle」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Angle", &effectPreviewHitSettings_.randomizeAngle);
        if (effectPreviewHitSettings_.randomizeAngle) {
            // ImGui スライダー「Angle Random Range」で小数値を調整する。
            ImGui::SliderFloat("Angle Random Range", &effectPreviewHitSettings_.angleRandomRange, 0.0f, 3.14f);
        }
        // ImGui チェックボックス「Random Scale」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Scale", &effectPreviewHitSettings_.randomizeScale);
        // ImGui チェックボックス「Random Lifetime」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Lifetime", &effectPreviewHitSettings_.randomizeLifetime);
        // ImGui チェックボックス「Random Color」で ON/OFF を切り替える。
        ImGui::Checkbox("Random Color", &effectPreviewHitSettings_.randomizeColor);
    }

    // ImGui セクション「Presets」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui ボタン「Saber Impact」を表示し、押されたら処理する。
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
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Blue Flash」を表示し、押されたら処理する。
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
        // ImGui ボタン「Spark Burst」を表示し、押されたら処理する。
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
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Heavy Hit」を表示し、押されたら処理する。
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
        // ImGui ボタン「Cinematic Finisher」を表示し、押されたら処理する。
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
        // ImGui ボタン「Lightning Slash」を表示し、押されたら処理する。
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
        // ImGui ボタン「Shock Ring」を表示し、押されたら処理する。
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
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Thin Cut」を表示し、押されたら処理する。
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

    // ImGui セクション「Saved Presets」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Saved Presets", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui チェックボックス「Include in Showcase」で ON/OFF を切り替える。
        ImGui::Checkbox("Include in Showcase", &effectPresetIncludeInShowcase_);
        // ImGui 入力欄「Preset Name」で名前や文字列を編集する。
        ImGui::InputText("Preset Name", effectPresetNameBuffer_.data(), effectPresetNameBuffer_.size());
        // ImGui ボタン「Save Preset」を表示し、押されたら処理する。
        if (ImGui::Button("Save Preset", ImVec2(135, 24))) {
            SaveEffectPreset(effectPresetNameBuffer_.data());
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Refresh」を表示し、押されたら処理する。
        if (ImGui::Button("Refresh", ImVec2(100, 24))) {
            LoadEffectPresetNames();
        }

        const char* selectedPresetName = effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())
            ? effectPresetNames_[effectPresetSelectedIndex_].c_str()
            : "Select saved preset";
        // ImGui コンボ「Saved」の選択リストを開始する。
        if (ImGui::BeginCombo("Saved", selectedPresetName)) {
            for (int i = 0; i < static_cast<int>(effectPresetNames_.size()); ++i) {
                bool selected = effectPresetSelectedIndex_ == i;
                // ImGui の選択項目を表示し、選ばれたら選択状態を更新する。
                if (ImGui::Selectable(effectPresetNames_[i].c_str(), selected)) {
                    effectPresetSelectedIndex_ = i;
                    CopyPresetName(effectPresetNameBuffer_, effectPresetNames_[i]);
                }
                if (selected) {
                    // 現在選択中の ImGui 項目へ既定フォーカスを当てる。
                    ImGui::SetItemDefaultFocus();
                }
            }
            // ImGui コンボの選択リストを閉じる。
            ImGui::EndCombo();
        }
        // ImGui ボタン「Load Selected」を表示し、押されたら処理する。
        if (ImGui::Button("Load Selected", ImVec2(135, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                LoadEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                effectPresetStatus_ = "Preset: nothing selected";
            }
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Save Over」を表示し、押されたら処理する。
        if (ImGui::Button("Save Over", ImVec2(100, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                SaveEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                SaveEffectPreset(effectPresetNameBuffer_.data());
            }
        }
        // ImGui に折り返し付きのステータス文字列を表示する。
        ImGui::TextWrapped("%s", effectPresetStatus_.c_str());
    }

    // ImGui セクション「Spawn」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Spawn", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui ドラッグ入力「Position」で 3D 座標を調整する。
        ImGui::DragFloat3("Position", &effectPreviewPosition_.x, 0.05f, -20.0f, 20.0f);
        // ImGui ボタン「Focus Camera」を表示し、押されたら処理する。
        if (ImGui::Button("Focus Camera", ImVec2(-1, 24))) {
            camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
        }
        // ImGui ボタン「Reset Tuning」を表示し、押されたら処理する。
        if (ImGui::Button("Reset Tuning", ImVec2(-1, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
    }
}

void MyGame::UpdateImGui() {
    // ImGui のフレーム情報と表示サイズを取得する。
    ImGuiIO& io        = ImGui::GetIO();
    const EditorLayout layout = MakeEditorLayout(io.DisplaySize);

    // 左パネル: Information
    // 次に開く ImGui ウィンドウの表示位置を固定する。
    ImGui::SetNextWindowPos( ImVec2(0, 0), ImGuiCond_Always);
    // 次に開く ImGui ウィンドウの表示サイズを固定する。
    ImGui::SetNextWindowSize(ImVec2(layout.leftPanelWidth, layout.mainPanelHeight), ImGuiCond_Always);
    // 次に開く ImGui ウィンドウの背景透明度を設定する。
    ImGui::SetNextWindowBgAlpha(1.0f);
    // ImGui ウィンドウ「Information」を開始する。
    ImGui::Begin("Information", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    // ImGui テキスト「FPS: %.1f (%.3f ms/f)」を表示する。
    ImGui::Text("FPS: %.1f (%.3f ms/f)", io.Framerate, 1000.0f / io.Framerate);
    // 次の ImGui 項目を同じ行に並べる。
    ImGui::SameLine(layout.leftPanelWidth - 60.0f);
    // ImGui ボタン「Exit」を表示し、押されたら処理する。
    if (ImGui::Button("Exit", ImVec2(50, 20))) {
        PostQuitMessage(0);
    }
    // ImGui 上に区切り線を表示する。
    ImGui::Separator();

    const bool isStageToolMode = (currentMode_ == AppMode::StageEditor ||
                                  currentMode_ == AppMode::GamePlay_BlockPlace);


    // ImGui セクション「Hierarchy / Mode」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Hierarchy / Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        const SceneType selectableScenes[] = {
            SceneType::DebugView,
            SceneType::StageEditor,
            SceneType::GamePlay,
            SceneType::SkinningEditor,
            SceneType::EffectPreview,
            SceneType::EffectShowcase,
            SceneType::PostEffectShowcase,
        };
        const char* modeNames[] = {
            "DebugView",
            "StageEditor",
            "GamePlay",
            "SkinningEditor",
            "EffectPreview",
            "EffectShowcase",
            "PostEffectShowcase"
        };
        int modeIndex = 0;
        const SceneType currentSceneType = GetCurrentSceneType();
        for (int i = 0; i < static_cast<int>(std::size(selectableScenes)); ++i) {
            if (selectableScenes[i] == currentSceneType) {
                modeIndex = i;
                break;
            }
        }
        // ImGui コンボ「App Mode」で候補から選択する。
        if (ImGui::Combo("App Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
            if (modeIndex >= 0 && modeIndex < static_cast<int>(std::size(selectableScenes))) {
                RequestSceneChange(selectableScenes[modeIndex]);
            }
        }
        if (currentMode_ == AppMode::EffectPreview) {
            // ImGui の UI 要素を表示・更新する。
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Effect only viewport");
            // ImGui チェックボックス「Show Particles」で ON/OFF を切り替える。
            ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);
        } else {

            // ImGui チェックボックス「Show 3D Objects」で ON/OFF を切り替える。
            ImGui::Checkbox("Show 3D Objects",      &debugFlags_.show3DObjects);
            // ImGui チェックボックス「Show Terrain」で ON/OFF を切り替える。
            ImGui::Checkbox("Show Terrain",          &debugFlags_.showTerrain);
            // ImGui チェックボックス「Show Skybox」で ON/OFF を切り替える。
            ImGui::Checkbox("Show Skybox",           &debugFlags_.showSkybox);
            // ImGui チェックボックス「Show Skybox (Cubemap)」で ON/OFF を切り替える。
            ImGui::Checkbox("Show Skybox (Cubemap)", &showSkyboxCubemap_);
            if (currentMode_ == AppMode::DebugView) {
                // ImGui チェックボックス「Show Sprite」で ON/OFF を切り替える。
                ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
            }
            // ImGui チェックボックス「Show Particles」で ON/OFF を切り替える。
            ImGui::Checkbox("Show Particles",        &debugFlags_.showParticles);
        }
    }


    // PostProcessRenderer 側の ImGui 設定パネルを描画する。
    if (postProcessInitialized_) {
        postProcess_.DrawImGui();
    }


    // ImGui セクション「Camera Settings」を開閉できる見出しとして表示する。
    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ImGui チェックボックス「Use First-Person Camera」で ON/OFF を切り替える。
        ImGui::Checkbox("Use First-Person Camera", &useFirstPersonCamera_);
        if (useFirstPersonCamera_) {
            // ImGui スライダー「FPS Yaw」で小数値を調整する。
            ImGui::SliderFloat("FPS Yaw",   &fpsCameraYaw_,   -6.28f, 6.28f);
            // ImGui スライダー「FPS Pitch」で小数値を調整する。
            ImGui::SliderFloat("FPS Pitch", &fpsCameraPitch_, -1.4f,  1.4f);
        }
        // Camera 側の ImGui 設定パネルを描画する。
        camera->DrawImGui();

        if (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) {
            gameplayCameraController_.SetFov(*camera->GetFovPtr());
        }
    }

    if (isStageToolMode) {
        // ImGui セクション「StageMap Info」を開閉できる見出しとして表示する。
        if (ImGui::CollapsingHeader("StageMap Info")) {
            // StageMap 側の ImGui 情報パネルを描画する。
            stageMap_.DrawImGui();
        }
        // ImGui セクション「Cursor Info」を開閉できる見出しとして表示する。
        if (ImGui::CollapsingHeader("Cursor Info")) {
            // MapCursor 側の ImGui 情報パネルを描画する。
            mapCursor_->DrawImGui();
        }
    }

    // 現在の ImGui ウィンドウを閉じる。
    ImGui::End(); // 左パネルここまで

    // 右パネル (Stage Editor) - StageEditorController に委譲
    if (isStageToolMode) {
        // StageEditorController 側の ImGui 編集パネルを描画する。
        stageEditorController_.DrawImGui(stageMap_, stageRenderer_.get(), mapCursor_.get(), player_.get());
    }

    if (currentMode_ == AppMode::EffectPreview) {
        // 次に開く ImGui ウィンドウの表示位置を固定する。
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - layout.rightPanelWidth, 0), ImGuiCond_Always);
        // 次に開く ImGui ウィンドウの表示サイズを固定する。
        ImGui::SetNextWindowSize(ImVec2(layout.rightPanelWidth, layout.mainPanelHeight), ImGuiCond_Always);
        // 次に開く ImGui ウィンドウの背景透明度を設定する。
        ImGui::SetNextWindowBgAlpha(1.0f);
        // ImGui ウィンドウ「Effect Editor」を開始する。
        ImGui::Begin("Effect Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        DrawEffectPreviewEditorImGui();
        // 現在の ImGui ウィンドウを閉じる。
        ImGui::End();
    }

    // 下パネル: Tools & Controls
    // 次に開く ImGui ウィンドウの表示位置を固定する。
    ImGui::SetNextWindowPos( ImVec2(0, io.DisplaySize.y - layout.bottomPanelHeight), ImGuiCond_Always);
    // 次に開く ImGui ウィンドウの表示サイズを固定する。
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, layout.bottomPanelHeight),     ImGuiCond_Always);
    // 次に開く ImGui ウィンドウの背景透明度を設定する。
    ImGui::SetNextWindowBgAlpha(1.0f);
    // ImGui ウィンドウ「Tools & Controls」を開始する。
    ImGui::Begin("Tools & Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (currentMode_ == AppMode::SkinningEditor && skinningEditorInitialized_ && skinningEditor_.HasPreviewObject()) {

        if (ImGui::BeginTabBar("SkinningBottomTabs")) {
            if (ImGui::BeginTabItem("Timeline")) {
                skinningEditor_.DrawImGuiTimeline();
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Assets")) {
                skinningEditor_.DrawAssetBrowserPanel(player_.get(), models[2].get());
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }

    } else if (currentMode_ == AppMode::EffectPreview) {
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Preview ]");
        // ImGui テキスト「Effect Type: %s」を表示する。
        ImGui::Text("Effect Type: %s", effectPreviewStormMode_ ? "Tempest Storm" : "Hit Effect");
        // ImGui テキスト「Editor controls are on the right panel.」を表示する。
        ImGui::Text("Editor controls are on the right panel.");
        // ImGui テキスト「SPACE / H : Trigger Saber Hit」を表示する。
        ImGui::Text("SPACE / H : Trigger Saber Hit");
        // ImGui テキスト「Saved preset: %s」を表示する。
        ImGui::Text("Saved preset: %s", effectPresetNameBuffer_.data());
        // ImGui テキスト「GPU Sphere: %s」を表示する。
        ImGui::Text("GPU Sphere: %s", effectPreviewShowGPUParticleSphere_ ? "ON" : "OFF");
        // ImGui ボタン「Trigger Saber Hit」を表示し、押されたら処理する。
        if (ImGui::Button("Trigger Saber Hit", ImVec2(180, 24)) && particleManager) {
            EmitEffectPreviewBurst();
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Clear Particles」を表示し、押されたら処理する。
        if (ImGui::Button("Clear Particles", ImVec2(140, 24)) && particleManager) {
            particleManager->ClearParticles();
        }

    } else if (false && currentMode_ == AppMode::EffectPreview) {
        // ImGui レイアウトを 2 カラムに切り替える。
        ImGui::Columns(2, "EffectPreviewColumns", false);
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Preview ]");
        // ImGui テキスト「SPACE / H : Trigger Saber Hit」を表示する。
        ImGui::Text("SPACE / H : Trigger Saber Hit");
        // ImGui チェックボックス「Auto Trigger」で ON/OFF を切り替える。
        ImGui::Checkbox("Auto Trigger", &effectPreviewAutoPlay_);
        // ImGui チェックボックス「Show GPU Particle Sphere」で ON/OFF を切り替える。
        ImGui::Checkbox("Show GPU Particle Sphere", &effectPreviewShowGPUParticleSphere_);
        // ImGui スライダー「Interval」で小数値を調整する。
        ImGui::SliderFloat("Interval", &effectPreviewInterval_, 0.2f, 3.0f);
        // ImGui ボタン「Trigger Saber Hit」を表示し、押されたら処理する。
        if (ImGui::Button("Trigger Saber Hit", ImVec2(180, 24)) && particleManager) {
            EmitEffectPreviewBurst();
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Clear Particles」を表示し、押されたら処理する。
        if (ImGui::Button("Clear Particles", ImVec2(140, 24)) && particleManager) {
            particleManager->ClearParticles();
        }
        // ImGui 上に区切り線を表示する。
        ImGui::Separator();
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.55f, 0.9f, 1.0f, 1.0f), "[ Hit Effect Tuning ]");
        // ImGui スライダー「Size」で小数値を調整する。
        ImGui::SliderFloat("Size", &effectPreviewHitSettings_.size, 0.2f, 3.0f);
        // ImGui スライダー「Brightness」で小数値を調整する。
        ImGui::SliderFloat("Brightness", &effectPreviewHitSettings_.brightness, 0.1f, 2.5f);
        // ImGui スライダー「Life Scale」で小数値を調整する。
        ImGui::SliderFloat("Life Scale", &effectPreviewHitSettings_.lifeScale, 0.2f, 3.0f);
        // ImGui スライダー「Slash Angle」で小数値を調整する。
        ImGui::SliderFloat("Slash Angle", &effectPreviewHitSettings_.slashAngle, -3.14f, 3.14f);
        // ImGui スライダー「Slash Spread」で小数値を調整する。
        ImGui::SliderFloat("Slash Spread", &effectPreviewHitSettings_.slashSpread, 0.2f, 3.14f);
        // ImGui チェックボックス「Mirror Slash」で ON/OFF を切り替える。
        ImGui::Checkbox("Mirror Slash", &effectPreviewMirrorSlash_);
        // ImGui スライダー「Burst Count」で整数値を調整する。
        ImGui::SliderInt("Burst Count", &effectPreviewBurstCount_, 1, 8);
        // ImGui スライダー「Burst Radius」で小数値を調整する。
        ImGui::SliderFloat("Burst Radius", &effectPreviewBurstRadius_, 0.0f, 2.0f);
        // ImGui 上に区切り線を表示する。
        ImGui::Separator();
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.95f, 0.95f, 0.35f, 1.0f), "[ Presets ]");
        // ImGui ボタン「Saber Impact」を表示し、押されたら処理する。
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
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Blue Flash」を表示し、押されたら処理する。
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
        // ImGui ボタン「Spark Burst」を表示し、押されたら処理する。
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
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Heavy Hit」を表示し、押されたら処理する。
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
        // ImGui ボタン「Cinematic Finisher」を表示し、押されたら処理する。
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
        // ImGui 上に区切り線を表示する。
        ImGui::Separator();
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.50f, 1.0f, 0.55f, 1.0f), "[ Saved Presets ]");
        // ImGui 入力欄「Preset Name」で名前や文字列を編集する。
        ImGui::InputText("Preset Name", effectPresetNameBuffer_.data(), effectPresetNameBuffer_.size());
        // ImGui ボタン「Save Preset」を表示し、押されたら処理する。
        if (ImGui::Button("Save Preset", ImVec2(120, 24))) {
            SaveEffectPreset(effectPresetNameBuffer_.data());
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Refresh List」を表示し、押されたら処理する。
        if (ImGui::Button("Refresh List", ImVec2(120, 24))) {
            LoadEffectPresetNames();
        }

        const char* selectedPresetName = effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())
            ? effectPresetNames_[effectPresetSelectedIndex_].c_str()
            : "Select saved preset";
        // ImGui コンボ「Saved Presets」の選択リストを開始する。
        if (ImGui::BeginCombo("Saved Presets", selectedPresetName)) {
            for (int i = 0; i < static_cast<int>(effectPresetNames_.size()); ++i) {
                bool selected = effectPresetSelectedIndex_ == i;
                // ImGui の選択項目を表示し、選ばれたら選択状態を更新する。
                if (ImGui::Selectable(effectPresetNames_[i].c_str(), selected)) {
                    effectPresetSelectedIndex_ = i;
                    CopyPresetName(effectPresetNameBuffer_, effectPresetNames_[i]);
                }
                if (selected) {
                    // 現在選択中の ImGui 項目へ既定フォーカスを当てる。
                    ImGui::SetItemDefaultFocus();
                }
            }
            // ImGui コンボの選択リストを閉じる。
            ImGui::EndCombo();
        }
        // ImGui ボタン「Load Selected」を表示し、押されたら処理する。
        if (ImGui::Button("Load Selected", ImVec2(120, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                LoadEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                effectPresetStatus_ = "Preset: nothing selected";
            }
        }
        // 次の ImGui 項目を同じ行に並べる。
        ImGui::SameLine();
        // ImGui ボタン「Save Over」を表示し、押されたら処理する。
        if (ImGui::Button("Save Over", ImVec2(120, 24))) {
            if (effectPresetSelectedIndex_ >= 0 && effectPresetSelectedIndex_ < static_cast<int>(effectPresetNames_.size())) {
                SaveEffectPreset(effectPresetNames_[effectPresetSelectedIndex_]);
            } else {
                SaveEffectPreset(effectPresetNameBuffer_.data());
            }
        }
        // ImGui に文字列を装飾なしで表示する。
        ImGui::TextUnformatted(effectPresetStatus_.c_str());

        // ImGui の次のカラムへ移動する。
        ImGui::NextColumn();
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.25f, 1.0f), "[ Spawn Transform ]");
        // ImGui ドラッグ入力「Position」で 3D 座標を調整する。
        ImGui::DragFloat3("Position", &effectPreviewPosition_.x, 0.05f, -20.0f, 20.0f);
        // ImGui ボタン「Focus Camera」を表示し、押されたら処理する。
        if (ImGui::Button("Focus Camera", ImVec2(160, 24))) {
            camera->ForceReset(effectPreviewPosition_, 4.0f, { 0.25f, 0.0f, 0.0f });
        }
        // ImGui 上に区切り線を表示する。
        ImGui::Separator();
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.35f, 1.0f), "[ Detail ]");
        // ImGui スライダー「Slash Count」で整数値を調整する。
        ImGui::SliderInt("Slash Count", &effectPreviewHitSettings_.slashCount, 3, 32);
        // ImGui スライダー「Spark Count」で整数値を調整する。
        ImGui::SliderInt("Spark Count", &effectPreviewHitSettings_.sparkCount, 0, 160);
        // ImGui スライダー「Spark Speed」で小数値を調整する。
        ImGui::SliderFloat("Spark Speed", &effectPreviewHitSettings_.sparkSpeed, 0.1f, 3.0f);
        // ImGui スライダー「Spark Length」で小数値を調整する。
        ImGui::SliderFloat("Spark Length", &effectPreviewHitSettings_.sparkLength, 0.1f, 3.0f);
        // ImGui スライダー「Scatter Radius」で小数値を調整する。
        ImGui::SliderFloat("Scatter Radius", &effectPreviewHitSettings_.scatterRadius, 0.0f, 3.0f);
        // ImGui スライダー「Blue Ratio」で小数値を調整する。
        ImGui::SliderFloat("Blue Ratio", &effectPreviewHitSettings_.blueRatio, 0.0f, 1.0f);
        // ImGui スライダー「Ring Power」で小数値を調整する。
        ImGui::SliderFloat("Ring Power", &effectPreviewHitSettings_.ringPower, 0.0f, 3.0f);
        // ImGui スライダー「Core Power」で小数値を調整する。
        ImGui::SliderFloat("Core Power", &effectPreviewHitSettings_.corePower, 0.0f, 3.0f);
        // ImGui スライダー「Cross Power」で小数値を調整する。
        ImGui::SliderFloat("Cross Power", &effectPreviewHitSettings_.crossPower, 0.0f, 3.0f);
        // ImGui スライダー「Pillar Power」で小数値を調整する。
        ImGui::SliderFloat("Pillar Power", &effectPreviewHitSettings_.pillarPower, 0.0f, 3.0f);
        // ImGui カラー編集「Cool Color」で RGB 色を調整する。
        ImGui::ColorEdit3("Cool Color", &effectPreviewHitSettings_.coolColor.x);
        // ImGui カラー編集「Warm Color」で RGB 色を調整する。
        ImGui::ColorEdit3("Warm Color", &effectPreviewHitSettings_.warmColor.x);
        // ImGui ボタン「Reset Tuning」を表示し、押されたら処理する。
        if (ImGui::Button("Reset Tuning", ImVec2(160, 24))) {
            effectPreviewHitSettings_ = ParticleManager::HitEffectSettings{};
            effectPreviewMirrorSlash_ = false;
            effectPreviewBurstCount_ = 1;
            effectPreviewBurstRadius_ = 0.0f;
        }
        // ImGui テキスト「Particles are forced visible in this mode.」を表示する。
        ImGui::Text("Particles are forced visible in this mode.");
        // ImGui レイアウトを 1 カラムに切り替える。
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::GamePlay && player_) {
        // ImGui レイアウトを 2 カラムに切り替える。
        ImGui::Columns(2, "GameplayColumns", false);

        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Game Controls & Objective ]");
        // ImGui テキスト「A / D : Move Left / Right」を表示する。
        ImGui::Text("A / D : Move Left / Right");
        // ImGui テキスト「SPACE : Jump」を表示する。
        ImGui::Text("SPACE : Jump");
        // ImGui テキスト「B     : Block Inventory」を表示する。
        ImGui::Text("B     : Block Inventory");
        // ImGui テキスト「ESC   : Return to Stage Select」を表示する。
        ImGui::Text("ESC   : Return to Stage Select");
        // ImGui 上に区切り線を表示する。
        ImGui::Separator();
        // ImGui テキスト「Objective: Pick up bubbles and reach the Goal!」を表示する。
        ImGui::Text("Objective: Pick up bubbles and reach the Goal!");

        // ImGui の次のカラムへ移動する。
        ImGui::NextColumn();
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Player Debug ]");
        Vector3 pos = player_->GetPosition();
        // ImGui テキスト「Pos: X:%.2f Y:%.2f Z:%.2f」を表示する。
        ImGui::Text("Pos: X:%.2f Y:%.2f Z:%.2f", pos.x, pos.y, pos.z);
        if (isGoalReached_) {
            // ImGui の UI 要素を表示・更新する。
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "GOAL REACHED!");
        } else {
            // ImGui テキスト「Status: Playing」を表示する。
            ImGui::Text("Status: Playing");
        }
        // ImGui レイアウトを 1 カラムに切り替える。
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::StageEditor) {
        // ImGui レイアウトを 2 カラムに切り替える。
        ImGui::Columns(2, "EditorColumns", false);

        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Stage Editor Controls ]");
        // ImGui テキスト「W/A/S/D : Cursor Horizontal」を表示する。
        ImGui::Text("W/A/S/D : Cursor Horizontal");
        // ImGui テキスト「Q/E     : Cursor Up/Down」を表示する。
        ImGui::Text("Q/E     : Cursor Up/Down");
        // ImGui テキスト「ENTER   : Place Block」を表示する。
        ImGui::Text("ENTER   : Place Block");
        // ImGui テキスト「SPACE   : Erase Block」を表示する。
        ImGui::Text("SPACE   : Erase Block");
        // ImGui テキスト「R       : Rotate Block」を表示する。
        ImGui::Text("R       : Rotate Block");

        // ImGui の次のカラムへ移動する。
        ImGui::NextColumn();
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Stage Map Data ]");
        const Int3& cursor = mapCursor_->GetIndex();
        // ImGui テキスト「Cursor: X:%d Y:%d Z:%d」を表示する。
        ImGui::Text("Cursor: X:%d Y:%d Z:%d", cursor.x, cursor.y, cursor.z);
        // ImGui テキスト「Block: %s (ID:%d)」を表示する。
        ImGui::Text("Block: %s (ID:%d)",
            BlockTypeToString(stageEditorController_.GetSelectedBlockType()),
            stageEditorController_.GetSelectedBlockType());
        // ImGui テキスト「Stock: %d」を表示する。
        ImGui::Text("Stock: %d", blockInventory_.GetBlockCount());
        // ImGui レイアウトを 1 カラムに切り替える。
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::GamePlay_BlockPlace) {
        // ImGui レイアウトを 2 カラムに切り替える。
        ImGui::Columns(2, "BlockPlaceColumns", false);

        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Block Placement ]");
        // ImGui テキスト「W/A/S/D : Move Cursor」を表示する。
        ImGui::Text("W/A/S/D : Move Cursor");
        // ImGui テキスト「Q/E     : Cursor Up/Down」を表示する。
        ImGui::Text("Q/E     : Cursor Up/Down");
        // ImGui テキスト「ENTER   : Place Block」を表示する。
        ImGui::Text("ENTER   : Place Block");
        // ImGui テキスト「R       : Rotate Block」を表示する。
        ImGui::Text("R       : Rotate Block");
        // ImGui テキスト「ESC / B : Cancel」を表示する。
        ImGui::Text("ESC / B : Cancel");

        // ImGui の次のカラムへ移動する。
        ImGui::NextColumn();
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Placement State ]");
        const Int3& cursor = mapCursor_->GetIndex();
        // ImGui テキスト「Cursor: X:%d Y:%d Z:%d」を表示する。
        ImGui::Text("Cursor: X:%d Y:%d Z:%d", cursor.x, cursor.y, cursor.z);
        if (blockInventoryUI_) {
            // ImGui テキスト「Selected: %s」を表示する。
            ImGui::Text("Selected: %s", BlockTypeToString(blockInventoryUI_->GetSelectedBlockType()));
        }
        // ImGui テキスト「Rotation Y: %.1f deg」を表示する。
        ImGui::Text("Rotation Y: %.1f deg", placeRotationY_ * 57.29578f);
        // ImGui レイアウトを 1 カラムに切り替える。
        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::EffectShowcase) {
        const int presetCount = static_cast<int>(effectShowcasePresetNames_.size());
        const int safeIndex = presetCount > 0
            ? std::clamp(effectShowcaseSelectedIndex_, 0, presetCount - 1)
            : 0;
        const char* presetName = presetCount > 0
            ? effectShowcasePresetNames_[safeIndex].c_str()
            : "No showcase presets";

        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "[ Effect Showcase ]");
        // ImGui テキスト「Preset: %02d / %02d  %s」を表示する。
        ImGui::Text("Preset: %02d / %02d  %s", presetCount > 0 ? safeIndex + 1 : 0, presetCount, presetName);
        // ImGui テキスト「LEFT / RIGHT : Select    SPACE : Replay    A : Auto Play [%s]    TAB : Game」を表示する。
        ImGui::Text("LEFT / RIGHT : Select    SPACE : Replay    A : Auto Play [%s]    TAB : Game",
            effectShowcaseAutoPlay_ ? "ON" : "OFF");
        // ImGui テキスト「MMB Drag : Orbit    Shift + MMB : Pan    Mouse Wheel : Zoom」を表示する。
        ImGui::Text("MMB Drag : Orbit    Shift + MMB : Pan    Mouse Wheel : Zoom");

    } else if (currentMode_ == AppMode::PostEffectShowcase) {
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.36f, 1.0f), "[ PostEffect Showcase ]");
        ImGui::Text("Current: %s", GetPostEffectShowcaseName(postProcess_.GetPostEffectMode()));
        ImGui::Text("1 Grayscale / 2 Vignetting / 3 Smoothing / 4-0 Extra PostEffects");
        ImGui::Text("Particle showcase effects are disabled in this mode.");

    } else if (currentMode_ == AppMode::StageSelect) {
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.4f, 0.85f, 1.0f, 1.0f), "[ Stage Select ]");
        // ImGui テキスト「Choose a stage from the center view.」を表示する。
        ImGui::Text("Choose a stage from the center view.");
        // ImGui テキスト「Only stage selection UI is active in this mode.」を表示する。
        ImGui::Text("Only stage selection UI is active in this mode.");

    } else if (currentMode_ == AppMode::DebugView) {
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Debug View ]");
        // ImGui テキスト「General object, terrain, sprite, and particle checks.」を表示する。
        ImGui::Text("General object, terrain, sprite, and particle checks.");

        // --- Player Options ---
        // ImGui セクション「Player Settings」を開閉できる見出しとして表示する。
        if (ImGui::CollapsingHeader("Player Settings")) {
            // ImGui スライダー「Player Glow」で小数値を調整する。
            ImGui::SliderFloat("Player Glow", &playerGlow_, 0.0f, 5.0f);
            if (player_) player_->SetGlow(playerGlow_);
        }

        // ImGui セクション「Environment Map」を開閉できる見出しとして表示する。
        if (ImGui::CollapsingHeader("Environment Map", ImGuiTreeNodeFlags_DefaultOpen)) {
            // ImGui スライダー「Debug Objects」で小数値を調整する。
            ImGui::SliderFloat("Debug Objects", &debugObjectEnvironmentCoefficient_, 0.0f, 1.0f);
            // ImGui スライダー「Terrain」で小数値を調整する。
            ImGui::SliderFloat("Terrain", &terrainEnvironmentCoefficient_, 0.0f, 1.0f);
            // ImGui スライダー「Player」で小数値を調整する。
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
        // ImGui の UI 要素を表示・更新する。
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Application Status ]");
        // ImGui テキスト「No mode-specific tools.」を表示する。
        ImGui::Text("No mode-specific tools.");
    }

    // 現在の ImGui ウィンドウを閉じる。
    ImGui::End(); // 下パネルここまで

    // 右パネル (Skinning Editor) - SkinningEditor モード時のみ
    if (currentMode_ == AppMode::SkinningEditor && skinningEditorInitialized_ && skinningEditor_.HasPreviewObject()) {
        // 次に開く ImGui ウィンドウの表示位置を固定する。
        ImGui::SetNextWindowPos( ImVec2(io.DisplaySize.x - layout.rightPanelWidth, 0), ImGuiCond_Always);
        // 次に開く ImGui ウィンドウの表示サイズを固定する。
        ImGui::SetNextWindowSize(ImVec2(layout.rightPanelWidth, layout.mainPanelHeight), ImGuiCond_Always);
        // 次に開く ImGui ウィンドウの背景透明度を設定する。
        ImGui::SetNextWindowBgAlpha(1.0f);
        // ImGui ウィンドウ「Skinning Editor」を開始する。
        ImGui::Begin("Skinning Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
        skinningEditor_.DrawImGuiSidePanel(camera.get(), player_.get(), models[2].get());
        // 現在の ImGui ウィンドウを閉じる。
        ImGui::End();
    }
}
#endif // !NDEBUG

void MyGame::Draw() {
    auto commandList = dxCommon->GetCommandList();

    if (skydomeObject_) {
        if (postProcessInitialized_ && postProcess_.GetSkyboxLinkMode() == 1) {
            skydomeObject_->SetColor(postProcess_.GetClearColor());
        } else {
            skydomeObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    shadowMap_->PreDraw(commandList);
    commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    commandList->SetPipelineState(object3dCommon->GetShadowPipelineState());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (auto& obj : objectList) {
        if (obj) { obj->DrawShadow(lightVP); }
    }
    if (player_) { player_->DrawShadow(lightVP); }
    if (currentMode_ == AppMode::SkinningEditor && skinningEditorInitialized_) { skinningEditor_.DrawShadow(lightVP); }
    if (stageRenderer_) { stageRenderer_->DrawShadow(lightVP); }

    shadowMap_->PostDraw(commandList);

    if (postProcessInitialized_ && postProcess_.IsEnabled()) {
        postProcess_.BeginRender(commandList, dxCommon.get());
        RenderScene();
        postProcess_.EndRender(commandList);
        dxCommon->PreDraw(false);
        postProcess_.DrawToBackBuffer(commandList, camera->GetProjectionMatrix());
    } else {
#ifdef NDEBUG
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)WinApp::kWindowWidth, (float)WinApp::kWindowHeight, 0.0f, 1.0f };
        D3D12_RECT scissor = { 0, 0, WinApp::kWindowWidth, WinApp::kWindowHeight };
#else
        D3D12_VIEWPORT viewport = { 320.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
        D3D12_RECT scissor = { 320, 0, 1600, 720 };
#endif
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
        dxCommon->PreDraw();
        RenderScene();
    }

    dxCommon->EndImGui();
    dxCommon->PostDraw();
}

void MyGame::RenderScene() {
    auto commandList = dxCommon->GetCommandList();

    if (!debugFlags_.show3DObjects &&
        currentMode_ != AppMode::EffectPreview &&
        currentMode_ != AppMode::EffectShowcase &&
        currentMode_ != AppMode::PostEffectShowcase) {
        return;
    }

    ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
    commandList->SetDescriptorHeaps(1, heaps);
    object3dCommon->PreDraw();
    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());

    if (currentMode_ == AppMode::EffectPreview || currentMode_ == AppMode::EffectShowcase) {
        if (terrainObject_) {
            terrainObject_->Draw();
        }
        if (debugFlags_.showParticles) {
            ID3D12DescriptorHeap* particleHeaps[] = { textureManager->GetSrvHeap() };
            commandList->SetDescriptorHeaps(1, particleHeaps);
            particleManager->Draw();
        }
        return;
    }

    if (currentMode_ == AppMode::PostEffectShowcase) {
        if (terrainObject_) {
            terrainObject_->Draw();
        }
        if (player_) {
            player_->Draw();
        }
        return;
    }

    DrawSkyboxForFrame();

    if (currentMode_ == AppMode::StageSelect) {
        if (stageSelect_) { stageSelect_->Draw(); }
    } else if (currentMode_ == AppMode::SkinningEditor && skinningEditorInitialized_) {
        skinningEditor_.Draw(object3dCommon.get(), camera.get());
    } else {
        const bool isGameMode =
            currentMode_ == AppMode::StageEditor ||
            currentMode_ == AppMode::GamePlay ||
            currentMode_ == AppMode::GamePlay_BlockPlace ||
            currentMode_ == AppMode::EffectPreview;

        if (isGameMode && stageRenderer_) {
            stageRenderer_->Draw();
            stageRenderer_->DrawTransparent();
            object3dCommon->PreDraw();
            commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
        }

        if (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::EffectPreview) {
            if (player_ && !useFirstPersonCamera_) {
                player_->Draw();
                if (IsPlayerHiddenByWall()) {
                    object3dCommon->PreDrawPlayerHighlight();
                    player_->DrawHighlight();
                    object3dCommon->PreDraw();
                    commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
                }
            }

            if (currentMode_ == AppMode::GamePlay && gameplayUIManager_) {
                gameplayUIManager_->Draw3DPrompts(
                    true, player_.get(), object3dCommon.get(), commandList, shadowMap_->GetSrvHandle());
            }
        }

        if ((currentMode_ == AppMode::StageEditor ||
             currentMode_ == AppMode::GamePlay_BlockPlace) &&
            mapCursor_) {
            mapCursor_->Draw();
        }

        if (currentMode_ == AppMode::DebugView) {
            if (terrainObject_ && debugFlags_.showTerrain) { terrainObject_->Draw(); }
            for (auto& obj : objectList) {
                if (obj) { obj->Draw(); }
            }
            if (player_) { player_->Draw(); }
        }
    }

    if (debugFlags_.showParticles) {
        ID3D12DescriptorHeap* particleHeaps[] = { textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, particleHeaps);
        particleManager->Draw();
    }

    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        spriteCommon->PreDraw();
        if (sprite) { sprite->Draw(); }
    }

    if (gameplayUIManager_) {
        gameplayUIManager_->DrawSprites(
            currentMode_ == AppMode::GamePlay ||
            currentMode_ == AppMode::GamePlay_BlockPlace,
            gameplayCameraController_.IsFollowPlayerMode());
    }

    if (blockInventoryUI_ &&
        (currentMode_ == AppMode::GamePlay ||
         currentMode_ == AppMode::GamePlay_BlockPlace)) {
        blockInventoryUI_->Draw();
    }

    const bool invOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();
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

void MyGame::DrawSkyboxForFrame() {
    auto commandList = dxCommon->GetCommandList();
    if (debugFlags_.showSkybox && showSkyboxCubemap_ && skybox_) {
        skybox_->Draw();
        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
    } else if (debugFlags_.showSkybox && skydomeObject_) {
        skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skydomeObject_->Draw();
    }
}

bool MyGame::IsPlayerHiddenByWall() const {
    if (!player_ || !camera) { return false; }

    Vector3 camPos = camera->GetPosition();
    Vector3 playerPos = player_->GetPosition();
    playerPos.y += 0.8f;

    Vector3 diff = {
        playerPos.x - camPos.x,
        playerPos.y - camPos.y,
        playerPos.z - camPos.z
    };
    const float len = std::sqrt(diff.x * diff.x + diff.y * diff.y + diff.z * diff.z);
    if (len <= 0.001f) { return false; }

    Vector3 dir = { diff.x / len, diff.y / len, diff.z / len };
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

//  MyGame::Finalize
//  GPU 完了を待ってから、UI、ゲームオブジェクト、エンジン基盤の順に解放する。


void MyGame::Finalize() {

    if (dxCommon) { dxCommon->WaitForGpu(); }

    if (sceneManager_) {
        sceneManager_->Finalize(*this);
        sceneManager_.reset();
    }

    sound.Finalize();

    dxCommon->FinalizeImGui();


    ModelManager::Finalize();
    objectList.clear();
    models.clear();

    // Finalize() を持つ UI は reset 前に明示的に終了させる。
    if (gameplayUIManager_) { gameplayUIManager_->Finalize(); }
    gameplayUIManager_.reset();
    if (blockInventoryUI_) { blockInventoryUI_->Finalize(); }
    blockInventoryUI_.reset();

    // ゲームオブジェクトを解放する。
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

//  更新サブルーチン群


//  UpdateDebugView : DebugView モードの更新

void MyGame::UpdateDebugView() {
    if (input->TriggerKey(DIK_SPACE)) {
        particleManager->Emit({ 0, 0, 0 }, 10);
    }

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

// 嵐エフェクトの現在値をプリセットとして保存する。
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

// 保存済みの嵐プリセットを読み込み、エディタ用の状態へ反映する。
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

// ヒットエフェクトと嵐エフェクトのプリセット一覧をまとめて更新する。
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

// ヒットエフェクトの現在値をプリセットとして保存する。
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

// 保存済みのヒットエフェクトプリセットを読み込む。
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

// 現在表示しているエフェクトが嵐プリセットかを判定する。
bool MyGame::IsCurrentEffectStorm() const {
    if (currentMode_ == AppMode::EffectPreview) {
        return effectPreviewStormMode_;
    }
    if (currentMode_ != AppMode::EffectShowcase ||
        effectShowcaseSelectedIndex_ < 0 ||
        effectShowcaseSelectedIndex_ >= static_cast<int>(effectShowcasePresetNames_.size())) {
        return false;
    }

    const std::string& presetName = effectShowcasePresetNames_[effectShowcaseSelectedIndex_];
    return std::find(stormPresetNames_.begin(), stormPresetNames_.end(), presetName) != stormPresetNames_.end();
}

// ヒットエフェクトを単発または扇状に複数発生させる。
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

// エフェクト調整画面の更新。
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

// 登録済みエフェクトを順番に見せるショーケース画面の更新。
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
        RequestSceneChange(SceneType::StageSelect);
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

// CG5課題1専用のPostEffect確認モードを更新する。
void MyGame::UpdatePostEffectShowcase() {
    EnsurePostProcessInitialized();
    EnsureTerrainInitialized();
    debugFlags_.showParticles = false;
    debugFlags_.showSkybox = false;
    postProcess_.SetEnabled(true);

    if (particleManager) {
        particleManager->SetStormActive(false);
        particleManager->SetDrawGPUParticleSphere(false);
        particleManager->ClearParticles();
    }

    // パーティクル演出とは別に、画面全体へ適用するPostEffectだけを数字キーで切り替える。
    for (const PostEffectHotKey& hotKey : kPostEffectHotKeys) {
        if (!input->TriggerKey(hotKey.key)) {
            continue;
        }
        postProcess_.SetPostEffectMode(hotKey.mode);
        if (hotKey.mode == 10) {
            // 初期値0.0だとDissolveが分かりづらいため、展示時は溶け始めが見える値にする。
            postProcess_.SetDissolveThreshold(0.35f);
        }
        if (hotKey.mode == 11) {
            // ノイズ量を明確にして、Randomの効果差を単体で確認できるようにする。
            postProcess_.SetRandomMode(0);
            postProcess_.SetRandomStrength(0.55f);
        }
    }

    if (input->TriggerKey(DIK_TAB)) {
        stageSelect_->Initialize(object3dCommon.get(), input.get());
        RequestSceneChange(SceneType::StageSelect);
    }
}

// ショーケース中に表示する操作ガイドとプリセット名。
void MyGame::DrawEffectShowcaseImGui() {
    // ImGui のフレーム情報と表示サイズを取得する。
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
    // 次に開く ImGui ウィンドウの表示位置を固定する。
    ImGui::SetNextWindowPos(ImVec2(headerX, 20.0f), ImGuiCond_Always);
    // 次に開く ImGui ウィンドウの表示サイズを固定する。
    ImGui::SetNextWindowSize(ImVec2(headerWidth, 94.0f), ImGuiCond_Always);
    // 次に開く ImGui ウィンドウの背景透明度を設定する。
    ImGui::SetNextWindowBgAlpha(0.72f);
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
    // ImGui ウィンドウ「Effect Showcase Header」を開始する。
    ImGui::Begin("Effect Showcase Header", nullptr, flags);
    // 現在の ImGui ウィンドウ内の文字サイズを調整する。
    ImGui::SetWindowFontScale(1.45f);
    // ImGui の UI 要素を表示・更新する。
    ImGui::TextColored(ImVec4(0.42f, 0.86f, 1.0f, 1.0f), "EFFECT SHOWCASE");
    // 現在の ImGui ウィンドウ内の文字サイズを調整する。
    ImGui::SetWindowFontScale(1.15f);
    if (presetCount > 0) {
        // ImGui テキスト「%02d / %02d    %s」を表示する。
        ImGui::Text("%02d / %02d    %s", safeIndex + 1, presetCount, presetName);
    } else {
        // ImGui に文字列を装飾なしで表示する。
        ImGui::TextUnformatted(presetName);
    }
    // 現在の ImGui ウィンドウを閉じる。
    ImGui::End();

#ifdef NDEBUG
    // Debug builds use the existing Tools & Controls panel. Release shows a compact guide for evaluators.
    const float panelMargin = 24.0f;
    const float panelHeight = 96.0f;
    const float panelY = io.DisplaySize.y - panelHeight - panelMargin;
    const float panelWidth = io.DisplaySize.x - panelMargin * 2.0f;

    // 次に開く ImGui ウィンドウの表示位置を固定する。
    ImGui::SetNextWindowPos(ImVec2(panelMargin, panelY), ImGuiCond_Always);
    // 次に開く ImGui ウィンドウの表示サイズを固定する。
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);
    // 次に開く ImGui ウィンドウの背景透明度を設定する。
    ImGui::SetNextWindowBgAlpha(0.72f);
    // ImGui ウィンドウ「Effect Showcase Controls」を開始する。
    ImGui::Begin("Effect Showcase Controls", nullptr, flags);
    ImGui::TextColored(ImVec4(0.42f, 0.86f, 1.0f, 1.0f), "Particle / Scene Effect");
    ImGui::Separator();
    ImGui::Text("LEFT / RIGHT : Select preset");
    ImGui::Text("SPACE        : Replay current preset");
    ImGui::Text("A            : Auto Play [%s]", effectShowcaseAutoPlay_ ? "ON" : "OFF");
    ImGui::Text("TAB          : Back to Game");
    ImGui::Spacing();
    ImGui::TextUnformatted("MMB          : Orbit camera");
    ImGui::SameLine();
    ImGui::TextUnformatted("Shift + MMB  : Pan camera");
    ImGui::SameLine();
    ImGui::TextUnformatted("Wheel        : Zoom camera");
    // 現在の ImGui ウィンドウを閉じる。
    ImGui::End();
#endif
}

// PostEffectだけを確認するCG5課題専用UI。
void MyGame::DrawPostEffectShowcaseImGui() {
    const ImGuiIO& io = ImGui::GetIO();
    constexpr ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;

#ifdef NDEBUG
    const float headerX = 24.0f;
    const float headerWidth = io.DisplaySize.x - 48.0f;
#else
    const float headerX = 344.0f;
    const float headerWidth = io.DisplaySize.x - headerX - 24.0f;
#endif

    ImGui::SetNextWindowPos(ImVec2(headerX, 20.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(headerWidth, 92.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("PostEffect Showcase Header", nullptr, flags);
    ImGui::SetWindowFontScale(1.45f);
    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.36f, 1.0f), "POST EFFECT SHOWCASE");
    ImGui::SetWindowFontScale(1.15f);
    ImGui::Text("Current : %s", GetPostEffectShowcaseName(postProcess_.GetPostEffectMode()));
    ImGui::TextUnformatted("CG5 Evaluation Task 1 dedicated mode");
    ImGui::End();

#ifdef NDEBUG
    const float panelMargin = 24.0f;
    const float panelHeight = 214.0f;
    const float panelY = io.DisplaySize.y - panelHeight - panelMargin;
    const float panelWidth = io.DisplaySize.x - panelMargin * 2.0f;

    ImGui::SetNextWindowPos(ImVec2(panelMargin, panelY), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, panelHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.72f);
    ImGui::Begin("PostEffect Showcase Controls", nullptr, flags);
    ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.36f, 1.0f), "PostEffect Controls");
    ImGui::Separator();
    ImGui::TextUnformatted("This mode disables particle showcase effects so the screen-space PostEffect is easy to inspect.");
    ImGui::Spacing();
    ImGui::Columns(2, "PostEffectOnlyKeyColumns", false);
    constexpr int postEffectHotKeyCount =
        static_cast<int>(sizeof(kPostEffectHotKeys) / sizeof(kPostEffectHotKeys[0]));
    for (int i = 0; i < postEffectHotKeyCount; ++i) {
        const PostEffectHotKey& hotKey = kPostEffectHotKeys[i];
        ImGui::Text("%s : %s", hotKey.keyLabel, hotKey.effectName);
        if (i == 4) {
            ImGui::NextColumn();
        }
    }
    ImGui::Columns(1);
    ImGui::Spacing();
    ImGui::TextUnformatted("TAB : Back to Stage Select     MMB : Orbit     Shift+MMB : Pan     Wheel : Zoom");
    ImGui::End();
#endif
}


void MyGame::UpdateGamePlay() {
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    if (input->TriggerKey(DIK_C)) {
        useFirstPersonCamera_ = !useFirstPersonCamera_;
        if (useFirstPersonCamera_ && player_) {
            fpsCameraYaw_ = player_->GetRotation().y;
            fpsCameraPitch_ = 0.0f;
        }
    }

    if (!useFirstPersonCamera_) {
        if (input->TriggerKey(DIK_V)) {
            bool cur = gameplayCameraController_.IsFollowPlayerMode();
            gameplayCameraController_.SetFollowPlayerMode(!cur);
            if (!cur && player_) {
                Vector3 pp = player_->GetPosition();
                pp.y += 0.8f;
                gameplayCameraController_.SetCameraPivot(pp);
            } else if (cur && stageSelect_) {
                gameplayCameraController_.ResetCamera(
                    camera.get(),
                    player_.get(),
                    stageMap_,
                    stageSelect_->GetSelectedIndex());
            }
        }
        camera->SetFov(gameplayCameraController_.GetFov());
        gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(), player_.get());
    } else {
        camera->SetFov(0.9f);

        bool isGuiCaptured = false;
#ifndef NDEBUG
        isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
#endif
        const auto& mouse = input->GetMouseState();
        if (mouse.buttons[0] && !isGuiCaptured) {
            RECT rect;
            GetClientRect(winApp->GetHwnd(), &rect);
            float cw = static_cast<float>(rect.right - rect.left);
            float ch = static_cast<float>(rect.bottom - rect.top);
            if (cw > 0.0f && ch > 0.0f) {
                float sx = static_cast<float>(WinApp::kWindowWidth) / cw;
                float sy = static_cast<float>(WinApp::kWindowHeight) / ch;
                float mx = static_cast<float>(mouse.posX) * sx;
                float my = static_cast<float>(mouse.posY) * sy;

                const float edgeRatio = 0.15f;
                const float rotateSpeed = 0.03f;
                float leftEdge = WinApp::kWindowWidth * edgeRatio;
                float rightEdge = WinApp::kWindowWidth * (1.0f - edgeRatio);
                float topEdge = WinApp::kWindowHeight * edgeRatio;
                float bottomEdge = WinApp::kWindowHeight * (1.0f - edgeRatio);

                if (mx < leftEdge) {
                    fpsCameraYaw_ += rotateSpeed;
                } else if (mx > rightEdge) {
                    fpsCameraYaw_ -= rotateSpeed;
                }
                if (my < topEdge) {
                    fpsCameraPitch_ += rotateSpeed;
                } else if (my > bottomEdge) {
                    fpsCameraPitch_ -= rotateSpeed;
                }
            }
        }

        const float keyRotateSpeed = 0.03f;
        if (input->PushKey(DIK_LEFT)) { fpsCameraYaw_ += keyRotateSpeed; }
        if (input->PushKey(DIK_RIGHT)) { fpsCameraYaw_ -= keyRotateSpeed; }
        if (input->PushKey(DIK_UP)) { fpsCameraPitch_ += keyRotateSpeed; }
        if (input->PushKey(DIK_DOWN)) { fpsCameraPitch_ -= keyRotateSpeed; }
        fpsCameraPitch_ = std::clamp(fpsCameraPitch_, -1.4f, 1.4f);

        if (player_) {
            Vector3 pp = player_->GetPosition();
            camera->SetPosition({ pp.x, pp.y + 1.2f, pp.z });
            camera->SetRotation({ fpsCameraPitch_, fpsCameraYaw_, 0.0f });
        }
        camera->Update();
    }

    if (gameplayUIManager_) {
        gameplayUIManager_->UpdateCameraGuide(currentMode_ == AppMode::GamePlay, input.get(), winApp.get());
    }

    bool invOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();
    if (stageSelect_ && stageSelect_->GetSelectedFileName() == "tutorial.txt" && tutorialSprite_ && !invOpen) {
        tutorialSprite_->Update();
    }
    if ((currentMode_ == AppMode::GamePlay_BlockPlace || invOpen) && placementTutorialSprite_) {
        placementTutorialSprite_->Update();
    }

    float dt = 1.0f / 60.0f;
    totalTime_ += dt;
    stageMap_.Update(dt, player_ ? player_->GetPosition() : Vector3{ 0, 0, 0 });
    stageRenderer_->UpdateEffect(stageMap_);

    if (player_) {
        float camRot = useFirstPersonCamera_ ? fpsCameraYaw_ : gameplayCameraController_.GetAngle();
        player_->Update(input.get(), stageMap_, camRot, lightVP, dxCommon.get());
    }

    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->BuildFromStageMap(stageMap_);
        stageMap_.ClearRebuildFlag();
    }

    stageRespawnController_.Update(
        stageMap_,
        backupMap_,
        stageRenderer_.get(),
        player_.get(),
        &blockInventory_,
        &bubblePickupController_,
        &blockPlacementController_,
        &stageEditorController_);

    Vector3 pPos = player_ ? player_->GetPosition() : Vector3{};
    if (player_) {
        bubblePickupController_.Update(pPos);
    }

    if (Goal::Check(pPos, { 0.4f, 0.9f, 0.4f }, stageMap_)) {
        isGoalReached_ = true;
    }

    if (input->TriggerKey(DIK_B) && blockInventory_.HasBlock()) {
        if (blockInventoryUI_) {
            blockInventoryUI_->ToggleOpen();
        }
    }

    if (isGoalReached_) {
        // Game clear handling can be connected here when the clear scene exists.
    }
}

void MyGame::UpdateGamePlayBlockPlace() {
    const Int3& cursor = mapCursor_->GetIndex();

    if (input->TriggerKey(DIK_R)) {
        placeRotationY_ += 1.5707963f;
        if (placeRotationY_ >= 6.0f) {
            placeRotationY_ = 0.0f;
        }
    }

    stageEditorController_.HandleCursorInput(
        input.get(),
        stageMap_,
        mapCursor_.get(),
        lightCamera_.get(),
        camera.get());

    BlockType selectedType = BlockType::Ground;
    int selectedCustomId = 0;
    if (blockInventoryUI_) {
        selectedType = blockInventoryUI_->GetSelectedBlockType();
        selectedCustomId = blockInventoryUI_->GetSelectedCustomId();
        blockPlacementController_.SetPlaceBlockType(selectedType);
        blockPlacementController_.SetPlaceCustomId(selectedCustomId);
    }

    if (stageRenderer_) {
        stageRenderer_->SetPlacementPreview(stageMap_, cursor, selectedType, selectedCustomId, placeRotationY_);
    }

    static bool prevMouse0 = false;
    bool mouseJustPressed = input->GetMouseState().buttons[0] && !prevMouse0;
    prevMouse0 = input->GetMouseState().buttons[0];
    bool mouseTrigger = false;
    if (mouseJustPressed && (!blockInventoryUI_ || !blockInventoryUI_->IsActive())) {
        mouseTrigger = true;
    }

    if (input->TriggerKey(DIK_RETURN) || mouseTrigger) {
        if (blockPlacementController_.TryPlace(cursor, placeRotationY_)) {
            bool hasRest = (selectedType == BlockType::Ground)
                || blockInventory_.HasBlock(selectedType, selectedCustomId);
            if (!hasRest) {
                RequestSceneChange(SceneType::GamePlay);
                placeRotationY_ = 0.0f;
                if (stageRenderer_) {
                    stageRenderer_->ClearPlacementPreview();
                }
            }
        }
    }

    if (input->TriggerKey(DIK_ESCAPE) || input->TriggerKey(DIK_B)) {
        RequestSceneChange(SceneType::GamePlay);
        placeRotationY_ = 0.0f;
        if (stageRenderer_) {
            stageRenderer_->ClearPlacementPreview();
        }
    }

    stageEditorController_.HandleCameraInput(input.get(), camera.get());
}

// ステージ選択が完了したら、選択ステージを読み込んでゲーム開始状態にする。
void MyGame::UpdateStageSelect() {
    stageSelect_->Update();
    if (stageSelect_->IsFnished()) {
        std::string path = "Resources/Stages/" + stageSelect_->GetSelectedFileName();
        if (std::filesystem::exists(path)) {
            stageMap_.LoadFromFile(path);
            backupMap_ = stageMap_;
            stageRenderer_->BuildFromStageMap(stageMap_);

            playerBasePosition_.ApplyFromStageMap(stageMap_, player_.get());

            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
            gameplayCameraController_.ResetCamera(
                camera.get(), player_.get(), stageMap_, stageSelect_->GetSelectedIndex());
            blockInventory_.Initialize(0);
        }
        RequestSceneChange(SceneType::GamePlay);
    }
}

//  GamePlay 中に ESC が押されたらステージ選択へ戻す。

void MyGame::UpdateSceneTransition() {
    if ((currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)
        && input->TriggerKey(DIK_ESCAPE)) {
        stageMap_ = backupMap_;
        stageRenderer_->BuildFromStageMap(stageMap_);

        bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_);
        stageSelect_->Initialize(object3dCommon.get(), input.get());
        isGoalReached_ = false;
        if (player_) { player_->Respawn(); }
        RequestSceneChange(SceneType::StageSelect);
    }
}

void MyGame::UpdateBGM() {
    BgmType nextBgmType = BgmType::None;

    if (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) {
        nextBgmType = BgmType::Game;
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

