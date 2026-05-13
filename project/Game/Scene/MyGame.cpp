#include "MyGame.h"
#include "TitleScene.h"
#include "GamePlayScene.h" // 新しく作成するシーン
#include "GameClearScene.h"
#include "EditorScene.h"

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

/// --- 初期化 ---
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

    // ステージファイル一覧を更新しておく
    RefreshStageList();

    // 2. 本編シーンを生成
    std::unique_ptr<GamePlayScene> newScene = std::make_unique<GamePlayScene>();

    // 【Debugビルド時】
    currentMode_ = AppMode::Title;


    // 2. ★手動配置を消して、保存した「プロトタイプ」をロードする
    std::string prototypePath = "Resources/Stages/stage1.txt"; // 保存したファイル名に合わせてください
    if (std::filesystem::exists(prototypePath)) {
        stageMap_.LoadFromFile(prototypePath);
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
    stageRenderer_->SetBlockScale(editorBlockScale_);
    stageRenderer_->BuildFromStageMap(stageMap_);

    // 4. シーン自身の初期化処理を実行
    newScene->Initialize();

    // 5. 管理下に置く
    scene_ = std::move(newScene);

	// 6. 影マップとライトカメラの初期化
    shadowMap_ = std::make_unique<ShadowMap>();
    shadowMap_->Initialize(dxCommon.get(), textureManager.get());

	// ライトカメラの初期化
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
    // カメラ回転用UIスプライト
    cameraGuideTextureHandle_ = textureManager->LoadTexture("Resources/UI/arrow.png");

    cameraGuideLeftSprite_ = std::make_unique<Sprite>();
    cameraGuideLeftSprite_->Initialize(spriteCommon.get(), cameraGuideTextureHandle_);

    cameraGuideRightSprite_ = std::make_unique<Sprite>();
    cameraGuideRightSprite_->Initialize(spriteCommon.get(), cameraGuideTextureHandle_);

    cameraGuideUpSprite_ = std::make_unique<Sprite>();
    cameraGuideUpSprite_->Initialize(spriteCommon.get(), cameraGuideTextureHandle_);

    cameraGuideDownSprite_ = std::make_unique<Sprite>();
    cameraGuideDownSprite_->Initialize(spriteCommon.get(), cameraGuideTextureHandle_);

    // 追加：ドア用3D F UI
    doorPromptModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(
            dxCommon.get(),
            "Resources/UI/F",
            "F.obj",
            textureManager.get()
        )
    );

    doorPromptObject_ = std::make_unique<Object3d>();
    doorPromptObject_->Initialize(object3dCommon.get());
    doorPromptObject_->SetModel(doorPromptModel_.get());
    doorPromptObject_->SetEnableLighting(false);
    doorPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    // ドア用3D F UI
    ladderPromptModel_ = std::unique_ptr<Model>(
        Model::CreateFromOBJ(
            dxCommon.get(),
            "Resources/UI/radderUI",
            "radderUI.obj",
            textureManager.get()
        )
    );

    // はしご用3D UI
    ladderPromptObject_ = std::make_unique<Object3d>();
    ladderPromptObject_->Initialize(object3dCommon.get());
    ladderPromptObject_->SetModel(ladderPromptModel_.get());
    ladderPromptObject_->SetEnableLighting(false);
    ladderPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

}

// --- 終了処理 ---
void MyGame::Finalize() {
    // シーンの解放（念のため明示的に）
    if (scene_) {
        scene_.reset();
    }

    // エンジン基盤の終了処理（GPU待機など）
    Framework::Finalize();
}

