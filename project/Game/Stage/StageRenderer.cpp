#include "StageRenderer.h"
#include <cassert>



// 解放
StageRenderer::~StageRenderer() {
	Clear();

	delete groundModel_;
	groundModel_ = nullptr;

	delete wallModel_;
	wallModel_ = nullptr;

	delete bubbleModel_;
	bubbleModel_ = nullptr;

	delete goalModel_;
	goalModel_ = nullptr;

	delete ladderModel_;
	ladderModel_ = nullptr;

	delete doorModel_;
	doorModel_ = nullptr;
}

// ステージマップの内容に応じて、描画用オブジェクトを生成していくクラス
void StageRenderer::Initialize(Object3dCommon* object3dCommon) {
	assert(object3dCommon);
	object3dCommon_ = object3dCommon;

	// 地面モデル1設定
	groundModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/block",
		"block.obj",
		object3dCommon_->GetTextureManager()
	);

	// 壁モデル1設定
	wallModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);

	// はしごモデル設定
	ladderModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/ladder",
		"ladder.obj",
		object3dCommon_->GetTextureManager()
	);

	// シャボン玉モデル設定
	bubbleModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/soapBubbles",
		"soapBubbles.obj",
		object3dCommon_->GetTextureManager()
	);


	// ゴールモデル設定
	goalModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/star",
		"star.obj",
		object3dCommon_->GetTextureManager()
	);

	// ドアモデル設定
	doorModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/door",
		"door.obj",
		object3dCommon_->GetTextureManager()
	);

	// Pスイッチモデル設定
	pSwichModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/switch",
		"switch.obj",
		object3dCommon_->GetTextureManager()
	);

	// Pブロックモデル設定
	pBlockOnModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/block",
		"block.obj",
		object3dCommon_->GetTextureManager()
	);
}

