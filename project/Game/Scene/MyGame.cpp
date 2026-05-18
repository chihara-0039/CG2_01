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
   

    // 1. ステージマップのサイズ初期化
    stageMap_.Initialize(16, 10, 16);

    // ステージエディタ管理で初期化済み

    // --- 3. ビルド設定による初期化分岐 ---
#ifdef NDEBUG
    // 【Releaseビルド時】直接ゲームを開始する
    currentMode_ = AppMode::GamePlay;

    // "Stage1.txt" があれば自動ロード
    std::string startStage = "Resources/Stages/Stage1.txt";
    if (std::filesystem::exists(startStage)) {
        stageMap_.LoadFromFile(startStage);
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    }

#else

    // 【Debugビルド時】
    currentMode_ = AppMode::Title;


    // 2. ★手動配置を消して、保存した「プロトタイプ」をロードする
    std::string prototypePath = "Resources/Stages/stage1.txt"; // 保存したファイル名に合わせてください
    if (std::filesystem::exists(prototypePath)) {
        stageMap_.LoadFromFile(prototypePath);
        stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
    }
#endif

    // 1. モデルのロード（フォルダとファイル名に注意）
    skydomeModel_ = std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources/Models/skydome", "skydome.obj", textureManager.get()));

    // 2. オブジェクトの生成と初期化
    skydomeObject_ = std::make_unique<Object3d>();
    skydomeObject_->Initialize(object3dCommon.get());
    skydomeObject_->SetModel(skydomeModel_.get());

    // 3. 設定：空は自ら光るのでライトをオフにする
    skydomeObject_->SetEnableLighting(false);

    // 4. 設定：ステージを包むサイズにする（500〜1000程度）
    skydomeObject_->SetScale({ 500.0f, 500.0f, 500.0f });

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

    gameplayCameraController_.SetAngle(1.5708f); // ★ここで開始時の向きを調整！
    gameplayCameraController_.SetPitch(0.75f);

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
    gameplayCameraController_.Initialize();
    stageEditorController_.Initialize();
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
    // マウスとキーボードの両方をガード
    isGuiCaptured = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
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

    if (skydomeObject_) {
        // 天球の座標を常にカメラと同じにする
        skydomeObject_->SetPosition(camera->GetPosition());
        skydomeObject_->Update(Math::MakeIdentity4x4());
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
        if (gameClearScene_) {
            gameClearScene_->Update();

            if (gameClearScene_->IsFinished() && input->TriggerKey(DIK_SPACE))
            {
                stageSelect_->Initialize(object3dCommon.get(), input.get());
                gameClearScene_->Initialize(object3dCommon.get());
              
                isGoalReached_ = false;
                stageMap_.Clear();
                player_->Respawn();
                
                currentMode_ = AppMode::StageSelect;
            }
        }
        break;
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

	// 3Dオブジェクトの更新
    if (debugFlags_.show3DObjects) {
        for (auto& obj : objectList) {
            if (obj) {
                obj->SetCamera(view, proj);
                obj->Update(lightVP);
            }
        }
    }

	// ステージ描画オブジェクトの更新
    if (stageRenderer_) {
        stageRenderer_->SetCamera(view, proj);
        // ※ StageRenderer内部でもObject3dのUpdate(lightVP)を呼ぶように修正が必要です
        stageRenderer_->Update(stageMap_,lightVP);
    }

	// マップカーソルの更新
    if (mapCursor_) {
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
    Vector3 lightDir = { 0.5f, -1.0f, 0.5f }; // ライトの向き（Object3dCommonの設定に合わせる）
    lightCamera_->Update(lightDir, player_->GetPosition());

    object3dCommon->SetLightDirection(lightDir);

    // UI・プロンプト更新
    if (gameplayUIManager_) {
        gameplayUIManager_->Update(currentMode_ == AppMode::GamePlay, player_.get(), camera.get(), lightCamera_.get());
    }
}

//パーティクル発生のテスト（スペースキーを押すと発生）
void MyGame::UpdateDebugView() {
    if (input->TriggerKey(DIK_SPACE)) {
        particleManager->Emit({ 0, 0, 0 }, 10);
    }
}

void MyGame::UpdateGamePlay() {

    gameplayCameraController_.Update(input.get(), camera.get(), winApp.get(),player_.get());
    // --- ステージマップの更新（崩れる足場のタイマー処理） ---

    //5/14佐倉追加
    if (gameplayUIManager_) {
        gameplayUIManager_->UpdateCameraGuide(
            currentMode_ == AppMode::GamePlay,
            input.get(),
            winApp.get()
        );
    }

    float deltaTime = 1.0f / 60.0f;
    totalTime_ += deltaTime;
    stageMap_.Update(deltaTime, totalTime_);

    // もし足場が消えて「再構築」が必要になったら Renderer を更新する
    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->BuildFromStageMap(stageMap_);
        stageMap_.ResetRebuildFlag(); // ★これを忘れると、1回きりしか更新されません！
    }

    stageRenderer_->UpdateEffect(stageMap_);

    // --- プレイヤー更新 ---
    if (player_) {
        player_->Update(input.get(), stageMap_, gameplayCameraController_.GetAngle(), lightCamera_->GetViewProjectionMatrix());
    }

    // --- ステージ再構築 ---
    if (stageMap_.NeedsRebuild()) {
        stageRenderer_->BuildFromStageMap(stageMap_);
        stageMap_.ClearRebuildFlag();
    }

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
        ▼ 配置モード切り替え
        Bキーで、所持ブロックがある時だけ配置モードへ入る
    ==================================================*/
    if (input->TriggerKey(DIK_B) && blockInventory_.HasBlock()) {
        currentMode_ = AppMode::GamePlay_BlockPlace;
        mapCursor_->SetIndex({ gx, gy, gz }, stageMap_);
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
        case AppMode::DebugView:   modeIndex = 0; break;
        case AppMode::StageEditor: modeIndex = 1; break;
        case AppMode::GamePlay:    modeIndex = 2; break;
        }

        const char* modeNames[] = { "DebugView", "StageEditor", "GamePlay" };
        if (ImGui::Combo("App Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
            switch (modeIndex) {
            case 0: currentMode_ = AppMode::DebugView; break;
            case 1: currentMode_ = AppMode::StageEditor; break;
            case 2: currentMode_ = AppMode::GamePlay; break;
            }
        }
        ImGui::Checkbox("Show 3D Objects", &debugFlags_.show3DObjects);
        ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
        ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);
    }

    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        camera->DrawImGui();
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

    ImGui::Columns(2, "BottomColumns", false);
    
    // 左カラム：操作説明
    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[ Editor Controls ]");
    ImGui::Text("Move Cursor : W, A, S, D, Q(Up), E(Down)");
    ImGui::Text("Place Block : Enter");
    ImGui::Text("Remove Block: Space / Backspace");
    ImGui::Text("Rotate Block: R");
    ImGui::Text("Move Camera : I, J, K, L, U, O");

    ImGui::NextColumn();

    // 右カラム：現在のステータス
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "[ Current Status ]");
    ImGui::Text("Selected Block: %s", BlockTypeToString(stageEditorController_.GetSelectedBlockType()));
    ImGui::Text("Placeable Blocks: %d", blockInventory_.GetBlockCount());
    
    if (currentMode_ == AppMode::StageEditor) {
        ImGui::Text("Mode: STAGE EDITOR");
    } else if (currentMode_ == AppMode::GamePlay) {
        ImGui::Text("Mode: GAME PLAY");
    } else {
        ImGui::Text("Mode: DEBUG VIEW");
    }

    ImGui::Columns(1);
    ImGui::End();
}
#endif




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

    const float step = 0.25f;

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

    // ==========================================================
    // 【パス1】 シャドウマップへの描き込み（影の生成）
    // ==========================================================
    // ※ タイトルやクリア画面で影が不要なら if で囲っても良いですが、
    //    まずは「常に生成する」方がバグが起きにくく安全です。
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

    if (stageRenderer_) {
        stageRenderer_->DrawShadow(lightVP);
    }

    shadowMap_->PostDraw(commandList);

