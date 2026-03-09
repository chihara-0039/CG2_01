#pragma once
#include <vector>
#include "WinApp.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "Input.h"
#include "Object3d.h"
#include "Model.h"
#include "Sprite.h"
#include "Camera.h"



class MyGame {
public:
    void Initialize();
    void Update();
    void Draw();
    void Finalize();
    bool IsRunning() { return !winApp->ProcessMessage(); }

private:

    enum class AppMode {
        DebugView,   // 今の確認用
        StageEditor, // これから作るエディター
        GamePlay     // 後で本編
    };

    struct DebugDrawFlags {
        bool show3DObjects = true;
        bool showSprite = true;
        bool showParticles = true;
    };

    // 基盤系
    WinApp* winApp = nullptr;
    DirectXCommon* dxCommon = nullptr;
    Input* input = nullptr;
    TextureManager* textureManager = nullptr;
    SpriteCommon* spriteCommon = nullptr;
    Object3dCommon* object3dCommon = nullptr;
    ParticleManager* particleManager = nullptr;

    // オブジェクト管理
    std::vector<Object3d*> objectList;
    std::vector<Model*> models; // モデル解放用
    Sprite* sprite = nullptr;
    std::unique_ptr<Camera> camera;

    AppMode currentMode_ = AppMode::DebugView;
    DebugDrawFlags debugFlags_;

    void UpdateImGui();
    void UpdateDebugView();
    void UpdateStageEditor();
    void UpdateGamePlay();

    // ヘルパー関数
    Object3d* CreateObject(Model* model, Vector3 pos);
};