// --- 毎フレーム更新 ---
void MyGame::Update() {
    Framework::Update();
#ifndef NDEBUG
    ImGui_ImplDX12_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
#endif

    input->Update();
    bool isGuiCaptured = false;
    // 2. カメラの更新（Blender風操作を適用）
#ifdef USE_IMGUI
    // ★修正ポイント：Release時は ImGui::GetIO() を呼ばないようにガードする
    // または、DebugView か StageEditor の時だけ判定するようにする
    
    // マウスとキーボードの両方をガード
    isGuiCaptured = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;

#endif

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

    //試しにサウンド更新 //佐倉
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
   
    // --- ImGuiに入力中（WantCaptureKeyboardがtrue）ならゲーム側の入力を無視する ---
    if (!isGuiCaptured) {
        switch (currentMode_) {

        case AppMode::Title:
        UpdateTitle();//4/3佐倉　追加
        break;

        case AppMode::StageSelect:
        UpdateStageSelect();//5/10追加　小林
        break;

        case AppMode::DebugView:
        UpdateDebugView();
        break;

        case AppMode::StageEditor:
        UpdateStageEditor(); // 名前入力中はここが呼ばれなくなる
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

                if (gameClearScene_->IsFinished()&&input->TriggerKey(DIK_SPACE))
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
    }

    camera->Update();
    UpdateCameraGuideSprites();


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
        stageRenderer_->Update(lightVP);
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

    //ドアUI更新
    UpdateDoorPrompt3D();

    // はしごUI更新
    UpdateLadderPrompt3D();
}

//パーティクル発生のテスト（スペースキーを押すと発生）
void MyGame::UpdateDebugView() {
    if (input->TriggerKey(DIK_SPACE)) {
        particleManager->Emit({ 0, 0, 0 }, 10);
    }
}

void MyGame::RefreshStageList() {
    stageFiles_.clear();
    std::string path = "Resources/Stages/";

    // フォルダがなければ作成する
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }

    // フォルダ内の .txt ファイルをリストに詰める
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.path().extension() == ".txt") {
            stageFiles_.push_back(entry.path().stem().string()); // ファイル名（拡張子なし）を取得
        }
    }
}

void MyGame::UpdateStageEditor() {

    // 現在カーソル位置
    const Int3& cursor = mapCursor_->GetIndex();

    // ブロック配置や削除を行った後、ステージ描画オブジェクトを再構築する必要があるか
    bool needRebuild = false;

    // カーソル移動
    if (input->TriggerKey(DIK_A)) {
        mapCursor_->Move(-1, 0, 0, stageMap_);
    }
    if (input->TriggerKey(DIK_D)) {
        mapCursor_->Move(1, 0, 0, stageMap_);
    }
    if (input->TriggerKey(DIK_W)) {
        mapCursor_->Move(0, 0, 1, stageMap_);
    }
    if (input->TriggerKey(DIK_S)) {
        mapCursor_->Move(0, 0, -1, stageMap_);
    }
    if (input->TriggerKey(DIK_Q)) {
        mapCursor_->Move(0, 1, 0, stageMap_);
    }
    if (input->TriggerKey(DIK_E)) {
        mapCursor_->Move(0, -1, 0, stageMap_);
    }

    if (input->TriggerKey(DIK_R)) {
        MapCell* cell = stageMap_.GetCell(cursor.x, cursor.y, cursor.z);
        if (cell && cell->type != BlockType::None) {
            // 90度 (π/2) ずつ回転させる
            cell->rotationY += 1.5708f;
            needRebuild = true;
        }
    }

    

    // ブロック配置
	// 今は数字キー1～4で4種類のブロックを配置できるようにしています
	// 例えば、1がGround、2がWall、3がBubblePickup、4がGoalなど
	// ここはお好みでキーやブロックの種類を変更してください
    
	// 地面
    if (input->TriggerKey(DIK_1)) {
        selectedBlockType_ = BlockType::Ground;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        needRebuild = true;
    }
	
	// 壁
    if (input->TriggerKey(DIK_2)) {
        selectedBlockType_ = BlockType::Wall;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        needRebuild = true;
    }

    if (scene_) {
        scene_->Update();
        // シーン終了時の自動遷移 (Play -> Clear など)
        if (scene_->IsFinished()) {
            if (dynamic_cast<GamePlayScene*>(scene_.get())) ChangeMode(AppMode::GameClear);
        }
    }
}

