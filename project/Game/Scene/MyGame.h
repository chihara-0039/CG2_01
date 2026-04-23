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

    // 現在実行中のシーン
    std::unique_ptr<IScene> scene_;

    // 次のシーンへの予約（遷移用）
    // ※今回は簡単のため、直接 scene_ を差し替える方式をとります
};