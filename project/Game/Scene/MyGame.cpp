#include <filesystem>

#include "MyGame.h"
#include "Goal.h"
#include "ModelManager.h"
#include <memory>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"



// --- MyGameクラスの実装 ---
void MyGame::Initialize() {
    // 基盤系の生成（new ではなく std::make_unique を使用） 
    winApp = std::make_unique<WinApp>();
    winApp->Initialize();

	// DirectXCommon の生成と初期化。Initialize には winApp の生ポインタを渡す
    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get()); // get() で中身の生ポインタを貸し出す 

	// Input クラスも同様に std::make_unique で生成し、Initialize には winApp の生ポインタを渡す
    input = std::make_unique<Input>();
    input->Initialize(winApp.get()); // get() を使用

	// TextureManager は SpriteCommon と Object3dCommon の両方で必要になるので、先に生成しておく
    textureManager = std::make_unique<TextureManager>();
    textureManager->Initialize(dxCommon.get());

	// SpriteCommon と Object3dCommon はテクスチャ管理も必要になるので、TextureManagerのセットを忘れずに
    spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->SetTextureManager(textureManager.get());
    spriteCommon->Initialize(dxCommon.get());

	// Object3dCommon はテクスチャ管理も必要になるので、TextureManagerのセットを忘れずに
    object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->SetTextureManager(textureManager.get());
    object3dCommon->Initialize(dxCommon.get());

	// ParticleManager も同様に TextureManager をセットして初期化
    particleManager = std::make_unique<ParticleManager>();
    particleManager->Initialize(dxCommon.get(), textureManager.get());

    // シーン管理 
    titleScene_ = std::make_unique<TitleScene>();
    titleScene_->Initialize(object3dCommon.get(), input.get());

    //4/20 5/10 小林
    stageSelect_ = std::make_unique<StageSelect>();
    stageSelect_->Initialize(object3dCommon.get(), input.get());

    gameClearScene_ = std::make_unique<GameClearScene>();
    gameClearScene_->Initialize(object3dCommon.get());

    // モデル読み込み（vector<unique_ptr<Model>> に入れるため unique_ptr で包む） 
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/block", "block.obj", textureManager.get())));
    models.push_back(std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/axis", "axis.obj", textureManager.get())));

    /*models.push_back(modelPlane);
    models.push_back(modelAxis);*/

    // --- オブジェクト生成 ---
    // models[index].get() で生ポインタを取得して渡す
    CreateObject(models[0].get(), { 0.0f, 0.0f, 0.0f })->SetScale({ 10.0f, 1.0f, 10.0f });
    CreateObject(models[1].get(), { 2.0f, 0.0f, 0.0f });
    CreateObject(models[1].get(), { -2.0f, 0.0f, 0.0f });

    // スプライト
    uint32_t texHandle = textureManager->LoadTexture("Resources/Models/axis/uvChecker.png");
    sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), texHandle);

    //サウンド初期化
    sound.Initialize();
    //読み込み
    wavSoundData = sound.SoundLoadFile("Resources/Sound/Alarm01.wav");

    mp4SoundData = sound.SoundLoadFile("Resources/Sound/AlarmMovie.mp4");

    mp3SoundData = sound.SoundLoadFile("Resources/Sound/maou_bgm_neorock83.mp3");

    // プレイヤーの生成
    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon.get(), models[0].get());
    player_->SetPosition({ 0.0f, 1.5f, 0.0f });

    // エディタ用カメラ
    camera = std::make_unique<Camera>();
#ifndef NDEBUG
    camera->SetAspectRatio(1280.0f / 720.0f);
#endif
   

    // 1. ステージマップのサイズ初期化
    stageMap_.Initialize(100, 100, 100);

    // ステージエディタ管理で初期化済み

    // --- 3. ビルド設定による初期化分岐 ---
#ifdef DEVELOPMENT
    // 【Developmentビルド時】デバッグビューモードから開始
    currentMode_ = AppMode::DebugView;

    // 天球は最初からOFF
    debugFlags_.showSkybox = false;

    // Offscreen Rendering は OFF
    offscreenEnabled_ = false;

    std::string prototypePath = "Resources/Stages/stage1.txt";
    if (std::filesystem::exists(prototypePath)) {
        stageMap_.LoadFromFile(prototypePath);
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    }
#elif defined(NDEBUG)
    // 【Releaseビルド時】タイトルから開始する
    currentMode_ = AppMode::Title;

    // 天球はON
    debugFlags_.showSkybox = true;

    // Offscreen Rendering は OFF
    offscreenEnabled_ = false;
#else
    // 【Debugビルド時】タイトルから開始
    currentMode_ = AppMode::Title;

    // 天球は最初からON
    debugFlags_.showSkybox = true;

    // 2. ★手動配置を消して、保存した「プロトタイプ」をロードする
    std::string prototypePath = "Resources/Stages/stage01.txt"; // 保存したファイル名に合わせてください
    if (std::filesystem::exists(prototypePath)) {
        stageMap_.LoadFromFile(prototypePath);
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    }
    // Offscreen Rendering は OFF
    offscreenEnabled_ = false;
#endif

    // 1. モデルのロード（フォルダとファイル名に注意）
    skydomeModel_ = std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/skydome", "skydome.obj", textureManager.get()));

    // 2. オブジェクトの生成と初期化
    skydomeObject_ = std::make_unique<Object3d>();
    skydomeObject_->Initialize(object3dCommon.get());
    skydomeObject_->SetModel(skydomeModel_.get());

    // 3. 設定：空は自ら光るのでライトをオフにする
    skydomeObject_->SetEnableLighting(false);

    // 4. 設定：ステージを包むサイズにする（カメラの Far Z が 100 なので 90 程度にする）
    skydomeObject_->SetScale({ 90.0f, 90.0f, 90.0f });

    // Skybox の初期化
    skyboxTextureHandle_ = textureManager->LoadTexture("Resources/dds/rostock_laage_airport_4k.dds");
    skybox_ = std::make_unique<Skybox>();
    skybox_->Initialize(object3dCommon.get(), skyboxTextureHandle_);
    skybox_->SetScale({ 50.0f, 50.0f, 50.0f }); // デフォルトサイズ

    // ステージ描画オブジェクトの生成と構築
    stageRenderer_ = std::make_unique<StageRenderer>();
    stageRenderer_->Initialize(object3dCommon.get());
    stageRenderer_->SetBlockScale({ 1.0f, 1.0f, 1.0f });
    stageRenderer_->BuildFromStageMap(stageMap_);

    // マップカーソルの初期化
    mapCursor_ = std::make_unique<MapCursor>();
    mapCursor_->Initialize(object3dCommon.get());
    mapCursor_->SetIndex({ 0, 0, 0 }, stageMap_);
    mapCursor_->SetScale({ 0.9f, 0.9f, 0.9f });

    // ★ 影の初期化
    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon.get(), textureManager.get());

    lightCamera_ = std::make_unique<LightCamera>();
    lightCamera_->Initialize();

    bubblePickupController_.Initialize(
    &stageMap_,
    stageRenderer_.get(),
    &blockInventory_
    );

    blockPlacementController_.Initialize(
        &stageMap_,
        stageRenderer_.get(),
        &blockInventory_
    );
    // UI管理の初期化
    gameplayUIManager_ = std::make_unique<GameplayUIManager>();
    gameplayUIManager_->Initialize(dxCommon.get(), textureManager.get(), spriteCommon.get(), object3dCommon.get());

    // インベントリの初期化
    blockInventory_.Initialize(0);

    // インベントリUIの初期化
    blockInventoryUI_ = std::make_unique<BlockInventoryUI>();
    blockInventoryUI_->Initialize(dxCommon.get(), spriteCommon.get(), textureManager.get(), &blockInventory_);

    //チュートリアル説明の初期化
    uint32_t tutorialTex = textureManager->LoadTexture("Resources/UI/tutorial/tutorial.png");
    tutorialSprite_ = std::make_unique<Sprite>();
    tutorialSprite_->Initialize(spriteCommon.get(), tutorialTex);
    tutorialSprite_->SetPosition({20, 20});
    // tutorial.png は 832x192px → 縮小率 0.666 で 554x128 に表示
    tutorialSprite_->SetSize({554, 128});

    //配置チュートリアル説明の初期化
    uint32_t placementTutorialTex = textureManager->LoadTexture("Resources/UI/tutorial/placement_tutorial.png");
    placementTutorialSprite_ = std::make_unique<Sprite>();
    placementTutorialSprite_->Initialize(spriteCommon.get(), placementTutorialTex);
    placementTutorialSprite_->SetPosition({20, 20});
    // placement_tutorial.png は 1024x278px → 縮小率 0.666 で 682x185 に表示
    placementTutorialSprite_->SetSize({682, 185});

    gameplayCameraController_.Initialize();
    stageEditorController_.Initialize();

    // スキニングオブジェクトとデバッグ用の立方体モデルを初期化
    skinnedObject_ = std::make_unique<SkinnedObject>();
    skinnedObject_->Initialize(object3dCommon.get(), dxCommon.get(), textureManager.get());
    skinnedObject_->SetPosition({ 0.0f, 0.0f, 0.0f }); // 地面(Y=0.0f)に接地させる
    skinnedObject_->SetScale({ 1.0f, 1.0f, 1.0f });

    debugCubeModel_ = std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/cube", "cube.obj", textureManager.get()));

    // 地面グリッド線の生成 (-10m から 10m まで 1m刻み)
    for (int i = -10; i <= 10; ++i) {
        // X方向に並び、Z方向に伸びる線 (縦線)
        auto lineX = std::make_unique<Object3d>();
        lineX->Initialize(object3dCommon.get());
        lineX->SetModel(debugCubeModel_.get());
        lineX->SetPosition({ (float)i, 0.0f, 0.0f });
        lineX->SetScale({ 0.015f, 0.002f, 10.0f }); // 極めて細長く、薄い
        lineX->SetRotation({ 0.0f, 0.0f, 0.0f });
        lineX->SetEnableLighting(false);
        // 中央(X=0)は赤色(X軸)、他はグレー
        lineX->SetColor((i == 0) ? Vector4{ 0.8f, 0.2f, 0.2f, 1.0f } : Vector4{ 0.35f, 0.35f, 0.38f, 1.0f });
        gridLines_.push_back(std::move(lineX));

        // Z方向に並び、X方向に伸びる線 (横線)
        auto lineZ = std::make_unique<Object3d>();
        lineZ->Initialize(object3dCommon.get());
        lineZ->SetModel(debugCubeModel_.get());
        lineZ->SetPosition({ 0.0f, 0.0f, (float)i });
        lineZ->SetScale({ 10.0f, 0.002f, 0.015f });
        lineZ->SetRotation({ 0.0f, 0.0f, 0.0f });
        lineZ->SetEnableLighting(false);
        // 中央(Z=0)は青色(Z軸)、他はグレー
        lineZ->SetColor((i == 0) ? Vector4{ 0.2f, 0.2f, 0.8f, 1.0f } : Vector4{ 0.35f, 0.35f, 0.38f, 1.0f });
        gridLines_.push_back(std::move(lineZ));
    }

    // オフスクリーンレンダリングの初期化
    InitializeOffscreenRendering();
}

