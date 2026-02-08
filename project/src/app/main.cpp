#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dxgidebug.h>
#include <dxcapi.h>
#include <vector> // ★追加：リスト管理用

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

// ImGuiを使用する場合はコメントアウトを外してパスを通してください
// #include "imgui.h"

#pragma comment(lib, "dxguid.lib")

// ★便利関数：オブジェクトを生成してリストに追加する
// 引数：共通基盤、モデル、座標、(オプション)回転
Object3d* CreateObject(Object3dCommon* common, Model* model, Vector3 pos, std::vector<Object3d*>& list) {
    Object3d* obj = new Object3d();
    obj->Initialize(common);
    obj->SetModel(model);
    obj->SetPosition(pos);
    obj->SetScale({ 1.0f, 1.0f, 1.0f }); // デフォルトサイズ

    // リストに登録（これで勝手に更新・描画される）
    list.push_back(obj);

    return obj; // 後で個別に操作したいとき用にポインタを返す
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct D3DResourceLeakChecker {
        ~D3DResourceLeakChecker() {
            Microsoft::WRL::ComPtr<IDXGIDebug1> debug;
            if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug)))) {
                debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
                debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
            }
        }
    };
    D3DResourceLeakChecker leakChecker;

    // 1. 基盤システム初期化
    WinApp* winApp = new WinApp();
    winApp->Initialize();
    DirectXCommon* dxCommon = new DirectXCommon();
    dxCommon->Initialize(winApp);
    Input* input = new Input();
    input->Initialize(winApp);

    // 2. マネージャ・コモン初期化
    TextureManager* textureManager = new TextureManager();
    textureManager->Initialize(dxCommon);

    SpriteCommon* spriteCommon = new SpriteCommon();
    spriteCommon->Initialize(dxCommon);
    spriteCommon->SetTextureManager(textureManager);

    Object3dCommon* object3dCommon = new Object3dCommon();
    object3dCommon->Initialize(dxCommon);
    object3dCommon->SetTextureManager(textureManager);

    // 3. リソース生成

    // Sprite
    uint32_t textureHandle = textureManager->LoadTexture("Resources/uvChecker.png");
    Sprite* sprite = new Sprite();
    sprite->Initialize(spriteCommon, textureHandle);
    sprite->SetPosition({ 10.0f, 10.0f });

    // --- ★ここから変更：モデル読み込みとオブジェクト生成 ---

    // 管理用リスト
    std::vector<Object3d*> objectList;

    // モデルデータの読み込み（これが「一括管理」の実体）
    // 読み込みは1回だけ！
    Model* modelMulti = Model::CreateFromOBJ(dxCommon, "Resources", "multiMaterial.obj", textureManager);

    // もしcube.objがあれば使う（無ければ同じmodelMultiを使ってもOK）
    Model* modelCube = Model::CreateFromOBJ(dxCommon, "Resources", "plane.obj", textureManager);


    // ★関数を呼ぶだけでポンポン配置できます！

    // 左に配置 (multiMaterial)
    Object3d* obj1 = CreateObject(object3dCommon, modelMulti, { 0.0f, -2.0f, 0.0f }, objectList);

    // 右に配置 (multiMaterial) -> 同じモデルデータを使い回している！
    Object3d* obj2 = CreateObject(object3dCommon, modelMulti, { 0.0f, 0.0f, 0.0f }, objectList);
    obj2->SetRotation({ 0.0f, 3.14f, 0.0f }); // 個別に回転を設定

    // 上に配置 (cube) -> 違うモデルに切り替え！
    /*if (modelCube) {
        Object3d* obj3 = CreateObject(object3dCommon, modelCube, { 0.0f, 2.0f, 0.0f }, objectList);
    }*/

    // カメラと演出用変数
    Transform cameraTransform = { {1,1,1}, {0.3f, 0.0f, 0.0f}, {0.0f, 0.0f, -10.0f} };
    Vector3 lightDir = { 0, -1, 0 };
    float time = 0.0f;

    // 4. メインループ
    while (true) {
        if (winApp->ProcessMessage()) break;

        input->Update();
        time += 0.02f;

        // --- 更新処理 ---

        // 特定のオブジェクトだけ動かす（要件：個別のワールド行列）
        obj1->SetRotation({ 0.0f, time, 0.0f });          // 左のやつはY回転
        obj2->SetRotation({ time, 0.0f, 0.0f });          // 右のやつはX回転

        // ライト回転
        lightDir.x = std::sin(time * 0.5f);
        lightDir.z = std::cos(time * 0.5f);
        object3dCommon->SetLightDirection(lightDir);

        // カメラ計算
        Matrix4x4 cameraWorld = Math::MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
        Matrix4x4 viewMatrix = Math::Inverse(cameraWorld);
        Matrix4x4 projectionMatrix = Math::MakePerspectiveFovMatrix(0.45f, (float)WinApp::kClientWidth / (float)WinApp::kClientHeight, 0.1f, 100.0f);

        // ★リスト内の全オブジェクトを一括更新
        for (Object3d* obj : objectList) {
            obj->SetCamera(viewMatrix, projectionMatrix); // カメラセット
            obj->Update(); // 行列更新
        }

        sprite->Update();

        // --- 描画処理 ---
        dxCommon->PreDraw();

        // ヒープセット (共通)
        ID3D12DescriptorHeap* descriptorHeaps[] = { textureManager->GetSrvHeap() };
        dxCommon->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);

        // 3D描画
        object3dCommon->PreDraw();

        // ★リスト内の全オブジェクトを一括描画
        for (Object3d* obj : objectList) {
            obj->Draw();
        }

        // Sprite描画
        spriteCommon->PreDraw();
        sprite->Draw();

        dxCommon->PostDraw();
    }

    // 5. 解放
    // ★リストの中身を全削除
    for (Object3d* obj : objectList) {
        delete obj;
    }
    objectList.clear();

    // モデルの解放
    delete modelMulti;
    if (modelCube) delete modelCube;

    delete object3dCommon;
    delete sprite;
    delete spriteCommon;
    delete textureManager;
    delete input;
    delete dxCommon;

    winApp->Finalize();
    delete winApp;
   

    return 0;
}