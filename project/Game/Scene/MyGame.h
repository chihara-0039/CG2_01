#pragma once
#include <vector>
#include <memory>
#include <filesystem>
#include <array>
#include <string>

// ===== エンジン基盤 =====
#include "WinApp.h"
#include "DirectXCommon.h"
#include "TextureManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "ParticleManager.h"
#include "Input.h"
#include "Camera.h"
#include "ShadowMap.h"
#include "LightCamera.h"

// ===== 3D オブジェクト =====
#include "Object3d.h"
#include "Model.h"
#include "Sprite.h"
#include "Skybox.h"
#include "SkinnedObject.h"

// ===== ゲームエンティティ =====
#include "Player.h"
#include "MapCursor.h"
#include "StageMap.h"
#include "StageRenderer.h"
#include"PlayerBasePosition.h"

// ===== UI =====
#include "GameplayUIManager.h"
#include "BlockInventoryUI.h"

// ===== シーン =====
#include "StageSelect.h"

// ===== コントローラー =====
#include "GameplayCameraController.h"
#include "StageEditorController.h"
#include "SkinningEditorController.h"

// ===== ブロック関連 =====
#include "../Block/BlockInventory.h"
#include "../Block/BubblePickupController.h"
#include "../Block/BlockPlacementController.h"

// ===== サウンド =====
#include "Sound.h"

// ===== ポストプロセス =====
#include "PostProcessRenderer.h"

// ===== ステージ補助 =====
#include "StageRespawnController.h"

/// <summary>
/// ゲーム全体を統括するメインクラス。
///
/// 役割：
///   - 全システム (エンジン基盤・シーン・UI・エフェクト) の生成・初期化・破棄の管理
///   - AppMode による画面遷移の制御
///   - 描画パス (シャドウマップ → オフスクリーン/直接 → ImGui) の実行
///
/// 各サブシステムは専用クラス (SkinningEditorController / PostProcessRenderer 等) に委譲し、
/// MyGame は「接続役 (オーケストレーター)」として機能する。
/// </summary>
class MyGame {
    friend class MyGameRenderer;
    friend class MyGameGameplay;
public:
    void Initialize();
    void Update();
    void Draw();
    void Finalize();

    /// <summary>ウィンドウが閉じられていなければ true を返す</summary>
    bool IsRunning() { return !winApp->ProcessMessage(); }

private:
    // ==========================================================
    //  AppMode : アプリケーションのモード定義
    // ==========================================================
    enum class AppMode {
        StageSelect,       ///< ステージ選択画面
        DebugView,         ///< デバッグ確認用ビュー
        StageEditor,       ///< ステージエディタ
        GamePlay,          ///< ゲームプレイ
        GamePlay_BlockPlace,///< ブロック配置モード (GamePlay のサブモード)
        SkinningEditor,    ///< スキニングエディタ
        EffectPreview,     ///< エフェクト編集・確認モード
        EffectShowcase,    ///< Release対応のエフェクト鑑賞モード
    };

    /// <summary>DebugView モードで表示するオブジェクトのフラグ群</summary>
    struct DebugDrawFlags {
        bool show3DObjects = true;  ///< 3D オブジェクトを表示するか
        bool showSkybox = true;  ///< スカイドーム / スカイボックスを表示するか
        bool showSprite = true;  ///< スプライトを表示するか
        bool showParticles = true;  ///< パーティクルを表示するか
        bool showTerrain = true;  ///< 地形を表示するか
    };

    // ==========================================================
    //  エンジン基盤システム (アプリ全体で共有)
    // ==========================================================
    std::unique_ptr<WinApp>         winApp;
    std::unique_ptr<DirectXCommon>  dxCommon;
    std::unique_ptr<Input>          input;
    std::unique_ptr<TextureManager> textureManager;
    std::unique_ptr<SpriteCommon>   spriteCommon;
    std::unique_ptr<Object3dCommon> object3dCommon;
    std::unique_ptr<ParticleManager>particleManager;

