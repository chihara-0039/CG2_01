#include <filesystem>

#include "MyGame.h"
#include "Goal.h"
#include "ModelManager.h"
#include <memory>

#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

// デバッグ用：BlockTypeを文字列に変換
static const char* BlockTypeToString(BlockType type) {
    switch (type) {
    case BlockType::None:         return "None";
    case BlockType::Ground:       return "Ground";
    case BlockType::Wall:         return "Wall";
	case BlockType::Ladder:       return "Ladder";
    case BlockType::Star:         return "Star";
    case BlockType::BubblePickup: return "BubblePickup";
    case BlockType::Goal:         return "Goal";
    case BlockType::PlayerStart:  return "PlayerStart";
    case BlockType::Door:         return "Door";
    case BlockType::PSwitch:      return "PSwitch";
    case BlockType::PBlock:       return "PBlock";
    default:                      return "Unknown";
    }
}

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

    // ステージファイル一覧を更新しておく
    RefreshStageList();

    // --- 3. ビルド設定による初期化分岐 ---
#ifdef NDEBUG
    // 【Releaseビルド時】直接ゲームを開始する
    currentMode_ = AppMode::GamePlay;

    // "Stage1.txt" があれば自動ロード
    std::string startStage = "Resources/Stages/Stage1.txt";
    if (std::filesystem::exists(startStage)) {
        stageMap_.LoadFromFile(startStage);

        // PlayerStartブロックを探してプレイヤーを配置
        bool foundStart = false;
        for (int y = 0; y < stageMap_.GetHeight(); ++y) {
            for (int z = 0; z < stageMap_.GetDepth(); ++z) {
                for (int x = 0; x < stageMap_.GetWidth(); ++x) {
                    const MapCell* cell = stageMap_.GetCell(x, y, z);
                    if (cell && cell->type == BlockType::PlayerStart) {
                        player_->SetPosition({ (float)x, (float)y + 1.1f, (float)z });
                        foundStart = true;
                        break;
                    }
                }
                if (foundStart) { break; }
            }
            if (foundStart) { break; }
        }
    }

#else

    //4/3佐倉タイトルから開始に変更

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

    // マップカーソルの初期化
    mapCursor_ = std::make_unique<MapCursor>();
    mapCursor_->Initialize(object3dCommon.get());
    mapCursor_->SetIndex({ 0, 0, 0 }, stageMap_);
    mapCursor_->SetScale({ 0.9f, 0.9f, 0.9f });

    cameraAngle_ = 1.5708f; // ★ここで開始時の向きを調整！
    cameraPitch_ = 0.75f;

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
    bool isGuiCaptured = false;
    // 2. カメラの更新（Blender風操作を適用）
