
#include "StageEditorController.h"
#include <cmath>
#include <algorithm>
#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void StageEditorController::Initialize() {
    // ブロック表示スケールの初期化
    editorBlockScale_ = { 1.0f, 1.0f, 1.0f };
    editorUniformBlockScale_ = 1.0f;
    
    selectedStageIndex_ = -1;
    selectedBlockType_ = BlockType::Ground;
    bubbleInsideBlockType_ = BlockType::Wall;
    selectedCustomPartSlot_ = 1;
    bubbleInsideCustomSlot_ = 0;
    selectedTimedGroupId_ = 1;
    selectedTimedOrderId_ = 0;
    
    // ドアのペアリング状態の初期化
    isWaitingForSecondDoor_ = false;
    firstDoorIndex_ = { -1, -1, -1 };
    
    // 保存済みのステージ一覧をスキャン・更新
    RefreshStageList();
}

// Resources/Stages フォルダ内のステージファイル（.txt）一覧を再取得・更新します。
void StageEditorController::RefreshStageList() {
    stageFiles_.clear();
    std::string path = "Resources/Stages/";

    // ディレクトリが存在しない場合は自動作成
    if (!std::filesystem::exists(path)) {
        std::filesystem::create_directories(path);
    }

    // フォルダ内の .txt ファイルを列挙してリストに格納（拡張子を除くファイル名）
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.path().extension() == ".txt") {
            stageFiles_.push_back(entry.path().stem().string());
        }
    }
}

// ステージマップ全体を走査し、プレイヤースタートブロック（PlayerStart）の位置へプレイヤーをリセットします。
void StageEditorController::ResetPlayerToStartCell(StageMap& stageMap, Player* player) {
    if (!player) return;

    bool foundStart = false;

    for (int y = 0; y < stageMap.GetHeight(); ++y) {
        for (int z = 0; z < stageMap.GetDepth(); ++z) {
            for (int x = 0; x < stageMap.GetWidth(); ++x) {
                const MapCell* cell = stageMap.GetCell(x, y, z);

                if (cell && cell->type == BlockType::PlayerStart) {
                    Vector3 startPos = {
                        static_cast<float>(x),
                        static_cast<float>(y) + 1.1f,
                        static_cast<float>(z)
                    };

                    player->SetPosition(startPos);
                    player->SetRespawnPosition(startPos);

                    foundStart = true;
                    break;
                }
            }

            if (foundStart) break;
        }

        if (foundStart) break;
    }

    if (!foundStart) {
        Vector3 defaultPos = { 0.0f, 1.5f, 0.0f };

        player->SetPosition(defaultPos);
        player->SetRespawnPosition(defaultPos);
    }
}

// 現在のカーソル位置に対して、選択中のブロック（またはドアのペアリング）を配置・適用します。
void StageEditorController::HandleCursorInput(Input* input, StageMap& stageMap, MapCursor* mapCursor, LightCamera* lightCamera, Camera* camera) {
    if (!input || !mapCursor || !lightCamera || !camera) return;

    float cameraRotY = camera->GetTransform().rotate.y;

    // 入力方向ベクトル
    int inputX = 0;
    int inputZ = 0;

    if (RepeatKey(input, DIK_A)) { inputX -= 1; }
    if (RepeatKey(input, DIK_D)) { inputX += 1; }
    if (RepeatKey(input, DIK_W)) { inputZ += 1; }
    if (RepeatKey(input, DIK_S)) { inputZ -= 1; }

    if (inputX != 0 || inputZ != 0) {
        // カメラの回転に基づいて移動ベクトルを計算
        float moveX = (float)inputX * std::cos(cameraRotY) + (float)inputZ * std::sin(cameraRotY);
        float moveZ = -(float)inputX * std::sin(cameraRotY) + (float)inputZ * std::cos(cameraRotY);

        // グリッド移動なので、絶対値が大きい方の軸へ移動する
        int dx = 0;
        int dz = 0;
        if (std::abs(moveX) > std::abs(moveZ)) {
            dx = moveX > 0.0f ? 1 : -1;
        } else {
            dz = moveZ > 0.0f ? 1 : -1;
        }

        mapCursor->Move(dx, 0, dz, stageMap);
    }

    if (RepeatKey(input, DIK_Q)) {
        mapCursor->Move(0, 1, 0, stageMap);
    }
    if (RepeatKey(input, DIK_E)) {
        mapCursor->Move(0, -1, 0, stageMap);
    }
    // 移動後のカーソル位置や描画用行列を更新
    mapCursor->Update(lightCamera->GetViewProjectionMatrix());
}

