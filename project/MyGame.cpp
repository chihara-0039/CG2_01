#include <filesystem>

#include "MyGame.h"
#include "Goal.h"
#include "ModelManager.h"

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
    default:                      return "Unknown";
    }
}

// --- MyGameクラスの実装 ---
void MyGame::Initialize() {
    // --- 基盤初期化 ---
    winApp = new WinApp();
    winApp->Initialize();

    dxCommon = new DirectXCommon();
    dxCommon->Initialize(winApp);

    input = new Input();
    input->Initialize(winApp);

    textureManager = new TextureManager();
    textureManager->Initialize(dxCommon);

    spriteCommon = new SpriteCommon();
    spriteCommon->SetTextureManager(textureManager);
    spriteCommon->Initialize(dxCommon);

    object3dCommon = new Object3dCommon();
    object3dCommon->SetTextureManager(textureManager);
    object3dCommon->Initialize(dxCommon);

    particleManager = new ParticleManager();
    particleManager->Initialize(dxCommon, textureManager);

    // --- モデル読み込み (各1回ずつ) ---
    Model* modelPlane = Model::CreateFromOBJ(dxCommon, "Resources", "block.obj", textureManager);
    Model* modelAxis = Model::CreateFromOBJ(dxCommon, "Resources", "axis.obj", textureManager);
    models.push_back(modelPlane);
    models.push_back(modelAxis);

    // --- オブジェクト生成 ---
    CreateObject(modelPlane, { 0.0f, 0.0f, 0.0f })->SetScale({ 10.0f, 1.0f, 10.0f });
    CreateObject(modelAxis, { 2.0f, 0.0f, 0.0f });
    CreateObject(modelAxis, { -2.0f, 0.0f, 0.0f });

    // スプライト
    uint32_t texHandle = textureManager->LoadTexture("Resources/uvChecker.png");
    sprite = new Sprite();
    sprite->Initialize(spriteCommon, texHandle);

    // プレイヤーの生成（既存のモデルリストからモデルを渡す）
    player_ = new Player();
    player_->Initialize(object3dCommon, models[0]); 

	// プレイヤーの初期位置をステージの中心付近に設定
	player_->SetPosition({ 0.0f, 1.5f, 0.0f });

    // カメラ
    camera = std::make_unique<Camera>();

	//テストでステージ配置を初期化
    stageMap_.Initialize(16, 8, 16);

    // テスト用に少しだけ置く
    stageMap_.SetBlock(0, 0, 0, BlockType::Ground);
    stageMap_.SetBlock(1, 0, 0, BlockType::Ground);
    stageMap_.SetBlock(2, 0, 0, BlockType::Ground);
    stageMap_.SetBlock(2, 1, 0, BlockType::Wall);
    stageMap_.SetBlock(3, 0, 1, BlockType::BubblePickup);
    stageMap_.SetBlock(4, 0, 2, BlockType::Goal);
    
	// ステージマップからステージ描画オブジェクトを生成
    stageRenderer_ = new StageRenderer();
    stageRenderer_->Initialize(object3dCommon);
    stageRenderer_->SetBlockScale(editorBlockScale_);
    stageRenderer_->BuildFromStageMap(stageMap_);

	// マップカーソルの初期化
    mapCursor_ = new MapCursor();
    mapCursor_->Initialize(object3dCommon);
    mapCursor_->SetIndex({ 0, 0, 0 }, stageMap_);
    mapCursor_->SetScale({ 0.9f, 0.9f, 0.9f });
}

// ヘルパー関数：モデルと位置を指定して3Dオブジェクトを生成し、リストに追加して返す
Object3d* MyGame::CreateObject(Model* model, Vector3 pos) {
    Object3d* obj = new Object3d();
    obj->Initialize(object3dCommon);
    obj->SetModel(model);
    obj->SetPosition(pos);
    obj->SetRotation({ 1.57f, 0.0f, 0.0f });

    objectList.push_back(obj);
    return obj;
}