#ifndef NDEBUG
    // Release時は ImGui::GetIO() を呼ばないようにガードする
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
    
	// バブルピックアップ
    if (input->TriggerKey(DIK_3)) {
        selectedBlockType_ = BlockType::BubblePickup;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        needRebuild = true;
    }

	// ゴール
    if (input->TriggerKey(DIK_4)) {
        selectedBlockType_ = BlockType::Goal;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        needRebuild = true;
    }

    // はしご
    if(input->TriggerKey(DIK_5)) {
        selectedBlockType_ = BlockType::Ladder;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        needRebuild = true;
	}

    // ドア
    if (input->TriggerKey(DIK_6))
    {
        selectedBlockType_ = BlockType::Door;
        MapCell* oldCell = stageMap_.GetCell(cursor.x, cursor.y, cursor.z);

        if (oldCell && oldCell->type == BlockType::Door) {
            Int3 target = oldCell->doorTargetIndex;

            // 1. すでに別のドアとペアリング済みの場合、相手のリンクを切る
            // （ワープ先が自分自身ではない場合＝ペアがいる）
            if (target.x != cursor.x || target.y != cursor.y || target.z != cursor.z) {
                MapCell* pairedCell = stageMap_.GetCell(target.x, target.y, target.z);
                if (pairedCell && pairedCell->type == BlockType::Door) {
                    // 相手のワープ先を相手自身の座標に戻す（リンク解除）
                    pairedCell->doorTargetIndex = target;
                }
            }

            // 2. ペアリング待機中（1つ目のドア）を消してしまった場合のキャンセル処理
            if (isWaitingForSecondDoor_ &&
                firstDoorIndex_.x == cursor.x &&
                firstDoorIndex_.y == cursor.y &&
                firstDoorIndex_.z == cursor.z) {

                isWaitingForSecondDoor_ = false; // 2つ目待ちをキャンセル
            }
        }
        stageMap_.SetBlock(cursor, selectedBlockType_);
        if (!isWaitingForSecondDoor_)
        {
            // ▼ 1つ目のドアを置いた時
            firstDoorIndex_ = cursor;
            isWaitingForSecondDoor_ = true;// 2つ目待ち状態へ

            // (オプション) この段階ではまだワープ先がないので自分自身をセットしておく
            MapCell* cell = stageMap_.GetCell(cursor.x, cursor.y, cursor.z);
            if (cell)
            {
                cell->doorTargetIndex = cursor;
            }
        }
        else
        {
            // ▼ 2つ目のドアを置いた時

                // 1. 2つ目のドアのワープ先を「1つ目のドア」に設定
            MapCell* cell2 = stageMap_.GetCell(cursor.x, cursor.y, cursor.z);
            if (cell2) cell2->doorTargetIndex = firstDoorIndex_;

            // 2. 1つ目のドアのワープ先を「今置いた2つ目のドア」に設定
            MapCell* cell1 = stageMap_.GetCell(firstDoorIndex_.x, firstDoorIndex_.y, firstDoorIndex_.z);
            if (cell1) cell1->doorTargetIndex = cursor;

            // 3. ペアリング完了！状態をリセットして次のペア作りに備える
            isWaitingForSecondDoor_ = false;
        }
        needRebuild = true;
        
    }

    // プレイヤーのスタート位置
    if (input->TriggerKey(DIK_7))
    {
        selectedBlockType_ = BlockType::PlayerStart;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        player_->SetPosition({ float(cursor.x),float(cursor.y),float(cursor.z) });
        needRebuild = true;
    }

    // 削除
    if (input->TriggerKey(DIK_SPACE)) {
        stageMap_.RemoveBlock(cursor);
        needRebuild = true;
    }

    // 再構築
    if (needRebuild && stageRenderer_) {
        stageRenderer_->BuildFromStageMap(stageMap_);
    }

	// カメラ操作（Blender風の操作もできるようにしているので、そちらとキーが被らないように注意してください）
    Transform& camTf = camera->GetTransform();

    if (input->PushKey(DIK_J)) {
        camTf.rotate.y -= 0.02f;
    }
    if (input->PushKey(DIK_L)) {
        camTf.rotate.y += 0.02f;
    }
    if (input->PushKey(DIK_I)) {
        camTf.translate.z += 0.2f;
    }
    if (input->PushKey(DIK_K)) {
        camTf.translate.z -= 0.2f;
    }
    if (input->PushKey(DIK_U)) {
        camTf.translate.y += 0.2f;
    }
    if (input->PushKey(DIK_O)) {
        camTf.translate.y -= 0.2f;
    }
}

