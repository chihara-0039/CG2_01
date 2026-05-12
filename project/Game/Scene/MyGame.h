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

#include "StageSelect.h"

#include"Sound.h"


class MyGame {
public:

    enum class AppMode { 
        Title,
<<<<<<< CG5
		GameClear,
=======
        StageSelect, //5/10追加　小林
>>>>>>> master
        DebugView,
        StageEditor,
        GamePlay 
    }; // モード定義

    // --- メンバ関数 ---

    // 初期化
    void Initialize() override;

    // 終了処理
    void Finalize() override;

    // 毎フレーム更新
    void Update() override;

    // 描画
    void Draw() override;

    // シーン管理
    std::unique_ptr<TitleScene> titleScene_;
    std::unique_ptr<StageSelect> stageSelect_; //5/10 小林
    std::unique_ptr<GameClearScene> gameClearScene_;

    // カメラ回転用UIスプライト
    std::unique_ptr<Sprite> cameraGuideLeftSprite_;
    std::unique_ptr<Sprite> cameraGuideRightSprite_;
    std::unique_ptr<Sprite> cameraGuideUpSprite_;
    std::unique_ptr<Sprite> cameraGuideDownSprite_;

    uint32_t cameraGuideTextureHandle_ = 0;

    // 追加：ドア用3D F UI
    std::unique_ptr<Model> doorPromptModel_;
    std::unique_ptr<Object3d> doorPromptObject_;

    // はしご用3D UI
    std::unique_ptr<Model> ladderPromptModel_;
    std::unique_ptr<Object3d> ladderPromptObject_;

    // ==========================================================
    // メンバ変数（値や状態）
    // ==========================================================
    AppMode currentMode_ = AppMode::DebugView;
    // 現在実行中のシーン
    std::unique_ptr<IScene> scene_;

    void DrawCommonUI(); // 左側のDebug Window
    void ChangeMode(AppMode newMode);

    // 描画フラグ (スクショ用)
    bool show3DObjects_ = true;
    bool showSprite_ = true;
    bool showParticles_ = true;

    struct DebugDrawFlags {
        bool show3DObjects = true;
        bool showSprite = true;
        bool showParticles = true;
    };

    DebugDrawFlags debugFlags_;

    void UpdateStageSelect(); //5/10追加　小林

    //カメラ回転用
    void UpdateCameraGuideSprites();
    void DrawCameraGuideSprites();

    // 追加：ドア用3D F UI更新
    void UpdateDoorPrompt3D();

    //追加　はしご用UI更新
    void UpdateLadderPrompt3D();

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
};