    // ==========================================================
    //  3D オブジェクト・モデル
    // ==========================================================
    std::vector<std::unique_ptr<Object3d>> objectList; ///< DebugView 用の汎用オブジェクト群
    std::vector<std::unique_ptr<Model>>    models;     ///< 共有モデルリスト (0:block / 1:axis / 2:player)

    std::unique_ptr<Sprite>       sprite;         ///< DebugView 用テストスプライト
    std::unique_ptr<Camera>       camera;         ///< メインカメラ
    std::unique_ptr<StageRenderer>stageRenderer_; ///< ステージブロックのレンダラー
    std::unique_ptr<MapCursor>    mapCursor_;     ///< エディタ・配置モード用カーソル
    std::unique_ptr<Player>       player_;        ///< プレイヤーキャラクター

    // ==========================================================
    //  地形 (Terrain) ・スカイボックス
    // ==========================================================
    std::unique_ptr<Model>   terrainModel_;    ///< 地形メッシュモデル
    std::unique_ptr<Object3d>terrainObject_;   ///< 地形のオブジェクト
    std::unique_ptr<Object3d>effectShowcaseGround_; ///< エフェクト鑑賞用の受光床
    std::unique_ptr<Model>   skydomeModel_;    ///< スカイドームモデル
    std::unique_ptr<Object3d>skydomeObject_;   ///< スカイドームのオブジェクト
    std::unique_ptr<Skybox>  skybox_;          ///< キューブマップスカイボックス
    uint32_t skyboxTextureHandle_ = 0;         ///< スカイボックスのテクスチャハンドル
    bool     showSkyboxCubemap_ = false;     ///< true: Skybox / false: Skydome

    // ==========================================================
    //  シャドウマップ・ライトカメラ
    // ==========================================================
    std::unique_ptr<ShadowMap>    shadowMap_;    ///< シャドウマップ生成器
    std::unique_ptr<LightCamera>  lightCamera_;  ///< ライト視点カメラ (影行列の生成元)

    // ==========================================================
    //  シーン管理
    // ==========================================================
    std::unique_ptr<StageSelect>    stageSelect_;    ///< ステージ選択画面

    // ==========================================================
    //  UI・チュートリアル
    // ==========================================================
    std::unique_ptr<GameplayUIManager> gameplayUIManager_;         ///< ゲームプレイ中の UI 管理
    std::unique_ptr<BlockInventoryUI>  blockInventoryUI_;          ///< インベントリ UI
    std::unique_ptr<Sprite>            tutorialSprite_;            ///< 操作チュートリアル画像
    std::unique_ptr<Sprite>            placementTutorialSprite_;   ///< 配置チュートリアル画像

    // UIガイド用スプライト
    uint32_t objectiveGuideTexture_ = 0;
    uint32_t stageSelectGuideTexture_ = 0;
    uint32_t clearGuideTexture_ = 0;
    std::unique_ptr<Sprite> objectiveGuideSprite_;
    std::unique_ptr<Sprite> stageSelectGuideSprite_;
    std::unique_ptr<Sprite> clearGuideSprite_;

    // ==========================================================
    //  コントローラー (値として保持: 常に有効)
    // ==========================================================
    GameplayCameraController  gameplayCameraController_; ///< ゲームプレイカメラ制御
    StageEditorController     stageEditorController_;    ///< ステージエディタ操作
    SkinningEditorController  skinningEditor_;           ///< スキニングエディタ全般
    PostProcessRenderer       postProcess_;              ///< オフスクリーン/ポストエフェクト

    // ==========================================================
    //  ブロック関連
    // ==========================================================
    BlockInventory           blockInventory_;           ///< プレイヤーのブロック所持状況
    BubblePickupController   bubblePickupController_;   ///< シャボン玉取得処理
    BlockPlacementController blockPlacementController_; ///< ブロック配置処理