void MyGame::UpdateGamePlay() {

    //4/20佐倉追加
    const auto& mouse = input->GetMouseState();

    // 画面サイズ取得（ウィンドウサイズ変更に対応するため動的に取得）
    RECT rect;
    GetClientRect(winApp->GetHwnd(), &rect);
    float screenWidth = static_cast<float>(rect.right - rect.left);
    float screenHeight = static_cast<float>(rect.bottom - rect.top);


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

    // 画面サイズ取得（ウィンドウサイズ変更に対応するため動的に取得）
    RECT rect;
    GetClientRect(winApp->GetHwnd(), &rect);
    float screenWidth = static_cast<float>(rect.right - rect.left);
    float screenHeight = static_cast<float>(rect.bottom - rect.top);


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

#ifndef NDEBUG
// ImGuiの更新と描画
void MyGame::UpdateImGui() {
    ImGuiIO& io = ImGui::GetIO();

    // 画面右側にインスペクター（設定・情報パネル）を配置
    float panelWidth = 350.0f;
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y), ImGuiCond_Always);
    
    // ウィンドウ作成
    ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    // FPSと終了ボタンを同じ行に配置
    ImGui::Text("FPS: %.1f (%.3f ms/f)", io.Framerate, 1000.0f / io.Framerate);
    ImGui::SameLine(panelWidth - 60.0f);
    if (ImGui::Button("Exit", ImVec2(50, 20))) {
        PostQuitMessage(0); // アプリケーション終了
    }

    ImGui::Separator();

    // モード切替（Hierarchy的な役割）
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

    // ステージマネージャー
    if (ImGui::CollapsingHeader("Stage Manager", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Save Name", newStageName_, IM_ARRAYSIZE(newStageName_));
        if (ImGui::Button("Save As New")) {
            std::string path = "Resources/Stages/" + std::string(newStageName_) + ".txt";
            stageMap_.SaveToFile(path);
            RefreshStageList();
        }

        ImGui::Text("Saved Stages:");
        if (ImGui::BeginListBox("##StageList", ImVec2(-FLT_MIN, 100))) {
            for (int n = 0; n < (int)stageFiles_.size(); n++) {
                const bool is_selected = (selectedStageIndex_ == n);
                if (ImGui::Selectable(stageFiles_[n].c_str(), is_selected)) {
                    selectedStageIndex_ = n;
                }
            }
            ImGui::EndListBox();
        }

        if (selectedStageIndex_ != -1 && selectedStageIndex_ < (int)stageFiles_.size()) {
            std::string fullPath = "Resources/Stages/" + stageFiles_[selectedStageIndex_] + ".txt";
            
            if (ImGui::Button("Load Selected")) {
                stageMap_.LoadFromFile(fullPath);
                if (stageRenderer_) {
                    stageRenderer_->BuildFromStageMap(stageMap_);
                }

                // PlayerStartブロックを探してプレイヤーを移動
                bool foundStart = false;
                for (int y = 0; y < stageMap_.GetHeight(); ++y) {
                    for (int z = 0; z < stageMap_.GetDepth(); ++z) {
                        for (int x = 0; x < stageMap_.GetWidth(); ++x) {
                            const MapCell* cell = stageMap_.GetCell(x, y, z);
                            if (cell && cell->type == BlockType::PlayerStart) {
                                player_->SetPosition({ (float)x, (float)y + 1.1f, (float)z });
                                foundStart = true;
                                break;
                            }
                        }
                        if (foundStart) { break; }
                    }
                    if (foundStart) { break; }
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Overwrite")) {
                stageMap_.SaveToFile(fullPath);
            }
            ImGui::SameLine();
            
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, (ImVec4)ImColor::HSV(0.0f, 0.7f, 0.7f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, (ImVec4)ImColor::HSV(0.0f, 0.8f, 0.8f));
            if (ImGui::Button("Delete")) {
                std::filesystem::remove(fullPath);
                RefreshStageList();
                selectedStageIndex_ = -1;
            }
            ImGui::PopStyleColor(3);
        }

        if (ImGui::Button("Refresh List")) { RefreshStageList(); }
    }

    // カメラ情報（委譲）
    if (ImGui::CollapsingHeader("Camera Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        camera->DrawImGui();
    }

    // ステージマップ情報（委譲）
    if (ImGui::CollapsingHeader("StageMap Info")) {
        stageMap_.DrawImGui();
    }

    // カーソル情報（委譲）
    if (ImGui::CollapsingHeader("Cursor Info")) {
        mapCursor_->DrawImGui();
        ImGui::Text("Placeable Blocks: %d", blockInventory_.GetBlockCount());
        ImGui::Text("Selected Block: %s", BlockTypeToString(selectedBlockType_));
    }

    // エディタ設定
    if (currentMode_ == AppMode::StageEditor) {
        if (ImGui::CollapsingHeader("Stage Editor Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::SliderFloat("Uniform Block Scale", &editorUniformBlockScale_, 0.1f, 3.0f)) {
                editorBlockScale_ = { editorUniformBlockScale_, editorUniformBlockScale_, editorUniformBlockScale_ };
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
            
            // ツールバーをここに統合
            DrawEditorToolbar();
        }
    }

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

    shadowMap_->PostDraw(commandList);

    // dxCommon->PreDraw() 内でこれを行っていない場合、ここで明示的に呼ぶ必要があります
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0, 1280, 720 };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

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

    //5/11佐倉

    // ==========================================================
// ドア用 3D F UI
// 壁の裏でも見えるように、通常3D描画の最後に強調描画で描く
// ==========================================================
    if (doorPromptObject_ &&
        currentMode_ == AppMode::GamePlay &&
        player_ &&
        player_->IsNearDoor()) {

        // プレイヤー壁裏強調と同じ描画設定を使う
        object3dCommon->PreDrawPlayerHighlight();

        doorPromptObject_->Draw();

        // 通常描画設定に戻す
        object3dCommon->PreDraw();
        commandList->SetGraphicsRootDescriptorTable(4, shadowMap_->GetSrvHandle());
    }


    //5/7佐倉
    DrawCameraGuideSprites();

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

    // カーソル移動処理
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


    // カメラ操作（Blender風の操作もできるようにしているので、そちらとキーが被らないように注意してください）
    Transform& camTf = camera->GetTransform();

    if (input->PushKey(DIK_J)) {
        camTf.rotate.y -= 0.02f;
    }
    if (input->PushKey(DIK_L)) {
        camTf.rotate.y += 0.02f;
    }
    if (input->PushKey(DIK_I)) {
        camTf.translate.z += 0.2f;
    }
    if (input->PushKey(DIK_K)) {
        camTf.translate.z -= 0.2f;
    }
    if (input->PushKey(DIK_U)) {
        camTf.translate.y += 0.2f;
    }
    if (input->PushKey(DIK_O)) {
        camTf.translate.y -= 0.2f;
    }
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

    ImGui::Text("1. Select Gimmick");
    ImGui::Separator();

    // ギミックごとのボタン
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

        if (isSelected) {
            ImGui::PopStyleColor();
        }
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
            if (stageRenderer_) {
                stageRenderer_->BuildFromStageMap(stageMap_);
            }
        }
    }

    ImGui::Spacing();

    // 配置実行ボタン
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("PLACE (Enter)", ImVec2(-FLT_MIN, 40))) {
        ApplyPlacement();
    }
    ImGui::PopStyleColor();

    if (input->TriggerKey(DIK_RETURN)) {
        ApplyPlacement();
    }

    if (ImGui::Button("REMOVE (Space)", ImVec2(-FLT_MIN, 40))) {
        stageMap_.RemoveBlock(mapCursor_->GetIndex());
        if (stageRenderer_) {
            stageRenderer_->BuildFromStageMap(stageMap_);
        }
    }

    if (input->TriggerKey(DIK_BACKSPACE)) {
        stageMap_.RemoveBlock(mapCursor_->GetIndex());
        if (stageRenderer_) {
            stageRenderer_->BuildFromStageMap(stageMap_);
        }
    }
}


