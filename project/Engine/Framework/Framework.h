#pragma once
#include <memory>

// --- エンジン基盤のヘッダ群 ---
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "D3DResourceLeakChecker.h"
#include "TextureManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "ModelManager.h"
#include "ShadowMap.h"
#include "LightCamera.h"
#include "Camera.h"

/**
 * @brief 全てのゲームに共通する「エンジン機能」を管理する基底クラス
 */
class Framework {
public:
    virtual ~Framework() = default;

    // 定型フロー：MyGame側で override して中身を書く
    virtual void Initialize(); // エンジン基盤の初期化
    virtual void Finalize();   // エンジン基盤の終了
    virtual void Update();     // 共通更新（入力など）
    virtual void Draw() = 0;   // 描画（ゲームごとに実装）

    // メインループの実行（mainからこれを呼ぶ）
    void Run();

    // アプリ終了判定
    virtual bool IsRunning();

protected:
    // --- 子クラス（MyGame）から直接触れる「エンジンの道具」 ---
    std::unique_ptr<WinApp> winApp;
    std::unique_ptr<DirectXCommon> dxCommon;
    std::unique_ptr<Input> input;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<SpriteCommon> spriteCommon;
    std::unique_ptr<Object3dCommon> object3dCommon;
    std::unique_ptr<ParticleManager> particleManager;
    std::unique_ptr<ShadowMap> shadowMap;
    std::unique_ptr<LightCamera> lightCamera;
    std::unique_ptr<Camera> camera;

    // 自動リークチェック（デストラクタで動作）
    D3DResourceLeakChecker leakChecker;
};