// ヘルパー関数：モデルと位置を指定して3Dオブジェクトを生成し、リストに追加して返す
Object3d* MyGame::CreateObject(Model* model, Vector3 pos) {
    auto obj = std::make_unique<Object3d>(); // 一時的な unique_ptr 
    obj->Initialize(object3dCommon.get());
    obj->SetModel(model);
    obj->SetPosition(pos);
    obj->SetRotation({ 1.57f, 0.0f, 0.0f });

    Object3d* ptr = obj.get(); // 戻り値用に中身の住所を控える
    objectList.push_back(std::move(obj)); // push_back で vector に「所有権」を移動させる 
    return ptr;
}

// --- 更新処理 ---
void MyGame::Update() {
    // モード切り替えを検知してカメラを自動初期化 (起動時含む)
    if (currentMode_ != prevMode_) {
        if (currentMode_ == AppMode::SkinningEditor) {
            camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
        }
        prevMode_ = currentMode_;
    }

    // ImGuiはDebug（Release以外）ビルドでのみ描画・更新する
#ifndef NDEBUG
    dxCommon->BeginImGui();
    UpdateImGui();
#endif


    input->Update();
    UpdateSceneTransition();//05/14小林 ESCでステージ選択に戻る
    bool isGuiCaptured = false;
    // 2. カメラの更新（Blender風操作を適用）
#ifndef NDEBUG
    // Release時は ImGui::GetIO() を呼ばないようにガードする
    // マウスのみをガード (キーボードフォーカスがImGuiにあっても中ドラッグ等は反応させる)
    isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
#endif


    // ライトカメラの更新（プレイヤーやカメラの位置に追従させる）
    if (lightCamera_) {
        Vector3 targetPos = player_ ? player_->GetPosition() : camera->GetPosition();
        lightCamera_->Update({ 0.2f, -1.0f, 0.5f }, targetPos);
    }

    //  ここで「ライト視点の行列」を取得！これが全ての lightVP
    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // --- 3. カメラの更新 ---
    if (currentMode_ != AppMode::GamePlay) {
        camera->UpdateBlenderStyle(input.get(), isGuiCaptured, winApp->GetHwnd());
    }

    // 天球とスカイボックスの更新 (描画フラグが有効な時のみ更新して負荷を削減)
    if (skydomeObject_ && debugFlags_.showSkybox && !showSkyboxCubemap_) {
        skydomeObject_->SetPosition(camera->GetPosition());
        skydomeObject_->Update(Math::MakeIdentity4x4());
    }

    if (skybox_ && showSkyboxCubemap_) {
        skybox_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        skybox_->SetPosition(camera->GetPosition());
        skybox_->Update();
    }

    //試しにサウンド更新 
    // SPACEでwav再生
    if (input->TriggerKey(DIK_SPACE)) {
        sound.SoundPlay(wavSoundData, wavVolume);
    }

    // Mキーでmp4音声再生
    if (input->TriggerKey(DIK_M)) {
        sound.SoundPlay(mp4SoundData, mp4Volume);

    }

    if (input->TriggerKey(DIK_N)) {
        sound.SoundPlay(mp3SoundData, mp3Volume);
    }

    //mp3版音量変更キー
    if (input->TriggerKey(DIK_UP)) {
        mp3Volume += 0.1f;
        if (mp3Volume > 1.0f) {
            mp3Volume = 1.0f;
        }
        OutputDebugStringA("[MyGame] mp3 音量アップ\n");
    }

    if (input->TriggerKey(DIK_DOWN)) {
        mp3Volume -= 0.1f;
        if (mp3Volume < 0.0f) {
            mp3Volume = 0.0f;
        }
        OutputDebugStringA("[MyGame] mp3 音量ダウン\n");
    }
   
    // --- ゲームループの更新 ---
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
        stageEditorController_.Update(input.get(), stageMap_, stageRenderer_.get(), mapCursor_.get(), lightCamera_.get(), player_.get(), camera.get());
        break;

    case AppMode::GamePlay:
        UpdateGamePlay();
        break;

    // ブロックを置けるようになる画面 04/01 秋元
    case AppMode::GamePlay_BlockPlace:
        UpdateGamePlayBlockPlace();
        break;

    case AppMode::GameClear://4/13佐倉
        // ▼ SPACEで演出スキップ
        if (input->TriggerKey(DIK_SPACE)) {

            // まだ演出途中ならスキップ
            if (!gameClearScene_->IsFinished()) {
                gameClearScene_->SkipAnimation();
            }
            // 演出終了後ならステージ選択へ戻る
            else {

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
        if (skinnedObject_) {
            // --- レイキャストによるジョイントクリック選択判定 ---
            const auto& mouse = input->GetMouseState();
            static bool mouse0Pre = false;
            bool mouse0Trigger = mouse.buttons[0] && !mouse0Pre;
            mouse0Pre = mouse.buttons[0];

            if (mouse0Trigger && !isGuiCaptured) {
                // デバッグ時は左側に320pxのオフセットがあるため、マウスのX座標を調整
                float mouseX = static_cast<float>(mouse.posX);
#ifndef NDEBUG
                mouseX -= 320.0f;          // ビューポートのXオフセット
                float drawWidth = 1280.0f; // ビューポートの幅
#else
                float drawWidth = (float)WinApp::kClientWidth;
#endif
                float ndcX = (2.0f * mouseX) / drawWidth - 1.0f;
                float ndcY = 1.0f - (2.0f * static_cast<float>(mouse.posY)) / WinApp::kClientHeight;

                // 逆ビュー・プロジェクション行列
                Matrix4x4 vp = Math::Multiply(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                Matrix4x4 invVP = Math::Inverse(vp);

                // クリップ空間
                Vector4 nearClip = { ndcX, ndcY, 0.0f, 1.0f };
                Vector4 farClip = { ndcX, ndcY, 1.0f, 1.0f };

                // 逆投影ヘルパー
                auto transformVec = [](const Vector4& v, const Matrix4x4& m) -> Vector4 {
                    Vector4 r;
                    r.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0];
                    r.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1];
                    r.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2];
                    r.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3];
                    return r;
                };

                Vector4 nearWorld4 = transformVec(nearClip, invVP);
                Vector4 farWorld4 = transformVec(farClip, invVP);

                Vector3 nearWorld = { nearWorld4.x / nearWorld4.w, nearWorld4.y / nearWorld4.w, nearWorld4.z / nearWorld4.w };
                Vector3 farWorld = { farWorld4.x / farWorld4.w, farWorld4.y / farWorld4.w, farWorld4.z / farWorld4.w };

                Vector3 rayOrigin = nearWorld;
                Vector3 rayDir = Math::Normalize(Math::Subtract(farWorld, nearWorld));

                // 各関節のワールド位置を取得
                const auto& joints = skinnedObject_->GetModel()->GetJoints();
                Matrix4x4 objWorld = Math::MakeAffineMatrix(
                    skinnedObject_->GetScale(),
                    skinnedObject_->GetRotation(),
                    skinnedObject_->GetPosition()
                );

                int closestJointIndex = -1;
                float minT = FLT_MAX;
                float clickRadius = 0.22f; // ボーンがクリックしやすいよう当たり判定を少し大きめに

                for (size_t i = 0; i < joints.size(); ++i) {
                    Matrix4x4 jointWorld = Math::Multiply(joints[i].globalMatrix, objWorld);
                    Vector3 jointPos = { jointWorld.m[3][0], jointWorld.m[3][1], jointWorld.m[3][2] };

                    // レイと球の交差判定 (m = rayOrigin - jointPos)
                    Vector3 m = Math::Subtract(rayOrigin, jointPos);
                    float b = m.x * rayDir.x + m.y * rayDir.y + m.z * rayDir.z; // 内積
                    float c = (m.x * m.x + m.y * m.y + m.z * m.z) - (clickRadius * clickRadius);

                    if (c > 0.0f && b > 0.0f) continue; // 球の外側かつ逆方向

                    float discr = b * b - c;
                    if (discr < 0.0f) continue; // 交点なし

                    float t = -b - std::sqrt(discr);
                    if (t < 0.0f) t = 0.0f;

                    if (t < minT) {
                        minT = t;
                        closestJointIndex = static_cast<int>(i);
                    }
                }

                // 最も手前にある関節を選択状態にする
                if (closestJointIndex != -1) {
                    skinnedObject_->SetSelectedJointIndex(closestJointIndex);
                }
            }

            skinnedObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
            skinnedObject_->Update(dxCommon.get(), lightVP);

            // ★デバッググリッド線の更新はSkinningEditorの時のみ、ここで1回だけ行う
            for (auto& line : gridLines_) {
                line->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                line->Update(lightVP);
            }
        }
        break;

        gameClearScene_->Update();
    }

    camera->Update();


    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();

    // ライト視点の行列を取得しておきます
    //const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // --- プレイヤーに最新のカメラ行列を教える ---
    if (player_) {
        // カメラ行列は常にセット
        player_->SetCamera(view, proj);

        // プレイモード中のみ移動などのロジックを更新
        if (currentMode_ == AppMode::GamePlay) {
            // ここは既存のコード（UpdateGamePlayの中から移動させてもOKです）
            // player_->Update(...) 
        } else {
            // ★ エディタモード等では、見た目（行列）の更新だけを行う
            player_->UpdateTransform(lightVP);
        }
    }

    // ★ 修正1：ウィンドウが最前面にない場合は即リターンして何もしない
    // これにより、ウィンドウ外をクリックした瞬間の挙動を無視できます
    if (GetActiveWindow() != winApp->GetHwnd()) {
        return;
    }

	// 3Dオブジェクトの更新 (DebugViewモードの時のみ更新して定数バッファ転送負荷を削減)
    if (debugFlags_.show3DObjects && currentMode_ == AppMode::DebugView) {
        for (auto& obj : objectList) {
            if (obj) {
                obj->SetCamera(view, proj);
                obj->Update(lightVP);
            }
        }
    }

	// ステージ描画オブジェクトの更新 (StageRenderer内部でのDirtyフラグ最適化に対応)
    if (stageRenderer_) {
        stageRenderer_->SetIsEditorMode(currentMode_ == AppMode::StageEditor);
        stageRenderer_->SetCamera(view, proj);
        stageRenderer_->Update(stageMap_, lightVP);
    }

    if (stageRenderer_ && player_ && camera) {
        stageRenderer_->UpdateWallTransparency(
            camera->GetPosition(),
            player_->GetPosition()
        );
    }

	// マップカーソルの更新 (エディタモードまたは配置モードの時のみ更新)
    if (mapCursor_ && (currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        mapCursor_->SetCamera(view, proj);
        mapCursor_->Update(lightVP);
    }

	// スプライトの更新
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        sprite->Update();
    }

	// パーティクルの更新
    if (debugFlags_.showParticles) {
        particleManager->Update(view, proj);
    }

    // ★ ライトカメラの更新
    // プレイヤーの位置に合わせて影の範囲を動かすことで、常に綺麗な影を出します
    Vector3 lightDir = stageMap_.GetLightDirection();
    lightCamera_->Update(lightDir, player_->GetPosition());

    object3dCommon->SetLightDirection(lightDir);
    object3dCommon->SetLightColor(Vector4(stageMap_.GetLightColor().x, stageMap_.GetLightColor().y, stageMap_.GetLightColor().z, 1.0f));
    object3dCommon->SetLightIntensity(stageMap_.GetLightIntensity());
    object3dCommon->SetCameraPosition(camera->GetPosition());

    // クリアカラー（背景色）をステージ設定と同期
    offscreenClearColor_ = stageMap_.GetClearColor();

    // UI・プロンプト更新
    if (gameplayUIManager_) {
        gameplayUIManager_->Update(currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace, player_.get(), camera.get(), lightCamera_.get());
    }

    if (blockInventoryUI_) {
        bool isPlayOrPlace = (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);
        blockInventoryUI_->Update(input.get(), winApp.get(), isPlayOrPlace, &stageMap_);

        // ダブルクリックによる配置モード移行要求を処理
        // （即時配置はせず、配置モードに移行してプレイヤーがカーソル操作して置く）
        if (blockInventoryUI_->ConsumeUseRequest()) {
            // 配置モードに移行（インベントリはダブルクリック時にToggleOpenで閉じている）
            currentMode_ = AppMode::GamePlay_BlockPlace;

            // カーソルをプレイヤー位置に初期化
            Vector3 pPosNow = player_ ? player_->GetPosition() : Vector3{0,0,0};
            int igx = static_cast<int>(std::floor(pPosNow.x + 0.5f));
            int igy = static_cast<int>(std::floor(pPosNow.y));
            int igz = static_cast<int>(std::floor(pPosNow.z + 0.5f));
            mapCursor_->SetIndex({ igx, igy, igz }, stageMap_);

            // 選択されたブロックタイプとカスタムIDをコントローラーに同期
            BlockType doubleClickedType = blockInventoryUI_->GetSelectedBlockType();
            int selectedCustomId = blockInventoryUI_->GetSelectedCustomId();
            blockPlacementController_.SetPlaceBlockType(doubleClickedType);
            blockPlacementController_.SetPlaceCustomId(selectedCustomId);
        }
    }
}

