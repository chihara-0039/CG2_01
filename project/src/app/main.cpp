#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cassert>
#include <dxgidebug.h>
#include <dxcapi.h>

#include "WinApp.h"
#include "DirectXCommon.h"
#include "TextureManager.h" 
#include "SpriteCommon.h"
#include "Input.h"
#include "Logger.h"
#include "Sprite.h"

// ImGuiを使用する場合はコメントアウトを外してパスを通してください
#include "imgui.h"

#pragma comment(lib, "dxguid.lib")

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {

    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // リソースリークチェック用
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

    // ==============================
    // 1. 基盤システムの初期化
    // ==============================
    WinApp* winApp = new WinApp();
    winApp->Initialize();

    DirectXCommon* dxCommon = new DirectXCommon();
    dxCommon->Initialize(winApp);

    Input* input = new Input();
    input->Initialize(winApp);

    SpriteCommon* spriteCommon = new SpriteCommon();
    spriteCommon->Initialize(dxCommon);

    TextureManager* textureManager = new TextureManager();
    textureManager->Initialize(dxCommon);

    spriteCommon->SetTextureManager(textureManager);

    // ==============================
    // 2. リソースの読み込み・生成
    // ==============================

    // ★修正: ここにあった descriptorHeaps の行は削除しました（ここにあっても意味がないため）

    // 画像読み込み (Resourcesフォルダにある場合)
    uint32_t textureHandle = textureManager->LoadTexture("C:/Users/CG2/generated/CG2_01/project/Resources/uvChecker.png");

    // スプライト生成
    Sprite* sprite = new Sprite();
    sprite->Initialize(spriteCommon, textureHandle);

    sprite->SetPosition({ 100.0f, 100.0f });
    sprite->SetSize({ 100.0f, 100.0f });


    // ==============================
    // 3. メインループ
    // ==============================
    while (true) {
        if (winApp->ProcessMessage()) {
            break;
        }

        // --- 更新処理 ---
        input->Update();
        sprite->Update();

        // --- 描画処理 ---
        dxCommon->PreDraw();      // 画面クリア
        spriteCommon->PreDraw();  // 共通設定

        // ------------------------------
        // 描画コマンド
        // ------------------------------

        // ★★★ ここが一番重要です！ ★★★
        // 毎フレーム「今からテクスチャの保管場所(Heap)を使うよ」とCommandListに伝える必要があります。
        ID3D12DescriptorHeap* descriptorHeaps[] = { textureManager->GetSrvHeap() };
        dxCommon->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);

        // その後にDrawを呼ぶ
        sprite->Draw();

        // ------------------------------

        dxCommon->PostDraw();     // 画面確定
    }

    // ==============================
    // 4. 終了処理
    // ==============================
    delete sprite;
    delete textureManager;
    delete spriteCommon;
    delete input;
    delete dxCommon;

    winApp->Finalize();
    delete winApp;

    CoUninitialize();

    return 0;
}