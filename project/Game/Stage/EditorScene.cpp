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

    if (selectedBlockType_ == BlockType::Door) {
        stageMap_.SetBlock(cursor, BlockType::Door);
        if (!isWaitingForSecondDoor_) {
            firstDoorIndex_ = cursor;
            isWaitingForSecondDoor_ = true;
        } else {
            MapCell* c1 = stageMap_.GetCell(firstDoorIndex_.x, firstDoorIndex_.y, firstDoorIndex_.z);
            MapCell* c2 = stageMap_.GetCell(cursor.x, cursor.y, cursor.z);
            if (c1) { c1->doorTargetIndex = cursor; }
            if (c2) { c2->doorTargetIndex = firstDoorIndex_; }
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
    // ツールバーの構築
    ImGui::Begin("Stage Editor Tools");

    // ブロック選択
    ImGui::Text("Block Selection");
    if (ImGui::Button("Ground")) { selectedBlockType_ = BlockType::Ground; }
    ImGui::SameLine();
    if (ImGui::Button("Wall")) { selectedBlockType_ = BlockType::Wall; }
    ImGui::SameLine();
    if (ImGui::Button("Ladder")) { selectedBlockType_ = BlockType::Ladder; }

    if (ImGui::Button("Door")) { selectedBlockType_ = BlockType::Door; }
    ImGui::SameLine();
    if (ImGui::Button("P-Switch")) { selectedBlockType_ = BlockType::PSwitch; }
    ImGui::SameLine();
    if (ImGui::Button("P-Block")) { selectedBlockType_ = BlockType::PBlock; }

    ImGui::Separator();

    // ステージ管理
    ImGui::Text("Stage Management");
    if (ImGui::Button("SAVE STAGE")) {
        stageMap_.SaveToFile("Resources/Stages/prototype.txt");
    }

    if (ImGui::Button("RELOAD")) {
        stageMap_.LoadFromFile("Resources/Stages/prototype.txt");
        stageRenderer_.BuildFromStageMap(stageMap_);
    }

    ImGui::Separator();

    // ヒント表示
    ImGui::Text("Controls:");
    ImGui::BulletText("ENTER: Place Block");
    ImGui::BulletText("SPACE: Remove Block");
    ImGui::BulletText("R: Rotate Block");

    ImGui::End();
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