// ステージマップの内容に応じて、描画用オブジェクトを生成していくクラス
void StageRenderer::BuildFromStageMap(const StageMap& stageMap) {
	// 既存のオブジェクトがあれば全て削除してから新しいオブジェクトを生成する
	Clear();

	// ステージマップの全セルを走査して、ブロックがある場所に対応するモデルのオブジェクトを生成していく
	for (int y = 0; y < stageMap.GetHeight(); y++) {
		// 深さ方向もループして、全てのセルをチェック
		for (int z = 0; z < stageMap.GetDepth(); z++) {
			// 横方向のループ
			for (int x = 0; x < stageMap.GetWidth(); x++) {
				// ステージマップからセルの情報を取得
				const MapCell* cell = stageMap.GetCell(x, y, z);
				// セルが存在しない（範囲外）場合はスキップ
				if (!cell) {
					// 範囲外のセルは無視
					continue;
				}

				// セルのタイプが None（空）ならスキップ
				if (cell->type == BlockType::None) {
					// 空のセルは描画しない
					continue;
				}

				// ブロックがあるセルに対して、ブロックの種類に応じたモデルのオブジェクトを生成
				Vector3 position = {
					// ステージマップのセルの位置をワールド座標に変換してオブジェクトの位置とする
					static_cast<float>(x),
					static_cast<float>(y),
					static_cast<float>(z)
				};

				// ブロックの種類に応じて、対応するモデルを使ってオブジェクトを生成
				switch (cell->type) {
				// ブロックの種類が Ground（地面）の場合
				case BlockType::Ground:
				CreateStageObject(
					groundModel_,
					position,
					blockScale_,
					{ 0.0f, 0.0f, 0.0f }
				);
				break;

				// ブロックの種類が Wall（壁）の場合
				case BlockType::Wall:
				CreateStageObject(
					wallModel_,
					position,
					blockScale_,
					{ 0.0f, 0.0f, 0.0f }
				);
				break;

				// ブロックの種類が Ladder（はしご）の場合
				case BlockType::Ladder:
				CreateStageObject(
					ladderModel_,
					position,
					blockScale_,
					{ 0.0f, cell->rotationY, 0.0f }
				);
				break;

				// ブロックの種類が BubblePickup（泡の回収アイテム）の場合
				case BlockType::BubblePickup:
				CreateStageObject(
					bubbleModel_,
					position,
					{ blockScale_.x * 0.7f, blockScale_.y * 0.7f, blockScale_.z * 0.7f },
					{ 0.0f, 0.0f, 0.0f }
				);
				break;

				// ブロックの種類が Goal（ゴール）の場合
				case BlockType::Goal:
				CreateStageObject(
					goalModel_,
					position,
					{ blockScale_.x * 0.8f, blockScale_.y * 0.8f, blockScale_.z * 0.8f },
					{ 0.0f, 0.0f, 0.0f }
				);
				break;

				// ブロックの種類が Star（仮のアイテム）の場合
				case BlockType::Star:
				CreateStageObject(
					wallModel_,
					position,
					blockScale_,
					{ 0.0f, 0.0f, 0.0f }
				);
				break;

				// ブロックの種類が PlayerStart（プレイヤーの開始位置）の場合
				case BlockType::PlayerStart:
				CreateStageObject(
					goalModel_,
					position,
					{ 0.6f, 0.6f, 0.6f },
					{ 0.0f, 0.0f, 0.0f }
				);
				break;
				
				// ブロックの種類が Door (ドア) の場合
				case BlockType::Door:
					CreateStageObject(
						doorModel_,
						position,
						{ 0.6f, 0.6f, 0.6f },
						{ 0.0f, 0.0f, 0.0f }
					);
					break;
					// ブロックの種類が PSwitch(Pスイッチ) の場合
				case BlockType::PSwitch:
					if (!stageMap.IsPSwitchActive())
					{
						CreateStageObject(
							pSwichModel_,
							position,
							{ 0.6f, 0.6f, 0.6f },
							{ 0.0f, 0.0f, 0.0f }
						);
					}
					break;
					// ブロックの種類が PBlock (Pブロック) の場合
				case BlockType::PBlock:
					if (stageMap.IsPSwitchActive()) {
						CreateStageObject(
							pBlockOnModel_,
							position,
							{ 0.6f, 0.6f, 0.6f },
							{ 0.0f, 0.0f, 0.0f }
						);
					}
					else
					{
						CreateStageObject(
							wallModel_,
							position,
							blockScale_,
							{ 0.0f, 0.0f, 0.0f }
						);
					}
					break;

				// ブロックの種類が不明な場合は何もしない
				default:
				break;
				}
			}
		}
	}
}

// カメラ設定を全てのオブジェクトに伝える
void StageRenderer::SetCamera(const Matrix4x4& view, const Matrix4x4& projection) {
	// 全てのオブジェクトに対して、カメラのビュー行列とプロジェクション行列を設定する
	for (Object3d* obj : objects_) {
		obj->SetCamera(view, projection);
	}
}

// 全てのオブジェクトの更新処理を呼び出す
void StageRenderer::Update(const Matrix4x4& lightVP) {
	for (Object3d* obj : objects_) {
		if (obj) {
			obj->Update(lightVP);
		}
	}
}

// 全てのオブジェクトの描画処理を呼び出す
void StageRenderer::Draw() {
	for (Object3d* obj : objects_) {
		obj->Draw();
	}
}

// 既存のオブジェクトを全て削除してリストをクリアする
void StageRenderer::Clear() {
	for (Object3d* obj : objects_) {
		delete obj;
	}
	objects_.clear();
}

// 指定したモデルと位置・スケール・回転を使ってオブジェクトを生成し、リストに追加して返す
Object3d* StageRenderer::CreateStageObject(
	Model* model,
	// ブロックの位置（ステージマップのセルの位置をワールド座標に変換したもの）
	const Vector3& position,
	const Vector3& scale,
	const Vector3& rotation
	// ブロックの回転（今回は全て0でいいと思う）
) {
	Object3d* obj = new Object3d();
	obj->Initialize(object3dCommon_);
	obj->SetModel(model);
	obj->SetPosition(position);
	obj->SetScale(scale);
	obj->SetRotation(rotation);

	objects_.push_back(obj);
	return obj;
}