void MyGame::UpdateGamePlay() {

    //4/20佐倉追加
    const auto& mouse = input->GetMouseState();

    //画面サイズ取得
    float screenWidth = (float)WinApp::kClientWidth;
    float screenHeight = (float)WinApp::kClientHeight;

    //どこを端とするか
    float edgeRatio = 0.1f;

	//端の範囲
    float leftEdge = screenWidth * edgeRatio;
    float rightEdge = screenWidth * (1.0f - edgeRatio);
    float topEdge = screenHeight * edgeRatio;
    float bottomEdge = screenHeight * (1.0f - edgeRatio);

    //マウス位置
    float mouseX = (float)mouse.posX;
    float mouseY = (float)mouse.posY;
   
	// --- 横回転 ---
    const float rotateSpeed = 0.025f;

    // --- 縦回転 ---
    const float minPitch = 0.4f;
    const float maxPitch = 1.5f;
    const float upperLimit = 3.0f;

     //クリック中のみ反応(左クリック)
    if (mouse.buttons[0]) {
        //横回転
        if (mouseX < leftEdge) {
            //左端Q
            cameraAngle_ += rotateSpeed;
        }
        else if (mouseX > rightEdge) {
            //右端E
            cameraAngle_ -= rotateSpeed;
        }

        //縦回転
        if (mouseY < topEdge) {
            //上端
            cameraPitch_ += rotateSpeed;
            if (cameraPitch_ > upperLimit) {
                cameraPitch_ = upperLimit;
            }
        }
        else if (mouseY > bottomEdge) {
            //下向き
            cameraPitch_ -= rotateSpeed;
            if (cameraPitch_ < minPitch) {
                cameraPitch_ = minPitch;
            }
        }
    }

	// カメラの縦回転の制限
    if (cameraPitch_ > maxPitch) {
        cameraPitch_ = maxPitch;
    }

    // --- カメラ位置計算 ---
    Vector3 pivot = { 4.0f, 9.0f, 4.5f };
    float distance = 35.0f;
    float height = 20.0f;

	// カメラの位置を極座標から計算
    Vector3 pos;
    pos.x = pivot.x - std::cos(cameraPitch_) * std::sin(cameraAngle_) * distance;
    pos.y = pivot.y + std::sin(cameraPitch_) * height;
    pos.z = pivot.z - std::cos(cameraPitch_) * std::cos(cameraAngle_) * distance;

	// カメラに位置と回転をセット
    camera->SetPosition(pos);
    camera->SetRotation({ cameraPitch_, cameraAngle_, 0.0f });
    // --- ステージマップの更新（崩れる足場のタイマー処理） ---

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
        player_->Update(input.get(), stageMap_, cameraAngle_, lightCamera_->GetViewProjectionMatrix());
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

void MyGame::UpdateCameraGuideSprites() {
    if (currentMode_ != AppMode::GamePlay) {
        return;
    }

    if (!cameraGuideLeftSprite_ ||
        !cameraGuideRightSprite_ ||
        !cameraGuideUpSprite_ ||
        !cameraGuideDownSprite_) {
        return;
    }

    float screenWidth = static_cast<float>(WinApp::kClientWidth);
    float screenHeight = static_cast<float>(WinApp::kClientHeight);

    float edgeRatio = 0.1f;

    float leftX = screenWidth * edgeRatio * 0.5f;
    float rightX = screenWidth * (1.0f - edgeRatio * 0.5f);
    float topY = screenHeight * edgeRatio * 0.5f;
    float bottomY = screenHeight * (1.0f - edgeRatio * 0.5f);

    float centerX = screenWidth * 0.5f;
    float centerY = screenHeight * 0.5f;

    // 画面端に配置
    cameraGuideLeftSprite_->SetPosition({ leftX, centerY });
    cameraGuideRightSprite_->SetPosition({ rightX, centerY });
    cameraGuideUpSprite_->SetPosition({ centerX, topY });
    cameraGuideDownSprite_->SetPosition({ centerX, bottomY });

    // サイズ
    cameraGuideLeftSprite_->SetSize({ 64.0f, 64.0f });
    cameraGuideRightSprite_->SetSize({ 64.0f, 64.0f });
    cameraGuideUpSprite_->SetSize({ 64.0f, 64.0f });
    cameraGuideDownSprite_->SetSize({ 64.0f, 64.0f });

    // arrow.png が上向き矢印想定
    cameraGuideUpSprite_->SetRotation(0.0f);
    cameraGuideRightSprite_->SetRotation(1.5708f);
    cameraGuideDownSprite_->SetRotation(3.1415f);
    cameraGuideLeftSprite_->SetRotation(-1.5708f);

    cameraGuideLeftSprite_->Update();
    cameraGuideRightSprite_->Update();
    cameraGuideUpSprite_->Update();
    cameraGuideDownSprite_->Update();
}

#ifdef USE_IMGUI
// ImGuiの更新と描画
void MyGame::UpdateImGui() {
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(340, 520), ImGuiCond_Always);

    ImGui::Begin("Debug Window");

    ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
        1000.0f / ImGui::GetIO().Framerate,
        ImGui::GetIO().Framerate);

    // モード切替
    int modeIndex = 0;
    switch (currentMode_) {
    case AppMode::DebugView:   modeIndex = 0; break;
    case AppMode::StageEditor: modeIndex = 1; break;
    case AppMode::GamePlay:    modeIndex = 2; break;
    }

    // ImGuiのコンボボックスでモード切替
    const char* modeNames[] = { "DebugView", "StageEditor", "GamePlay" };
    if (ImGui::Combo("App Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
        switch (modeIndex) {
        case 0: currentMode_ = AppMode::DebugView; break;
        case 1: currentMode_ = AppMode::StageEditor; break;
        case 2: currentMode_ = AppMode::GamePlay; break;
        }
    }

    // 描画オプション
    ImGui::Separator();
    ImGui::Text("Draw Flags");
    ImGui::Checkbox("Show 3D Objects", &debugFlags_.show3DObjects);
    ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
    ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);

    // ステージエディタ関連のUI
    ImGui::Separator();
    ImGui::Text("--- Stage MySet Manager ---");

    // 1. 新規保存
    ImGui::InputText("Save Name", newStageName_, IM_ARRAYSIZE(newStageName_));
    if (ImGui::Button("Save As New")) {
        std::string path = "Resources/Stages/" + std::string(newStageName_) + ".txt";
        stageMap_.SaveToFile(path);
        RefreshStageList(); // リストを更新
    }

    ImGui::Spacing();

    // 2. ステージリスト
    ImGui::Text("Saved Stages:");
    if (ImGui::BeginListBox("##StageList", ImVec2(-FLT_MIN, 150))) {
        for (int n = 0; n < (int)stageFiles_.size(); n++) {
            const bool is_selected = (selectedStageIndex_ == n);
            if (ImGui::Selectable(stageFiles_[n].c_str(), is_selected)) {
                selectedStageIndex_ = n;
            }
        }
        ImGui::EndListBox();
    }

    // 3. 選択したステージへの操作
    if (selectedStageIndex_ != -1 && selectedStageIndex_ < (int)stageFiles_.size()) {
        std::string selectedName = stageFiles_[selectedStageIndex_];
        std::string fullPath = "Resources/Stages/" + selectedName + ".txt";

        if (ImGui::Button("Load Selected")) {
            stageMap_.LoadFromFile(fullPath);
            if (stageRenderer_) {
                stageRenderer_->BuildFromStageMap(stageMap_);
            }

            // --- 追加：PlayerStartブロックを探してプレイヤーを移動させる ---
            bool foundStart = false;

            // ステージマップは3次元なので、Y軸を固定してX-Z平面を探索する形になります
            for (int y = 0; y < stageMap_.GetHeight(); ++y) {
                // ステージマップは3次元なので、Y軸を固定してX-Z平面を探索する形になります
                for (int z = 0; z < stageMap_.GetDepth(); ++z) {
                    // ステージマップを全探索してPlayerStartブロックを探す
                    for (int x = 0; x < stageMap_.GetWidth(); ++x) {
                        // セルを取得して、タイプが PlayerStart かチェック
                        const MapCell* cell = stageMap_.GetCell(x, y, z);
                        // PlayerStartブロックが見つかったら
                        if (cell && cell->type == BlockType::PlayerStart) {
                            // そのブロックの少し上にプレイヤーを配置
                            player_->SetPosition({ (float)x, (float)y + 1.1f, (float)z });

                            // 見つけたらフラグを立ててループを抜ける
                            foundStart = true;
                            break;
                        }
                    }
                    // PlayerStartブロックが見つかったら、残りのループは回さない
                    if (foundStart) break;
                }
                // PlayerStartブロックが見つかったら、残りのループは回さない
                if (foundStart) break;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Overwrite (Save)")) {
            stageMap_.SaveToFile(fullPath);
        }

        // --- ここから追加：削除ボタン ---
        ImGui::SameLine();

        // ボタンの色を赤系に変更（色相 0.0=赤）
        ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));

        if (ImGui::Button("Delete")) {
            // 1. 物理ファイルを削除
            std::filesystem::remove(fullPath);
            // 2. リストを最新の状態に更新
            RefreshStageList();
            // 3. 削除した項目が選択されたままだと危ないのでリセット
            selectedStageIndex_ = -1;
        }

        ImGui::PopStyleColor(3); // 色設定を戻す
    }

    if (ImGui::Button("Refresh List")) { RefreshStageList(); }

    // カメラの情報表示と操作
    ImGui::Separator();
    if (ImGui::TreeNode("Camera")) {
        Transform& camTf = camera->GetTransform();

        ImGui::DragFloat3("Position", &camTf.translate.x, 0.1f);
        ImGui::DragFloat3("Rotation", &camTf.rotate.x, 0.01f);
        ImGui::SliderFloat("FOV", camera->GetFovPtr(), 0.01f, 3.14f);

        // カメラリセットボタン
        if (ImGui::Button("Reset Camera")) {
            camera->SetPosition({ 6.0f, 8.0f, -12.0f });
            camera->SetRotation({ 0.6f, 0.0f, 0.0f });
            camera->SetFov(0.45f);
        }

        ImGui::TreePop();
    }

    // ステージマップの情報表示
    if (ImGui::TreeNode("StageMap Info")) {
        ImGui::Text("Size: %d x %d x %d",
            stageMap_.GetWidth(),
            stageMap_.GetHeight(),
            stageMap_.GetDepth());

        const MapCell* cell = stageMap_.GetCell(2, 1, 0);
        if (cell) {
            ImGui::Text("Cell(2,1,0) type = %d", static_cast<int>(cell->type));
            ImGui::Text("Cell(2,1,0) solid = %s", cell->isSolid ? "true" : "false");
        }

        ImGui::TreePop();
    }

    // マップカーソルの情報表示
    ImGui::Separator();
    if (ImGui::TreeNode("Cursor Info")) {
        const Int3& cursor = mapCursor_->GetIndex();
        ImGui::Text("Cursor Index: (%d, %d, %d)", cursor.x, cursor.y, cursor.z);
        ImGui::TreePop();
    }

    // ステージエディタ用の設定項目
    ImGui::Separator();
    if (currentMode_ == AppMode::StageEditor && ImGui::TreeNode("StageEditor Settings")) {

        if (ImGui::SliderFloat("Uniform Block Scale", &editorUniformBlockScale_, 0.1f, 3.0f)) {
            editorBlockScale_ = {
                editorUniformBlockScale_,
                editorUniformBlockScale_,
                editorUniformBlockScale_
            };

            if (stageRenderer_) {
                stageRenderer_->SetBlockScale(editorBlockScale_);
                stageRenderer_->BuildFromStageMap(stageMap_);
            }
        }

        ImGui::DragFloat3("Block Scale XYZ", &editorBlockScale_.x, 0.01f, 0.1f, 5.0f);
        if (ImGui::Button("Apply Block Scale")) {

            if (stageRenderer_) {
                stageRenderer_->SetBlockScale(editorBlockScale_);
                stageRenderer_->BuildFromStageMap(stageMap_);
            }
        }
        ImGui::TreePop();
    }

    ImGui::Separator();
    ImGui::Text("Placeable Blocks: %d", blockInventory_.GetBlockCount());
    // デバッグ用：現在選択中のブロックタイプを表示
    ImGui::Text("Selected Block: %s", BlockTypeToString(selectedBlockType_));


    ImGui::End();
}
#endif