    // ==========================================================
    //  ステージ管理
    // ==========================================================
    StageMap             stageMap_;                  ///< 現在のステージマップデータ
    StageMap             backupMap_;                 ///< ESC で復元するためのバックアップ
    StageRespawnController stageRespawnController_;  ///< リスポーン処理

    // ==========================================================
    //  自機管理
    // ==========================================================
    PlayerBasePosition playerBasePosition_;


    // ==========================================================
    //  サウンド (Sound.h のラッパー)
    // ==========================================================
    Sound            sound;         ///< サウンドシステム

    Sound::SoundData titleBgmData;
    Sound::SoundData gameBgmData;
    Sound::SoundData clearBgmData;

    float bgmVolume_ = 0.5f;

    enum class BgmType {
        None,
        Title,
        Game,
        Clear,
    };

    BgmType currentBgmType_ = BgmType::None;

    void UpdateBGM();

    Sound::SoundData wavSoundData;  ///< WAV 効果音 (SPACE で再生)
    Sound::SoundData mp4SoundData;  ///< MP4 音声 (M で再生)
    Sound::SoundData mp3SoundData;  ///< MP3 BGM (N で再生 / UP・DOWN で音量調整)
    float wavVolume = 0.5f;         ///< WAV の音量 (0.0 〜 1.0)
    float mp4Volume = 0.5f;         ///< MP4 の音量 (0.0 〜 1.0)
    float mp3Volume = 0.5f;         ///< MP3 の音量 (0.0 〜 1.0)

    // ==========================================================
    //  状態変数
    // ==========================================================
    AppMode        currentMode_ = AppMode::DebugView; ///< 現在のアプリモード
    AppMode        prevMode_ = AppMode::DebugView; ///< 前フレームのモード (変化検知用)
    DebugDrawFlags debugFlags_;                        ///< DebugView の描画フラグ群
    bool           isGoalReached_ = false;        ///< ゴール到達フラグ
    int            placeableBlockCount_ = 0;           ///< 配置可能なブロック数 (現在未使用)
    float          totalTime_ = 0.0f;         ///< 累積時間 (秒)

    // 一人称カメラ (FPS Camera) 関連
    bool  useFirstPersonCamera_ = false; ///< FPS カメラフラグ
    float fpsCameraYaw_ = 0.0f;  ///< FPS カメラの回転角 (Yaw)
    float fpsCameraPitch_ = 0.0f;  ///< FPS カメラの回転角 (Pitch)
    float fpsCameraFov_ = 0.9f;
    float placeRotationY_ = 0.0f;  ///< ブロック配置の回転角
    float playerGlow_ = 0.0f;  ///< プレイヤー発光量
    float debugObjectEnvironmentCoefficient_ = 0.25f;
    float terrainEnvironmentCoefficient_ = 0.0f;
    float playerEnvironmentCoefficient_ = 0.0f;
    Vector3 effectPreviewPosition_ = { 0.0f, 1.0f, 0.0f };
    float effectPreviewTimer_ = 0.0f;
    float effectPreviewInterval_ = 1.0f;
    bool effectPreviewAutoPlay_ = false;
    bool effectPreviewShowGPUParticleSphere_ = true;
    bool effectPreviewMirrorSlash_ = false;
    bool effectPreviewStormMode_ = false;
    bool effectPresetIncludeInShowcase_ = true;
    bool stormPresetIncludeInShowcase_ = true;
    int effectPreviewBurstCount_ = 1;
    float effectPreviewBurstRadius_ = 0.0f;
    ParticleManager::HitEffectSettings effectPreviewHitSettings_{};
    std::array<char, 64> effectPresetNameBuffer_{ "CinematicFinisher" };
    std::vector<std::string> effectPresetNames_;
    std::vector<std::string> effectShowcasePresetNames_;
    int effectPresetSelectedIndex_ = -1;
    int effectShowcaseSelectedIndex_ = 0;
    bool effectShowcaseAutoPlay_ = true;
    float effectShowcaseTimer_ = 0.0f;
    float effectShowcaseInterval_ = 2.5f;
    bool effectShowcaseFirstPlay_ = true;
    float effectShowcaseLightTimer_ = 0.0f;
    static constexpr float kEffectShowcaseLightDuration_ = 0.7f;
    std::string effectPresetStatus_ = "Preset: not loaded";
    std::array<char, 64> stormPresetNameBuffer_{ "Tempest Storm" };
    std::vector<std::string> stormPresetNames_;
    std::vector<std::string> stormShowcasePresetNames_;
    int stormPresetSelectedIndex_ = -1;
    std::string stormPresetStatus_ = "Storm preset: default";
    std::string cachedWeatherPresetName_;
    std::string cachedWeatherParticleTexturePath_;
    uint32_t cachedWeatherParticleTexture_ = 0;

