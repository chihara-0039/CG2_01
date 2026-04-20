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

    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get()); // get() で中身の生ポインタを貸し出す 

    input = std::make_unique<Input>();
    input->Initialize(winApp.get()); // get() を使用

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

    // シーン管理 
    titleScene_ = std::make_unique<TitleScene>();
    titleScene_->Initialize(object3dCommon.get(), input.get());

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

    // プレイヤーの生成
    player_ = std::make_unique<Player>();
    player_->Initialize(object3dCommon.get(), models[0].get());
    player_->SetPosition({ 0.0f, 1.5f, 0.0f });

    // エディタ用カメラ
    camera = std::make_unique<Camera>();

    // 1. ステージマップのサイズ初期化
    stageMap_.Initialize(16, 8, 16);

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
                if (foundStart) break;
            }
            if (foundStart) break;
        }
    }
#else

    //4/3佐倉タイトルから開始に変更

    // 【Debugビルド時】
    currentMode_ = AppMode::Title;


    // 2. ★手動配置を消して、保存した「プロトタイプ」をロードする
    std::string prototypePath = "Resources/Stages/prototype.txt"; // 保存したファイル名に合わせてください
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
#ifdef USE_IMGUI
    dxCommon->BeginImGui();
    UpdateImGui();
    DrawEditorToolbar(); // ★追加：新しいエディタ窓 04/03 秋元
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

    
   
    // --- ImGuiに入力中（WantCaptureKeyboardがtrue）ならゲーム側の入力を無視する ---
    if (!isGuiCaptured) {
        switch (currentMode_) {

        case AppMode::Title:
        UpdateTitle();//4/3佐倉　追加
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
            }
            break;
        }
    }

    camera->Update();

    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();
    // ライト視点の行列を取得しておきます
    //const Matrix4x4& lightVP = lightCamera_->GetViewProjectionMatrix();

    // --- プレイヤーに最新のカメラ行列を教える ---
    if (player_) {
        player_->SetCamera(view, proj);
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
    const float rotateSpeed = 0.025f;

    if (input->PushKey(DIK_Q)) cameraAngle_ -= rotateSpeed;
    if (input->PushKey(DIK_E)) cameraAngle_ += rotateSpeed;

    // --- 縦回転 ---
    const float minPitch = 0.4f;
    const float maxPitch = 1.5f;
    const float upperLimit = 3.0f;

    if (input->PushKey(DIK_Z)) {
        cameraPitch_ += rotateSpeed;
        if (cameraPitch_ > upperLimit) {
            cameraPitch_ = upperLimit;
        }
    }

    if (input->PushKey(DIK_C)) {
        cameraPitch_ -= rotateSpeed;
        if (cameraPitch_ < minPitch) {
            cameraPitch_ = minPitch;
        }
    }

    if (cameraPitch_ > maxPitch) {
        cameraPitch_ = maxPitch;
    }

    // --- カメラ位置計算 ---
    Vector3 pivot = { 4.0f, 9.0f, 4.5f };
    float distance = 35.0f;
    float height = 20.0f;

    Vector3 pos;
    pos.x = pivot.x - std::cos(cameraPitch_) * std::sin(cameraAngle_) * distance;
    pos.y = pivot.y + std::sin(cameraPitch_) * height;
    pos.z = pivot.z - std::cos(cameraPitch_) * std::cos(cameraAngle_) * distance;

    camera->SetPosition(pos);
    camera->SetRotation({ cameraPitch_, cameraAngle_, 0.0f });

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

    int gx = static_cast<int>(std::floor(pPos.x + 0.5f));
    int gy = static_cast<int>(std::floor(pPos.y));      // ★安定版
    int gz = static_cast<int>(std::floor(pPos.z + 0.5f));

    /*==================================================
        ▼ アイテム取得
    ==================================================*/
    MapCell* cell = stageMap_.GetCell(gx, gy, gz);
    if (cell && cell->type == BlockType::BubblePickup) {
        placeableBlockCount_++;
        stageMap_.RemoveBlock(gx, gy, gz);
        stageRenderer_->BuildFromStageMap(stageMap_);
    }

    /*==================================================
        ▼ ゴール判定（★追加部分）
    ==================================================*/
    Vector3 radius = { 0.4f, 0.9f, 0.4f }; // プレイヤーサイズに合わせる

    if (Goal::Check(pPos, radius, stageMap_)) {
        isGoalReached_ = true;
    }

    /*==================================================
        ▼ 配置モード切り替え
    ==================================================*/
    if (input->TriggerKey(DIK_TAB) && placeableBlockCount_ > 0) {
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

	// デバッグ用：現在選択中のブロックタイプを表示
    ImGui::Text("Selected Block: %s", BlockTypeToString(selectedBlockType_));


    ImGui::End();
#endif
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
                if (player_) player_->Draw();

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

    // --- 4. ImGui と 最終出力 ---
#ifdef USE_IMGUI
    dxCommon->EndImGui();
#endif

    dxCommon->PostDraw();
}

void MyGame::Finalize() {
    ModelManager::Finalize();
#ifdef USE_IMGUI
    dxCommon->FinalizeImGui();
#endif

    // delete はすべて削除！ 
    // 依存関係のあるリソースを、明示的に安全な順番で消去（reset）します 
    objectList.clear();
    models.clear();

    player_.reset();
    skydomeObject_.reset();
    skydomeModel_.reset();
    sprite.reset();
    stageRenderer_.reset();
    mapCursor_.reset();
    shadowMap_.reset();
    lightCamera_.reset();
    camera.reset();
    titleScene_.reset();
    gameClearScene_.reset();

    particleManager.reset();
    object3dCommon.reset();
    spriteCommon.reset();
    textureManager.reset();
    input.reset();

    // 最後に基盤を消す 
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
    if (input->TriggerKey(DIK_TAB))
    {
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
            currentMode_ = AppMode::GamePlay;
        }
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
            BlockType::PBlock
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
