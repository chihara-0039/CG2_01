#include "Player.h"
#include <cmath>

Player::~Player() = default;

// 初期化：描画用コンポーネントとモデルを設定
void Player::Initialize(Object3dCommon* common, Model* model) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel(model);
	// キノピオ隊長のように、モデルを直立させるための初期回転
	object_->SetRotation({ 0.0f, 0.0f, 0.0f });
	
	// 自機キャラクターの微小光沢・高級メタル反射設定
	object_->SetShininess(0.6f);
	object_->SetMetallic(0.15f);
	object_->SetEmissive(0.0f);
}

// 更新：移動・重力・当たり判定の処理
void Player::Update(const Input* input,StageMap& map, float cameraRotY, const Matrix4x4& lightVP)
{
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

	// はしごUI用フラグを毎フレーム更新
	isOnLadder_ = isOnLadder;

	if (isOnLadder_) {
		int ladderY = gyWaist;

		ladderWorldPos_ = {
			static_cast<float>(gx),
			static_cast<float>(ladderY) + 1.2f,
			static_cast<float>(gz)
		};
	}

	Vector3 move = { 0, 0, 0 };

	if (isOnLadder) 
	{

#pragma region はしご

		velocity_.y = 0; // ハシゴ中は重力による落下を止める

		// 入力方向の強さを計算（W/Sキーで上下、A/Dキーで左右に整理）
		float moveVertical = 0.0f;
		if (input->PushKey(DIK_W)) moveVertical += 1.0f;
		if (input->PushKey(DIK_S)) moveVertical -= 1.0f;

		float moveSide = 0.0f;
		if (input->PushKey(DIK_D)) moveSide += 1.0f;
		if (input->PushKey(DIK_A)) moveSide -= 1.0f;

		// 1. 登る・下りる入力がある時だけ上下移動と吸い寄せを行う
		if (moveVertical != 0.0f) {
			Vector3 nextPos = position_;
			nextPos.y += moveVertical * walkSpeed_;

			if (!CheckCollision(nextPos, map))
			{
				position_.y = nextPos.y;

				// ★修正：ハシゴの芯に吸い寄せる際にも壁判定を行う！
				// これで横からハシゴに触れてもブロックにめり込みません
				Vector3 targetPosX = position_;
				targetPosX.x += (static_cast<float>(gx) - position_.x) * 0.6f;
				if (!CheckCollision(targetPosX, map)) position_.x = targetPosX.x;

				Vector3 targetPosZ = position_;
				targetPosZ.z += (static_cast<float>(gz) - position_.z) * 0.6f;
				if (!CheckCollision(targetPosZ, map)) position_.z = targetPosZ.z;
			}
			else if (moveVertical > 0.0f)
			{
				// ★登りきり：ハシゴ自体の向きを使って押し出す
				float exitAngle = (cellWaist ? cellWaist->rotationY : rotation_.y);

				float pushForward = 0.2f;
				Vector3 exitPos = position_;
				exitPos.x += std::sin(exitAngle) * pushForward;
				exitPos.z += std::cos(exitAngle) * pushForward;
				// 少し上に上げて床判定を確実に踏ませる
				exitPos.y += 0.1f;

				// 押し出し先にも壁がないか一応チェックしてから移動
				if (!CheckCollision(exitPos, map)) {
					position_ = exitPos;
				}
			}
		}
		else if (moveSide != 0.0f)
		{
			// 2. 上下入力がなく、左右入力がある場合（ハシゴからの離脱など）
			Vector3 nextPosX = position_;
			nextPosX.x += moveSide * 0.3f;
			if (!CheckCollision(nextPosX, map)) position_.x = nextPosX.x;
		}

		// ハシゴ中は接地扱いにしてジャンプなどを可能にする
		isGrounded_ = true;

#pragma endregion

	} 
	else
	{
		// --- 【通常移動モード】（ここが消えていたのでフリーズしていました） ---

		// 入力方向をカメラの回転に合わせる
		Vector3 inputDir = { 0, 0, 0 };
		if (input->PushKey(DIK_W)) inputDir.z += 0.5f;
		if (input->PushKey(DIK_S)) inputDir.z -= 0.5f;
		if (input->PushKey(DIK_A)) inputDir.x -= 0.5f;
		if (input->PushKey(DIK_D)) inputDir.x += 0.5f;

		if (inputDir.x != 0 || inputDir.z != 0) {
			// カメラのY軸回転に合わせて移動ベクトルを計算
			move.x = inputDir.x * std::cos(cameraRotY) + inputDir.z * std::sin(cameraRotY);
			move.z = -inputDir.x * std::sin(cameraRotY) + inputDir.z * std::cos(cameraRotY);

			//// 速度と向きを更新
			//move.x *= walkSpeed_;
			//move.z *= walkSpeed_;
			rotation_.y = std::atan2f(move.x, move.z);
		}

#pragma region 滑る足場

		// 1. 足元のブロックを特定
		int gx = static_cast<int>(std::floor(position_.x + 0.5f));
		int gyBelow = static_cast<int>(std::floor(position_.y - 0.1f)); // 足の少し下
		int gz = static_cast<int>(std::floor(position_.z + 0.5f));

		const MapCell* cellBelow = map.GetCell(gx, gyBelow, gz);
		bool isOnIce = (cellBelow && cellBelow->type == BlockType::IceBlock);

		// 2. 加速度と摩擦係数を決定
		float acceleration = isOnIce ? 0.01f : 0.08f; // 氷なら加速が鈍い
		float friction = isOnIce ? 0.98f : 0.7f;     // 氷なら速度が減りにくい（1.0に近いほど滑る）

		// 加速
		velocity_.x += move.x * acceleration;
		velocity_.z += move.z * acceleration;

		// 摩擦（減速）
		velocity_.x *= friction;
		velocity_.z *= friction;

#pragma endregion

#pragma region 動く足場

		// ▼ ▼ 追加：動く足場への追従処理 ▼ ▼
		// 足元より少し下の位置をチェック
		Vector3 footCheckPos = position_;
		// プレイヤーの足元よりほんの少し低い位置を判定するための座標
		float footCheckY = position_.y - 0.05f;
		// 左右の判定半径を少しだけ小さく（0.8倍に）することで、ギリギリの端っこに乗ったときのガタつきを防ぐ
		float footRadiusX = radius_.x * 0.8f;
		float footRadiusZ = radius_.z * 0.8f;
		float footRadiusY = 0.05f; // 足元チェック用の薄い判定ボックスの高さ

		const MapCell* ridingFloor = map.GetIntersectingMovingFloor(position_.x, footCheckY, position_.z, footRadiusX, footRadiusY, footRadiusZ);
		if (ridingFloor) {
			// 足場が動いた分（deltaOffset）だけ、プレイヤーの座標も強制的に動かす
			position_.x += ridingFloor->deltaOffsetX;
			position_.y += ridingFloor->deltaOffsetY;
			position_.z += ridingFloor->deltaOffsetZ;

			// 足場の上に乗っているので、接地フラグを立てて重力による落下速度をリセット
			isGrounded_ = true;
			velocity_.y = 0.0f;
		}
		// ▲ ▲ ここまで ▲ ▲

#pragma endregion

		// ジャンプ
		if (isGrounded_ && input->TriggerKey(DIK_SPACE)) {
			velocity_.y = 0.2f;
			isGrounded_ = false;
		}

		// 重力適用
		velocity_.y += gravity_;

		// X軸衝突判定
		Vector3 nextPosX = position_;
		nextPosX.x += velocity_.x;
		if (!CheckCollision(nextPosX, map)) position_.x = nextPosX.x;

		// Z軸衝突判定
		Vector3 nextPosZ = position_;
		nextPosZ.z += velocity_.z;
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

	CrumbleUpdate(map);
	PSwitchUpdate(map);
	DoorWarp(map);
	
	// ▼ 追加：鍵の取得チェック
	KeyUpdate(map);

	// --- 表示更新 ---
	object_->SetPosition(position_);
	object_->SetRotation(rotation_);
	object_->Update(lightVP);
}

// Object3d の行列を更新する（ライトカメラの行列も渡す）
void Player::UpdateTransform(const Matrix4x4& lightVP) 
{
	if (object_) {
		// 内部で持っている Object3d の行列計算だけを行う
		object_->Update(lightVP);
	}
}

// ドアに触れているか判定して、触れていてかつFキーがトリガーされたらワープする
void Player::DoorWarp(const StageMap& map)
{
	// 1. プレイヤーの足元のグリッド座標（整数）を計算
	int gx = static_cast<int>(std::floor(position_.x + 0.5f));
	int gyBottom = static_cast<int>(std::floor(position_.y + 0.1f));
	int gz = static_cast<int>(std::floor(position_.z + 0.5f));

	// 現在足元にあるセルを取得
	const MapCell* cell = map.GetCell(gx, gyBottom, gz);

	// 毎フレーム一度falseに戻す
	isNearDoor_ = false;

	// 足元がドアブロックだった場合
	if (cell && cell->type == BlockType::Door)
	{
		isNearDoor_ = true;

		// ドアの上に「Fキー」などのUIを出すワールド座標を設定
		nearDoorWorldPos_ = {
			static_cast<float>(gx),
			static_cast<float>(gyBottom) + 1.0f,
			static_cast<float>(gz)
		};

		// 【ワープ実行処理】
		// ドアの中にいて、Fキーが押され、かつ「ワープ直後フラグ」が立っていない場合のみ実行
		if (input_->TriggerKey(DIK_F) && !hasJustWarped_)
		{
			// 2. マップ全体から、自分（gx, gyBottom, gz）と同じドア番号(variant)を持つ相方の座標を検索
			// ※前回 StageMap に追加した関数を呼び出します
			Int3 destination = map.FindPairedDoor(gx, gyBottom, gz);

			// 3. 相方のドアが見つかった場合（検索結果が現在の座標と異なる場合）
			if (destination.x != gx || destination.y != gyBottom || destination.z != gz)
			{
				// ワープ先の座標を設定
				// ※ destination.y（ブロックの底面）にプレイヤーの足元がぴったり乗るよう、
				//    キノピオ隊長の着地位置として少しだけ高さを浮かせます（+0.1fなど環境に合わせて調整）
				position_.x = static_cast<float>(destination.x);
				position_.y = static_cast<float>(destination.y) + 0.1f;
				position_.z = static_cast<float>(destination.z);

				// ワープした衝撃で物理移動がバグらないよう、落下速度や移動慣性を完全にゼロにリセット
				velocity_ = { 0.0f, 0.0f, 0.0f };

				// ワープ直後フラグを立てる（これでこのフレームや次フレームでの連続誤作動を防ぐ）
				hasJustWarped_ = true;
			}
		}
	}
	else
	{
		// 4. ドアから完全に離れたら、再ワープ防止フラグをリセットする
		// これにより、別のドア（または一度離れて入り直した時）で再びワープができるようになります
		hasJustWarped_ = false;
	}
}

void Player::KeyUpdate(StageMap& map)
{
	// プレイヤーの中心付近（足元〜腰）のマスを取得
	int gx = static_cast<int>(std::floor(position_.x + 0.5f));
	int gy = static_cast<int>(std::floor(position_.y + 0.5f));
	int gz = static_cast<int>(std::floor(position_.z + 0.5f));

	MapCell* cell = map.GetCell(gx, gy, gz);

	// 触れているのが鍵だったら取得
	if (cell && cell->type == BlockType::Key) {
		hasKey_ = true;                 // 鍵を取得
		cell->type = BlockType::None;   // マップから鍵を消す
		cell->isSolid = false;
		map.RequestRebuild();           // ステージの見た目を再構築
	}
}

// 衝突判定ロジック
bool Player::CheckCollision(const Vector3& pos, StageMap& map) {
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
				MapCell* cell = map.GetCell(gx, gy, gz);

				// ▼ 追加：鍵ブロックの判定と破壊 ▼
				if (cell && cell->type == BlockType::KeyBlock) {
					if (hasKey_) {
						// 鍵を持っている場合は開ける（消費する）
						hasKey_ = false;
						// ★ 変更：1マスだけではなく、繋がっている塊をすべて消す
						map.RemoveConnectedKeyBlocks(gx, gy, gz);
						map.RequestRebuild();         // ステージの見た目を再構築

						// ブロックが消えたので、ここには壁が無いこととして判定を続ける
						continue;
					}
					else {
						// 鍵を持っていない場合は普通の壁として扱う
						return true;
					}
				}
				// ▲ ここまで ▲

				// ★ 変更：動く足場は固定グリッド判定から除外する
				if (cell && cell->isSolid && cell->type != BlockType::MovingFloor) {
					return true;
				}



				// 秋元追加 04/03
				if (cell && cell->type == BlockType::PBlock) {
					// PスイッチがONの時だけ「壁」として扱う
					if (map.IsPSwitchActive()) {
						return false;
					}
					return true; // OFFの時は通り抜けられる
				}
			}
		}
	}

	// ★ 追加：動いている足場のワールド座標判定（floatにバラして渡す）
	if (map.GetIntersectingMovingFloor(pos.x, pos.y, pos.z, radius_.x, radius_.y, radius_.z)) {
		return true;
	}

	return false;
}

