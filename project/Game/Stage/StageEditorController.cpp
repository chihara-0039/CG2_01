#include "StageEditorController.h"
#include <cmath>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void StageEditorController::Initialize() {
    // ブロック表示スケールの初期化
    editorBlockScale_ = { 1.0f, 1.0f, 1.0f };
    editorUniformBlockScale_ = 1.0f;
    
    selectedStageIndex_ = -1;
    selectedBlockType_ = BlockType::Ground;
    
    // ドアのペアリング状態の初期化
    isWaitingForSecondDoor_ = false;
    firstDoorIndex_ = { -1, -1, -1 };
    
    // 保存済みのステージ一覧をスキャン・更新
    RefreshStageList();
}

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

//5/18佐倉変更
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

void StageEditorController::HandleCursorInput(Input* input, StageMap& stageMap, MapCursor* mapCursor, LightCamera* lightCamera) {
    if (!input || !mapCursor || !lightCamera) return;

    // WASD と QE キーによる三次元グリッド上のカーソル移動
    if (input->TriggerKey(DIK_A)) {
        mapCursor->Move(-1, 0, 0, stageMap);
    }
    if (input->TriggerKey(DIK_D)) {
        mapCursor->Move(1, 0, 0, stageMap);
    }
    if (input->TriggerKey(DIK_W)) {
        mapCursor->Move(0, 0, 1, stageMap);
    }
    if (input->TriggerKey(DIK_S)) {
        mapCursor->Move(0, 0, -1, stageMap);
    }
    if (input->TriggerKey(DIK_Q)) {
        mapCursor->Move(0, 1, 0, stageMap);
    }
    if (input->TriggerKey(DIK_E)) {
        mapCursor->Move(0, -1, 0, stageMap);
    }
    // 移動後のカーソル位置や描画用行列を更新
    mapCursor->Update(lightCamera->GetViewProjectionMatrix());
}

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

