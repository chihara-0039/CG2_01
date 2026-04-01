#pragma once
#include "Object3d.h"
#include "Input.h"
#include "StageMap.h"

class Player {
public:
    Player() = default;
    ~Player();

    // 初期化：描画用コンポーネントとモデルを設定
    void Initialize(Object3dCommon* common, Model* model);

    // 更新：移動・重力・当たり判定の処理
    void Update(const Input* input, const StageMap& map, float cameraRotY);

    // 描画：内部で持っている Object3d を描画
    void Draw();

    // 座標の設定と取得
    void SetPosition(const Vector3& pos) { position_ = pos; }
    const Vector3& GetPosition() const { return position_; }

    // カメラの行列をセットする
    void SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
        if (object_) {
            object_->SetCamera(view, projection);
        }
    }

    // 3/27 佐倉追加
    const Vector3& GetRadius()const { return radius_; }
    void DoorWarp();

private:
    // マップのブロックと衝突しているかチェックするヘルパー
    bool CheckCollision(const Vector3& pos, const StageMap& map);

    //4/1 佐倉追加　プレイヤー透過関数
    bool CheckHiddenByWall(const Vector3& cameraPos, const StageMap&map);

private:
    Object3d* object_ = nullptr;    // プレイヤーの見た目
    Vector3 position_ = { 0, 0, 0 }; // 世界座標
    Vector3 rotation_ = { 0, 0, 0 };
    Vector3 velocity_ = { 0, 0, 0 }; // 速度（落下速度を管理）

    // 当たり判定の大きさ（中心からの半径）
    // モデルのサイズに合わせて調整してください
    Vector3 radius_ = { 0.35f, 0.8f, 0.35f };

    float walkSpeed_ = 0.12f;   // 歩く速さ
    float gravity_ = -0.015f;    // 重力の強さ

    float jumpSpeed_ = 0.3f;    // ジャンプの初速度（高さ）
    bool isGrounded_ = false;   // 接地フラグ
   
    const StageMap* stageMap_ = nullptr;

    const Input* input_ = nullptr;

    //4/1 佐倉追加　プレイヤー透過変数
    bool isHidden_ = false;
};