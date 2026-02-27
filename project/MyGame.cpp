#include "MyGame.h"

void MyGame::Initialize() {
    // --- 基盤初期化 ---
    winApp = new WinApp(); winApp->Initialize();
    dxCommon = new DirectXCommon(); dxCommon->Initialize(winApp);
    input = new Input(); input->Initialize(winApp);
    textureManager = new TextureManager(); textureManager->Initialize(dxCommon);
    spriteCommon = new SpriteCommon(); spriteCommon->SetTextureManager(textureManager);
    spriteCommon->Initialize(dxCommon);
    object3dCommon = new Object3dCommon(); object3dCommon->SetTextureManager(textureManager);
    object3dCommon->Initialize(dxCommon);
    particleManager = new ParticleManager(); particleManager->Initialize(dxCommon, textureManager);

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

    cameraTransform = { {1,1,1}, {0.3f, 0, 0}, {0, 5, -10} };
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
    input->Update();
    if (input->TriggerKey(DIK_SPACE)) particleManager->Emit({ 0,0,0 }, 10);

    Matrix4x4 cameraWorld = Math::MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
    Matrix4x4 view = Math::Inverse(cameraWorld);
    Matrix4x4 projection = Math::MakePerspectiveFovMatrix(0.45f, (float)WinApp::kClientWidth / (float)WinApp::kClientHeight, 0.1f, 100.0f);

    for (Object3d* obj : objectList) {
        obj->SetCamera(view, projection);
        obj->Update();
    }
    sprite->Update();
    particleManager->Update(view, projection);
}

void MyGame::Draw() {
    dxCommon->PreDraw();
    ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
    dxCommon->GetCommandList()->SetDescriptorHeaps(1, heaps);

    // 3D描画 (リスト内の全オブジェクトをループで描画)
    object3dCommon->PreDraw();
    for (Object3d* obj : objectList) {
        obj->Draw();
    }
    particleManager->Draw();

    // 2D描画
    spriteCommon->PreDraw();
    sprite->Draw();

    dxCommon->PostDraw();
}

void MyGame::Finalize() {
    for (Object3d* obj : objectList) delete obj;
    for (Model* m : models) delete m;
    delete sprite; delete particleManager; delete object3dCommon;
    delete spriteCommon; delete textureManager; delete input;
    delete dxCommon; delete winApp;
}