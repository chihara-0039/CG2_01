#pragma once
#include <vector>
#include <memory>
#include <filesystem>

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

// ===== UI =====
#include "GameplayUIManager.h"
#include "BlockInventoryUI.h"

// ===== シーン =====
#include "TitleScene.h"
#include "StageSelect.h"
#include "GameClearScene.h"

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
        Title,             ///< タイトル画面
        StageSelect,       ///< ステージ選択画面
        DebugView,         ///< デバッグ確認用ビュー
        StageEditor,       ///< ステージエディタ
        GamePlay,          ///< ゲームプレイ
        GamePlay_BlockPlace,///< ブロック配置モード (GamePlay のサブモード)
        GameClear,         ///< ゲームクリア画面
        SkinningEditor,    ///< スキニングエディタ
    };

    /// <summary>DebugView モードで表示するオブジェクトのフラグ群</summary>
    struct DebugDrawFlags {
        bool show3DObjects = true;  ///< 3D オブジェクトを表示するか
        bool showSkybox    = true;  ///< スカイドーム / スカイボックスを表示するか
        bool showSprite    = true;  ///< スプライトを表示するか
        bool showParticles = true;  ///< パーティクルを表示するか
        bool showTerrain   = true;  ///< 地形を表示するか
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
    std::unique_ptr<Model>   skydomeModel_;    ///< スカイドームモデル
    std::unique_ptr<Object3d>skydomeObject_;   ///< スカイドームのオブジェクト
    std::unique_ptr<Skybox>  skybox_;          ///< キューブマップスカイボックス
    uint32_t skyboxTextureHandle_ = 0;         ///< スカイボックスのテクスチャハンドル
    bool     showSkyboxCubemap_   = false;     ///< true: Skybox / false: Skydome

    // ==========================================================
    //  シャドウマップ・ライトカメラ
    // ==========================================================
    std::unique_ptr<ShadowMap>    shadowMap_;    ///< シャドウマップ生成器
    std::unique_ptr<LightCamera>  lightCamera_;  ///< ライト視点カメラ (影行列の生成元)

    // ==========================================================
    //  シーン管理
    // ==========================================================
    std::unique_ptr<TitleScene>     titleScene_;     ///< タイトル画面
    std::unique_ptr<StageSelect>    stageSelect_;    ///< ステージ選択画面
    std::unique_ptr<GameClearScene> gameClearScene_; ///< ゲームクリア画面

    // ==========================================================
    //  UI・チュートリアル
    // ==========================================================
    std::unique_ptr<GameplayUIManager> gameplayUIManager_;         ///< ゲームプレイ中の UI 管理
    std::unique_ptr<BlockInventoryUI>  blockInventoryUI_;          ///< インベントリ UI
    std::unique_ptr<Sprite>            tutorialSprite_;            ///< 操作チュートリアル画像
    std::unique_ptr<Sprite>            placementTutorialSprite_;   ///< 配置チュートリアル画像

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
    //  サウンド (Sound.h のラッパー)
    // ==========================================================
    Sound            sound;         ///< サウンドシステム
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
    AppMode        prevMode_    = AppMode::DebugView; ///< 前フレームのモード (変化検知用)
    DebugDrawFlags debugFlags_;                        ///< DebugView の描画フラグ群
    bool           isGoalReached_      = false;        ///< ゴール到達フラグ
    int            placeableBlockCount_ = 0;           ///< 配置可能なブロック数 (現在未使用)
    float          totalTime_          = 0.0f;         ///< 累積時間 (秒)

    // 一人称カメラ (FPS Camera) 関連
    bool  useFirstPersonCamera_ = false; ///< FPS カメラ有効フラグ
    float fpsCameraYaw_         = 0.0f;  ///< FPS カメラの水平回転角 (Yaw)
    float fpsCameraPitch_       = 0.0f;  ///< FPS カメラの垂直回転角 (Pitch)
    float placeRotationY_       = 0.0f;  ///< ブロック配置時の回転角

    // ==========================================================
    //  内部メソッド
    // ==========================================================

    // --- 更新サブルーチン ---
    void UpdateImGui();                 ///< ImGui の更新・描画 (Debug ビルドのみ)
    void UpdateDebugView();             ///< DebugView モードの更新
    void UpdateGamePlay();              ///< GamePlay モードの更新
    void UpdateGamePlayBlockPlace();    ///< GamePlay_BlockPlace モードの更新
    void UpdateTitle();                 ///< タイトル画面の更新
    void UpdateStageSelect();           ///< ステージ選択画面の更新
    void UpdateSceneTransition();       ///< ESC によるシーン遷移処理

    // --- 描画サブルーチン ---
    /// <summary>オフスクリーンパスと直接パスで共通するシーン描画</summary>
    void RenderScene(ID3D12GraphicsCommandList* commandList, const Matrix4x4& lightVP);

    // --- ヘルパー ---
    /// <summary>モデルと位置を指定して Object3d を生成し objectList に追加する (非所有ポインタを返す)</summary>
    Object3d* CreateObject(Model* model, Vector3 pos);

    /// <summary>スカイドーム / スカイボックスを描画する内部ヘルパー</summary>
    void DrawSkybox(ID3D12GraphicsCommandList* commandList);

    /// <summary>カメラとプレイヤーの間に壁ブロックがあるか判定する (シルエット描画の判定用)</summary>
    bool IsPlayerHiddenByWall() const;
};
