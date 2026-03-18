#include "MyGame.h"
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
    case BlockType::Stair:        return "Stair";
    case BlockType::BubblePickup: return "BubblePickup";
    case BlockType::Goal:         return "Goal";
    case BlockType::PlayerStart:  return "PlayerStart";
    default:                      return "Unknown";
    }
}

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
}

Object3d* MyGame::CreateObject(Model* model, Vector3 pos) {
    Object3d* obj = new Object3d();
    obj->Initialize(object3dCommon);
    obj->SetModel(model);
    obj->SetPosition(pos);
    obj->SetRotation({ 1.57f, 0.0f, 0.0f });

    objectList.push_back(obj);
    return obj;
}
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
   
    switch (currentMode_) {
    case AppMode::DebugView:
    UpdateDebugView();
    break;

    case AppMode::StageEditor:
    UpdateStageEditor();
    break;

    case AppMode::GamePlay:
    UpdateGamePlay();
    break;
    }

    camera->Update();

    const Matrix4x4& view = camera->GetViewMatrix();
    const Matrix4x4& proj = camera->GetProjectionMatrix();

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


void MyGame::UpdateStageEditor() {

    // カーソル移動
    if (input->TriggerKey(DIK_A)) {
        mapCursor_->Move(-1, 0, 0, stageMap_);
    }
    if (input->TriggerKey(DIK_D)) {
        mapCursor_->Move(1, 0, 0, stageMap_);
    }
    if (input->TriggerKey(DIK_W)) {
        mapCursor_->Move(0, 0, -1, stageMap_);
    }
    if (input->TriggerKey(DIK_S)) {
        mapCursor_->Move(0, 0, 1, stageMap_);
    }
    if (input->TriggerKey(DIK_Q)) {
        mapCursor_->Move(0, 1, 0, stageMap_);
    }
    if (input->TriggerKey(DIK_E)) {
        mapCursor_->Move(0, -1, 0, stageMap_);
    }

    // 現在カーソル位置
    const Int3& cursor = mapCursor_->GetIndex();

    bool needRebuild = false;

    // ブロック配置
    if (input->TriggerKey(DIK_1)) {
        selectedBlockType_ = BlockType::Ground;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        needRebuild = true;
    }
    if (input->TriggerKey(DIK_2)) {
        selectedBlockType_ = BlockType::Wall;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        needRebuild = true;
    }
    if (input->TriggerKey(DIK_3)) {
        selectedBlockType_ = BlockType::BubblePickup;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        needRebuild = true;
    }
    if (input->TriggerKey(DIK_4)) {
        selectedBlockType_ = BlockType::Goal;
        stageMap_.SetBlock(cursor, selectedBlockType_);
        needRebuild = true;
    }

    // 削除
    if (input->TriggerKey(DIK_BACKSPACE)) {
        stageMap_.RemoveBlock(cursor);
        needRebuild = true;
    }

    // 再構築
    if (needRebuild && stageRenderer_) {
        stageRenderer_->BuildFromStageMap(stageMap_);
    }

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
    // まだ空でOK
    // 後でプレイヤー処理を入れる
}

#ifdef USE_IMGUI
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

    const char* modeNames[] = { "DebugView", "StageEditor", "GamePlay" };
    if (ImGui::Combo("App Mode", &modeIndex, modeNames, IM_ARRAYSIZE(modeNames))) {
        switch (modeIndex) {
        case 0: currentMode_ = AppMode::DebugView; break;
        case 1: currentMode_ = AppMode::StageEditor; break;
        case 2: currentMode_ = AppMode::GamePlay; break;
        }
    }

    ImGui::Separator();
    ImGui::Text("Draw Flags");
    ImGui::Checkbox("Show 3D Objects", &debugFlags_.show3DObjects);
    ImGui::Checkbox("Show Sprite", &debugFlags_.showSprite);
    ImGui::Checkbox("Show Particles", &debugFlags_.showParticles);

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

    
    ImGui::Separator();
    if (ImGui::TreeNode("Cursor Info")) {
        const Int3& cursor = mapCursor_->GetIndex();
        ImGui::Text("Cursor Index: (%d, %d, %d)", cursor.x, cursor.y, cursor.z);
        ImGui::TreePop();
    }

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

		// 3Dオブジェクトの描画
        if (currentMode_ == AppMode::DebugView) {
            for (Object3d* obj : objectList) {
                obj->Draw();
            }
        }


		// ステージ描画オブジェクトの描画
        if (currentMode_ == AppMode::StageEditor) {
            if (stageRenderer_) {
                stageRenderer_->Draw();
            }
            if (mapCursor_) {
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