// --- 更新処理 ---
void MyGame::Update() {
#ifdef USE_IMGUI
    dxCommon->BeginImGui();
    UpdateImGui();
#endif

    input->Update();

    // 2. カメラの更新（Blender風操作を適用）
#ifdef USE_IMGUI
    // ImGuiに触っていない時だけカメラを動かすようにすると操作しやすいです
    if (!ImGui::GetIO().WantCaptureMouse) {
        camera->UpdateBlenderStyle(input);
    }
#else
    camera->UpdateBlenderStyle(input);
#endif
   
    // --- ImGuiに入力中（WantCaptureKeyboardがtrue）ならゲーム側の入力を無視する ---
    if (!ImGui::GetIO().WantCaptureKeyboard) {
        switch (currentMode_) {
        case AppMode::DebugView:
        UpdateDebugView();
        break;

        case AppMode::StageEditor:
        UpdateStageEditor(); // 名前入力中はここが呼ばれなくなる
        break;

        case AppMode::GamePlay:
        UpdateGamePlay();
        break;
        }
    }

    camera->Update();

    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();

    // --- プレイヤーに最新のカメラ行列を教える ---
    if (player_) {
        player_->SetCamera(view, proj);
    }

	// 3Dオブジェクトの更新
    if (debugFlags_.show3DObjects) {
        for (Object3d* obj : objectList) {
            obj->SetCamera(view, proj);
            obj->Update();
        }
    }

	// ステージ描画オブジェクトの更新
    if (stageRenderer_) {
        stageRenderer_->SetCamera(view, proj);
        stageRenderer_->Update();
    }

	// マップカーソルの更新
    if (mapCursor_) {
        mapCursor_->SetCamera(view, proj);
        mapCursor_->Update();
    }

	// スプライトの更新
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        sprite->Update();
    }

	// パーティクルの更新
    if (debugFlags_.showParticles) {
        particleManager->Update(view, proj);
    }
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
    if (player_) {
        // 第3引数にカメラのY軸回転角を渡すように変更
        player_->Update(input, stageMap_, camera->GetTransform().rotate.y);

        // 3/27 佐倉追加 
        if (!isGoalReached_) {
            if (Goal::Check(player_->GetPosition(), player_->GetRadius(), stageMap_)) {
                OutputDebugStringA("GOAL!\n");
                isGoalReached_ = true;
            }
        }
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
}

#endif

void MyGame::Draw() {
    dxCommon->PreDraw();

    ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
    dxCommon->GetCommandList()->SetDescriptorHeaps(1, heaps);
    if (debugFlags_.show3DObjects) {
        object3dCommon->PreDraw();

		// プレイヤーの描画は3Dオブジェクトの描画の中で行う（プレイヤーもObject3dを使っているため）
        if (currentMode_ == AppMode::GamePlay && player_) {
            player_->Draw();
        }

		// 3Dオブジェクトの描画
        if (currentMode_ == AppMode::DebugView) {
            for (Object3d* obj : objectList) {
                obj->Draw();
            }
        }


		// ステージ描画オブジェクトの描画
        if (currentMode_ == AppMode::StageEditor || currentMode_ == AppMode::GamePlay) {
            if (stageRenderer_) {
                stageRenderer_->Draw();
            }

            // カーソルはエディタモードのときだけ出す
            if (currentMode_ == AppMode::StageEditor && mapCursor_) {
                mapCursor_->Draw();
            }
        }
    }

	// パーティクル描画は3Dオブジェクトの後にするのが見栄え的に良いと思う
    if (debugFlags_.showParticles) {
        particleManager->Draw();
    }

	// スプライト描画は最後にするのが基本
    if (debugFlags_.showSprite && currentMode_ == AppMode::DebugView) {
        spriteCommon->PreDraw();
        sprite->Draw();
    }

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

    for (Object3d* obj : objectList) {
        delete obj;
    }

    for (Model* m : models) {
        delete m;
    }

    delete player_;
    delete sprite;
    delete particleManager;
    delete object3dCommon;
    delete spriteCommon;
    delete textureManager;
    delete input;
    delete dxCommon; // 基盤は最後の方
    delete winApp;


    if (stageRenderer_) {
        delete stageRenderer_;
        stageRenderer_ = nullptr;
    }

    if (mapCursor_) {
        delete mapCursor_;
        mapCursor_ = nullptr;
    }
}