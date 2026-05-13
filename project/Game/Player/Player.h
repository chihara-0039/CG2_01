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
    void Update(const Input* input,  StageMap& map, float cameraRotY, const Matrix4x4& lightVP);

	// Object3d の行列を更新する（ライトカメラの行列も渡す）
    void UpdateTransform(const Matrix4x4& lightVP);

    void CrumbleUpdate(StageMap& map);

    // 描画：内部で持っている Object3d を描画
    void Draw();

	// 影の描画：ライトカメラの行列を渡して影を描く
    void DrawShadow(const Matrix4x4& lightViewProjection);

    //自機の影強調表示関数
    void DrawHighlight();

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
    void DoorWarp(const StageMap& map);

    // 04/01 小林追加：リスポーン
    void SetRespawnPosition(const Vector3& pos) { respawnPosition = pos; }
    void Respawn();

    // Pスイッチの追加 04/03 秋元
    void PSwitchUpdate(StageMap& map);
    
    //リスポーン用の座標
    Vector3 respawnPosition = { 0.0f,1.5f,0.0f };

    // ドアUI表示フラグ
    bool IsNearDoor() const { return isNearDoor_; }

    const Vector3& GetNearDoorWorldPos() const {
        return nearDoorWorldPos_;
    }

private:
    // マップのブロックと衝突しているかチェックするヘルパー
    bool CheckCollision(const Vector3& pos, const StageMap& map);

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

    //ドアUIの座標変換用変数
    Vector3 nearDoorWorldPos_ = { 0.0f,0.0f,0.0f };

    const Input* input_ = nullptr;
    bool isNearDoor_ = false;
};