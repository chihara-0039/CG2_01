#pragma once
#include <vector>
#include <memory> // unique_ptr のために必須
#include <filesystem>

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
#include "Player.h"
#include "ShadowMap.h"
#include "LightCamera.h"
#include "TitleScene.h"
#include "GameClearScene.h"

class MyGame {
public:
    void Initialize();
    void Update();
    void Draw();
    void Finalize();

    // アプリが終了していないか
    bool IsRunning() { return !winApp->ProcessMessage(); }

private:
    // アプリのモード定義
    enum class AppMode {
        Title,
        DebugView,
        StageEditor,
        GamePlay,
        GamePlay_BlockPlace,
        GameClear
    };

    struct DebugDrawFlags {
        bool show3DObjects = true;
        bool showSprite = true;
        bool showParticles = true;
    };

    // ==========================================================
    // 所有権を持つリソース（unique_ptr）
    // これらは MyGame が消えるとき、または Finalize で reset するときに自動解放されます
    // ==========================================================

    // 基盤システム（すべて unique_ptr に統一！）
    std::unique_ptr<WinApp> winApp;
    std::unique_ptr<DirectXCommon> dxCommon;
    std::unique_ptr<Input> input;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<SpriteCommon> spriteCommon;
    std::unique_ptr<Object3dCommon> object3dCommon;
    std::unique_ptr<ParticleManager> particleManager;

    // オブジェクト管理（ここも unique_ptr の vector にします）
    std::vector<std::unique_ptr<Object3d>> objectList;
    std::vector<std::unique_ptr<Model>> models;

    std::unique_ptr<Sprite> sprite;
    std::unique_ptr<Camera> camera;
    std::unique_ptr<StageRenderer> stageRenderer_;
    std::unique_ptr<MapCursor> mapCursor_;
    std::unique_ptr<Player> player_;

    // スカイドーム
    std::unique_ptr<Model> skydomeModel_;
    std::unique_ptr<Object3d> skydomeObject_;

    // 影用
    std::unique_ptr<ShadowMap> shadowMap_;
    std::unique_ptr<LightCamera> lightCamera_;

    // シーン管理
    std::unique_ptr<TitleScene> titleScene_;
    std::unique_ptr<GameClearScene> gameClearScene_;

    // カメラ回転用UIスプライト
    std::unique_ptr<Sprite> cameraGuideLeftSprite_;
    std::unique_ptr<Sprite> cameraGuideRightSprite_;
    std::unique_ptr<Sprite> cameraGuideUpSprite_;
    std::unique_ptr<Sprite> cameraGuideDownSprite_;

    uint32_t cameraGuideTextureHandle_ = 0;

    // ==========================================================
    // メンバ変数（値や状態）
    // ==========================================================
    AppMode currentMode_ = AppMode::DebugView;
    DebugDrawFlags debugFlags_;
    StageMap stageMap_;
    BlockType selectedBlockType_ = BlockType::Ground;

    float gameCameraAngle_ = 0.0f;
    float targetCameraAngle_ = 0.0f;
    float cameraAngle_ = 0.0f;
    float cameraPitch_ = 0.75f;

    bool isGoalReached_ = false;
    bool isWaitingForSecondDoor_ = false;
    Int3 firstDoorIndex_ = { -1, -1, -1 };
    int placeableBlockCount_ = 0;

    // エディタUI用
    Vector3 editorBlockScale_{ 1.0f, 1.0f, 1.0f };
    float editorUniformBlockScale_ = 1.0f;
    std::vector<std::string> stageFiles_;
    char newStageName_[64] = "new_stage";
    int selectedStageIndex_ = -1;

    // 内部関数
    void UpdateImGui();
    void UpdateDebugView();
    void RefreshStageList();
    void UpdateStageEditor();
    void UpdateGamePlay();
    void UpdateGamePlayBlockPlace();
    void UpdateTitle();
    void DrawEditorToolbar();
    void ApplyPlacement();
    //カメラ回転用
    void UpdateCameraGuideSprites();
    void DrawCameraGuideSprites();

    // ヘルパー関数の戻り値は「生ポインタ」のままでOK（所有権を渡さない「参照」のため）
    Object3d* CreateObject(Model* model, Vector3 pos);
};