#ifdef NDEBUG
    // Releaseビルド時は全画面表示にする
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, static_cast<float>(WinApp::kWindowWidth), static_cast<float>(WinApp::kWindowHeight), 0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0, WinApp::kWindowWidth, WinApp::kWindowHeight };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
#else
    // Debugビルド時は ImGui パネル用にビューポートを狭める
    D3D12_VIEWPORT viewport = { 320.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    D3D12_RECT scissor = { 320, 0, 1600, 720 };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
#endif

    // ==========================================================
    // 【パス2】 メイン描画（通常のレンダリング ＋ 影の適用）
    // ==========================================================
    dxCommon->PreDraw();

    if (debugFlags_.show3DObjects) {
        // --- 1. ヒープと共通設定（影を出すための最重要準備） ---
        ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
        commandList->SetDescriptorHeaps(1, heaps);

        object3dCommon->PreDraw();

        // ★重要：スロット4(t1)に影テクスチャを渡す
        // これを各シーンの描画（Draw）より「前」に呼ぶのが影を出す秘訣です
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());

        // --- 2. シーンごとの分岐描画 ---

        // A. タイトルシーン
        if (currentMode_ == AppMode::Title) {
            if (titleScene_) titleScene_->Draw();
        }
        // ステージセレクト追加　05/10小林
        else if (currentMode_ == AppMode::StageSelect)
        {
            if (stageSelect_) stageSelect_->Draw();
        }
        // B. クリアシーン
        else if (currentMode_ == AppMode::GameClear) {
            if (gameClearScene_) gameClearScene_->Draw();
        }
        // C. 通常ゲーム画面（エディタ・プレイ中・デバッグ）
        else {
            // 背景（天球）
            if (skydomeObject_) {
                skydomeObject_->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
                skydomeObject_->Draw();
            }

            // ステージとプレイヤー
            if (currentMode_ == AppMode::StageEditor ||
                currentMode_ == AppMode::GamePlay ||
                currentMode_ == AppMode::GamePlay_BlockPlace) {

                if (stageRenderer_) stageRenderer_->Draw();
                if (currentMode_ == AppMode::GamePlay)
                {
                    if (player_) player_->Draw();

                    // 壁で隠れている時だけ白強調
                    if (IsPlayerHiddenByWall()) {
                        object3dCommon->PreDrawPlayerHighlight();
                        player_->DrawHighlight();

                        // 通常描画設定に戻す
                        object3dCommon->PreDraw();
                        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
                    }

                    if (gameplayUIManager_) {
                        gameplayUIManager_->Draw3DPrompts(currentMode_ == AppMode::GamePlay, player_.get(), object3dCommon.get(), commandList, shadowMap_->GetSrvHandle());
                    }

                }               
               
                if ((currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay_BlockPlace) && mapCursor_) {
                    mapCursor_->Draw();
                }


            }

            // デバッグビュー（リストの全表示）
            if (currentMode_ == AppMode::DebugView) {
                for (auto& obj : objectList) {
                    if (obj) {
                        obj->Draw();
                    }
                }
                if (player_) {
                    player_->Draw();
                }
            }
        }
    }

    // --- 3. パーティクル・スプライト（共通） ---
    if (debugFlags_.showParticles) {
        particleManager->Draw();
    }

    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        spriteCommon->PreDraw();
        if (sprite) sprite->Draw();
    }

    //5/11佐倉

    if (gameplayUIManager_) {
        gameplayUIManager_->DrawSprites(currentMode_ == AppMode::GamePlay);
    }

    // --- 4. ImGui と 最終出力 ---
#ifndef NDEBUG
    dxCommon->EndImGui();
#endif


    dxCommon->PostDraw();
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
    player_.reset();
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

    // カーソル移動処理
    stageEditorController_.HandleCursorInput(input.get(), stageMap_, mapCursor_.get(), lightCamera_.get());

    // ② ブロックを置く決定処理 (Enterキー)
    if (input->TriggerKey(DIK_RETURN)) {
        Int3 cursorPos = mapCursor_->GetIndex();

        // コントローラーを使ってブロックを配置
        // 成功した場合は所持数も減り、見た目も更新される
        blockPlacementController_.SetPlaceBlockType(BlockType::Ground);
        if (blockPlacementController_.TryPlace(cursorPos)) {
            currentMode_ = AppMode::GamePlay; // 成功したら通常のプレイ画面に戻る
        }
    }

    // ③ キャンセルして戻る処理 (Escapeキー)
    if (input->TriggerKey(DIK_ESCAPE)) {
        currentMode_ = AppMode::GamePlay;
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
            gameplayCameraController_.ResetCamera(camera.get(),player_.get());

            // プレイヤーの位置をスタート地点に戻すなどの処理
            stageEditorController_.ResetPlayerToStartCell(stageMap_, player_.get());
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