#pragma once
#include "IScene.h"
#include "Player.h"
#include "StageMap.h"
#include "StageRenderer.h"
#include "Camera.h"
#include "Object3dCommon.h"
#include "Input.h"
#include "ShadowMap.h"
#include "LightCamera.h"
#include <memory>

/**
 * @brief ゲーム本編のロジックを管理するシーン
 * ISceneを継承し、MyGameから呼び出される
 */
class GamePlayScene : public IScene {
public:
    // --- メンバ関数 ---

    // デストラクタ
    virtual ~GamePlayScene() = default;

    // 初期化：必要なマネージャ等のポインタを受け取る
    void Initialize() override;

    // 2. エンジンの道具（ポインタ）を受け取るための専用関数
    void SetEnginePointers(
        Object3dCommon* objCommon,
        Input* input,
        TextureManager* texManager,
        ShadowMap* shadowMap,
        LightCamera* lightCamera
    );

    // 更新処理
    void Update() override;

    // 描画処理
    void Draw() override;

	// UI描画処理（ImGuiなど）
	void DrawUI() override;

    // 影用描画処理
    void DrawShadow();

    // シーン終了判定
    bool IsFinished() const override {
        return isFinished_;
    }

private:
    // --- 外部から借りるポインタ（所有権は持たない） ---
    Object3dCommon* objCommon_ = nullptr;
    Input* input_ = nullptr;
    TextureManager* texManager_ = nullptr;
    ShadowMap* shadowMap_ = nullptr;
    LightCamera* lightCamera_ = nullptr;

    // --- このシーンが所有するゲームオブジェクト ---
    std::unique_ptr<Player> player_;            // プレイヤー
    StageMap stageMap_;                         // マップデータ
    StageRenderer stageRenderer_;               // ステージ描画
    Camera camera_;                             // ゲーム用カメラ
    std::unique_ptr<Object3d> skydomeObject_;    // 天球
    std::unique_ptr<Model> skydomeModel_;       // 天球モデル

    // --- シーンの状態管理 ---
    bool isFinished_ = false;                   // 次のシーンへ移るフラグ
    bool isGoalReached_ = false;                // ゴールしたか
	float gameCameraAngle_ = 0.0f;              // カメラの回転角（X軸） 
	float cameraPitch_ = 0.75;                  // カメラの高さ（プレイヤーからの相対位置）
};