// リスポーン処理：座標をリスポーンポイントに戻し、速度と回転をリセット
void Player::Respawn()
{
	position_ = respawnPosition;
	velocity_ = { 0.0f,0.0f,0.0f };
	rotation_ = { 0.0f,0.0f,0.0f };

	// ▼ 追加：リスポーン時は鍵を失う
	hasKey_ = false;
}

// Pスイッチの更新：足元のセルをチェックして、Pスイッチがあればマップに状態変更を通知
void Player::PSwitchUpdate(StageMap& map)
{
	// プレイヤーの中心座標から足元のインデックスを計算
	int gx = static_cast<int>(std::floor(position_.x + 0.5f));
	// 0.1fだと浮いている判定になりやすいので、少し余裕を持たせるか
	// 現在の座標(position_.y)の真下を正確に狙います
	int gyBottom = static_cast<int>(std::floor(position_.y + 0.1f));
	int gz = static_cast<int>(std::floor(position_.z + 0.5f));

	const MapCell* cellBelow = map.GetCell(gx, gyBottom, gz);

	if (input_->TriggerKey(DIK_F))
	{
		// Pスイッチの判定
		if (cellBelow && cellBelow->type == BlockType::PSwitch) {
			map.SetPSwitchActive(true); // これで needsRebuild_ が true になる
		}
	}
}