void StageEditorController::Update(Input* input, StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, LightCamera* lightCamera, Player* player, Camera* camera) {
    if (!input || !mapCursor) return;

    bool needRebuild = false;
    bool isGuiCaptured = false;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    // ImGui のウィンドウやボタンを操作中の場合はエディタのショートカットキーを無効化
    if (ImGui::GetCurrentContext()) {
        isGuiCaptured = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    }
#endif

    if (!isGuiCaptured) {
        // カーソルのキー操作を処理
        HandleCursorInput(input, stageMap, mapCursor, lightCamera);

        const Int3& cursor = mapCursor->GetIndex();

        // Rキー：ブロックのY軸回転（90度ずつ）
        if (input->TriggerKey(DIK_R)) {
            MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
            if (cell && cell->type != BlockType::None) {
                cell->rotationY += 1.5708f;
                needRebuild = true;
            }
        }

        // 数字キーによる配置ブロックのダイレクト切り替えと即時配置
        if (input->TriggerKey(DIK_1)) { selectedBlockType_ = BlockType::Ground; ApplyPlacement(stageMap, stageRenderer, mapCursor, player); }
        if (input->TriggerKey(DIK_2)) { selectedBlockType_ = BlockType::Wall; ApplyPlacement(stageMap, stageRenderer, mapCursor, player); }
        if (input->TriggerKey(DIK_3)) { selectedBlockType_ = BlockType::BubblePickup; ApplyPlacement(stageMap, stageRenderer, mapCursor, player); }
        if (input->TriggerKey(DIK_4)) { selectedBlockType_ = BlockType::Goal; ApplyPlacement(stageMap, stageRenderer, mapCursor, player); }
        if (input->TriggerKey(DIK_5)) { selectedBlockType_ = BlockType::Ladder; ApplyPlacement(stageMap, stageRenderer, mapCursor, player); }
        if (input->TriggerKey(DIK_6)) { selectedBlockType_ = BlockType::Door; ApplyPlacement(stageMap, stageRenderer, mapCursor, player); }
        if (input->TriggerKey(DIK_7)) { selectedBlockType_ = BlockType::PlayerStart; ApplyPlacement(stageMap, stageRenderer, mapCursor, player); }

        // Enterキー：選択中ブロックの配置
        if (input->TriggerKey(DIK_RETURN)) { ApplyPlacement(stageMap, stageRenderer, mapCursor, player); }

        // Space / Backspaceキー：ブロックの削除
        if (input->TriggerKey(DIK_SPACE) || input->TriggerKey(DIK_BACKSPACE)) {
            stageMap.RemoveBlock(cursor);
            needRebuild = true;
        }
    }

    // 変更があった場合のみステージの3Dモデル表示を再構築する
    if (needRebuild && stageRenderer) {
        stageRenderer->BuildFromStageMap(stageMap);
    }

    // カメラのキー操作を処理
    HandleCameraInput(input, camera);
}

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

    if (ImGui::CollapsingHeader("Stage Manager", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputText("Save Name", newStageName_, IM_ARRAYSIZE(newStageName_));
        if (ImGui::Button("Save As New")) {
            std::string path = "Resources/Stages/" + std::string(newStageName_) + ".txt";
            stageMap.SaveToFile(path);
            RefreshStageList();
        }

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
    
    // ツールバー（配置ブロックやアクション）の描画
    DrawEditorToolbar(stageMap, stageRenderer, mapCursor, player);
    ImGui::End();
#endif
}

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
                BlockType::CrumblingFloor
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
                BlockType::PSwitch
            }
        },
        {
            "System", //  その他
            {
                BlockType::PlayerStart
            }
        }
    };

    // タブバーを使ってカテゴリを分ける（省スペースで探しやすい）
    if (ImGui::BeginTabBar("BlockCategoryTabs")) {
        for (const auto& cat : categories) {
            if (ImGui::BeginTabItem(cat.name)) {

                // ボタンの配置（2列のグリッドにするとさらに見やすくなります）
                float window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
                for (int n = 0; n < cat.types.size(); n++) {
                    BlockType type = cat.types[n];
                    bool isSelected = (selectedBlockType_ == type);

                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                    }

                    // ボタン幅を調整して横に並べる例（100px幅）
                    if (ImGui::Button(BlockTypeToString(type), ImVec2(140, 30))) {
                        selectedBlockType_ = type;
                    }

                    if (isSelected) {
                        ImGui::PopStyleColor();
                    }

                    // 次のボタンがウィンドウ幅を超えるなら改行
                    float last_button_x2 = ImGui::GetItemRectMax().x;
                    float next_button_x2 = last_button_x2 + ImGui::GetStyle().ItemSpacing.x + 140;
                    if (n + 1 < cat.types.size() && next_button_x2 < window_visible_x2)
                        ImGui::SameLine();
                }
                ImGui::EndTabItem();
            }
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

    //5/18佐倉変更

void StageEditorController::ApplyPlacement(StageMap& stageMap, StageRenderer* stageRenderer, MapCursor* mapCursor, Player* player) {
    if (!mapCursor) return;

    const Int3& cursor = mapCursor->GetIndex();
    MapCell* oldCell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);

    // ==========================================================
    // ドア配置時の特殊処理（ペアリング管理）
    // ==========================================================
    if (selectedBlockType_ == BlockType::Door) {
        // すでにドアがある場所へ再配置・削除する場合のリンク解除処理
        if (oldCell && oldCell->type == BlockType::Door) {
            Int3 target = oldCell->doorTargetIndex;
            if (target.x != cursor.x || target.y != cursor.y || target.z != cursor.z) {
                MapCell* pairedCell = stageMap.GetCell(target.x, target.y, target.z);
                if (pairedCell && pairedCell->type == BlockType::Door) {
                    pairedCell->doorTargetIndex = target; // 相手のワープ先を自身へ戻す（解除）
                }
            }

            // ペアリング待機中の1つ目ドアを削除した場合のキャンセル処理
            if (isWaitingForSecondDoor_ &&
                firstDoorIndex_.x == cursor.x &&
                firstDoorIndex_.y == cursor.y &&
                firstDoorIndex_.z == cursor.z) {
                isWaitingForSecondDoor_ = false;
            }
        }

        stageMap.SetBlock(cursor, BlockType::Door);
        if (!isWaitingForSecondDoor_) {
            // ▼ 1つ目のドア配置時：2つ目の配置待機モードへ移行
            firstDoorIndex_ = cursor;
            isWaitingForSecondDoor_ = true;
            MapCell* cell = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
            if (cell) cell->doorTargetIndex = cursor;
        } else {
            // ▼ 2つ目のドア配置時：相互にリンク先座標を設定してペアリングを完了
            MapCell* cell2 = stageMap.GetCell(cursor.x, cursor.y, cursor.z);
            if (cell2) cell2->doorTargetIndex = firstDoorIndex_;

            MapCell* cell1 = stageMap.GetCell(firstDoorIndex_.x, firstDoorIndex_.y, firstDoorIndex_.z);
            if (cell1) cell1->doorTargetIndex = cursor;

            isWaitingForSecondDoor_ = false;
        }
    } else {
        // 通常のブロック配置
        stageMap.SetBlock(cursor, selectedBlockType_);
        // プレイヤースタート地点の場合は即座にプレイヤー座標も更新する
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
