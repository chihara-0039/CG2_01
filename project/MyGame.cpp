#include "MyGame.h"
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_win32.h"
#include "externals/imgui/imgui_impl_dx12.h"

void MyGame::Initialize() {
    // --- 基盤初期化 (std::make_unique を使用) ---
    winApp = std::make_unique<WinApp>();
    winApp->Initialize();

    dxCommon = std::make_unique<DirectXCommon>();
    dxCommon->Initialize(winApp.get()); // ポインタを渡す場合は .get() を使う

    textureManager = std::make_unique<TextureManager>();
    textureManager->Initialize(dxCommon.get());

    // 2. ここで ImGui の初期化を一本化する
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(winApp->GetHwnd());
    ImGui_ImplDX12_Init(
        dxCommon->GetDevice(),
        2, // フレームバッファ数
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        textureManager->GetSrvHeap(),
        textureManager->GetSrvHeap()->GetCPUDescriptorHandleForHeapStart(),
        textureManager->GetSrvHeap()->GetGPUDescriptorHandleForHeapStart()
    );

    input = std::make_unique<Input>();
    input->Initialize(winApp.get());

    
    spriteCommon = std::make_unique<SpriteCommon>();
    spriteCommon->SetTextureManager(textureManager.get());
    spriteCommon->Initialize(dxCommon.get());

    object3dCommon = std::make_unique<Object3dCommon>();
    object3dCommon->SetTextureManager(textureManager.get());
    object3dCommon->Initialize(dxCommon.get());

    particleManager = std::make_unique<ParticleManager>();
    particleManager->Initialize(dxCommon.get(), textureManager.get());

    // --- モデル読み込み ---
    // modelPlane 自体も unique_ptr としてリストに追加
    auto modelPlane = std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources", "plane.obj", textureManager.get()));
    auto modelAxis = std::unique_ptr<Model>(Model::CreateFromOBJ(dxCommon.get(), "Resources", "axis.obj", textureManager.get()));

    // get() で生ポインタを取得して CreateObject に渡す
    Model* pPlane = modelPlane.get();
    Model* pAxis = modelAxis.get();

    models.push_back(std::move(modelPlane)); // 所有権をリストへ移動
    models.push_back(std::move(modelAxis));

    // --- オブジェクト生成 ---
    CreateObject(pPlane, { 0.0f, 0.0f, 0.0f })->SetScale({ 10.0f, 1.0f, 10.0f });
    CreateObject(pAxis, { 2.0f, 0.0f, 0.0f });
    CreateObject(pAxis, { -2.0f, 0.0f, 0.0f });

    // スプライト
    uint32_t texHandle = textureManager->LoadTexture("Resources/uvChecker.png");
    sprite = std::make_unique<Sprite>();
    sprite->Initialize(spriteCommon.get(), texHandle);

    // カメラ
    camera = std::make_unique<Camera>();
}

Object3d* MyGame::CreateObject(Model* model, Vector3 pos) {
    auto obj = std::make_unique<Object3d>();
    obj->Initialize(object3dCommon.get());
    obj->SetModel(model);
    obj->SetPosition(pos);
    obj->SetRotation({ 1.57f, 0.0f, 0.0f });

    Object3d* pObj = obj.get();
    objectList.push_back(std::move(obj)); // リストに移動
    return pObj;
}

void MyGame::Update() {
    dxCommon->BeginImGui();
    ImGui::Begin("Camera Control");
    ImGui::DragFloat3("Position", &camera->transform.translate.x, 0.1f);
    ImGui::SliderFloat3("Rotation", &camera->transform.rotate.x, -3.14f, 3.14f);
    ImGui::SliderFloat("FOV", &camera->fovY, 0.01f, 1.5f);
    if (ImGui::Button("Reset")) {
        camera->transform.translate = { 0, 5, -10 };
        camera->transform.rotate = { 0.3f, 0, 0 };
    }
    ImGui::End();

    input->Update();
    if (input->TriggerKey(DIK_SPACE)) particleManager->Emit({ 0,0,0 }, 10);

    camera->Update();

    for (auto& obj : objectList) { // auto& で受ける
        obj->SetCamera(camera->GetViewMatrix(), camera->GetProjectionMatrix());
        obj->Update();
    }
    sprite->Update();
    particleManager->Update(camera->GetViewMatrix(), camera->GetProjectionMatrix());
}

void MyGame::Draw() {
    dxCommon->PreDraw();
    ID3D12DescriptorHeap* heaps[] = { textureManager->GetSrvHeap() };
    dxCommon->GetCommandList()->SetDescriptorHeaps(1, heaps);

    object3dCommon->PreDraw();
    for (auto& obj : objectList) obj->Draw();
    particleManager->Draw();

    spriteCommon->PreDraw();
    sprite->Draw();

    dxCommon->EndImGui();
    dxCommon->PostDraw();
}

void MyGame::Finalize() {
    // delete 命令は一切不要！
    // メンバ変数の unique_ptr たちが、MyGame が消えるときに自動で適切な順番で解放してくれます。
}