/// <summary>
///  ギミックで複雑な処理持ちのやつをここに入れる 04/03 秋元
/// </summary>
void MyGame::ApplyPlacement()
{
    const Int3& cursor = mapCursor_->GetIndex();
    MapCell* oldCell = stageMap_.GetCell(cursor.x, cursor.y, cursor.z);
    // ドアの場合の特殊処理（既存のコードから移植）
    if (selectedBlockType_ == BlockType::Door) 
    {
        if (oldCell && oldCell->type == BlockType::Door) {
            Int3 target = oldCell->doorTargetIndex;

            // 1. すでに別のドアとペアリング済みの場合、相手のリンクを切る
            // （ワープ先が自分自身ではない場合＝ペアがいる）
            if (target.x != cursor.x || target.y != cursor.y || target.z != cursor.z) {
                MapCell* pairedCell = stageMap_.GetCell(target.x, target.y, target.z);
                if (pairedCell && pairedCell->type == BlockType::Door) {
                    // 相手のワープ先を相手自身の座標に戻す（リンク解除）
                    pairedCell->doorTargetIndex = target;
                }
            }

            // 2. ペアリング待機中（1つ目のドア）を消してしまった場合のキャンセル処理
            if (isWaitingForSecondDoor_ &&
                firstDoorIndex_.x == cursor.x &&
                firstDoorIndex_.y == cursor.y &&
                firstDoorIndex_.z == cursor.z) {

                isWaitingForSecondDoor_ = false; // 2つ目待ちをキャンセル
            }
        }
        // ※ longContentからコピーしたドアのペアリング処理
        stageMap_.SetBlock(cursor, BlockType::Door);
        if (!isWaitingForSecondDoor_)
        {
            // ▼ 1つ目のドアを置いた時
            firstDoorIndex_ = cursor;
            isWaitingForSecondDoor_ = true;// 2つ目待ち状態へ

            // (オプション) この段階ではまだワープ先がないので自分自身をセットしておく
            MapCell* cell = stageMap_.GetCell(cursor.x, cursor.y, cursor.z);
            if (cell)
            {
                cell->doorTargetIndex = cursor;
            }
        }
        else
        {
            // ▼ 2つ目のドアを置いた時

                // 1. 2つ目のドアのワープ先を「1つ目のドア」に設定
            MapCell* cell2 = stageMap_.GetCell(cursor.x, cursor.y, cursor.z);
            if (cell2) cell2->doorTargetIndex = firstDoorIndex_;

            // 2. 1つ目のドアのワープ先を「今置いた2つ目のドア」に設定
            MapCell* cell1 = stageMap_.GetCell(firstDoorIndex_.x, firstDoorIndex_.y, firstDoorIndex_.z);
            if (cell1) cell1->doorTargetIndex = cursor;

            // 3. ペアリング完了！状態をリセットして次のペア作りに備える
            isWaitingForSecondDoor_ = false;
        }
    }
    else {
        // 通常のブロック
        stageMap_.SetBlock(cursor, selectedBlockType_);
        if (selectedBlockType_ == BlockType::PlayerStart) {
            player_->SetPosition({ (float)cursor.x, (float)cursor.y + 1.1f, (float)cursor.z });
        }
    }

    // 再構築
    if (stageRenderer_) {
        stageRenderer_->BuildFromStageMap(stageMap_);
    }
}
