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
#include"TitleScene.h"
#include"GameClearScene.h"

class MyGame {
public:
    void Initialize();
    void Update();
    void Draw();
    void Finalize();

    
	// アプリが終了していないか
    bool IsRunning() { return !winApp->ProcessMessage(); }

private:

	// アプリのモード
    enum class AppMode {
        Title,
        DebugView,   // 今の確認用
        StageEditor, // これから作るエディター
        GamePlay,     // 後で本編
        GamePlay_BlockPlace, // ブロックを置ける用の画面を追加(エンドフィールの工業画面のやつ)04/01 秋元
        GameClear //4/13追加　佐倉
    };

	// デバッグ表示のフラグ
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
    // ★ 2. これを追加（実体を持たせるため）
    //std::unique_ptr<StageCamera> gameplayCamera_;
    // 代わりに、回転角だけ MyGame で持っておく
    float gameCameraAngle_ = 0.0f;
    float targetCameraAngle_ = 0.0f;

	// アプリのモード管理
    AppMode currentMode_ = AppMode::DebugView;
    DebugDrawFlags debugFlags_;
	StageMap stageMap_;

    void UpdateImGui();
    void UpdateDebugView();
	void RefreshStageList();
    void UpdateStageEditor();
    void UpdateGamePlay();

    // 配置画面用の更新関数を宣言
    void UpdateGamePlayBlockPlace(); // 04/01 秋元

    //4/3佐倉タイトル用キー入力関数
    void UpdateTitle();

    // ギミック用のImGuiの追加 04/03 秋元
    void DrawEditorToolbar();
    // ★配置実行用の共通関数 04/01 秋元
    void ApplyPlacement();    

    // ヘルパー関数
    Object3d* CreateObject(Model* model, Vector3 pos);

	// エディタ用のUI制御変数
    Vector3 editorBlockScale_{ 1.0f, 1.0f, 1.0f };
    float editorUniformBlockScale_ = 1.0f;

	// ステージファイルの管理
    std::vector<std::string> stageFiles_; // 見つかったステージ名リスト
    char newStageName_[64] = "new_stage"; // 新規保存用の名前入力バッファ
    int selectedStageIndex_ = -1;         // リストで選択中の番号

	// プレイヤー
    Player* player_ = nullptr;

    // カメラの現在の回転角度をクラスで保持する
    float cameraAngle_ = 0.0f;

    //4/1 佐倉
   //縦回転用変数
    float cameraPitch_ = 0.75f;

    // 3/27 佐倉追加　ゴール判定変数
    bool isGoalReached_ = false;
    // ドア
    bool isWaitingForSecondDoor_ = false; // 2つ目のドア配置待ちか？
    Int3 firstDoorIndex_ = { -1, -1, -1 };  // 1つ目に置いたドアの座標

    // 置けるブロックの所持数 04/01 秋元
    int placeableBlockCount_ = 0;

    //4/3佐倉
    std::unique_ptr<TitleScene> titleScene_;
    Model* skydomeModel_ = nullptr;
    Object3d* skydomeObject_ = nullptr;
   
    //4/13佐倉
    std::unique_ptr<GameClearScene> gameClearScene_;
};