// パーティクル発生テスト（スペースキーを押すと発生）
void MyGame::UpdateDebugView() {
    if (input->TriggerKey(DIK_SPACE)) {
        particleManager->Emit({ 0, 0, 0 }, 10);
    }
    // デバッグモードでもWASDカーソル移動をカメラ相対で動かす
    stageEditorController_.HandleCursorInput(input.get(), stageMap_, mapCursor_.get(), lightCamera_.get(), camera.get());
}

void MyGame::UpdateGamePlay() {
    // Cキーでカメラ切り替え (トグル)
    if (input->TriggerKey(DIK_C)) {
        useFirstPersonCamera_ = !useFirstPersonCamera_;
        if (useFirstPersonCamera_ && player_) {
            // 一人称に切り替えた瞬間、カメラの向きをプレイヤーの向きと水平方向に同期する
            fpsCameraYaw_ = player_->GetRotation().y;
            fpsCameraPitch_ = 0.0f;
        }
    }

    if (!useFirstPersonCamera_) {
        camera->SetFov(gameplayCameraController_.GetFov()); // 三人称は元のFOVに戻す
        gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(), player_.get());
    } else {
        camera->SetFov(0.9f); // 一人称視点は高FOV(広角)にする！

        // 一人称カメラ (FPS Camera) の更新
        const auto& mouse = input->GetMouseState();
        bool isGuiCaptured = false;
        #ifndef NDEBUG
        isGuiCaptured = ImGui::GetIO().WantCaptureMouse;
        #endif

        if (mouse.buttons[0] && !isGuiCaptured) {
            RECT rect;
            GetClientRect(winApp->GetHwnd(), &rect);
            float currentClientW = static_cast<float>(rect.right - rect.left);
            float currentClientH = static_cast<float>(rect.bottom - rect.top);
            if (currentClientW > 0.0f && currentClientH > 0.0f) {
                float scaleX = static_cast<float>(WinApp::kWindowWidth) / currentClientW;
                float scaleY = static_cast<float>(WinApp::kWindowHeight) / currentClientH;
                float mouseX = static_cast<float>(mouse.posX) * scaleX;
                float mouseY = static_cast<float>(mouse.posY) * scaleY;

                float edgeRatio = 0.15f;
                float leftEdge = WinApp::kWindowWidth * edgeRatio;
                float rightEdge = WinApp::kWindowWidth * (1.0f - edgeRatio);
                float topEdge = WinApp::kWindowHeight * edgeRatio;
                float bottomEdge = WinApp::kWindowHeight * (1.0f - edgeRatio);

                const float rotateSpeed = 0.03f;
                if (mouseX < leftEdge) fpsCameraYaw_ += rotateSpeed;
                else if (mouseX > rightEdge) fpsCameraYaw_ -= rotateSpeed;
                if (mouseY < topEdge) fpsCameraPitch_ += rotateSpeed;
                else if (mouseY > bottomEdge) fpsCameraPitch_ -= rotateSpeed;
            }
        }

        // 矢印キーによるカメラ回転操作
        const float keyRotateSpeed = 0.03f;
        if (input->PushKey(DIK_LEFT))  fpsCameraYaw_ += keyRotateSpeed;
        if (input->PushKey(DIK_RIGHT)) fpsCameraYaw_ -= keyRotateSpeed;
        if (input->PushKey(DIK_UP))    fpsCameraPitch_ += keyRotateSpeed;
        if (input->PushKey(DIK_DOWN))  fpsCameraPitch_ -= keyRotateSpeed;

        fpsCameraPitch_ = std::clamp(fpsCameraPitch_, -1.4f, 1.4f);

        if (player_) {
            Vector3 playerPos = player_->GetPosition();
            // プレイヤーの頭の高さ (目の高さ: Y + 1.2f) にカメラを配置
            Vector3 cameraPos = { playerPos.x, playerPos.y + 1.2f, playerPos.z };
            camera->SetPosition(cameraPos);
            camera->SetRotation({ fpsCameraPitch_, fpsCameraYaw_, 0.0f });
        }
        camera->Update();
    }
    // --- ステージマップの更新、崩れる足場のタイマー処理 ---

    // 5/14佐倉追加
    if (gameplayUIManager_) {
        gameplayUIManager_->UpdateCameraGuide(
            currentMode_ == AppMode::GamePlay,
            input.get(),
            winApp.get()
        );
    }

    //チュートリアルUI　05/21 小林
    bool inventoryOpenForUpdate = blockInventoryUI_ && blockInventoryUI_->IsActive();

    if (stageSelect_)
    {
        std::string currentStage = stageSelect_->GetSelectedFileName();
        // 通常プレイ中のみ操作チュートリアルをUpdate
        if (currentStage == "tutorial.txt" && tutorialSprite_ && !inventoryOpenForUpdate)
        {
            tutorialSprite_->Update();
        }
    }

    // 配置チュートリアルはインベントリが開いている時にUpdate
    if ((currentMode_ == AppMode::GamePlay_BlockPlace || inventoryOpenForUpdate) && placementTutorialSprite_)
    {
        placementTutorialSprite_->Update();
    }

    float deltaTime = 1.0f / 60.0f;
    totalTime_ += deltaTime;
    stageMap_.Update(deltaTime, totalTime_, player_ ? player_->GetPosition() : Vector3{0.0f, 0.0f, 0.0f});

    stageRenderer_->UpdateEffect(stageMap_);

    // --- プレイヤー更新 ---
    if (player_) {
        float cameraRotY = useFirstPersonCamera_ ? fpsCameraYaw_ : gameplayCameraController_.GetAngle();
        player_->Update(input.get(), stageMap_, cameraRotY, lightCamera_->GetViewProjectionMatrix());
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
        &stageEditorController_
    );

   
    /*==================================================
    ▼ プレイヤー座標取得
    ==================================================*/
    Vector3 pPos{};
    if (player_) {
        pPos = player_->GetPosition();
    }

    /*==================================================
    ▼ シャボン玉取得
    ==================================================*/
    if (player_) {
        bubblePickupController_.Update(pPos);
    }

    int gx = static_cast<int>(std::floor(pPos.x + 0.5f));
    int gy = static_cast<int>(std::floor(pPos.y));
    int gz = static_cast<int>(std::floor(pPos.z + 0.5f));

    /*==================================================
        ▼ ゴール判定（★追加部分）
    ==================================================*/
    Vector3 radius = { 0.4f, 0.9f, 0.4f }; // プレイヤーサイズに合わせる

    if (Goal::Check(pPos, radius, stageMap_)) {
        isGoalReached_ = true;
    }

    /*==================================================
        ▼ インベントリを開く
        Bキーでインベントリを開く。配置モードへの移行はダブルクリック時のみ。
    ==================================================*/
    if (input->TriggerKey(DIK_B) && blockInventory_.HasBlock()) {
        if (blockInventoryUI_) {
            blockInventoryUI_->ToggleOpen(); // Bキーでインベントリを開閉
        }
    }

    /*==================================================
        ▼ クリア遷移
    ==================================================*/
    if (isGoalReached_) {
        currentMode_ = AppMode::GameClear;
    }
}