    // ==========================================================
    //  内部メソッド
    // ==========================================================

    // --- 更新サブルーチン ---
    void UpdateImGui();                 ///< ImGui の更新・描画 (Debug ビルドのみ)
    void UpdateDebugView();             ///< DebugView モードの更新
    void UpdateEffectPreview();         ///< EffectPreview モードの更新
    void UpdateEffectShowcase();        ///< EffectShowcase モードの更新
    void EmitEffectPreviewBurst();       ///< EffectPreview のバースト発生
    void UpdateGamePlay();              ///< GamePlay モードの更新
    void UpdateGamePlayBlockPlace();    ///< GamePlay_BlockPlace モードの更新
    void UpdateTitle();                 ///< タイトル画面の更新
    void UpdateStageSelect();           ///< ステージ選択画面の更新
    void UpdateSceneTransition();       ///< ESC によるシーン遷移処理
    void HandleModeChange();
    void BeginFrameImGui();
    bool IsGuiCapturingMouse();
    Vector3 UpdateLightCameraForFrame();
    void UpdateHitEffectShortcut();
    void UpdateSharedCameraControls(bool isGuiCaptured);
    void UpdateBackgroundObjects();
    void UpdateParticleDebugVisibility();
    void UpdateCurrentMode(const Matrix4x4& lightVP, bool isGuiCaptured);
    void UpdatePlayerCameraAndTransform(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP);
    bool IsWindowInactive();
    void UpdateDebugAndEffectObjects(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP);
    void UpdateStagePresentation(const Matrix4x4& view, const Matrix4x4& proj, const Matrix4x4& lightVP);
    void UpdateWeatherParticles(const Matrix4x4& view, const Matrix4x4& proj);
    void ApplySceneLighting(const Vector3& lightDir);
    void UpdateClearColorForFrame();
    void UpdateGameplayUserInterface();

    // --- 描画サブルーチン ---
    /// <summary>オフスクリーンパスと直接パスで共通するシーン描画</summary>
    void LoadEffectPresetNames();
    bool SaveEffectPreset(const std::string& name);
    bool LoadEffectPreset(const std::string& name);
    void DrawEffectPreviewEditorImGui();
    void DrawStormEffectEditorImGui();
    void DrawEffectShowcaseImGui();
    void LoadStormPresetNames();
    bool SaveStormPreset(const std::string& name);
    bool LoadStormPreset(const std::string& name);
    bool IsCurrentEffectStorm() const;

    // --- ヘルパー ---
    /// <summary>モデルと位置を指定して Object3d を生成し objectList に追加する (非所有ポインタを返す)</summary>
    Object3d* CreateObject(Model* model, Vector3 pos);

    /// <summary>スカイドーム / スカイボックスを描画する内部ヘルパー</summary>

    /// <summary>カメラとプレイヤーの間に壁ブロックがあるか判定する (シルエット描画の判定用)</summary>
};
