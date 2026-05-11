#pragma once
#include "../../Engine/Framework/Framework.h"
#include "IScene.h"
#include <memory>

/**
 * @brief ゲーム全体の司令塔クラス
 * Frameworkを継承し、エンジンの基盤機能を利用する
 */
class MyGame : public Framework {
public:

    enum class AppMode { 
        Title,
		GameClear,
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

private:
    // --- シーン管理 ---

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

	// --- シーンに渡すためのエンジン道具のポインタ ---
    std::unique_ptr<ShadowMap> shadowMap_;
    std::unique_ptr<LightCamera> lightCamera_;
};