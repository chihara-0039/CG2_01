#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <vector>

#include "WinApp.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SpriteCommon.h"
#include "Input.h"
#include "Logger.h"
#include "Sprite.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "Model.h"

// ★追加：パーティクルマネージャー
#include "ParticleManager.h"

#pragma comment(lib, "dxguid.lib")

// オブジェクト生成ヘルパー関数 (変更なし)
Object3d* CreateObject(Object3dCommon* common, Model* model, Vector3 pos, std::vector<Object3d*>& list) {
    Object3d* obj = new Object3d();
    obj->Initialize(common);
    obj->SetModel(model);
    obj->SetPosition(pos);
    obj->SetScale({ 1.0f, 1.0f, 1.0f });
    obj->SetRotation({ 0.0f, 3.14f, 0.0f });
    list.push_back(obj);
    return obj;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    WinApp* winApp = new WinApp();
    winApp->Initialize();

    DirectXCommon* dxCommon = new DirectXCommon();
    dxCommon->Initialize(winApp);

    Input* input = new Input();
    input->Initialize(winApp);

    TextureManager* textureManager = new TextureManager();
    textureManager->Initialize(dxCommon);

    SpriteCommon* spriteCommon = new SpriteCommon();
    spriteCommon->SetTextureManager(textureManager);
    spriteCommon->Initialize(dxCommon);

    Object3dCommon* object3dCommon = new Object3dCommon();
    object3dCommon->SetTextureManager(textureManager);
    object3dCommon->Initialize(dxCommon);

    // ★追加：パーティクルマネージャーの初期化
    ParticleManager* particleManager = new ParticleManager();
    particleManager->Initialize(dxCommon, textureManager);

    // --- モデル・オブジェクトの準備 ---
    Model* modelPlane = Model::CreateFromOBJ(dxCommon, "Resources", "plane.obj", textureManager);
    std::vector<Object3d*> objectList;
    CreateObject(object3dCommon, modelPlane, { 0, 0, 0 }, objectList);

    // スプライト
    uint32_t texHandle = textureManager->LoadTexture("project/Resources/uvChecker.png");
    Sprite* sprite = new Sprite();
    sprite->Initialize(spriteCommon, texHandle);
    sprite->SetPosition({ 10, 10 });

    // カメラ
    Transform cameraTransform = { {1,1,1}, {0.3f, 0, 0}, {0, 5, -10} };

    while (true) {
        if (winApp->ProcessMessage()) break;
        input->Update();

        // --- 更新処理 ---

        // ★追加：スペースキーを押したらパーティクル発射！
        if (input->TriggerKey(DIK_SPACE)) {
            // 原点から10個発射
            particleManager->Emit({ 0.0f, 0.0f, 0.0f }, 10);
        }

        // カメラ行列
        Matrix4x4 cameraWorld = Math::MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
        Matrix4x4 viewMatrix = Math::Inverse(cameraWorld);
        Matrix4x4 projectionMatrix = Math::MakePerspectiveFovMatrix(0.45f, (float)WinApp::kClientWidth / (float)WinApp::kClientHeight, 0.1f, 100.0f);

        for (Object3d* obj : objectList) {
            obj->SetCamera(viewMatrix, projectionMatrix);
            obj->Update();
        }

        sprite->Update();

        // ★追加：パーティクルの更新
        particleManager->Update(viewMatrix, projectionMatrix);

        // --- 描画処理 ---
        dxCommon->PreDraw();

        // ヒープセット
        ID3D12DescriptorHeap* descriptorHeaps[] = { textureManager->GetSrvHeap() };
        dxCommon->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);

        // 3Dオブジェクト描画
        object3dCommon->PreDraw();
        for (Object3d* obj : objectList) {
            obj->Draw();
        }

        // ★追加：パーティクル描画
        // (Object3dのPreDrawで共通設定がされているので、そのまま描画可能だが、
        //  ParticleManager::Draw内でパイプラインを再設定しているので問題ない)
        particleManager->Draw();

        // スプライト描画
        spriteCommon->PreDraw();
        sprite->Draw();

        dxCommon->PostDraw();
    }

    // --- 解放 ---
    for (Object3d* obj : objectList) delete obj;
    delete modelPlane;
    delete sprite;
    delete particleManager; // ★忘れずに削除
    delete object3dCommon;
    delete spriteCommon;
    delete textureManager;
    delete input;
    delete dxCommon;
    delete winApp;

    return 0;
}