#include "StageRenderer.h"
#include <cassert>

// 解放
StageRenderer::~StageRenderer() {
	Clear();
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
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);

	// 崩れる足場
	crumbleModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/CollapsedBlocks",
		"CollapsedBlocks.obj",
		object3dCommon_->GetTextureManager()
	);
	// 滑る足場
	iceBlockModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/iceBlock",
		"iceBlock.obj",
		object3dCommon_->GetTextureManager()
	);
	// 動く足場
	movingFloorModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);
	// ▼ 追加：鍵モデル設定
	keyModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/star",
		"star.obj",
		object3dCommon_->GetTextureManager()
	);

	// ▼ 追加：鍵ブロックモデル設定
	keyBlockModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);
}

void StageRenderer::UpdateEffect(const StageMap& stageMap) {
	size_t objIndex = 0;
	for (int y = 0; y < stageMap.GetHeight(); ++y) {
		for (int z = 0; z < stageMap.GetDepth(); ++z) {
			for (int x = 0; x < stageMap.GetWidth(); ++x) {
				const MapCell* cell = stageMap.GetCell(x, y, z);
				if (cell->type == BlockType::None) continue;

				if (objIndex < objects_.size()) {
					Object3d* obj = objects_[objIndex].get();
					if (cell->type == BlockType::CrumblingFloor) {
						// マップデータの色と透明度をモデルに反映
						obj->SetColor({ 1.0f, cell->colorG, cell->colorB, cell->opacity });
					}
					objIndex++;
				}
			}
		}
	}
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
				if (cell->type == BlockType::None || cell->isHidden) {
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
					groundModel_.get(),
					position,
					blockScale_,
					{ 0.0f, 0.0f, 0.0f },
					BlockType::Ground
				);
				break;

				// ブロックの種類が Wall（壁）の場合
				case BlockType::Wall:
				{
					Object3d* obj = CreateStageObject(
						wallModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::Wall
					);
					if (cell->variant >= 1 && cell->variant <= 5) {
						const auto* part = stageMap.GetCustomPart(cell->variant);
						if (part) {
							obj->SetColor({ part->colorR, part->colorG, part->colorB, 1.0f });
						}
					} else if (cell->variant == 6) {
						// プレイヤー設置の Wall (可愛いライトレッド)
						obj->SetColor({ 1.0f, 0.4f, 0.4f, 1.0f });
					}
				}
				break;

				// ブロックの種類が Ladder（はしご）の場合
				case BlockType::Ladder:
				{
					Object3d* obj = CreateStageObject(
						ladderModel_.get(),
						position,
						blockScale_,
						{ 0.0f, cell->rotationY, 0.0f },
						BlockType::Ladder
					);
					if (cell->variant >= 1 && cell->variant <= 5) {
						const auto* part = stageMap.GetCustomPart(cell->variant);
						if (part) {
							obj->SetColor({ part->colorR, part->colorG, part->colorB, 1.0f });
						}
					} else if (cell->variant == 7) {
						// プレイヤー設置の Ladder (可愛いライトグリーン)
						obj->SetColor({ 0.4f, 1.0f, 0.4f, 1.0f });
					}
				}
				break;

				// ブロックの種類が BubblePickup（泡の回収アイテム）の場合
				case BlockType::BubblePickup:
				{
					Object3d* obj = CreateStageObject(
						bubbleModel_.get(),
						position,
						{ blockScale_.x * 0.7f, blockScale_.y * 0.7f, blockScale_.z * 0.7f },
						{ 0.0f, 0.0f, 0.0f },
						BlockType::BubblePickup
					);
					int insideCustomId = UnpackBubbleCustomId(cell->variant);
					BlockType insideType = UnpackBubbleType(cell->variant);

					if (insideCustomId >= 1 && insideCustomId <= 5) {
						const auto* part = stageMap.GetCustomPart(insideCustomId);
						if (part) {
							obj->SetColor({ part->colorR, part->colorG, part->colorB, 0.8f });
						}
					} else {
						if (insideType == BlockType::Wall) {
							obj->SetColor({ 1.0f, 0.5f, 0.5f, 0.8f });
						} else if (insideType == BlockType::Ladder) {
							obj->SetColor({ 0.5f, 1.0f, 0.5f, 0.8f });
						}
					}
				}
				break;

				// ブロックの種類が Goal（ゴール）の場合
				case BlockType::Goal:
				CreateStageObject(
					goalModel_.get(),
					position,
					{ blockScale_.x * 0.8f, blockScale_.y * 0.8f, blockScale_.z * 0.8f },
					{ 0.0f, 0.0f, 0.0f },
					BlockType::Goal
				);
				break;

				// ブロックの種類が Star（仮のアイテム）の場合
				case BlockType::Star:
				CreateStageObject(
					wallModel_.get(),
					position,
					blockScale_,
					{ 0.0f, 0.0f, 0.0f },
					BlockType::Star
				);
				break;

				// ブロックの種類が PlayerStart（プレイヤーの開始位置）の場合
				case BlockType::PlayerStart:
				{
					Object3d* pObj = CreateStageObject(
						groundModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::PlayerStart
					);
					pObj->SetColor({ 1.0f, 0.0f, 0.0f, 1.0f });
				}
				break;
				
				// ブロックの種類が Door (ドア) の場合
				case BlockType::Door:
					CreateStageObject(
						doorModel_.get(),
						position,
						{ 0.6f, 0.6f, 0.6f },
						{ 0.0f, 0.0f, 0.0f },
						BlockType::Door
					);
					break;
					// ブロックの種類が PSwitch(Pスイッチ) の場合
				case BlockType::PSwitch:
					if (!stageMap.IsPSwitchActive())
					{
						CreateStageObject(
							pSwichModel_.get(),
							position,
							{ 0.6f, 0.6f, 0.6f },
							{ 0.0f, 0.0f, 0.0f },
							BlockType::PSwitch
						);
					}
					break;
					// ブロックの種類が PBlock (Pブロック) の場合
				case BlockType::PBlock:
					if (!stageMap.IsPSwitchActive()) {
						CreateStageObject(
							pBlockOnModel_.get(),
							position,
							blockScale_,
							{ 0.0f, 0.0f, 0.0f },
							BlockType::PBlock
						);
					}
					break;

				case BlockType::CrumblingFloor:
					CreateStageObject(
						crumbleModel_.get(), 
						position,
						blockScale_, 
						{ 0.0f, 0.0f, 0.0f },
						BlockType::CrumblingFloor
					);
					break;
					// ブロックの種類が IceBlock（滑る足場）の場合
				case BlockType::IceBlock:
					CreateStageObject(
						iceBlockModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::IceBlock
					);
					break;
					// ブロックの種類が MovingFloor（動く足場）の場合
				case BlockType::MovingFloor:
				{
					// 1. 3Dオブジェクトを生成 (既存の他のブロックと同様の生成処理)
					Object3d* newObj = CreateStageObject(
						movingFloorModel_.get(),
						position,
						blockScale_,
						{ 0.0f, cell->rotationY, 0.0f },
						BlockType::MovingFloor
					);

					// 2. 生成に成功したら、更新用のリストに「オブジェクト」と「セルのインデックス」を記録
					if (newObj) {
						MovingFloorInstance instance;
						instance.object = newObj;
						instance.cellIndex = { x, y, z }; // 現在ループで走査中の [x, y, z]
						movingFloorInstances_.push_back(instance);
					}
				}
					break;
					// ▼ 追加：鍵の場合
				case BlockType::Key:
					CreateStageObject(
						keyModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f } // 必要に応じて回転
					);
					break;

					// ▼ 追加：鍵ブロックの場合
				case BlockType::KeyBlock:
					CreateStageObject(
						keyBlockModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f }
					);
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
	for (const auto& obj : objects_) {
		obj->SetCamera(view, projection);
	}
	for (const auto& obj : previewObjects_) {
		obj->SetCamera(view, projection);
	}
}

