#pragma once
#include <vector>
#include <memory> // unique_ptr のために必須
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

#include "StageMap.h"
#include "StageRenderer.h"
#include "MapCursor.h"

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
    BlockType selectedBlockType_ = BlockType::Ground;

    // オブジェクト管理
    std::vector<Object3d*> objectList;
    std::vector<Model*> models; // モデル解放用
    Sprite* sprite = nullptr;
    std::unique_ptr<Camera> camera;
    StageRenderer* stageRenderer_ = nullptr;
	MapCursor* mapCursor_ = nullptr;

    AppMode currentMode_ = AppMode::DebugView;
    DebugDrawFlags debugFlags_;
	StageMap stageMap_;

    void UpdateImGui();
    void UpdateDebugView();
    void UpdateStageEditor();
    void UpdateGamePlay();

    // ヘルパー関数
    Object3d* CreateObject(Model* model, Vector3 pos);


    /// <summary>
	/// ImGuiの表示フラグ。
    /// </summary>
    bool showImGui_ = true;

    // カメラパラメータ（すでに cameraTransform を持ってるならそれを使う）
    Vector3 camTranslate_{ 0, 1.0f, -10.0f };
    Vector3 camRotate_{ 0, 0, 0 };
    float camFovY_ = 0.45f;
    float camNear_ = 0.1f;
    float camFar_ = 1000.0f;

    // ImGuiが使うSRVの予約index（例：0番を予約）
    uint32_t imguiSrvIndex_ = 0;
};