// 描画：内部で持っている Object3d を描画
void Player::Draw() {
	if (object_) {
		object_->Draw();
	}
}

// 影の描画：ライトカメラの行列を渡して影を描く
void Player::DrawShadow(const Matrix4x4& lightViewProjection) {
	// 自身が持っている 3Dオブジェクトの影用描画を呼ぶ
	if (object_) {
		object_->DrawShadow(lightViewProjection);
	}
}

void Player::DrawHighlight() {
	if (!object_) {
		return;
	}

	// 1回目：一番外側の大きい白
	object_->SetScale({ 1.55f, 1.55f, 1.55f });
	object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	object_->SetEnableLighting(false);
	object_->Draw();

	// 2回目：中間の白
	object_->SetScale({ 1.35f, 1.35f, 1.35f });
	object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	object_->SetEnableLighting(false);
	object_->Draw();

	// 3回目：本体に近い白
	object_->SetScale({ 1.18f, 1.18f, 1.18f });
	object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	object_->SetEnableLighting(false);
	object_->Draw();

	// 元に戻す
	object_->SetScale({ 1.0f, 1.0f, 1.0f });
	object_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	object_->SetEnableLighting(true);
}

void Player::CrumbleUpdate(StageMap& map) {
	int gx = static_cast<int>(std::floor(position_.x ));
	int gyBottom = static_cast<int>(std::floor(position_.y - 0.05f));
	int gz = static_cast<int>(std::floor(position_.z));

	MapCell* cellBelow = map.GetCell(gx, gyBottom, gz);

	if (cellBelow && cellBelow->type == BlockType::CrumblingFloor && !cellBelow->isHidden) {
		cellBelow->isCrumbling = true;
	}
}