// 全てのオブジェクトの更新処理を呼び出す
void StageRenderer::Update(const StageMap& stageMap, const Matrix4x4& lightVP) {
	// ▼ 追加：動く足場の位置を StageMap の計算結果と同期させる
	for (auto& instance : movingFloorInstances_) {
		// マップから対応するセルのデータを取得
		const MapCell* cell = stageMap.GetCell(instance.cellIndex.x, instance.cellIndex.y, instance.cellIndex.z);

		if (cell && cell->type == BlockType::MovingFloor) {
			// エディタで配置した時のベース座標（グリッド座標からワールド座標に変換）
			Vector3 basePosition = {
				static_cast<float>(instance.cellIndex.x) * blockScale_.x,
				static_cast<float>(instance.cellIndex.y) * blockScale_.y,
				static_cast<float>(instance.cellIndex.z) * blockScale_.z
			};

			// ベース座標に、StageMap.cpp の Update で計算された滑らかなオフセット（currentOffset）を加算する
			Vector3 newPosition = {
				basePosition.x + (cell->currentOffsetX * blockScale_.x),
				basePosition.y + (cell->currentOffsetY * blockScale_.y),
				basePosition.z + (cell->currentOffsetZ * blockScale_.z)
			};

			// 3Dオブジェクトの座標を更新
			instance.object->SetPosition(newPosition);
		}
	}

	// 全てのメインオブジェクトに対して、更新処理を呼び出す
	for (const auto& obj : objects_) {
		if (obj) {
			obj->Update(lightVP);
		}
	}

	// 全てのプレビューオブジェクトに対して、更新処理を呼び出す
	for (const auto& obj : previewObjects_) {
		if (obj) {
			obj->Update(lightVP);
		}
	}
}