#ifndef NDEBUG
// ImGuiの更新と描画
void MyGame::UpdateImGui() {
    ImGuiIO& io = ImGui::GetIO();
    float panelWidth = 320.0f;
    float bottomHeight = 360.0f; // 下パネルのサイズを大きくしてピッタリはめる

    // ==========================================
    // 1. 左パネル (Information)
    // ==========================================
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y - bottomHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f); // 透過なし
    ImGui::Begin("Information", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    ImGui::Text("FPS: %.1f (%.3f ms/f)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::SameLine(panelWidth - 60.0f);
    if (ImGui::Button("Exit", ImVec2(50, 20))) {
        PostQuitMessage(0); // アプリケーション終了
    }
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Hierarchy / Mode", ImGuiTreeNodeFlags_DefaultOpen)) {
        int modeIndex = 0;
        switch (currentMode_) {
        case AppMode::DebugView:      modeIndex = 0; break;
        case AppMode::StageEditor:    modeIndex = 1; break;
        case AppMode::GamePlay:       modeIndex = 2; break;
        case AppMode::SkinningEditor: modeIndex = 3; break;
        }

        const char* modeNames[] = { "DebugView", "StageEditor", "GamePlay", "SkinningEditor" };
        if (ImGui::Combo("App Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
            switch (modeIndex) {
            case 0: currentMode_ = AppMode::DebugView; break;
            case 1: currentMode_ = AppMode::StageEditor; break;
            case 2: currentMode_ = AppMode::GamePlay; break;
            case 3: 
                currentMode_ = AppMode::SkinningEditor; 
                camera->ForceReset({ 0.0f, 1.0f, 0.0f }, 3.5f, { 0.1f, 0.0f, 0.0f });
                break;
            }
        }
        ImGui::Checkbox("Show 3D Objects", &debugFlags_.show3DObjects);
        ImGui::Checkbox("Show Skybox", &debugFlags_.showSkybox);
        ImGui::Checkbox("Show Skybox (Cubemap)", &showSkyboxCubemap_);
        ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
        ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);
    }

    if (ImGui::CollapsingHeader("Offscreen Rendering (RenderTexture)", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable Offscreen Rendering", &offscreenEnabled_);
        ImGui::ColorEdit4("Clear Color (VRAM)", &offscreenClearColor_.x);

        const char* skyboxModes[] = { "Ignore", "Link (Multiply)" };
        ImGui::Combo("Skybox Color Link", &skyboxLinkMode_, skyboxModes, IM_ARRAYSIZE(skyboxModes));

        const char* effectNames[] = { "Normal", "Grayscale", "Sepia" };
        ImGui::Combo("Post Effect", &postEffectMode_, effectNames, IM_ARRAYSIZE(effectNames));
    }

    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Use First-Person Camera", &useFirstPersonCamera_);
        if (useFirstPersonCamera_) {
            ImGui::SliderFloat("FPS Camera Yaw", &fpsCameraYaw_, -6.28f, 6.28f);
            ImGui::SliderFloat("FPS Camera Pitch", &fpsCameraPitch_, -1.4f, 1.4f);
        }
        camera->DrawImGui();
        // ゲームプレイモード時はImGuiで変更されたFOVをコントローラに書き戻す
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
    ImGui::End();

    // ==========================================
    // 2. 右パネル (Stage Editor)
    // ==========================================
    stageEditorController_.DrawImGui(stageMap_, stageRenderer_.get(), mapCursor_.get(), player_.get());

    // ==========================================
    // 3. 下パネル (Tools & Controls)
    // ==========================================
    ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - bottomHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, bottomHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f); // 透過なし
    ImGui::Begin("Tools & Controls", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

    if (currentMode_ == AppMode::SkinningEditor && skinnedObject_) {
        // --- スキニングエディター用のビジュアルタイムライン ＆ トラック表示 ---
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Custom Motion Animation Timeline ]");
        
        auto* model = skinnedObject_->GetModel();
        float duration = model->GetMotionDuration();
        float curTime = skinnedObject_->GetCurrentKeyframeTime();
        bool playCustom = skinnedObject_->IsPlayCustomAnimation();

        // タイムラインシークスライダー
        ImGui::PushItemWidth(-1.0f);
        if (ImGui::SliderFloat("##TimelineSlider", &curTime, 0.0f, duration, "Current Time: %.2f sec / %.2f sec")) {
            skinnedObject_->SetCurrentKeyframeTime(curTime);
            if (!playCustom) {
                skinnedObject_->ApplyMotion(curTime);
            }
        }
        ImGui::PopItemWidth();

        // トラックのビジュアル描画
        float width = ImGui::GetContentRegionAvail().x;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
        
        float trackHeight = 22.0f;
        ImVec2 trackMin = cursorScreenPos;
        ImVec2 trackMax = ImVec2(trackMin.x + width, trackMin.y + trackHeight);
        
        // 背景トラック
        drawList->AddRectFilled(trackMin, trackMax, IM_COL32(40, 40, 42, 255), 4.0f);
        drawList->AddRect(trackMin, trackMax, IM_COL32(80, 80, 85, 255), 4.0f);
        
        // 目盛り (0.1秒ごと)
        for (float t = 0.0f; t <= duration; t += 0.1f) {
            float ratio = t / duration;
            float posX = trackMin.x + ratio * width;
            float lineLen = (std::fmod(t, 0.5f) < 0.01f || std::abs(t - duration) < 0.01f) ? 14.0f : 7.0f;
            drawList->AddLine(ImVec2(posX, trackMin.y), ImVec2(posX, trackMin.y + lineLen), IM_COL32(130, 130, 135, 255));
        }

        // キーフレームマーク（ひし形）の描画
        const auto& motionData = model->GetMotionData();
        std::vector<float> kfTimes;
        if (!motionData.jointAnimations.empty()) {
            for (const auto& kf : motionData.jointAnimations[0].keyframes) {
                kfTimes.push_back(kf.time);
            }
        }

        for (float kfTime : kfTimes) {
            float ratio = kfTime / duration;
            float posX = trackMin.x + ratio * width;
            ImVec2 center = ImVec2(posX, trackMin.y + trackHeight * 0.5f);
            float r = 6.0f;
            drawList->AddQuadFilled(
                ImVec2(center.x, center.y - r),
                ImVec2(center.x + r, center.y),
                ImVec2(center.x, center.y + r),
                ImVec2(center.x - r, center.y),
                IM_COL32(255, 196, 0, 255)
            );
            drawList->AddQuad(
                ImVec2(center.x, center.y - r),
                ImVec2(center.x + r, center.y),
                ImVec2(center.x, center.y + r),
                ImVec2(center.x - r, center.y),
                IM_COL32(255, 255, 255, 200)
            );
        }

        // 再生時間の縦線バーカーソル
        float currentRatio = curTime / duration;
        float cursorX = trackMin.x + currentRatio * width;
        drawList->AddLine(ImVec2(cursorX, trackMin.y - 3.0f), ImVec2(cursorX, trackMax.y + 3.0f), IM_COL32(255, 60, 60, 255), 2.5f);
        drawList->AddTriangleFilled(
            ImVec2(cursorX - 5.0f, trackMin.y - 3.0f),
            ImVec2(cursorX + 5.0f, trackMin.y - 3.0f),
            ImVec2(cursorX, trackMin.y + 4.0f),
            IM_COL32(255, 60, 60, 255)
        );

        ImGui::Dummy(ImVec2(0.0f, trackHeight + 8.0f));

        // タイムライン詳細リスト
        ImGui::Separator();
        ImGui::BeginChild("KeyframeDetails", ImVec2(0, 0), true);
        ImGui::Columns(3, "TimelineColumns", false);
        ImGui::SetColumnWidth(0, 160.0f);
        ImGui::SetColumnWidth(1, width - 400.0f);
        ImGui::SetColumnWidth(2, 240.0f);

        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Joint Name");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Registered Keyframes (Click to jump / preview)");
        ImGui::NextColumn();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "Current Trans / Rot (Euler)");
        ImGui::NextColumn();
        ImGui::Separator();

        auto& joints = model->GetJoints();
        for (size_t i = 0; i < motionData.jointAnimations.size(); ++i) {
            const auto& anim = motionData.jointAnimations[i];
            
            if (static_cast<int>(i) == skinnedObject_->GetSelectedJointIndex()) {
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s (Selected)", anim.name.c_str());
            } else {
                ImGui::Text("%s", anim.name.c_str());
            }
            ImGui::NextColumn();

            if (anim.keyframes.empty()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No keyframes registered.");
            } else {
                for (size_t k = 0; k < anim.keyframes.size(); ++k) {
                    char btnLabel[64];
                    sprintf_s(btnLabel, "%.2fs##%d_%d", anim.keyframes[k].time, (int)i, (int)k);
                    if (ImGui::Button(btnLabel, ImVec2(48, 20))) {
                        skinnedObject_->SetCurrentKeyframeTime(anim.keyframes[k].time);
                        skinnedObject_->ApplyMotion(anim.keyframes[k].time);
                    }
                    ImGui::SameLine();
                }
            }
            ImGui::NextColumn();

            if (i < joints.size()) {
                Vector3 rotDeg = {
                    joints[i].rotation.x * 180.0f / 3.14159265f,
                    joints[i].rotation.y * 180.0f / 3.14159265f,
                    joints[i].rotation.z * 180.0f / 3.14159265f
                };
                ImGui::Text("T:(%.1f, %.1f) R:(%.0f, %.0f, %.0f)", 
                    joints[i].translation.x, joints[i].translation.y,
                    rotDeg.x, rotDeg.y, rotDeg.z);
            }
            ImGui::NextColumn();
            ImGui::Separator();
        }
        ImGui::EndChild();

    } else if (currentMode_ == AppMode::GamePlay && player_) {
        // --- ゲームプレイ時のリアルタイムデバッグ情報 ---
        ImGui::Columns(2, "GameplayColumns", false);
        
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Game Controls & Objective ]");
        ImGui::Text("A / D : Move Left / Right");
        ImGui::Text("SPACE : Jump (Can hold for higher jump)");
        ImGui::Text("B     : Enter Block Placement Mode (Inventory)");
        ImGui::Text("ESC   : Return to Stage Select");
        ImGui::Separator();
        ImGui::Text("Objective: Pick up bubbles and reach the Green Goal flag!");

        ImGui::NextColumn();

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Real-time Player Debug Physics ]");
        Vector3 pos = player_->GetPosition();
        ImGui::Text("Position: X: %.3f, Y: %.3f, Z: %.3f", pos.x, pos.y, pos.z);
        
        if (isGoalReached_) {
            ImGui::TextColored(ImVec4(0.2f, 1.0f, 0.2f, 1.0f), "GOAL REACHED! Transitioning to Clear Scene...");
        } else {
            ImGui::Text("Status: Playing");
        }

        ImGui::Columns(1);

    } else if (currentMode_ == AppMode::StageEditor) {
        // --- ステージエディター用最新マニュアル ＆ 情報 ---
        ImGui::Columns(2, "EditorColumns", false);

        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Stage Editor Controls ]");
        ImGui::Text("W / A / S / D : Move Cursor Horizontal");
        ImGui::Text("Q / E         : Move Cursor Up / Down");
        ImGui::Text("ENTER         : Place Block");
        ImGui::Text("SPACE / BACK  : Erase Block");
        ImGui::Text("R             : Rotate Placement Block (Direction)");
        ImGui::Text("I / J / K / L : Rotate/Orbit Debug Camera");

        ImGui::NextColumn();

        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Stage Map Data ]");
        const Int3& cursor = mapCursor_->GetIndex();
        ImGui::Text("Cursor Index: X:%d, Y:%d, Z:%d", cursor.x, cursor.y, cursor.z);
        ImGui::Text("Selected Block: %s (ID: %d)", BlockTypeToString(stageEditorController_.GetSelectedBlockType()), stageEditorController_.GetSelectedBlockType());
        ImGui::Text("Placable Stock: %d blocks", blockInventory_.GetBlockCount());

        ImGui::Columns(1);
    } else {
        // --- タイトル、クリア、デバッグビュー等の汎用表示 ---
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Application Status ]");
        if (currentMode_ == AppMode::Title) {
            ImGui::Text("Current Scene: TITLE SCREEN (Press Enter or Space to Start)");
        } else if (currentMode_ == AppMode::GameClear) {
            ImGui::Text("Current Scene: COURSE CLEAR (Press Space to Return to Stage Select)");
        } else {
            ImGui::Text("Current Scene: DEBUG VIEW");
        }
    }

    ImGui::End();

    // ==========================================
    // 4. 右パネル (Skinning Editor Options)
    // ==========================================
    if (currentMode_ == AppMode::SkinningEditor && skinnedObject_) {
        // 右側にパネルを配置
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelWidth, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y - bottomHeight), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.0f); // 透過なし
        ImGui::Begin("Skinning Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Skinned Mesh Settings ]");
        
        bool playAnim = skinnedObject_->IsPlayAnimation();
        if (ImGui::Checkbox("Play Test Animation", &playAnim)) {
            skinnedObject_->SetPlayAnimation(playAnim);
        }

        float speed = skinnedObject_->GetAnimationSpeed();
        if (ImGui::SliderFloat("Anim Speed", &speed, 0.0f, 3.0f, "%.2f")) {
            skinnedObject_->SetAnimationSpeed(speed);
        }

        bool showSkeleton = skinnedObject_->IsShowSkeleton();
        if (ImGui::Checkbox("Show Skeleton Bones", &showSkeleton)) {
            skinnedObject_->SetShowSkeleton(showSkeleton);
        }

        if (ImGui::Button("Reset to T-Pose", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->GetModel()->ResetPose();
        }

        // --- Blender Camera Presets ---
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Camera Presets (Blender Style) ]");

        if (ImGui::Button("Focus Model", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 24))) {
            camera->SetTarget({ 0.0f, 1.0f, 0.0f });
            camera->SetDistance(3.5f);
            camera->SetRotation({ 0.1f, 0.0f, 0.0f }); // ほぼ正面
        }
        ImGui::SameLine();
        if (ImGui::Button("Front View", ImVec2(-FLT_MIN, 24))) {
            camera->SetTarget({ 0.0f, 1.0f, 0.0f });
            camera->SetDistance(3.5f);
            camera->SetRotation({ 0.0f, 0.0f, 0.0f }); // 完全正面
        }

        if (ImGui::Button("Side View", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 24))) {
            camera->SetTarget({ 0.0f, 1.0f, 0.0f });
            camera->SetDistance(3.5f);
            camera->SetRotation({ 0.0f, 1.5708f, 0.0f }); // 右横から
        }
        ImGui::SameLine();
        if (ImGui::Button("Top View", ImVec2(-FLT_MIN, 24))) {
            camera->SetTarget({ 0.0f, 1.0f, 0.0f });
            camera->SetDistance(3.5f);
            camera->SetRotation({ 1.5708f, 0.0f, 0.0f }); // 真上から
        }

        // --- Custom Motion Editor ---
        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Custom Motion Editor ]");

        bool playCustom = skinnedObject_->IsPlayCustomAnimation();
        if (ImGui::Checkbox("Play Custom Motion", &playCustom)) {
            skinnedObject_->SetPlayCustomAnimation(playCustom);
            if (playCustom) {
                skinnedObject_->SetPlayAnimation(false); // テストアニメーションと排他
            }
        }

        float duration = skinnedObject_->GetModel()->GetMotionDuration();
        if (ImGui::InputFloat("Motion Duration", &duration, 0.1f, 1.0f, "%.2f")) {
            if (duration < 0.1f) duration = 0.1f;
            skinnedObject_->GetModel()->SetMotionDuration(duration);
        }

        float curTime = skinnedObject_->GetCurrentKeyframeTime();
        if (ImGui::SliderFloat("Timeline Time", &curTime, 0.0f, duration, "%.2f sec")) {
            skinnedObject_->SetCurrentKeyframeTime(curTime);
            if (!playCustom) {
                skinnedObject_->ApplyMotion(curTime);
            }
        }

        if (ImGui::Button("Add Keyframe (Current Pose)", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->AddKeyframe(curTime);
        }

        if (ImGui::Button("Clear All Keyframes", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->ClearKeyframes();
        }

        if (ImGui::Button("Generate Walk Preset", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->GenerateWalkPreset();
        }

        if (ImGui::Button("Generate Run Preset", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->GenerateRunPreset();
        }

        static char motionPath[256] = "Resources/Animations/test_motion.txt";
        ImGui::InputText("Motion Path", motionPath, IM_ARRAYSIZE(motionPath));

        if (ImGui::Button("Save Motion to File", ImVec2(ImGui::GetContentRegionAvail().x * 0.5f, 24))) {
            skinnedObject_->SaveMotion(motionPath);
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Motion from File", ImVec2(-FLT_MIN, 24))) {
            skinnedObject_->LoadMotion(motionPath);
        }

        ImGui::Separator();
        ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "[ Bone Transformations ]");

        auto& joints = skinnedObject_->GetModel()->GetJoints();
        int selectedJoint = skinnedObject_->GetSelectedJointIndex();
        
        // ジョイント名のリスト作成
        std::vector<const char*> jointNames;
        for (const auto& j : joints) {
            jointNames.push_back(j.name.c_str());
        }

        if (ImGui::Combo("Select Bone", &selectedJoint, jointNames.data(), static_cast<int>(jointNames.size()))) {
            skinnedObject_->SetSelectedJointIndex(selectedJoint);
        }

        if (selectedJoint >= 0 && selectedJoint < static_cast<int>(joints.size())) {
            auto& joint = joints[selectedJoint];
            
            ImGui::Text("Index: %d | Parent: %d", selectedJoint, joint.parentIndex);
            ImGui::Separator();

            // 回転スライダー (ラジアン -> デグリー)
            Vector3 rotDeg = {
                joint.rotation.x * 180.0f / 3.14159265f,
                joint.rotation.y * 180.0f / 3.14159265f,
                joint.rotation.z * 180.0f / 3.14159265f
            };

            ImGui::Text("Rotation (Degrees):");
            if (ImGui::SliderFloat("Rot X", &rotDeg.x, -180.0f, 180.0f, "%.1f")) {
                joint.rotation.x = rotDeg.x * 3.14159265f / 180.0f;
            }
            if (ImGui::SliderFloat("Rot Y", &rotDeg.y, -180.0f, 180.0f, "%.1f")) {
                joint.rotation.y = rotDeg.y * 3.14159265f / 180.0f;
            }
            if (ImGui::SliderFloat("Rot Z", &rotDeg.z, -180.0f, 180.0f, "%.1f")) {
                joint.rotation.z = rotDeg.z * 3.14159265f / 180.0f;
            }

            ImGui::Separator();
            ImGui::Text("Translation Offset:");
            ImGui::DragFloat3("Translate", &joint.translation.x, 0.01f, -2.0f, 2.0f, "%.3f");

            ImGui::Text("Scale:");
            ImGui::DragFloat3("Scale", &joint.scale.x, 0.01f, 0.1f, 5.0f, "%.3f");
        } else {
            ImGui::Text("No bone selected.");
        }

        ImGui::End();
    }
}
#endif

// --- プレイヤーが壁に隠れているかどうかの判定 (レイキャスト) ---
bool MyGame::IsPlayerHiddenByWall() const {
    if (!player_ || !camera) {
        return false;
    }

    Vector3 camPos = camera->GetPosition();
    Vector3 playerPos = player_->GetPosition();

    // プレイヤーの中心より少し上を狙う
    playerPos.y += 0.8f;

    Vector3 diff = {
        playerPos.x - camPos.x,
        playerPos.y - camPos.y,
        playerPos.z - camPos.z
    };

    float length = std::sqrt(
        diff.x * diff.x +
        diff.y * diff.y +
        diff.z * diff.z
    );

    if (length <= 0.001f) {
        return false;
    }

    Vector3 dir = {
        diff.x / length,
        diff.y / length,
        diff.z / length
    };

    const float step = 0.8f;

    for (float t = step; t < length - 1.0f; t += step) {
        Vector3 checkPos = {
            camPos.x + dir.x * t,
            camPos.y + dir.y * t,
            camPos.z + dir.z * t
        };

        int gx = static_cast<int>(std::floor(checkPos.x + 0.5f));
        int gy = static_cast<int>(std::floor(checkPos.y));
        int gz = static_cast<int>(std::floor(checkPos.z + 0.5f));

        const MapCell* cell = stageMap_.GetCell(gx, gy, gz);

        if (cell && cell->isSolid) {
            return true;
        }
    }

    return false;
}

void MyGame::Draw() {
    auto commandList = dxCommon->GetCommandList();

    // 天球のカラー同期
    if (skydomeObject_) {
        if (skyboxLinkMode_ == 1) {
            skydomeObject_->SetColor(offscreenClearColor_);
        } else {
            skydomeObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        }
    }

    // ==========================================================
    // 【パス1】 シャドウマップへの描き込み（影の生成）
    // ==========================================================
    shadowMap_->PreDraw(commandList);

    commandList->SetGraphicsRootSignature(object3dCommon->GetRootSignature());
    commandList->SetPipelineState(object3dCommon->GetShadowPipelineState());
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // 影を描く対象（動くものすべて）
    for (auto& obj : objectList) {
        if (obj) {
            obj->DrawShadow(lightVP);
        }
    }

    if (player_) {
        player_->DrawShadow(lightVP);
    }

    if (currentMode_ == AppMode::SkinningEditor && skinnedObject_) {
        skinnedObject_->DrawShadow(lightVP);
    }

    if (stageRenderer_) {
        stageRenderer_->DrawShadow(lightVP);
    }

    shadowMap_->PostDraw(commandList);

    if (offscreenEnabled_) {
        // ==========================================================
        // 【オフスクリーン描画】 RenderTexture へのレンダリング
        // ==========================================================
        
        // 1. RenderTexture の状態を RENDER_TARGET に遷移
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = renderTexture_.Get();
        barrier.Transition.StateBefore = renderTextureState_;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        if (barrier.Transition.StateBefore != barrier.Transition.StateAfter) {
            commandList->ResourceBarrier(1, &barrier);
        }
        renderTextureState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;

        // 2. レンダーターゲットに RenderTexture をセット、クリア
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap_->GetCPUDescriptorHandleForHeapStart();
        D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dxCommon->GetDsvHeap()->GetCPUDescriptorHandleForHeapStart();
        commandList->OMSetRenderTargets(1, &rtvHandle, false, &dsvHandle);

        // ビューポートとシザーは RenderTexture のサイズ (1280x720) に合わせる
        D3D12_VIEWPORT offscreenViewport = { 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
        D3D12_RECT offscreenScissor = { 0, 0, 1280, 720 };
        commandList->RSSetViewports(1, &offscreenViewport);
        commandList->RSSetScissorRects(1, &offscreenScissor);

        // クリア処理
        float clearColor[4] = { offscreenClearColor_.x, offscreenClearColor_.y, offscreenClearColor_.z, offscreenClearColor_.w };
        commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
        commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // 3. 通常のシーン描画処理を実行
        RenderScene(commandList, lightVP);

        // 4. RenderTexture の状態を PIXEL_SHADER_RESOURCE に遷移
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        commandList->ResourceBarrier(1, &barrier);
        renderTextureState_ = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        // ==========================================================
        // 【メインコピーパス】 バックバッファへの転送
        // ==========================================================
        dxCommon->PreDraw();

        // コピー用 PSO と RootSignature のバインド
        commandList->SetGraphicsRootSignature(copyRootSignature_.Get());
        if (postEffectMode_ == 1) {
            commandList->SetPipelineState(grayscalePipelineState_.Get());
        } else if (postEffectMode_ == 2) {
            commandList->SetPipelineState(sepiaPipelineState_.Get());
        } else {
            commandList->SetPipelineState(copyPipelineState_.Get());
        }

        // コピー用のデスクリプタヒープをバインド
        ID3D12DescriptorHeap* copyHeaps[] = { srvHeap_.Get() };
        commandList->SetDescriptorHeaps(1, copyHeaps);

        // スロット0(t0)に RenderTexture の SRV をバインド
        commandList->SetGraphicsRootDescriptorTable(0, srvHeap_->GetGPUDescriptorHandleForHeapStart());

        // 三角形3頂点の描画
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->DrawInstanced(3, 1, 0, 0);

    } else {
        // ==========================================================
        // 【従来パス】 直接バックバッファに描画
        // ==========================================================
#ifdef NDEBUG
		// リリースモードでは、描画領域をウィンドウ全体 (0,0)-(1280,720) に設定して描画する。
        D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(WinApp::kWindowWidth), static_cast<float>(WinApp::kWindowHeight), 0.0f, 1.0f };
        D3D12_RECT scissor = { 0, 0, WinApp::kWindowWidth, WinApp::kWindowHeight };
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
#else
		// デバッグモードでは、描画領域をウィンドウの右側 (320,0)-(1600,720) に限定して描画する。
        D3D12_VIEWPORT viewport = { 320.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
        D3D12_RECT scissor = { 320, 0, 1600, 720 };
        commandList->RSSetViewports(1, &viewport);
        commandList->RSSetScissorRects(1, &scissor);
#endif
		// クリア処理
        dxCommon->PreDraw();

        // 通常のシーン描画処理を実行
        RenderScene(commandList, lightVP);
    }

    // --- 4. ImGui と 最終出力 ---
#ifndef NDEBUG
    dxCommon->EndImGui();
#endif

    dxCommon->PostDraw();
}

// --- 重複していたオフスクリーンパスと通常パスの描画処理を RenderScene に一括集約 ---
void MyGame::RenderScene(ID3D12GraphicsCommandList* commandList, const Matrix4x4& lightVP) {
    if (debugFlags_.show3DObjects) {
        // 描画に必要なSRVヒープをセット
        ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, heaps);

        // 3Dオブジェクトの描画前に共通設定を行う
        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());

        // A. タイトルシーン
        if (currentMode_ == AppMode::Title) {
            if (titleScene_) titleScene_->Draw();
        }
        // ステージセレクト
        else if (currentMode_ == AppMode::StageSelect) {
            if (stageSelect_) stageSelect_->Draw();
        }
        // B. クリアシーン
        else if (currentMode_ == AppMode::GameClear) {
            if (gameClearScene_) gameClearScene_->Draw();
        }
        // スキニングエディター
        else if (currentMode_ == AppMode::SkinningEditor) {
            if (showSkyboxCubemap_ && skybox_) {
                skybox_->Draw();
                // 標準のPSOとShadowMapのバインドを復帰
                object3dCommon->PreDraw();
                commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
            } else if (debugFlags_.showSkybox && skydomeObject_) {
                skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                skydomeObject_->Draw();
            }
            // デバッグ用グリッド線 (描画時のUpdateはUpdate()内へ移管して負荷削減)
            for (auto& line : gridLines_) {
                line->Draw();
            }
            if (skinnedObject_) {
                skinnedObject_->Draw();
                skinnedObject_->DrawSkeleton(object3dCommon.get(), debugCubeModel_.get(), camera->GetViewMatrix(), camera->GetProjectionMatrix());
            }
        }
        // C. 通常ゲーム画面
        else {
            if (showSkyboxCubemap_ && skybox_) {
                skybox_->Draw();
                // 標準のPSOとShadowMapのバインドを復帰
                object3dCommon->PreDraw();
                commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
            } else if (debugFlags_.showSkybox && skydomeObject_) {
                skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                skydomeObject_->Draw();
            }
            if (currentMode_ == AppMode::StageEditor ||
                currentMode_ == AppMode::GamePlay ||
                currentMode_ == AppMode::GamePlay_BlockPlace) {

                if (stageRenderer_) {
                    stageRenderer_->Draw(); 

                    // 半透明ブロックを最後に描画
                    stageRenderer_->DrawTransparent();

                    // 通常描画に戻す
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
                        gameplayUIManager_->Draw3DPrompts(currentMode_ == AppMode::GamePlay, player_.get(), object3dCommon.get(), commandList, shadowMap_->GetSrvHandle());
                    }
                }
                if ((currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace) && mapCursor_) {
                    mapCursor_->Draw();
                }
            }
            if (currentMode_ == AppMode::DebugView) {
                for (auto& obj : objectList) {
                    if (obj) obj->Draw();
                }
                if (player_) { player_->Draw(); }
            }
        }
    }

    // パーティクルの描画
    if (debugFlags_.showParticles) {
        ID3D12DescriptorHeap* particleHeaps[] = { textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, particleHeaps);
        particleManager->Draw();
    }

    // スプライトの描画
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        spriteCommon->PreDraw();
        if (sprite) sprite->Draw();
    }

    // UIスプライトの描画
    if (gameplayUIManager_) {
        gameplayUIManager_->DrawSprites(currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace);
    }

    // インベントリUIの描画
    if (blockInventoryUI_ && (currentMode_ == AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace)) {
        blockInventoryUI_->Draw();
    }

    // チュートリアルUIの描画
    // インベントリが開いているとき（GamePlay_BlockPlace）は配置チュートリアルを優先表示し、
    // 通常ゲームプレイ時のみ操作チュートリアルを表示する（被り防止）
    bool inventoryIsOpen = blockInventoryUI_ && blockInventoryUI_->IsActive();

    if (currentMode_ == AppMode::GamePlay && !inventoryIsOpen)
    {
        // 通常プレイ中のみ操作チュートリアルを表示
        if (stageSelect_) {
            std::string currentStage = stageSelect_->GetSelectedFileName();
            if (currentStage == "tutorial.txt" && tutorialSprite_)
            {
                spriteCommon->PreDraw();
                tutorialSprite_->Draw();
            }
        }
    }

    // 配置チュートリアルUIの描画（インベントリが開いている時に表示）
    if ((currentMode_ == AppMode::GamePlay_BlockPlace || inventoryIsOpen) && placementTutorialSprite_)
    {
        spriteCommon->PreDraw();
        placementTutorialSprite_->Draw();
    }
}

