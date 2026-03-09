#include "MyGame.h"
#include "externals/imgui/imgui.h"


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
    Model* modelPlane = Model::CreateFromOBJ(dxCommon, "Resources", "plane.obj", textureManager);
    Model* modelAxis = Model::CreateFromOBJ(dxCommon, "Resources", "axis.obj", textureManager);
    models.push_back(modelPlane);
    models.push_back(modelAxis);

    // --- オブジェクト生成 ---
    // 1つ目: 床
    Object3d* floor = CreateObject(modelPlane, { 0.0f, 0.0f, 0.0f });
    floor->SetScale({ 10.0f, 1.0f, 10.0f });

    // 2つ目: 右側の軸
    CreateObject(modelAxis, { 2.0f, 0.0f, 0.0f });

    // 3つ目: 左側の軸
    CreateObject(modelAxis, { -2.0f, 0.0f, 0.0f });

    // スプライト
    uint32_t texHandle = textureManager->LoadTexture("Resources/uvChecker.png");
    sprite = new Sprite();
    sprite->Initialize(spriteCommon, texHandle);

    camera = std::make_unique<Camera>();
    
}

Object3d* MyGame::CreateObject(Model* model, Vector3 pos) {
    Object3d* obj = new Object3d();
    obj->Initialize(object3dCommon);
    obj->SetModel(model);
    obj->SetPosition(pos);
    obj->SetRotation({ 1.57f, 0.0f, 0.0f }); // デフォルトで寝かせる
    objectList.push_back(obj); // ここでリストに追加されるので、Draw()で自動描画される
    return obj;
}

void MyGame::Update() {
    dxCommon->BeginImGui();

    input->Update();
    UpdateImGui();

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

    if (debugFlags_.show3DObjects) {
        for (Object3d* obj : objectList) {
            obj->SetCamera(view, proj);
            obj->Update();
        }
    }

    if (debugFlags_.showSprite) {
        sprite->Update();
    }

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
    // まだ空でOK
    // 次にカーソル移動やブロック配置を入れる
}

void MyGame::UpdateGamePlay() {
    // まだ空でOK
    // 後でプレイヤー処理を入れる
}

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

    ImGui::Separator();
    if (ImGui::TreeNode("Camera")) {
        Transform& camTf = camera->GetTransform();

        ImGui::DragFloat3("Position", &camTf.translate.x, 0.1f);
        ImGui::DragFloat3("Rotation", &camTf.rotate.x, 0.01f);
        ImGui::SliderFloat("FOV", camera->GetFovPtr(), 0.01f, 3.14f);

        if (ImGui::Button("Reset Camera")) {
            camera->SetPosition({ 0.0f, 5.0f, -10.0f });
            camera->SetRotation({ 0.3f, 0.0f, 0.0f });
            camera->SetFov(0.45f);
        }

        ImGui::TreePop();
    }

    ImGui::End();
}

void MyGame::Draw() {
    dxCommon->PreDraw();

    ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
    dxCommon->GetCommandList()->SetDescriptorHeaps(1, heaps);

    if (debugFlags_.show3DObjects) {
        object3dCommon->PreDraw();
        for (Object3d* obj : objectList) {
            obj->Draw();
        }
    }

    if (debugFlags_.showParticles) {
        particleManager->Draw();
    }

    if (debugFlags_.showSprite) {
        spriteCommon->PreDraw();
        sprite->Draw();
    }

    dxCommon->EndImGui();
    dxCommon->PostDraw();
}

void MyGame::Finalize() {
    for (Object3d* obj : objectList) delete obj;
    for (Model* m : models) delete m;
    delete sprite; delete particleManager; delete object3dCommon;
    delete spriteCommon; delete textureManager; delete input;
    delete dxCommon; delete winApp;
}