// 全てのオブジェクトの影描画処理を呼び出す
void StageRenderer::DrawShadow(const Matrix4x4& lightVP) {
	for (const auto& obj : objects_) {
		if (obj) {
			obj->DrawShadow(lightVP);
		}
	}
}

// 全てのオブジェクトの描画処理を呼び出す
void StageRenderer::Draw() {
	for (const auto& obj : objects_) {
		obj->Draw();
	}
	// 🌟 半透明プレビューを描画
	for (const auto& obj : previewObjects_) {
		obj->Draw();
	}
}

// 既存のオブジェクトを全て削除してリストをクリアする
void StageRenderer::Clear() {
	objects_.clear();
	previewObjects_.clear(); // 🌟 プレビューも一緒にクリア
	movingFloorInstances_.clear(); // ★追加：動く足場の管理リストもクリアしてダングリングポインタを防ぐ
}

// 🌟 配置プレビュー表示機能の実装
void StageRenderer::SetPlacementPreview(
	const StageMap& stageMap,
	const Int3& cursorIndex,
	BlockType type,
	int customId
) {
	previewObjects_.clear(); // 既存のプレビューをリセット

	float colorR = 1.0f;
	float colorG = 1.0f;
	float colorB = 1.0f;
	Model* targetModel = wallModel_.get();

	if (customId >= 1 && customId <= 5) {
		const auto* part = stageMap.GetCustomPart(customId);
		if (part) {
			colorR = part->colorR;
			colorG = part->colorG;
			colorB = part->colorB;
			targetModel = (part->baseType == BlockType::Ladder) ? ladderModel_.get() : wallModel_.get();
		}
	} else {
		// 通常ブロックのテーマカラー
		if (type == BlockType::Wall) {
			colorR = 1.0f; colorG = 0.4f; colorB = 0.4f;
			targetModel = wallModel_.get();
		} else if (type == BlockType::Ladder) {
			colorR = 0.4f; colorG = 1.0f; colorB = 0.4f;
			targetModel = ladderModel_.get();
		} else if (type == BlockType::Ground) {
			colorR = 0.7f; colorG = 0.7f; colorB = 0.7f;
			targetModel = groundModel_.get();
		} else if (type == BlockType::IceBlock) {
			colorR = 0.5f; colorG = 0.85f; colorB = 1.0f; // 美しいアイスブルー
			targetModel = iceBlockModel_.get();
		} else if (type == BlockType::MovingFloor) {
			colorR = 0.9f; colorG = 0.65f; colorB = 0.4f; // オレンジプレート
			targetModel = movingFloorModel_.get();
		} else if (type == BlockType::CrumblingFloor) {
			colorR = 0.8f; colorG = 0.6f; colorB = 0.4f;   // ボロボロのブロック色
			targetModel = crumbleModel_.get();
		} else {
			return; // プレビュー対象外
		}
	}

	// 🌟 3x3x3 複合カスタムアセンブリプレビューの作成
	if (customId >= 1 && customId <= 5) {
		const auto* part = stageMap.GetCustomPart(customId);
		if (part && !part->IsEmpty()) {
			for (int ly = 0; ly < 3; ++ly) {
				for (int lz = 0; lz < 3; ++lz) {
					for (int lx = 0; lx < 3; ++lx) {
						const auto& cell = part->cells[ly][lz][lx];
						if (cell.type == BlockType::None) continue;

						Vector3 pos = {
							static_cast<float>(cursorIndex.x + lx),
							static_cast<float>(cursorIndex.y + ly),
							static_cast<float>(cursorIndex.z + lz)
						};
						Model* cellModel = (cell.type == BlockType::Ladder) ? ladderModel_.get() : wallModel_.get();

						auto obj = std::make_unique<Object3d>();
						obj->Initialize(object3dCommon_);
						obj->SetModel(cellModel);
						obj->SetPosition(pos);
						obj->SetScale(blockScale_);
						obj->SetColor({ colorR, colorG, colorB, 0.4f }); // 40%の美しい半透明

						previewObjects_.push_back(std::move(obj));
					}
				}
			}
			return;
		}
	}

	// 🌟 通常の 1 マスプレビューの作成
	Vector3 pos = {
		static_cast<float>(cursorIndex.x),
		static_cast<float>(cursorIndex.y),
		static_cast<float>(cursorIndex.z)
	};
	auto obj = std::make_unique<Object3d>();
	obj->Initialize(object3dCommon_);
	obj->SetModel(targetModel);
	obj->SetPosition(pos);
	obj->SetScale(blockScale_);
	obj->SetColor({ colorR, colorG, colorB, 0.4f }); // 40%の美しい半透明
	previewObjects_.push_back(std::move(obj));
}