void MyGame::Finalize() {
    // 1. GPUの完了を待機（リソースが「使用中」にならないようにする）
    if (dxCommon) {
        dxCommon->WaitForGpu();
    }


    sound.Finalize();

#ifndef NDEBUG
    dxCommon->FinalizeImGui();
#endif


    // 2. シーン（描画物の所有者）を先に消す
    // GameClearSceneの11文字（COURSECLEAR）はこのタイミングで unique_ptr により解放されます
    gameClearScene_.reset();
    titleScene_.reset();

    // 3. 静的マネージャーの解放
    // モデル本体を消去（Object3dCommonより先に消す必要がある）
    ModelManager::Finalize();

    // 4. 動的に生成したオブジェクトリストのクリア
    // unique_ptr の vector なので、clear() で中身のデストラクタが呼ばれます
    objectList.clear();
    models.clear();

   
    // 5. その他のゲームオブジェクトを reset
    if (gameplayUIManager_) gameplayUIManager_->Finalize();
    gameplayUIManager_.reset();
    if (blockInventoryUI_) blockInventoryUI_->Finalize();
    blockInventoryUI_.reset();
    player_.reset();
    skinnedObject_.reset();
    debugCubeModel_.reset();
    skydomeObject_.reset();
    skydomeModel_.reset();
    sprite.reset();
    stageRenderer_.reset();
    mapCursor_.reset();
    camera.reset();
    shadowMap_.reset();
    lightCamera_.reset();

    // 6. システムマネージャー類を reset
    particleManager.reset();
    object3dCommon.reset();
    spriteCommon.reset();
    textureManager.reset();
    input.reset();

    // 7. 最後：DirectX基盤とウィンドウを reset
    // ここで Device の Refcount が正常に 0 へ向かいます
    dxCommon.reset();
    winApp.reset();
}

