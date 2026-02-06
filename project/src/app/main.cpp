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

    // 画像読み込み（1回だけ！）
    // ※パスはご自身の環境に合わせて調整してください
    uint32_t textureHandle = textureManager->LoadTexture("Resources/uvChecker.png");

    // --- A. 元画像用スプライト（背景用） ---
    Sprite* spriteOriginal = new Sprite();
    spriteOriginal->Initialize(spriteCommon, textureHandle);

    // 左側に配置
    spriteOriginal->SetPosition({ 100.0f, 100.0f });
    spriteOriginal->SetSize({ 200.0f, 200.0f }); // 見やすくちょっと縮小

    // --- B. 切り抜き用スプライト（操作キャラ用） ---
    Sprite* spriteCropped = new Sprite();
    spriteCropped->Initialize(spriteCommon, textureHandle); // ★同じハンドルを使う

    // 右側に配置（座標管理用変数）
    Vector2 croppedPosition = { 400.0f, 100.0f };
    spriteCropped->SetPosition(croppedPosition);

    // ★ここで切り抜き設定（ループの外でOK）
    // 画像の (0, 0) から 64x64 切り抜く
    spriteCropped->SetTextureRect({ 0.0f, 0.0f }, { 64.0f, 64.0f });
    spriteCropped->SetSize({ 64.0f, 64.0f }); // サイズも合わせる


    // ==============================
    // 3. メインループ
    // ==============================
    while (true) {
        if (winApp->ProcessMessage()) {
            break;
        }

        // --- 更新処理 ---
        input->Update();

        // ★★★ 移動処理（切り抜きキャラの方を動かす） ★★★
        const float kSpeed = 5.0f;

        if (input->PushKey(DIK_RIGHT)) {
            croppedPosition.x += kSpeed;
        }
        if (input->PushKey(DIK_LEFT)) {
            croppedPosition.x -= kSpeed;
        }
        if (input->PushKey(DIK_UP)) {
            croppedPosition.y -= kSpeed;
        }
        if (input->PushKey(DIK_DOWN)) {
            croppedPosition.y += kSpeed;
        }

        // 移動した座標をスプライトに反映
        spriteCropped->SetPosition(croppedPosition);

        // ★2つとも更新する
        spriteOriginal->Update();
        spriteCropped->Update();


        // --- 描画処理 ---
        dxCommon->PreDraw();      // 画面クリア
        spriteCommon->PreDraw();  // 共通設定

        // ------------------------------
        // 描画コマンド
        // ------------------------------

        // ★ Heap設定（これを忘れると描画されません）
        ID3D12DescriptorHeap* descriptorHeaps[] = { textureManager->GetSrvHeap() };
        dxCommon->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);

        // ★2つとも描画する
        spriteOriginal->Draw();
        spriteCropped->Draw();

        // ------------------------------

        dxCommon->PostDraw();     // 画面確定
    }

    // ==============================
    // 4. 終了処理
    // ==============================
    // 作ったものはすべて解放
    delete spriteOriginal;
    delete spriteCropped;

    delete textureManager;
    delete spriteCommon;
    delete input;
    delete dxCommon;

    winApp->Finalize();
    delete winApp;

    CoUninitialize();

    return 0;
}