#include "EditorScene.h"
#include "externals/imgui/imgui.h"
#include "MyMath.h" // Math::用
#include <filesystem>

void EditorScene::SetEnginePointers(Object3dCommon* obj, Input* in, TextureManager* tex) {
    objCommon_ = obj;
    input_ = in;
    texManager_ = tex;
}

void EditorScene::Initialize() {
    if (objCommon_) {
        stageRenderer_.Initialize(objCommon_);
    }

    // マップカーソルの生成
    mapCursor_ = std::make_unique<MapCursor>();
    if (mapCursor_) {
        mapCursor_->Initialize(objCommon_);
    }

    // カメラ初期位置
    editorCamera_.SetPosition({ 0.0f, 15.0f, -30.0f });

    RefreshStageList();
}

void EditorScene::Update() {
    // 1. カメラ更新（Blender風操作）
    bool isGuiCaptured = ImGui::GetIO().WantCaptureMouse || ImGui::GetIO().WantCaptureKeyboard;
    
    // Blender風操作を呼び出し（マウス中ボタン:回転、Ctrl+中:ズームなど）
    editorCamera_.UpdateBlenderStyle(input_, isGuiCaptured, nullptr);
    editorCamera_.Update();

    if (isGuiCaptured) {
        return;
    }

    // 2. カーソル移動（WASD/QE）
    if (input_->TriggerKey(DIK_A)) { mapCursor_->Move(-1, 0, 0, stageMap_); }
    if (input_->TriggerKey(DIK_D)) { mapCursor_->Move(1, 0, 0, stageMap_); }
    if (input_->TriggerKey(DIK_W)) { mapCursor_->Move(0, 0, 1, stageMap_); }
    if (input_->TriggerKey(DIK_S)) { mapCursor_->Move(0, 0, -1, stageMap_); }
    if (input_->TriggerKey(DIK_Q)) { mapCursor_->Move(0, 1, 0, stageMap_); }
    if (input_->TriggerKey(DIK_E)) { mapCursor_->Move(0, -1, 0, stageMap_); }

    // 3. 【移植】ブロックの配置 (Enter)
    if (input_->TriggerKey(DIK_RETURN)) {
        ApplyPlacement(); // 旧MyGameのロジックを呼ぶ
    }

    // 4. 【移植】ブロックの削除 (Space)
    if (input_->TriggerKey(DIK_SPACE)) {
        stageMap_.RemoveBlock(mapCursor_->GetIndex());
        stageRenderer_.BuildFromStageMap(stageMap_);
    }

    // 5. 【移植】ブロックの回転 (R)
    if (input_->TriggerKey(DIK_R)) {
        const Int3& idx = mapCursor_->GetIndex();
        MapCell* cell = stageMap_.GetCell(idx.x, idx.y, idx.z);
        if (cell && cell->type != BlockType::None) {
            cell->rotationY += 1.5708f; // 90度回転
            stageRenderer_.BuildFromStageMap(stageMap_);
        }
    }

    mapCursor_->Update(Math::MakeIdentity4x4());
}

void EditorScene::ApplyPlacement() {
    const Int3& cursor = mapCursor_->GetIndex();
    MapCell* oldCell = stageMap_.GetCell(cursor);

    if (selectedBlockType_ == BlockType::Door) {
        // --- ドアのペアリングロジック (masterから完コピ) ---
        if (oldCell && oldCell->type == BlockType::Door) {
            Int3 target = oldCell->doorTargetIndex;
            if (target.x != cursor.x || target.y != cursor.y || target.z != cursor.z) {
                MapCell* paired = stageMap_.GetCell(target);
                if (paired && paired->type == BlockType::Door) paired->doorTargetIndex = target;
            }
            if (isWaitingForSecondDoor_ && firstDoorIndex_.x == cursor.x) isWaitingForSecondDoor_ = false;
        }

        stageMap_.SetBlock(cursor, BlockType::Door);
        if (!isWaitingForSecondDoor_) {
            firstDoorIndex_ = cursor;
            isWaitingForSecondDoor_ = true;
            stageMap_.GetCell(cursor)->doorTargetIndex = cursor;
        } else {
            stageMap_.GetCell(cursor)->doorTargetIndex = firstDoorIndex_;
            stageMap_.GetCell(firstDoorIndex_)->doorTargetIndex = cursor;
            isWaitingForSecondDoor_ = false;
        }
    } else {
        stageMap_.SetBlock(cursor, selectedBlockType_);
    }
    stageRenderer_.BuildFromStageMap(stageMap_);
}
void EditorScene::Draw() {

    objCommon_->PreDraw();

    const Matrix4x4& view = editorCamera_.GetViewMatrix();
    const Matrix4x4& proj = editorCamera_.GetProjectionMatrix();

    stageRenderer_.SetCamera(view, proj);
    stageRenderer_.Draw();

    if (mapCursor_) {
        mapCursor_->SetCamera(view, proj);
        mapCursor_->Draw();
    }
}

void EditorScene::DrawUI() {
#ifndef NDEBUG // 【重要】リリース時には一切映さないようにガード

    // 画面右側にツールバーを表示
    ImGui::SetNextWindowPos(ImVec2(1280 - 260, 20), ImGuiCond_FirstUseEver);
    ImGui::Begin("Editor Toolbar", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    // --- 1. Select Gimmick (削らずに全種類網羅すべき) ---
    ImGui::Text("1. Select Gimmick");

    // 全種類ボタン化。選択中のものは色を変えると使いやすいです
    if (ImGui::Button("Ground", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::Ground; }
    if (ImGui::Button("Wall", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::Wall; }
    if (ImGui::Button("Ladder", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::Ladder; }
    if (ImGui::Button("Star", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::Star; }
    if (ImGui::Button("BubblePickup", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::BubblePickup; }
    if (ImGui::Button("Goal", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::Goal; }
    if (ImGui::Button("PlayerStart", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::PlayerStart; }
    if (ImGui::Button("Door", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::Door; }
    if (ImGui::Button("PSwitch", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::PSwitch; }
    if (ImGui::Button("PBlock", ImVec2(-1, 0))) { selectedBlockType_ = BlockType::PBlock; }

    ImGui::Separator();

    // --- 2. Action (マウス操作がメインでも、ボタンがある方が便利) ---
    ImGui::Text("2. Action");

    if (ImGui::Button("Rotate (R)", ImVec2(-1, 0))) {
        // 回転処理（Rキーと同じロジック）
    }

    // 【重要】PLACEボタンは赤くして目立たせる（スクショ再現）
    ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(0.0f, 0.6f, 0.6f));
    if (ImGui::Button("PLACE (Enter)", ImVec2(-1, 40))) {
        ApplyPlacement();
    }
    ImGui::PopStyleColor();

    if (ImGui::Button("REMOVE (Space)", ImVec2(-1, 0))) {
        stageMap_.RemoveBlock(mapCursor_->GetIndex());
        stageRenderer_.BuildFromStageMap(stageMap_);
    }

    ImGui::End();

#endif
}

void EditorScene::RefreshStageList() {
    stageFiles_.clear();
    std::string path = "Resources/Stages/";
    if (!std::filesystem::exists(path)) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(path)) {
        if (entry.path().extension() == ".txt") {
            stageFiles_.push_back(entry.path().stem().string());
        }
    }
}