void StageRenderer::ClearPlacementPreview() {
	previewObjects_.clear();
	// ▼ 追加：動く足場の管理リストをクリア (Object3d自体は objects_ 側で解放されるため clear だけでOK)
	movingFloorInstances_.clear();
}

// 指定したモデルと位置・スケール・回転を使ってオブジェクトを生成し、リストに追加して返す
Object3d* StageRenderer::CreateStageObject(
	Model* model,
	// ブロックの位置（ステージマップ of セルの位置をワールド座標に変換したもの）
	const Vector3& position,
	const Vector3& scale,
	const Vector3& rotation,
	BlockType type
) {
	auto obj = std::make_unique<Object3d>();
	obj->Initialize(object3dCommon_);
	obj->SetModel(model);
	obj->SetPosition(position);
	obj->SetScale(scale);
	obj->SetRotation(rotation);
	
	// 高品質マイクロマテリアル設定の自動適用
	switch (type) {
	case BlockType::Ground:
		obj->SetShininess(0.3f);
		obj->SetMetallic(0.0f);
		obj->SetEmissive(0.0f);
		break;
	case BlockType::Wall:
		obj->SetShininess(0.4f);
		obj->SetMetallic(0.0f);
		obj->SetEmissive(0.0f);
		break;
	case BlockType::Ladder:
		obj->SetShininess(0.5f);
		obj->SetMetallic(0.2f);
		obj->SetEmissive(0.0f);
		break;
	case BlockType::IceBlock:
		obj->SetShininess(0.95f); // 氷ならではの鋭く美しいハイライト
		obj->SetMetallic(0.7f);   // 氷ならではの鏡面感のある反射
		obj->SetEmissive(0.1f);   // ほんのりと輝く氷の質感
		break;
	case BlockType::Goal:
	case BlockType::Star:
		obj->SetShininess(0.8f);
		obj->SetMetallic(0.5f);
		obj->SetEmissive(0.7f);   // ゴールやスターは幻想的に自己発光
		break;
	case BlockType::CrumblingFloor:
		obj->SetShininess(0.15f); // 崩れそうなボロボロの床（マットでザラザラした質感）
		obj->SetMetallic(0.0f);
		obj->SetEmissive(0.0f);
		break;
	case BlockType::MovingFloor:
		obj->SetShininess(0.7f);  // 重厚な金属・石の質感
		obj->SetMetallic(0.4f);
		obj->SetEmissive(0.0f);
		break;
	case BlockType::PSwitch:
	case BlockType::PBlock:
		obj->SetShininess(0.8f);
		obj->SetMetallic(0.3f);
		obj->SetEmissive(0.4f);   // スイッチ系は軽く自己発光して目立たせる
		break;
	case BlockType::BubblePickup:
		obj->SetShininess(0.9f);  // シャボン玉の透明で滑らかなハイライト
		obj->SetMetallic(0.1f);
		obj->SetEmissive(0.3f);   // 内部の輝きを表現
		break;
	default:
		break;
	}

	Object3d* ptr = obj.get();
	objects_.push_back(std::move(obj));
	return ptr;
}