void MyGame::DrawCameraGuideSprites() {
    if (currentMode_ != AppMode::GamePlay) {
        return;
    }

    if (!cameraGuideLeftSprite_ ||
        !cameraGuideRightSprite_ ||
        !cameraGuideUpSprite_ ||
        !cameraGuideDownSprite_) {
        return;
    }

    spriteCommon->PreDraw();

    cameraGuideLeftSprite_->Draw();
    cameraGuideRightSprite_->Draw();
    cameraGuideUpSprite_->Draw();
    cameraGuideDownSprite_->Draw();
}

//5/7佐倉追加
void MyGame::UpdateDoorPrompt3D()
{
    if (!doorPromptObject_ || !player_) {
        return;
    }

    if (currentMode_ != AppMode::GamePlay || !player_->IsNearDoor()) {
        return;
    }

    Vector3 pos = player_->GetNearDoorWorldPos();

    doorPromptObject_->SetPosition(pos);
    doorPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    // カメラの方向を向かせる
    Vector3 camPos = camera->GetPosition();

    float angleY = std::atan2f(
        camPos.x - pos.x,
        camPos.z - pos.z
    );

    doorPromptObject_->SetRotation({ 0.0f, angleY, 0.0f });

    doorPromptObject_->SetCamera(
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix()
    );

    doorPromptObject_->Update(
        lightCamera_->GetViewProjectionMatrix()
    );
}

