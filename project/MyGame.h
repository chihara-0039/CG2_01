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
        DebugView,   // 今の確認用
        StageEditor, // これから作るエディター
        GamePlay     // 後で本編
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

	// アプリのモード管理
    AppMode currentMode_ = AppMode::DebugView;
    DebugDrawFlags debugFlags_;
	StageMap stageMap_;

    void UpdateImGui();
    void UpdateDebugView();
	void RefreshStageList();
    void UpdateStageEditor();
    void UpdateGamePlay();

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


    // 3/27 佐倉追加　ゴール判定変数
    bool isGoalReached_ = false;
    // ドア
    bool isWaitingForSecondDoor_ = false; // 2つ目のドア配置待ちか？
    Int3 firstDoorIndex_ = { -1, -1, -1 };  // 1つ目に置いたドアの座標

};