// IJKL / UO キーによるエディタカメラの平行移動・回転
void StageEditorController::HandleCameraInput(Input* input, Camera* camera) {
    if (!input || !camera) return;
    Transform& camTf = camera->GetTransform();

    // IJKL / UO キーによるエディタカメラの平行移動・回転
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

// エディタモードにおける毎フレームの更新処理（キー入力、配置・削除判定など）を行います。
void StageEditorController::Update(Input* input, StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, LightCamera* lightCamera, Player* player, Camera* camera) {
    if (!input) return;

    // 長押しフレームの更新
    if (input->PushKey(DIK_RETURN) || input->PushKey(DIK_SPACE) || 
        input->PushKey(DIK_A) || input->PushKey(DIK_D) || 
        input->PushKey(DIK_W) || input->PushKey(DIK_S) ||
        input->PushKey(DIK_Q) || input->PushKey(DIK_E)) {
        holdFrame_++;
    } else {
        holdFrame_ = 0;
    }

    // 1. カーソル操作 (WASD / QE)
    HandleCursorInput(input, stageMap, mapCursor, lightCamera, camera);

    // 2. エディタカメラ操作 (IJKL / UO)
    HandleCameraInput(input, camera);

    // 3. ブロック配置 (Enter)
    if (input->TriggerKey(DIK_RETURN) || RepeatKey(input, DIK_RETURN, 20, 5)) {
        ApplyPlacement(stageMap, stageRenderer, mapCursor, player);
    }

    // 4. ブロック削除 (Space)
    if (input->TriggerKey(DIK_SPACE) || RepeatKey(input, DIK_SPACE, 20, 5)) {
        if (mapCursor) {
            stageMap.RemoveBlock(mapCursor->GetIndex());
            if (stageRenderer) {
                stageRenderer->BuildFromStageMap(stageMap);
            }
        }
    }

    // 5. ブロック回転 (Rキー)
    if (input->TriggerKey(DIK_R)) {
        if (mapCursor) {
            const Int3& cursor = mapCursor->GetIndex();
            MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
            if (cell && cell->type != BlockType::None) {
                cell->rotationY += 1.5708f;
                if (stageRenderer) {
                    stageRenderer->BuildFromStageMap(stageMap);
                }
            }
        }
    }
}


// ImGui によるエディタ用パネル（セーブロード、設定、ツールバー）を描画します。
void StageEditorController::DrawImGui(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player) {
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();
    float panelWidth = 320.0f;
    float bottomHeight = 360.0f;

    // 右側パネルに配置：ステージの保存・読み込み管理
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - panelWidth, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(panelWidth, io.DisplaySize.y - bottomHeight), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(1.0f); // 透過なし
    ImGui::Begin("Stage Editor", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);

	// --- ステージの保存・読み込み管理 UI ---
    if (ImGui::CollapsingHeader("Stage Manager", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Save Name", newStageName_, IM_ARRAYSIZE(newStageName_));
		// 新規保存ボタン
        if (ImGui::Button("Save As New")) {
            std::string path = "Resources/Stages/" + std::string(newStageName_) + ".txt";
            stageMap.SaveToFile(path);
            RefreshStageList();
        }

        ImGui::Separator();
        ImGui::Text("New Blank Stage");

        ImGui::InputInt("Width", &newStageWidth_);
        ImGui::InputInt("Height", &newStageHeight_);
        ImGui::InputInt("Depth", &newStageDepth_);

        // std::max は Windows の max マクロと衝突することがあるので使わない
        if (newStageWidth_ < 1) { newStageWidth_ = 1; }
        if (newStageHeight_ < 1) { newStageHeight_ = 1; }
        if (newStageDepth_ < 1) { newStageDepth_ = 1; }

        if (ImGui::Button("Create Blank Stage")) {
            stageMap.Initialize(newStageWidth_, newStageHeight_, newStageDepth_);

            if (stageRenderer) {
                stageRenderer->BuildFromStageMap(stageMap);
            }

            ResetPlayerToStartCell(stageMap, player);
        }

        // 保存されているステージファイルの一覧を表示
        ImGui::Text("Saved Stages:");
        // 保存されているステージファイルの一覧をリストボックス表示
        if (ImGui::BeginListBox("##StageList", ImVec2(-FLT_MIN, 100))) {
            for (int n = 0; n < (int)stageFiles_.size(); n++) {
                const bool is_selected = (selectedStageIndex_ == n);
                if (ImGui::Selectable(stageFiles_[n].c_str(), is_selected)) {
                    selectedStageIndex_ = n;
                }
            }
            ImGui::EndListBox();
        }

        // リストから選択されているステージに対するアクション（ロード・上書き・削除）
        if (selectedStageIndex_ != -1 && selectedStageIndex_ < (int)stageFiles_.size()) {
            std::string fullPath = "Resources/Stages/" + stageFiles_[selectedStageIndex_] + ".txt";
            if (ImGui::Button("Load Selected")) {
                stageMap.LoadFromFile(fullPath);
                if (stageRenderer) {
                    stageRenderer->BuildFromStageMap(stageMap);
                }
                ResetPlayerToStartCell(stageMap, player);
            }
            ImGui::SameLine();
            if (ImGui::Button("Overwrite")) {
                stageMap.SaveToFile(fullPath);
            }
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
            if (ImGui::Button("Delete")) {
                std::filesystem::remove(fullPath);
                RefreshStageList();
                selectedStageIndex_ = -1;
            }
            ImGui::PopStyleColor();
        }
        if (ImGui::Button("Refresh List")) { RefreshStageList(); }
    }

    if (ImGui::CollapsingHeader("Environment & Lighting", ImGuiTreeNodeFlags_DefaultOpen)) {
        Vector4 clearColor = stageMap.GetClearColor();
        if (ImGui::ColorEdit4("Sky/Clear Color", &clearColor.x)) {
            stageMap.SetClearColor(clearColor);
        }

        Vector3 lightColor = stageMap.GetLightColor();
        if (ImGui::ColorEdit3("Light Color", &lightColor.x)) {
            stageMap.SetLightColor(lightColor);
        }

        float lightIntensity = stageMap.GetLightIntensity();
        if (ImGui::SliderFloat("Light Intensity", &lightIntensity, 0.0f, 3.0f)) {
            stageMap.SetLightIntensity(lightIntensity);
        }

        Vector3 lightDir = stageMap.GetLightDirection();
        bool dirChanged = false;
        dirChanged |= ImGui::SliderFloat("Light Dir X", &lightDir.x, -1.0f, 1.0f);
        dirChanged |= ImGui::SliderFloat("Light Dir Y", &lightDir.y, -1.0f, 1.0f);
        dirChanged |= ImGui::SliderFloat("Light Dir Z", &lightDir.z, -1.0f, 1.0f);
        if (dirChanged) {
            stageMap.SetLightDirection(lightDir);
        }
    }

    if (ImGui::CollapsingHeader("Stage Editor Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        // ブロック全体の均等スケール調整用スライダー
        if (ImGui::SliderFloat("Uniform Block Scale", &editorUniformBlockScale_, 0.1f, 3.0f)) {
            editorBlockScale_ = { editorUniformBlockScale_, editorUniformBlockScale_, editorUniformBlockScale_ };
            if (stageRenderer) {
                stageRenderer->SetBlockScale(editorBlockScale_);
                stageRenderer->BuildFromStageMap(stageMap);
            }
        }
    }

    // --- 自分でブロックパーツを作成・カスタマイズする UI ---
    if (ImGui::CollapsingHeader("Custom Block Maker", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Select Custom Slot:");
        for (int i = 1; i <= 5; ++i) {
            char label[16];
            sprintf_s(label, "Part %d", i);
            if (i > 1) ImGui::SameLine();
            
            bool isCurrent = (selectedCustomPartSlot_ == i);
            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.6f, 0.6f, 0.6f));
            }
            if (ImGui::Button(label)) {
                selectedCustomPartSlot_ = i;
            }
            if (isCurrent) {
                ImGui::PopStyleColor();
            }
        }

        auto* part = stageMap.GetCustomPart(selectedCustomPartSlot_);
        if (part) {
            bool changed = false;

            // 1. パーツ名編集
            char nameBuf[32];
            strcpy_s(nameBuf, part->name.c_str());
            if (ImGui::InputText("Part Name", nameBuf, sizeof(nameBuf))) {
                part->name = nameBuf;
            }

            // 2. カラー編集
            float color[3] = { part->colorR, part->colorG, part->colorB };
            if (ImGui::ColorEdit3("Color (RGB)", color)) {
                part->colorR = color[0];
                part->colorG = color[1];
                part->colorB = color[2];
                changed = true;
            }

            ImGui::Separator();
            ImGui::Text("--- 3x3x3 Shape Assembly Editor ---");
            ImGui::Text("Click cells to cycle: None -> Wall -> Ladder");

            // 編集対象のY座標（レイヤー 0〜2）
            static int editY = 0;
            ImGui::Text("Layer (Height Y):");
            for (int ly = 0; ly < 3; ++ly) {
                char layerLabel[16];
                sprintf_s(layerLabel, "Y = %d", ly);
                if (ly > 0) ImGui::SameLine();
                if (ImGui::RadioButton(layerLabel, &editY, ly)) {
                    // レイヤー変更
                }
            }

            // 3x3 グリッドの描画 (z, x)
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
            for (int lz = 2; lz >= 0; --lz) { // 奥から手前へ
                for (int lx = 0; lx < 3; ++lx) {
                    if (lx > 0) ImGui::SameLine();

                    auto& cell = part->cells[editY][lz][lx];
                    char btnLabel[64];
                    
                    // スロット番号とセル座標で一意なIDを作る
                    sprintf_s(btnLabel, "%s##%d_%d_%d_%d", 
                        (cell.type == BlockType::Wall) ? "WALL" : (cell.type == BlockType::Ladder) ? "LAD" : " . ",
                        selectedCustomPartSlot_, editY, lz, lx);

                    // セル別のカラーをボタンカラーに反映
                    if (cell.type != BlockType::None) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(part->colorR, part->colorG, part->colorB, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(part->colorR * 1.1f, part->colorG * 1.1f, part->colorB * 1.1f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(part->colorR * 0.8f, part->colorG * 0.8f, part->colorB * 0.8f, 1.0f));
                    } else {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
                    }

                    if (ImGui::Button(btnLabel, ImVec2(55.0f, 40.0f))) {
                        // トグル切り替え: None -> Wall -> Ladder -> None
                        if (cell.type == BlockType::None) {
                            cell.type = BlockType::Wall;
                        } else if (cell.type == BlockType::Wall) {
                            cell.type = BlockType::Ladder;
                        } else {
                            cell.type = BlockType::None;
                        }
                        changed = true;
                    }
                    ImGui::PopStyleColor(3);
                }
            }
            ImGui::PopStyleVar();

            // 一括クリアボタン
            if (ImGui::Button("Clear Entire Shape")) {
                for (int y = 0; y < 3; ++y) {
                    for (int z = 0; z < 3; ++z) {
                        for (int x = 0; x < 3; ++x) {
                            part->cells[y][z][x].type = BlockType::None;
                        }
                    }
                }
                changed = true;
            }

            // 変更があったら3D表示をリアルタイム再構築！
            if (changed && stageRenderer) {
                stageRenderer->BuildFromStageMap(stageMap);
            }
        }
    }

    if (selectedBlockType_ == BlockType::BubblePickup)
    {
        if (ImGui::CollapsingHeader("Bubble Settings", ImGuiTreeNodeFlags_DefaultOpen)) 
        {
            ImGui::Text("Bubble Contents:");
            
            int currentItem = 0;
            if (bubbleInsideCustomSlot_ >= 1 && bubbleInsideCustomSlot_ <= 5) {
                currentItem = bubbleInsideCustomSlot_ + 1;
            } else {
                currentItem = (bubbleInsideBlockType_ == BlockType::Ladder) ? 1 : 0;
            }

            std::vector<std::string> comboItems = { "Default Wall", "Default Ladder" };
            for (int i = 1; i <= 5; ++i) {
                const auto* part = stageMap.GetCustomPart(i);
                std::string displayName = "Custom " + std::to_string(i);
                if (part && !part->name.empty()) {
                    displayName += " (" + part->name + ")";
                }
                comboItems.push_back(displayName);
            }

            std::vector<const char*> itemsPtr;
            for (const auto& item : comboItems) {
                itemsPtr.push_back(item.c_str());
            }

            if (ImGui::Combo("Inside Block", &currentItem, itemsPtr.data(), static_cast<int>(itemsPtr.size()))) {
                if (currentItem == 0) {
                    bubbleInsideBlockType_ = BlockType::Wall;
                    bubbleInsideCustomSlot_ = 0;
                } else if (currentItem == 1) {
                    bubbleInsideBlockType_ = BlockType::Ladder;
                    bubbleInsideCustomSlot_ = 0;
                } else {
                    bubbleInsideCustomSlot_ = currentItem - 1; // 1〜5
                    const auto* part = stageMap.GetCustomPart(bubbleInsideCustomSlot_);
                    if (part) {
                        bubbleInsideBlockType_ = part->baseType;
                    }
                }
            }
        }
    }
    
    if (selectedBlockType_ == BlockType::Door) 
    {
        if (ImGui::CollapsingHeader("Door Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            // ドアの番号を 1 〜 9 の間で選べるスライダー（または InputInt）
            ImGui::SliderInt("Door ID Number", &selectedDoorId_, 1, 9, "ID: %d");
            ImGui::Text("Doors with the same ID will connect to each other.");
        }
    }

    // ▼ ここから追加
    if (selectedBlockType_ == BlockType::PSwitch ||
        selectedBlockType_ == BlockType::PBlock ||
        selectedBlockType_ == BlockType::PBlockAppears)
    {
        if (ImGui::CollapsingHeader("P Switch Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt("P Switch ID Number", &selectedPSwitchId_, 1, 9, "ID: %d");
            ImGui::Text("PSwitch and PBlock with the same ID will connect.");
        }
    }

    if (selectedBlockType_ == BlockType::TimedBlock)
    {
        if (ImGui::CollapsingHeader("Timed Block Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::SliderInt("Group ID", &selectedTimedGroupId_, 1, 9, "Group: %d");
            ImGui::SliderInt("Order ID", &selectedTimedOrderId_, 0, 9, "Order: %d");
            ImGui::Text("Blocks in the same group appear sequentially.");
        }
    }

    // ツールバー（配置ブロックやアクション）の描画
    DrawEditorToolbar(stageMap, stageRenderer, mapCursor, player);
    ImGui::End();
#endif
}

// ImGui 内にブロック一覧ボタンや回転・配置・削除ボタンなどのツールバーを描画します。
void StageEditorController::DrawEditorToolbar(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player) {
#ifdef USE_IMGUI
    if (!mapCursor) return;

    ImGui::Text("1. Select Type");
    ImGui::Separator();

    // カテゴリー定義
    struct Category 
    {
        const char* name;
        std::vector<BlockType> types;
    };

    std::vector<Category> categories =
    {
        {
            "Basic Blocks", // ブロック類
            {
                BlockType::Ground,
                BlockType::Wall,
                BlockType::PBlock,
                BlockType::PBlockAppears, // 🌟 追加
                BlockType::CrumblingFloor,
                BlockType::IceBlock,
                BlockType::MovingFloor,
                BlockType::KeyBlock,
                BlockType::TimedBlock      // 🌟 追加
            }
        },
        {
            "Gimmicks & Interactables", // ギミック類
            {
                BlockType::Ladder,
                BlockType::Star,
                BlockType::BubblePickup,
                BlockType::Goal,
                BlockType::Door,
                BlockType::PSwitch,
                BlockType::Key
            }
        },
        {
            "Enemies", // 敵キャラクター
            {
                BlockType::EnemyWalker,
                BlockType::EnemyFlyer,
                BlockType::EnemyChaser
            }
        },
        {
            "System", //  その他
            {
                BlockType::PlayerStart,
                BlockType::Checkpoint
            }
        }
    };

    // タブバーを使ってカテゴリを分ける
    if (ImGui::BeginTabBar("BlockCategoryTabs")) {
        for (const auto& cat : categories) {
            if (ImGui::BeginTabItem(cat.name)) {

                float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
                for (int n = 0; n < cat.types.size(); n++) {
                    BlockType type = cat.types[n];
                    // 通常ブロックかつ bubbleInsideCustomSlot_ が 0 の場合のみ選択中とみなす
                    bool isSelected = (selectedBlockType_ == type && bubbleInsideCustomSlot_ == 0);

                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                    }

                    if (ImGui::Button(BlockTypeToString(type), ImVec2(140, 30))) {
                        selectedBlockType_ = type;
                        bubbleInsideCustomSlot_ = 0; // 通常選択時はカスタムIDを解除
                    }

                    if (isSelected) {
                        ImGui::PopStyleColor();
                    }

                    float last_button_x2 = ImGui::GetItemRectMax().x;
                    float next_button_x2 = last_button_x2 + ImGui::GetStyle().ItemSpacing.x + 140;
                    if (n + 1 < cat.types.size() && next_button_x2 < window_visible_x2)
                        ImGui::SameLine();
                }
                ImGui::EndTabItem();
            }
        }

        // --- 新しいカテゴリタブ「Custom Blocks」を追加 ---
        if (ImGui::BeginTabItem("Custom Blocks")) {
            float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
            const auto& parts = stageMap.GetCustomParts();
            for (int i = 1; i <= 5; ++i) {
                const auto& part = parts[i - 1];
                
                // ボタンの選択状態：選択中ブロックタイプが part.baseType 且つ bubbleInsideCustomSlot_ == i
                bool isSelected = (selectedBlockType_ == part.baseType && bubbleInsideCustomSlot_ == i);
                
                if (isSelected) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.8f, 1.0f)); // カスタムブロック選択は青色
                }

                std::string btnLabel = part.name;
                if (btnLabel.empty()) {
                    btnLabel = "Part " + std::to_string(i);
                }

                if (ImGui::Button(btnLabel.c_str(), ImVec2(140, 30))) {
                    selectedBlockType_ = part.baseType;
                    bubbleInsideCustomSlot_ = i; // カスタムIDを適用
                }

                if (isSelected) {
                    ImGui::PopStyleColor();
                }

                float last_button_x2 = ImGui::GetItemRectMax().x;
                float next_button_x2 = last_button_x2 + ImGui::GetStyle().ItemSpacing.x + 140;
                if (i < 5 && next_button_x2 < window_visible_x2)
                    ImGui::SameLine();
            }
            ImGui::EndTabItem();
        }

        // ブロック選択UIの近くに追加
        if (selectedBlockType_ == BlockType::MovingFloor) {
            ImGui::Separator();
            ImGui::Text("Moving Floor Settings");
            // X, Y, Z の移動量を設定するスライダー（-10マス 〜 10マス の範囲など）
            ImGui::SliderInt("Move X", &currentMoveOffset_.x, -10, 10);
            ImGui::SliderInt("Move Y", &currentMoveOffset_.y, -10, 10);
            ImGui::SliderInt("Move Z", &currentMoveOffset_.z, -10, 10);
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("2. Action");
    // 回転ボタン（Rキーと同等）
    if (ImGui::Button("Rotate (R)", ImVec2(-FLT_MIN, 30))) {
        const Int3& cursor = mapCursor->GetIndex();
        MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
        if (cell && cell->type != BlockType::None) {
            cell->rotationY += 1.5708f;
            if (stageRenderer) {
                stageRenderer->BuildFromStageMap(stageMap);
            }
        }
    }

    ImGui::Spacing();

    // 配置実行ボタン（Enterキーと同等）
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button("PLACE (Enter)", ImVec2(-FLT_MIN, 40))) {
        ApplyPlacement(stageMap, stageRenderer, mapCursor, player);
    }
    ImGui::PopStyleColor();

    // 削除ボタン（Spaceキーと同等）
    if (ImGui::Button("REMOVE (Space)", ImVec2(-FLT_MIN, 40))) {
        stageMap.RemoveBlock(mapCursor->GetIndex());
        if (stageRenderer) {
            stageRenderer->BuildFromStageMap(stageMap);
        }
    }
#endif
}

// 現在のカーソル位置に対して、選択中のブロック（またはドアのペアリング）を配置・適用します。
void StageEditorController::ApplyPlacement(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player) {
    if (!mapCursor) return;

    const Int3& cursor = mapCursor->GetIndex();
    MapCell* oldCell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);

    // ==========================================================
    // ドア配置時の特殊処理（ペアリング管理）
    // ==========================================================
    if (selectedBlockType_ == BlockType::Door)
    {
        // 第3引数の variant に選択中のドア番号 (selectedDoorId_) を渡して配置
        stageMap.SetBlock(cursor, BlockType::Door, selectedDoorId_);

        // 既存のファイル保存フォーマット（直後に座標を3つ要求する仕様）との互換性を保つため、
        // doorTargetIndex には一旦自身の座標かダミー値を書き込んでおきます
        MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
        if (cell) {
            cell->doorTargetIndex = cursor;
        }
    } // ==========================================================
    // ▼ 追加：Pスイッチ / Pブロック配置時の特殊処理
    // ==========================================================
    else if (selectedBlockType_ == BlockType::PSwitch ||
        selectedBlockType_ == BlockType::PBlock ||
        selectedBlockType_ == BlockType::PBlockAppears)
    {
        stageMap.SetBlock(cursor, selectedBlockType_, selectedPSwitchId_);

        MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
        if (cell && selectedBlockType_ == BlockType::PBlock) {
            cell->isSolid = true;
            cell->isHidden = false;
        }
        else if (cell && selectedBlockType_ == BlockType::PBlockAppears) {
            cell->isSolid = false;  // 最初はすり抜ける状態
            cell->isHidden = false; // エディタで見えるようにする
        }
    }
    else if (selectedBlockType_ == BlockType::TimedBlock)
    {
        int variant = selectedTimedGroupId_ * 10 + selectedTimedOrderId_;
        stageMap.SetBlock(cursor, selectedBlockType_, variant);
    }
    else 
    {
        // 通常のブロック配置
        int variant = 0;
        if (selectedBlockType_ == BlockType::BubblePickup) {
            // シャボン玉の場合：中身のベースタイプとカスタムIDをパックして variant に仕込む
            variant = PackBubbleContents(bubbleInsideBlockType_, bubbleInsideCustomSlot_);
            stageMap.SetBlock(cursor, selectedBlockType_, variant);
        } else if (selectedBlockType_ == BlockType::Wall || selectedBlockType_ == BlockType::Ladder) {
            // カスタムブロックを直接配置する場合：variant にカスタムIDをそのまま仕込む
            if (bubbleInsideCustomSlot_ >= 1 && bubbleInsideCustomSlot_ <= 5) {
                variant = bubbleInsideCustomSlot_;

                // 🌟 複合カスタムアセンブリパーツを一括配置！！！
                const auto* part = stageMap.GetCustomPart(bubbleInsideCustomSlot_);
                if (part && !part->IsEmpty()) {
                    // アセンブリの各セルを一括配置
                    for (int ly = 0; ly < 3; ++ly) {
                        for (int lz = 0; lz < 3; ++lz) {
                            for (int lx = 0; lx < 3; ++lx) {
                                const auto& cell = part->cells[ly][lz][lx];
                                if (cell.type == BlockType::None) continue; // 空セルは無視

                                Int3 targetPos = { cursor.x + lx, cursor.y + ly, cursor.z + lz };
                                if (stageMap.IsInside(targetPos)) {
                                    stageMap.SetBlock(targetPos, cell.type, bubbleInsideCustomSlot_);
                                }
                            }
                        }
                    }
                } else {
                    // 空なら1マスだけフォールバック配置
                    stageMap.SetBlock(cursor, selectedBlockType_, variant);
                }
            } else {
                // 通常の1マス配置
                stageMap.SetBlock(cursor, selectedBlockType_, variant);
            }
        } else {
            // その他の通常ブロック配置
            stageMap.SetBlock(cursor, selectedBlockType_, variant);
        }

        // プレイヤースタート地点の場合は即座にプレイヤー座標も更新する
        MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
        if (cell && selectedBlockType_ == BlockType::MovingFloor) {
            cell->moveOffset = currentMoveOffset_;
        }
        if (selectedBlockType_ == BlockType::PlayerStart && player) {
            Vector3 startPos = {
                static_cast<float>(cursor.x),
                static_cast<float>(cursor.y) + 1.1f,
                static_cast<float>(cursor.z)
            };

            player->SetPosition(startPos);
            player->SetRespawnPosition(startPos);
        }
    }

    // 見た目の再構築
    if (stageRenderer) {
        stageRenderer->BuildFromStageMap(stageMap);
    }
}


bool StageEditorController::RepeatKey(Input* input, BYTE key, int firstDelay, int interval) {
    if (input->TriggerKey(key)) {
        return true; // 押した瞬間
    }

    if (!input->PushKey(key)) {
        return false;
    }

    // 押しっぱなし中の連続入力
    if (holdFrame_ >= firstDelay && ((holdFrame_ - firstDelay) % interval == 0)) {
        return true;
    }

    return false;
}