void MyGame::UpdateLadderPrompt3D()
{
    if (!ladderPromptObject_ || !player_) {
        return;
    }

    if (currentMode_ != AppMode::GamePlay || !player_->IsOnLadder()) {
        return;
    }

    Vector3 pos = player_->GetLadderWorldPos();

    ladderPromptObject_->SetPosition(pos);
    ladderPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    Vector3 camPos = camera->GetPosition();

    float angleY = std::atan2f(
        camPos.x - pos.x,
        camPos.z - pos.z
    );

    ladderPromptObject_->SetRotation({ 0.0f, angleY, 0.0f });

    ladderPromptObject_->SetCamera(
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix()
    );

    ladderPromptObject_->Update(
        lightCamera_->GetViewProjectionMatrix()
    );
}

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

void MyGame::UpdateLadderPrompt3D()
{
    if (!ladderPromptObject_ || !player_) {
        return;
    }

    if (currentMode_ != AppMode::GamePlay || !player_->IsOnLadder()) {
        return;
    }

    Vector3 pos = player_->GetLadderWorldPos();

    ladderPromptObject_->SetPosition(pos);
    ladderPromptObject_->SetScale({ 0.6f, 0.6f, 0.6f });

    Vector3 camPos = camera->GetPosition();

    float angleY = std::atan2f(
        camPos.x - pos.x,
        camPos.z - pos.z
    );

    ladderPromptObject_->SetRotation({ 0.0f, angleY, 0.0f });

    ladderPromptObject_->SetCamera(
        camera->GetViewMatrix(),
        camera->GetProjectionMatrix()
    );

    ladderPromptObject_->Update(
        lightCamera_->GetViewProjectionMatrix()
    );
}