/// <summary>
/// ブロックを置けるようになる画面 
/// </summary>
void MyGame::UpdateGamePlayBlockPlace()
{
    // 現在カーソル位置
    const Int3& cursor = mapCursor_->GetIndex();

    // Rキーで配置回転角を90度ずつ更新 (反時計回りに1.57rad=90度)
    if (input->TriggerKey(DIK_R)) {
        placeRotationY_ += 1.5707963f;
        if (placeRotationY_ >= 6.0f) { // 360度に達したらリセット
            placeRotationY_ = 0.0f;
        }
    }

    // カーソル移動処理
    stageEditorController_.HandleCursorInput(input.get(), stageMap_, mapCursor_.get(), lightCamera_.get(), camera.get());

    // インベントリで選択されているブロックタイプを同期
    BlockType selectedType = BlockType::Ground;
    int selectedCustomId = 0;
    if (blockInventoryUI_) {
        selectedType = blockInventoryUI_->GetSelectedBlockType();
        selectedCustomId = blockInventoryUI_->GetSelectedCustomId();
        blockPlacementController_.SetPlaceBlockType(selectedType);
        blockPlacementController_.SetPlaceCustomId(selectedCustomId);
    } else {
        blockPlacementController_.SetPlaceBlockType(BlockType::Ground);
        blockPlacementController_.SetPlaceCustomId(0);
    }

    // 🌟 半透明リアルタイムプレビューを毎フレーム更新！！！
    if (stageRenderer_) {
        stageRenderer_->SetPlacementPreview(stageMap_, cursor, selectedType, selectedCustomId, placeRotationY_);
    }

    // ② ブロックを置く決定処理 (Enterキー または ゲーム画面上の左クリック)
    // 配置モード時はインベントリが閉じた状態のため、常にゲーム画面のクリックで配置
    static bool prevMouse0 = false;
    bool mouseJustPressed = input->GetMouseState().buttons[0] && !prevMouse0;
    prevMouse0 = input->GetMouseState().buttons[0];

    bool mouseTrigger = false;
    if (mouseJustPressed) {
        // インベントリが閉じている（または完全に閉まっている）場合のみゲーム画面クリックで配置
        bool inventoryClosed = !blockInventoryUI_ || !blockInventoryUI_->IsActive();
        if (inventoryClosed) {
            mouseTrigger = true;
        }
    }

    if (input->TriggerKey(DIK_RETURN) || mouseTrigger) {
        Int3 cursorPos = mapCursor_->GetIndex();

        // コントローラーを使ってブロックを配置
        if (blockPlacementController_.TryPlace(cursorPos, placeRotationY_)) {
            // 設置完了後、所持数が 0 になったら自動的に通常プレイに戻る
            BlockType currentType = blockInventoryUI_ ? blockInventoryUI_->GetSelectedBlockType() : BlockType::Ground;
            int currentCustomId = blockInventoryUI_ ? blockInventoryUI_->GetSelectedCustomId() : 0;
            bool hasRest = (currentType == BlockType::Ground) || blockInventory_.HasBlock(currentType, currentCustomId);
            if (!hasRest) {
                currentMode_ = AppMode::GamePlay;
                placeRotationY_ = 0.0f;
                if (stageRenderer_) {
                    stageRenderer_->ClearPlacementPreview();
                }
            }
        }
    }

    // ③ キャンセルして戻る処理
    // ESCキー → キャンセル
    // Bキー → インベントリを開いてキャンセル（インベントリで別ブロックを選べる）
    if (input->TriggerKey(DIK_ESCAPE)) {
        currentMode_ = AppMode::GamePlay;
        placeRotationY_ = 0.0f;
        if (stageRenderer_) {
            stageRenderer_->ClearPlacementPreview();
        }
    } else if (input->TriggerKey(DIK_B)) {
        // Bキーで配置モードをキャンセルして通常プレイに戻る（インベントリは開かない）
        currentMode_ = AppMode::GamePlay;
        placeRotationY_ = 0.0f;
        if (stageRenderer_) {
            stageRenderer_->ClearPlacementPreview();
        }
    }

    // カメラ操作
    stageEditorController_.HandleCameraInput(input.get(), camera.get());
}

