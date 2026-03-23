#include "Player.h"
#include <cmath>

Player::~Player() {
    delete object_;
}

void Player::Initialize(Object3dCommon* common, Model* model) {
    object_ = new Object3d();
    object_->Initialize(common);
    object_->SetModel(model);
    // キノピオ隊長のように、モデルを直立させるための初期回転
    object_->SetRotation({ 0.0f, 0.0f, 0.0f });
}

void Player::Update(const Input* input, const StageMap& map) {
    Vector3 move = { 0, 0, 0 };

    // 1. キーボード入力による移動量の計算 (XZ平面)
    if (input->PushKey(DIK_W)) move.z += walkSpeed_;
    if (input->PushKey(DIK_S)) move.z -= walkSpeed_;
    if (input->PushKey(DIK_A)) move.x -= walkSpeed_;
    if (input->PushKey(DIK_D)) move.x += walkSpeed_;

    // --- ジャンプ入力 ---
    // 地面にいて、かつスペースキーが押された瞬間
    if (isGrounded_ && input->TriggerKey(DIK_SPACE)) {
        velocity_.y = jumpSpeed_;
        isGrounded_ = false; // ジャンプした瞬間は空中扱い
    }

    // 2. 重力の適用
    velocity_.y += gravity_;

    // 3. 軸別の当たり判定 (AABB方式)

    // --- X軸の移動 ---
    Vector3 nextPosX = position_;
    nextPosX.x += move.x;
    if (!CheckCollision(nextPosX, map)) {
        position_.x = nextPosX.x;
    }

    // --- Z軸の移動 ---
    Vector3 nextPosZ = position_;
    nextPosZ.z += move.z;
    if (!CheckCollision(nextPosZ, map)) {
        position_.z = nextPosZ.z;
    }

    // --- Y軸（落下・上昇）の移動処理 ---
    Vector3 nextPosY = position_;
    nextPosY.y += velocity_.y;

    if (CheckCollision(nextPosY, map)) {
        // 衝突した場合
        if (velocity_.y < 0) {
            // 下方向に移動中に衝突 ＝ 着地
            isGrounded_ = true;
        }
        velocity_.y = 0; // 速度をリセット
    } else {
        // 衝突していない ＝ 空中
        position_.y = nextPosY.y;
        isGrounded_ = false;
    }

    // 4. 表示更新
    object_->SetPosition(position_);
    object_->Update();
}

// 衝突判定ロジック
bool Player::CheckCollision(const Vector3& pos, const StageMap& map) {
    // プレイヤーの当たり判定ボックス（四隅など）が StageMap の solid なセルに重なっているか
    // 足元、腰、頭の3段階で高さをチェック
    float checkOffsetsY[] = { 0.1f, 0.8f, 1.5f };

    for (float dy : checkOffsetsY) {
        for (float dx : { -radius_.x, radius_.x }) {
            for (float dz : { -radius_.z, radius_.z }) {
                // ワールド座標からマップのインデックス（整数）に変換
                int gx = static_cast<int>(std::floor(pos.x + dx + 0.5f));
                int gy = static_cast<int>(std::floor(pos.y + dy));
                int gz = static_cast<int>(std::floor(pos.z + dz + 0.5f));

                // 指定した座標のセル情報を取得
                const MapCell* cell = map.GetCell(gx, gy, gz);
                if (cell && cell->isSolid) {
                    return true; // 壁または地面にぶつかった
                }
            }
        }
    }
    return false;
}

void Player::Draw() {
    if (object_) {
        object_->Draw();
    }
}