void MyGame::Draw() {
    auto commandList = dxCommon->GetCommandList();
    // 1. 影パス
    shadowMap_->PreDraw(commandList);
    if (scene_) scene_->DrawShadow();
    shadowMap_->PostDraw(commandList);

    // 2. 本編パス
    dxCommon->PreDraw();
    if (scene_) {
        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
        scene_->Draw();

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

                    bool drawDoorPrompt =
                        doorPromptObject_ &&
                        currentMode_ == AppMode::GamePlay &&
                        player_ &&
                        player_->IsNearDoor();

                    bool drawLadderPrompt =
                        ladderPromptObject_ &&
                        currentMode_ == AppMode::GamePlay &&
                        player_ &&
                        player_->IsOnLadder();

                    if (drawDoorPrompt || drawLadderPrompt) {

                        object3dCommon->PreDrawPlayerHighlight();

                        if (drawDoorPrompt) {
                            doorPromptObject_->Draw();
                        }

                        if (drawLadderPrompt) {
                            ladderPromptObject_->Draw();
                        }

                        object3dCommon->PreDraw();
                        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
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

    //5/12佐倉置き換え

 // ==========================================================
// 3D UI
// 壁の裏でも見えるように、通常3D描画の最後に強調描画で描く
// ==========================================================
    bool drawDoorPrompt =
        doorPromptObject_ &&
        currentMode_ == AppMode::GamePlay &&
        player_ &&
        player_->IsNearDoor();

    bool drawLadderPrompt =
        ladderPromptObject_ &&
        currentMode_ == AppMode::GamePlay &&
        player_ &&
        player_->IsOnLadder();

    if (drawDoorPrompt || drawLadderPrompt) {

        object3dCommon->PreDrawPlayerHighlight();

        if (drawDoorPrompt) {
            doorPromptObject_->Draw();
        }

        if (drawLadderPrompt) {
            ladderPromptObject_->Draw();
        }

        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
    }


    //5/7佐倉
    DrawCameraGuideSprites();

    // --- 4. ImGui と 最終出力 ---
#ifdef USE_IMGUI
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

#ifdef USE_IMGUI
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
/// ブロックを置けるようになる画面 04/01 秋元
/// </summary>
void MyGame::UpdateGamePlayBlockPlace()
{
    // 現在カーソル位置
    const Int3& cursor = mapCursor_->GetIndex();

    if (input->TriggerKey(DIK_A)) {
        mapCursor_->Move(-1, 0, 0, stageMap_);
    }
    if (input->TriggerKey(DIK_D)) {
        mapCursor_->Move(1, 0, 0, stageMap_);
    }
    if (input->TriggerKey(DIK_W)) {
        mapCursor_->Move(0, 0, 1, stageMap_);
    }
    if (input->TriggerKey(DIK_S)) {
        mapCursor_->Move(0, 0, -1, stageMap_);
    }
    if (input->TriggerKey(DIK_Q)) {
        mapCursor_->Move(0, 1, 0, stageMap_);
    }
    if (input->TriggerKey(DIK_E)) {
        mapCursor_->Move(0, -1, 0, stageMap_);
    }

    // カーソルの座標を更新
    if (mapCursor_) {
        mapCursor_->Update(lightCamera_->GetViewProjectionMatrix());
    }

    // ② ブロックを置く決定処理 (Enterキー)
    if (input->TriggerKey(DIK_RETURN)) {
        Int3 cursorPos = mapCursor_->GetIndex();

        // カーソルの位置に何もない（None）場合のみ置けるようにする
        if (stageMap_.GetCell(cursorPos)->type == BlockType::None) {

            // ブロックを配置！
            stageMap_.SetBlock(cursorPos, BlockType::Ground); // 足場を置く
            stageRenderer_->BuildFromStageMap(stageMap_);     // 描画モデルを再構築（超重要）

            placeableBlockCount_--; // 所持数を減らす
            currentMode_ = AppMode::GamePlay; // 通常のプレイ画面に戻る
        }
    }

    // ③ キャンセルして戻る処理 (Qキーでもう一度戻るなど)
    if (input->TriggerKey(DIK_ESCAPE))
    {
        currentMode_ = AppMode::GamePlay;
    }

    // --- 描画フラグ ---
    if (ImGui::CollapsingHeader("Draw Flags", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Show 3D Objects", &debugFlags_.show3DObjects);
        ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
        ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);
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
            stageRenderer_->BuildFromStageMap(stageMap_); // 見た目の更新

            // プレイヤーの位置をスタート地点に戻すなどの処理
            // ResetPlayer(); 
        }
        // ゲームプレイモードへ切り替え
        currentMode_ = AppMode::GamePlay;
    }
}

/// <summary>
///  ギミック用のImGui 04/03 秋元
/// </summary>
void MyGame::DrawEditorToolbar()
{
    // ステージエディタモードの時だけ表示する
    if (currentMode_ != AppMode::StageEditor) return;

    // ウィンドウの設定（位置やサイズを固定したい場合はここを調整）
    ImGui::SetNextWindowPos(ImVec2(360, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 400), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Editor Toolbar", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("1. Select Gimmick");
        ImGui::Separator();

        // ギミックごとのボタン
        // 選択中のものは色を変えるとかっこいいです
        BlockType types[] = {
            BlockType::Ground, BlockType::Wall, BlockType::Ladder,
            BlockType::Star, BlockType::BubblePickup, BlockType::Goal,
            BlockType::PlayerStart, BlockType::Door,BlockType::PSwitch,
            BlockType::PBlock,BlockType::CrumblingFloor
        };

        for (auto type : types) {
            bool isSelected = (selectedBlockType_ == type);
            if (isSelected) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f)); // 選択中は緑っぽく
            }

            if (ImGui::Button(BlockTypeToString(type), ImVec2(-FLT_MIN, 30))) {
                selectedBlockType_ = type;
            }

            if (isSelected) ImGui::PopStyleColor();
        }

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("2. Action");
        // --- 回転ボタン (Rキーの機能) ---
        if (ImGui::Button("Rotate (R)", ImVec2(-FLT_MIN, 30))) {
            const Int3& cursor = mapCursor_->GetIndex();
            MapCell* cell = stageMap_.GetCell(cursor.x, cursor.y, cursor.z);
            if (cell && cell->type != BlockType::None) {
                cell->rotationY += 1.5708f; // 90度回転
                if (stageRenderer_) stageRenderer_->BuildFromStageMap(stageMap_);
            }
        }

        ImGui::Spacing();

        // 配置実行ボタン
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
        if (ImGui::Button("PLACE (Enter)", ImVec2(180, 40))) {
            ApplyPlacement();
        }
        ImGui::PopStyleColor();

        if (input->TriggerKey(DIK_RETURN))
        {
            ApplyPlacement();
        }

        if (ImGui::Button("REMOVE (Space)", ImVec2(180, 40))) {
            stageMap_.RemoveBlock(mapCursor_->GetIndex());
            stageRenderer_->BuildFromStageMap(stageMap_);
        }

        if (input->TriggerKey(DIK_BACKSPACE))
        {
            stageMap_.RemoveBlock(mapCursor_->GetIndex());
            stageRenderer_->BuildFromStageMap(stageMap_);
        }
        
    }
    ImGui::End();
#endif
}