void MyGame::UpdateTitle() {
    if (titleScene_) {
        titleScene_->Update();
    //シーン変化用のキー入力
        if (titleScene_->IsFinished()) {
            currentMode_ = AppMode::StageSelect;
        }
    }
}

void MyGame::UpdateStageSelect()
{
    stageSelect_->Update();

    if (stageSelect_->IsFnished())
    {
        // ① ステージセレクトから、選ばれたファイル名をもらってくる
        std::string fileName = stageSelect_->GetSelectedFileName();

        // ② 正しいパスを作る (Resources/Stages/ フォルダの中の fileName)
        std::string filePath = "Resources/Stages/" + fileName;

        // ③ そのファイルを読み込む！
        if (std::filesystem::exists(filePath))
        {
            stageMap_.LoadFromFile(filePath);
            backupMap_ = stageMap_;//バックアップ　05/14小林
            stageRenderer_->BuildFromStageMap(stageMap_); // 見た目の更新

            //5/14佐倉追加
            //カメラリセット処理
            // プレイヤーの位置をスタート地点に戻すなどの処理
            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());

            int stageIndex = stageSelect_->GetSelectedIndex();
            gameplayCameraController_.ResetCamera(camera.get(), player_.get(),stageMap_, stageIndex);

            // インベントリを0個に初期化
            blockInventory_.Initialize(0);
        }
        // ゲームプレイモードへ切り替え
        currentMode_ = AppMode::GamePlay;
    }
}

