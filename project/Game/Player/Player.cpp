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

//4/1 佐倉　プレイヤー透過関数
bool Player::CheckHiddenByWall(const Vector3& cameraPos, const StageMap& map)
{
	Vector3 dir;
	dir.x = position_.x - cameraPos.x;
	dir.y = position_.y - cameraPos.y;
	dir.z = position_.z - cameraPos.z;

	float length = std::sqrt(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
	if (length == 0) return false;

	// 正規化
	dir.x /= length;
	dir.y /= length;
	dir.z /= length;

	float step = 0.2f;

	for (float t = 0; t < length; t += step) {
		Vector3 checkPos;
		checkPos.x = cameraPos.x + dir.x * t;
		checkPos.y = cameraPos.y + dir.y * t;
		checkPos.z = cameraPos.z + dir.z * t;

		int gx = static_cast<int>(std::floor(checkPos.x + 0.5f));
		int gy = static_cast<int>(std::floor(checkPos.y));
		int gz = static_cast<int>(std::floor(checkPos.z + 0.5f));

		const MapCell* cell = map.GetCell(gx, gy, gz);

		if (cell && cell->isSolid) {
			return true; // 壁あり
		}
	}

	return false;
}


void Player::Update(const Input* input, const StageMap& map, float cameraRotY)
{
	stageMap_ = &map;
	input_ = input;
	// --- 1. ハシゴ判定 ---
	int gx = static_cast<int>(std::floor(position_.x + 0.5f));
	int gyBottom = static_cast<int>(std::floor(position_.y + 0.1f));
	int gyWaist = static_cast<int>(std::floor(position_.y + 0.8f));
	int gz = static_cast<int>(std::floor(position_.z + 0.5f));

	const MapCell* cellBottom = map.GetCell(gx, gyBottom, gz);
	const MapCell* cellWaist = map.GetCell(gx, gyWaist, gz);

	// 足元か腰がハシゴならハシゴモード
	bool isOnLadder = (cellBottom && cellBottom->type == BlockType::Ladder) ||
		(cellWaist && cellWaist->type == BlockType::Ladder);

	Vector3 move = { 0, 0, 0 };

	if (isOnLadder) {
		velocity_.y = 0;

		// 入力方向の強さを計算
		float moveForward = 0.0f;
		if (input->PushKey(DIK_W)) moveForward += 1.0f;
		if (input->PushKey(DIK_S)) moveForward -= 1.0f;

		float moveSide = 0.0f;
		if (input->PushKey(DIK_D)) moveSide += 1.0f;
		if (input->PushKey(DIK_A)) moveSide -= 1.0f;

		// 「前」か「右」への入力があれば登る、逆なら下りる
		float verticalDir = moveForward + moveSide;

		if (verticalDir != 0.0f) {
			Vector3 nextPos = position_;
			nextPos.y += (verticalDir > 0 ? 1.0f : -1.0f) * walkSpeed_;

			if (!CheckCollision(nextPos, map)) {
				position_.y = nextPos.y;
				// ハシゴの芯に吸い寄せる
				position_.x += (static_cast<float>(gx) - position_.x) * 0.2f;
				position_.z += (static_cast<float>(gz) - position_.z) * 0.2f;
			} else if (verticalDir > 0) {
				// ★登りきり：ハシゴ自体の向き（cellWaistの回転）を使って押し出す
				// cellWaist がハシゴのはずなので、その rotationY を取得
				float exitAngle = (cellWaist ? cellWaist->rotationY : rotation_.y);

				float pushForward = 0.2f;
				position_.x += std::sin(exitAngle) * pushForward;
				position_.z += std::cos(exitAngle) * pushForward;

				// 少し上に上げて床判定を確実に踏ませる
				position_.y += 0.1f;
			}
		}
		isGrounded_ = true;
	} else {
		// --- 【通常移動モード】（ここが消えていたのでフリーズしていました） ---

		// 入力方向をカメラの回転に合わせる
		Vector3 inputDir = { 0, 0, 0 };
		if (input->PushKey(DIK_W)) inputDir.z += 1.0f;
		if (input->PushKey(DIK_S)) inputDir.z -= 1.0f;
		if (input->PushKey(DIK_A)) inputDir.x -= 1.0f;
		if (input->PushKey(DIK_D)) inputDir.x += 1.0f;

		if (inputDir.x != 0 || inputDir.z != 0) {
			// カメラのY軸回転に合わせて移動ベクトルを計算
			move.x = inputDir.x * std::cos(cameraRotY) + inputDir.z * std::sin(cameraRotY);
			move.z = -inputDir.x * std::sin(cameraRotY) + inputDir.z * std::cos(cameraRotY);

			// 速度と向きを更新
			move.x *= walkSpeed_;
			move.z *= walkSpeed_;
			rotation_.y = std::atan2f(move.x, move.z);
		}

		// ジャンプ
		if (isGrounded_ && input->TriggerKey(DIK_SPACE)) {
			velocity_.y = 0.3f;
			isGrounded_ = false;
		}

		// 重力適用
		velocity_.y += gravity_;

		// X軸衝突判定
		Vector3 nextPosX = position_;
		nextPosX.x += move.x;
		if (!CheckCollision(nextPosX, map)) position_.x = nextPosX.x;

		// Z軸衝突判定
		Vector3 nextPosZ = position_;
		nextPosZ.z += move.z;
		if (!CheckCollision(nextPosZ, map)) position_.z = nextPosZ.z;

		// Y軸衝突判定
		Vector3 nextPosY = position_;
		nextPosY.y += velocity_.y;
		if (CheckCollision(nextPosY, map)) {
			if (velocity_.y < 0) isGrounded_ = true;
			velocity_.y = 0;
		} else {
			position_.y = nextPosY.y;
			isGrounded_ = false;
		}
	}

	// カメラ位置（簡易的な追従カメラ想定）
	Vector3 cameraPos = {
		position_.x - std::sin(cameraRotY) * 10.0f,
		position_.y + 5.0f,
		position_.z - std::cos(cameraRotY) * 10.0f
	};

	isHidden_ = CheckHiddenByWall(cameraPos, map);

	DoorWarp();

	// --- 表示更新 ---
	object_->SetPosition(position_);
	object_->SetRotation(rotation_);
	object_->Update();
}

void Player::DoorWarp()
{
	int gx = static_cast<int>(std::floor(position_.x + 0.5f));
	int gyBottom = static_cast<int>(std::floor(position_.y + 0.1f));
	int gz = static_cast<int>(std::floor(position_.z + 0.5f));

	const MapCell* cell = stageMap_->GetCell(gx, gyBottom, gz);

	if (cell && cell->type == BlockType::Door && input_->TriggerKey(DIK_F))
	{
		Int3 doorWarpTarget = cell->doorTargetIndex;

		position_.x = static_cast<float>(doorWarpTarget.x);
		position_.y = static_cast<float>(doorWarpTarget.y);
		position_.z = static_cast<float>(doorWarpTarget.z);

		velocity_ = { 0.0f,0.0f,0.0f };
	}

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

		if (isHidden_) {
			object_->SetColor({ 1.0f, 1.0f, 1.0f, 0.3f }); // 半透明
		} else {
			object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
		}

		object_->Draw();
	}
}