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
#include "../Block/BlockInventory.h"
#include "../Block/BubblePickupController.h"
#include "../Block/BlockPlacementController.h"


#include "StageSelect.h"

#include "Sound.h"
#include "GameplayUIManager.h"
#include "GameplayCameraController.h"
#include "StageEditorController.h"
#include "../UI/BlockInventoryUI.h"

#include "StageRespawnController.h"

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
        StageSelect, //5/10追加　小林
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
    std::unique_ptr<StageSelect> stageSelect_; //5/10 小林
    std::unique_ptr<GameClearScene> gameClearScene_;

    // ゲームプレイ中のUI・プロンプト管理
    std::unique_ptr<GameplayUIManager> gameplayUIManager_;
    std::unique_ptr<BlockInventoryUI> blockInventoryUI_;

    // ==========================================================
    // メンバ変数（値や状態）
    // ==========================================================
    AppMode currentMode_ = AppMode::DebugView;
    DebugDrawFlags debugFlags_;
    StageMap stageMap_;
    GameplayCameraController gameplayCameraController_;
    StageEditorController stageEditorController_;

    bool isGoalReached_ = false;
    int placeableBlockCount_ = 0;

    // 内部関数
    void UpdateImGui();
    void UpdateDebugView();
    void UpdateGamePlay();
    void UpdateGamePlayBlockPlace();
    void UpdateTitle();
    void UpdateSceneTransition();

    // シャボン玉取得・ブロック配置関連
    BlockInventory blockInventory_;
    BubblePickupController bubblePickupController_;
    BlockPlacementController blockPlacementController_;
    void UpdateStageSelect(); //5/10追加　小林



    // 追加：プレイヤーが壁に隠れているか判定
    bool IsPlayerHiddenByWall() const;

    // ヘルパー関数の戻り値は「生ポインタ」のままでOK（所有権を渡さない「参照」のため）
    Object3d* CreateObject(Model* model, Vector3 pos);

    //5/5佐倉追加
    //サウンド管理
    Sound sound;
    //音声データ
    Sound::SoundData wavSoundData;
    Sound::SoundData mp4SoundData;
    Sound::SoundData mp3SoundData;

    //音量メンバ変数
    float wavVolume = 0.5f;
    float mp4Volume = 0.5f;
    float mp3Volume = 0.5f;

    //5/7佐倉


    float totalTime_ = 0.0f; // 累積時間を保存する変数

    //5/14 小林
    StageMap backupMap_;
    StageRespawnController stageRespawnController_;
};