void MyGame::UpdateSceneTransition()
{
    if ((currentMode_==AppMode::GamePlay || currentMode_ == AppMode::GamePlay_BlockPlace) &&input->TriggerKey(DIK_ESCAPE))
    {
        //保存したのを復元
        stageMap_ = backupMap_;

        stageRenderer_->BuildFromStageMap(stageMap_); // モデルを初期配置に戻す
        bubblePickupController_.Initialize(&stageMap_, stageRenderer_.get(), &blockInventory_); // 取得状況をリセット
        stageSelect_->Initialize(object3dCommon.get(),input.get());
        
        isGoalReached_ = false;

        if (player_){player_->Respawn();}
        currentMode_ = AppMode::StageSelect;
    }
}

void MyGame::InitializeOffscreenRendering() {
    auto device = dxCommon->GetDevice();

    // 1. RenderTexture の生成
    renderTexture_ = CreateRenderTextureResource(
        device,
        1280,
        720,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        offscreenClearColor_
    );

    // 2. RTV デスクリプタヒープと RTV の作成
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = 1;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvHeap_));
    assert(SUCCEEDED(hr));

    device->CreateRenderTargetView(
        renderTexture_.Get(),
        nullptr,
        rtvHeap_->GetCPUDescriptorHandleForHeapStart()
    );

    // 3. SRV デスクリプタヒープと SRV の作成
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{};
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    hr = device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap_));
    assert(SUCCEEDED(hr));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    device->CreateShaderResourceView(
        renderTexture_.Get(),
        &srvDesc,
        srvHeap_->GetCPUDescriptorHandleForHeapStart()
    );

    // 4. コピー用 RootSignature の作成
    D3D12_DESCRIPTOR_RANGE descriptorRange{};
    descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    descriptorRange.NumDescriptors = 1;
    descriptorRange.BaseShaderRegister = 0; // t0
    descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER rootParameter{};
    rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    rootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
    rootParameter.DescriptorTable.NumDescriptorRanges = 1;
    rootParameter.DescriptorTable.pDescriptorRanges = &descriptorRange;

    D3D12_STATIC_SAMPLER_DESC staticSampler{};
    staticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    staticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    staticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    staticSampler.MaxLOD = D3D12_FLOAT32_MAX;
    staticSampler.ShaderRegister = 0; // s0
    staticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc{};
    rootSignatureDesc.NumParameters = 1;
    rootSignatureDesc.pParameters = &rootParameter;
    rootSignatureDesc.NumStaticSamplers = 1;
    rootSignatureDesc.pStaticSamplers = &staticSampler;
    rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    hr = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        assert(false);
    }
    hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&copyRootSignature_));
    assert(SUCCEEDED(hr));

    // 5. ポストプロセス用 PipelineState (PSO) の作成
    Microsoft::WRL::ComPtr<IDxcBlob> vsBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/Fullscreen.VS.hlsl", L"vs_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psCopyBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/CopyImage.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psGrayBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/Grayscale.PS.hlsl", L"ps_6_0");
    Microsoft::WRL::ComPtr<IDxcBlob> psSepiaBlob = dxCommon->CompileShader(L"Resources/shaders/hlsl/Sepia.PS.hlsl", L"ps_6_0");

    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = copyRootSignature_.Get();
    psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
    psoDesc.InputLayout.pInputElementDescs = nullptr;
    psoDesc.InputLayout.NumElements = 0;
    psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    psoDesc.BlendState.RenderTarget[0].BlendEnable = false;
    psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    psoDesc.DepthStencilState.DepthEnable = false;
    psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    psoDesc.SampleDesc.Count = 1;
    psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

    // A. コピー (Normal)
    psoDesc.PS = { psCopyBlob->GetBufferPointer(), psCopyBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&copyPipelineState_));
    assert(SUCCEEDED(hr));

    // B. グレースケール
    psoDesc.PS = { psGrayBlob->GetBufferPointer(), psGrayBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&grayscalePipelineState_));
    assert(SUCCEEDED(hr));

    // C. セピア調
    psoDesc.PS = { psSepiaBlob->GetBufferPointer(), psSepiaBlob->GetBufferSize() };
    hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&sepiaPipelineState_));
    assert(SUCCEEDED(hr));
}

Microsoft::WRL::ComPtr<ID3D12Resource> MyGame::CreateRenderTextureResource(
    ID3D12Device* device,
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const Vector4& clearColor) {

    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Width = width;
    resourceDesc.Height = height;
    resourceDesc.MipLevels = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.Format = format;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = format;
    clearValue.Color[0] = clearColor.x;
    clearValue.Color[1] = clearColor.y;
    clearValue.Color[2] = clearColor.z;
    clearValue.Color[3] = clearColor.w;

    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&resource)
    );
    assert(SUCCEEDED(hr));
    return resource;
}