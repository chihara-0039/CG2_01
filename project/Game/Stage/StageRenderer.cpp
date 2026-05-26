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
		"Resources/Models/key",
		"key.obj",
		object3dCommon_->GetTextureManager()
	);

	// ▼ 追加：鍵ブロックモデル設定
	keyBlockModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/wall",
		"wall.obj",
		object3dCommon_->GetTextureManager()
	);

	// ▼ 追加：トゲモデル設定
	spikeModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/spike",
		"spike.obj",
		object3dCommon_->GetTextureManager()
	);


	// インスタンシング用の ViewProjection 定数バッファを作成
	D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = (sizeof(ViewProjectionMatrix) + 0xff) & ~0xff;
	resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

	HRESULT hr = object3dCommon_->GetDxCommon()->GetDevice()->CreateCommittedResource(
		&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
		IID_PPV_ARGS(&viewProjectionResource_)
	);
	assert(SUCCEEDED(hr));
	viewProjectionResource_->Map(0, nullptr, (void**)&viewProjectionData_);
}

void StageRenderer::UpdateEffect(const StageMap& stageMap) {
	size_t objIndex = 0;

	for (int y = 0; y < stageMap.GetHeight(); ++y) {
		for (int z = 0; z < stageMap.GetDepth(); ++z) {
			for (int x = 0; x < stageMap.GetWidth(); ++x) {

				const MapCell* cell = stageMap.GetCell(x, y, z);
				if (!cell || cell->type == BlockType::None) {
					continue;
				}

				if (objIndex >= objects_.size()) {
					return;
				}

				Object3d* obj = objects_[objIndex].get();

				if (cell->type == BlockType::CrumblingFloor) {
					obj->SetColor({
						1.0f,
						cell->colorG,
						cell->colorB,
						cell->opacity
					});
					// 崩れる床の色・透明度更新によりDirty化
					MarkDirty(obj);
				}

				objIndex++;
			}
		}
	}
}

// ステージマップの内容に応じて、描画用オブジェクトを生成していくクラス
void StageRenderer::BuildFromStageMap(const StageMap& stageMap) {
	// 既存のオブジェクトがあれば全て削除してから新しいオブジェクトを生成する
	Clear();

	// 雲の生成
	int mapWidth = stageMap.GetWidth();
	int mapHeight = stageMap.GetHeight();
	int mapDepth = stageMap.GetDepth();
	float scaleX = blockScale_.x;
	float scaleY = blockScale_.y;
	float scaleZ = blockScale_.z;

	int cloudCount = 12; // 12個浮かべる
	for (int i = 0; i < cloudCount; ++i) {
		CloudInstance cloud;
		// ランダムな位置 (ステージの少し上空、周囲)
		float rx = (static_cast<float>(rand()) / RAND_MAX) * (mapWidth * scaleX + 80.0f) - 40.0f;
		float ry = (static_cast<float>(rand()) / RAND_MAX) * 6.0f + 3.0f; // 3.0f 〜 9.0f の高さ
		float rz = (static_cast<float>(rand()) / RAND_MAX) * (mapDepth * scaleZ + 80.0f) - 40.0f;
		cloud.basePosition = { rx, ry, rz };

		// 流れる速度 (X軸方向へゆっくり流れる)
		float speedX = (static_cast<float>(rand()) / RAND_MAX) * 0.4f + 0.1f;
		cloud.speed = { speedX, 0.0f, 0.0f };

		// フワフワパラメータ
		cloud.floatTimer = (static_cast<float>(rand()) / RAND_MAX) * 6.28f;
		cloud.floatSpeed = (static_cast<float>(rand()) / RAND_MAX) * 0.3f + 0.1f;

		// 1つの雲を構成する球体数 (3〜5個)
		int partCount = rand() % 3 + 3;
		for (int j = 0; j < partCount; ++j) {
			// 中心からのオフセット
			float ox = (static_cast<float>(rand()) / RAND_MAX) * 4.0f - 2.0f;
			float oy = (static_cast<float>(rand()) / RAND_MAX) * 1.5f - 0.75f;
			float oz = (static_cast<float>(rand()) / RAND_MAX) * 4.0f - 2.0f;
			cloud.localOffsets.push_back({ ox, oy, oz });

			// ランダムスケール
			float s = (static_cast<float>(rand()) / RAND_MAX) * 2.0f + 1.5f;
			cloud.localScales.push_back({ s, s * 0.5f, s }); // 雲らしく少し平べったくする
		}

		// 3Dオブジェクトの作成
		for (int j = 0; j < partCount; ++j) {
			std::unique_ptr<Object3d> obj = std::make_unique<Object3d>();
			obj->Initialize(object3dCommon_);
			obj->SetModel(bubbleModel_.get()); // 球体モデルを雲のパーツとして使用
			obj->SetEnableLighting(true);       // ライティングで立体感（ローポリ雲の綺麗な陰影）を出す
			obj->SetShininess(0.0f);            // テカらせない
			obj->SetMetallic(0.0f);             // テカらせない
			
			// 初期位置とスケール
			Vector3 pos = {
				cloud.basePosition.x + cloud.localOffsets[j].x,
				cloud.basePosition.y + cloud.localOffsets[j].y,
				cloud.basePosition.z + cloud.localOffsets[j].z
			};
			obj->SetPosition(pos);
			obj->SetScale(cloud.localScales[j]);
			
			// 色を完全な白に設定
			obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

			cloud.objects.push_back(std::move(obj));
		}

		clouds_.push_back(std::move(cloud));
	}


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
				{
					Vector3 scale = { 0.6f, 0.6f, 0.6f };

					Object3d* obj = CreateStageObject(
						pSwichModel_.get(),
						position,
						scale,
						{ 0.0f, 0.0f, 0.0f },
						BlockType::PSwitch
					);

					if (obj) {
						pSwitchObjects_.push_back({ obj, scale });
					}
				}
				break;

				case BlockType::PBlock:
				case BlockType::PBlockAppears:
				{
					Object3d* obj = CreateStageObject(
						pBlockOnModel_.get(),
						position,
						blockScale_,
						{ 0.0f, 0.0f, 0.0f },
						cell->type
					);

					if (obj) {
						if (!cell->isSolid) {
							// 押されて消えている状態（すり抜ける状態）は青色で半透明にする
							// ※マテリアルのアルファブレンドが有効になっている必要があります
							obj->SetColor({ 0.3f, 0.3f, 0.8f, 0.4f });
						}
						else {
							// 実体化している状態は元の色
							obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
						}

						// リストで管理している場合は追加
						if (cell->type == BlockType::PBlock) {
							pBlockObjects_.push_back({ obj, blockScale_ });
						}
					}
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

				case BlockType::Spike:
					{
						CreateStageObject(
							spikeModel_.get(),
							position,
							blockScale_,
							{ 0.0f, 0.0f, 0.0f },
							BlockType::Spike
						);
					}
					break;


				case BlockType::EnemyWalker:
					{
						Object3d* obj = CreateStageObject(
							bubbleModel_.get(), // 球体モデルを流用
							position,
							{ blockScale_.x * 0.6f, blockScale_.y * 0.6f, blockScale_.z * 0.6f },
							{ 0.0f, 0.0f, 0.0f },
							BlockType::EnemyWalker
						);
						if (obj) {
							obj->SetColor({ 0.7f, 0.1f, 0.7f, 1.0f }); // 紫色の敵
							EnemyInstance inst;
							inst.object = obj;
							inst.cellIndex = { x, y, z };
							enemyInstances_.push_back(inst);
						}
					}
					break;

				case BlockType::EnemyFlyer:
					{
						Object3d* obj = CreateStageObject(
							bubbleModel_.get(),
							position,
							{ blockScale_.x * 0.6f, blockScale_.y * 0.6f, blockScale_.z * 0.6f },
							{ 0.0f, 0.0f, 0.0f },
							BlockType::EnemyFlyer
						);
						if (obj) {
							obj->SetColor({ 0.8f, 0.8f, 0.1f, 1.0f }); // 黄色の敵
							EnemyInstance inst;
							inst.object = obj;
							inst.cellIndex = { x, y, z };
							enemyInstances_.push_back(inst);
						}
					}
					break;

				case BlockType::EnemyChaser:
					{
						Object3d* obj = CreateStageObject(
							bubbleModel_.get(),
							position,
							{ blockScale_.x * 0.6f, blockScale_.y * 0.6f, blockScale_.z * 0.6f },
							{ 0.0f, 0.0f, 0.0f },
							BlockType::EnemyChaser
						);
						if (obj) {
							obj->SetColor({ 0.1f, 0.8f, 0.8f, 1.0f }); // シアンの敵
							EnemyInstance inst;
							inst.object = obj;
							inst.cellIndex = { x, y, z };
							enemyInstances_.push_back(inst);
						}
					}
					break;
				// ブロックの種類が不明な場合は何もしない
				default:
				break;
				}
			}
		}
	}
	// リビルド後にインスタンス描画グループを再構築する
	BuildRenderGroups();
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
	for (const auto& cloud : clouds_) {
		for (const auto& obj : cloud.objects) {
			obj->SetCamera(view, projection);
		}
	}
}

// 全てのオブジェクトの更新処理を呼び出す
void StageRenderer::Update(const StageMap& stageMap, const Matrix4x4& lightVP) {
	lastLightVP_ = lightVP;

	for (auto& instance : movingFloorInstances_) {
		const MapCell* cell = stageMap.GetCell(instance.cellIndex.x, instance.cellIndex.y, instance.cellIndex.z);
		if (cell && cell->type == BlockType::MovingFloor) {
			Vector3 basePosition = {
				static_cast<float>(instance.cellIndex.x) * blockScale_.x,
				static_cast<float>(instance.cellIndex.y) * blockScale_.y,
				static_cast<float>(instance.cellIndex.z) * blockScale_.z
			};

			Vector3 newPosition = {
				basePosition.x + (cell->currentOffsetX * blockScale_.x),
				basePosition.y + (cell->currentOffsetY * blockScale_.y),
				basePosition.z + (cell->currentOffsetZ * blockScale_.z)
			};

			instance.object->SetPosition(newPosition);
			instance.object->Update(lightVP); // 動く床のみ行列を更新
			// 更新した動く床のインスタンスデータをDirty化
			MarkDirty(instance.object);
		}
	}

	for (auto& instance : enemyInstances_) {
		const MapCell* cell = stageMap.GetCell(instance.cellIndex.x, instance.cellIndex.y, instance.cellIndex.z);
		if (cell && (cell->type == BlockType::EnemyWalker || cell->type == BlockType::EnemyFlyer || cell->type == BlockType::EnemyChaser)) {
			Vector3 basePosition = {
				static_cast<float>(instance.cellIndex.x) * blockScale_.x,
				static_cast<float>(instance.cellIndex.y) * blockScale_.y,
				static_cast<float>(instance.cellIndex.z) * blockScale_.z
			};

			Vector3 newPosition = {
				basePosition.x + (cell->currentOffsetX * blockScale_.x),
				basePosition.y + (cell->currentOffsetY * blockScale_.y),
				basePosition.z + (cell->currentOffsetZ * blockScale_.z)
			};

			instance.object->SetPosition(newPosition);
			instance.object->Update(lightVP);
			MarkDirty(instance.object);
		}
	}

	// 雲の更新
	float dt = 1.0f / 60.0f;
	int mapWidth = stageMap.GetWidth();
	float scaleX = blockScale_.x;
	float rightLimit = mapWidth * scaleX + 50.0f;
	float leftLimit = -50.0f;

	for (auto& cloud : clouds_) {
		// X方向への移動
		cloud.basePosition.x += cloud.speed.x * dt;
		if (cloud.basePosition.x > rightLimit) {
			cloud.basePosition.x = leftLimit;
		}

		// Y方向のフワフワ運動
		cloud.floatTimer += cloud.floatSpeed * dt;
		float offsetY = std::sin(cloud.floatTimer) * 0.4f;

		// 各パーツ（球体）の位置を更新
		for (size_t i = 0; i < cloud.objects.size(); ++i) {
			Vector3 partPos = {
				cloud.basePosition.x + cloud.localOffsets[i].x,
				cloud.basePosition.y + cloud.localOffsets[i].y + offsetY,
				cloud.basePosition.z + cloud.localOffsets[i].z
			};
			cloud.objects[i]->SetPosition(partPos);
			
			// 雲の色を完全に白に設定 (陰影のみライティングで反映される)
			cloud.objects[i]->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

			cloud.objects[i]->Update(lightVP);
		}
	}
}

// 全てのオブジェクトの影描画処理を呼び出す
void StageRenderer::DrawShadow(const Matrix4x4& lightVP) {
	auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();
	if (!commandList) return;

	// インスタンシング用PSOの設定（影パス用）
	commandList->SetPipelineState(object3dCommon_->GetInstancedShadowPipelineState());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファの更新 (最初のグループからビュー、プロジェクション行列を取得)
	RenderGroup* firstGroup = nullptr;
	if (!renderGroups_.empty()) {
		firstGroup = &renderGroups_.front();
	} else if (!previewRenderGroups_.empty()) {
		firstGroup = &previewRenderGroups_.front();
	}

	if (firstGroup && !firstGroup->instances.empty() && viewProjectionData_) {
		Object3d* firstObj = firstGroup->instances.front().object;
		viewProjectionData_->viewProjection = Math::Multiply(firstObj->GetViewMatrix(), firstObj->GetProjectionMatrix());
		viewProjectionData_->lightViewProjection = lightVP;
	}

	// 1: ViewProjection
	commandList->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());

	// 描画処理を実行するラムダ関数 (Dirtyフラグ制御によるメモリ転送の最小化)
	auto drawGroups = [commandList, this](std::vector<RenderGroup>& groups) {
		for (auto& group : groups) {
			UINT numInstances = static_cast<UINT>(group.instances.size());
			if (numInstances == 0) continue;

			// Dirtyならキャッシュ内容をGPUバッファに転送する
			if (group.isDirty) {
				InstanceData* dataBegin = nullptr;
				HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);
				if (SUCCEEDED(hr)) {
					std::memcpy(dataBegin, group.instanceData.data(), sizeof(InstanceData) * numInstances);
					group.buffer->Unmap(0, nullptr);
				}
				group.isDirty = false; // 転送完了
			}

			// 5: InstanceBuffer (VS t2)
			commandList->SetGraphicsRootShaderResourceView(5, group.buffer->GetGPUVirtualAddress());

			// 頂点バッファをバインドして一括描画
			group.model->DrawInstanced(commandList, numInstances);
		}
	};

	drawGroups(renderGroups_);
	drawGroups(previewRenderGroups_);

	// 元の非インスタンシング影PSOに戻す
	commandList->SetPipelineState(object3dCommon_->GetShadowPipelineState());
}

// 全てのオブジェクトの描画処理を呼び出す
void StageRenderer::Draw() {
	auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();
	if (!commandList) return;

	// インスタンシング用のテクスチャ記述子ヒープの設定
	if (object3dCommon_->GetTextureManager()) {
		ID3D12DescriptorHeap* heaps[] = { object3dCommon_->GetTextureManager()->GetSrvHeap() };
		commandList->SetDescriptorHeaps(1, heaps);
	}

	commandList->SetPipelineState(object3dCommon_->GetInstancedPipelineState());
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// 定数バッファの更新 (最初のグループからビュー、プロジェクション行列を取得)
	RenderGroup* firstGroup = nullptr;
	if (!renderGroups_.empty()) {
		firstGroup = &renderGroups_.front();
	} else if (!previewRenderGroups_.empty()) {
		firstGroup = &previewRenderGroups_.front();
	}

	if (firstGroup && !firstGroup->instances.empty() && viewProjectionData_) {
		Object3d* firstObj = firstGroup->instances.front().object;
		viewProjectionData_->viewProjection = Math::Multiply(firstObj->GetViewMatrix(), firstObj->GetProjectionMatrix());
		viewProjectionData_->lightViewProjection = lastLightVP_;
	}

	// 1: ViewProjection (VS b0 にバインド)
	commandList->SetGraphicsRootConstantBufferView(1, viewProjectionResource_->GetGPUVirtualAddress());
	// 2: Light (PS b1 にバインド)
	commandList->SetGraphicsRootConstantBufferView(2, object3dCommon_->GetLightGPUVirtualAddress());

	// 描画処理を実行するラムダ関数 (Dirtyフラグ制御によるメモリ転送の最小化)
	auto drawGroups = [commandList, this](std::vector<RenderGroup>& groups) {
		for (auto& group : groups) {
			UINT numInstances = static_cast<UINT>(group.instances.size());
			if (numInstances == 0) continue;

			// Dirtyならキャッシュ内容をGPUバッファに転送する
			if (group.isDirty) {
				InstanceData* dataBegin = nullptr;
				HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);
				if (SUCCEEDED(hr)) {
					std::memcpy(dataBegin, group.instanceData.data(), sizeof(InstanceData) * numInstances);
					group.buffer->Unmap(0, nullptr);
				}
				group.isDirty = false; // 転送完了
			}

			// 3: Texture (PS t0)
			if (object3dCommon_->GetTextureManager()) {
				auto gpuHandle = object3dCommon_->GetTextureManager()->GetSrvHandleGPU(group.model->GetTextureHandle());
				commandList->SetGraphicsRootDescriptorTable(3, gpuHandle);
			}

			// 5: InstanceBuffer (VS t2)
			commandList->SetGraphicsRootShaderResourceView(5, group.buffer->GetGPUVirtualAddress());

			// 頂点バッファをバインドして一括描画
			group.model->DrawInstanced(commandList, numInstances);
		}
	};

	drawGroups(renderGroups_);
	drawGroups(previewRenderGroups_);

	// 元の非インスタンシングPSOに戻す
	commandList->SetPipelineState(object3dCommon_->GetPipelineState());

	// 雲の描画
	for (const auto& cloud : clouds_) {
		for (const auto& obj : cloud.objects) {
			obj->Draw();
		}
	}
}

ID3D12Resource* StageRenderer::GetOrCreateInstancedBuffer(Model* model, UINT numInstances) {
	auto& info = instancedBuffers_[model];
	if (!info.buffer || info.maxInstances < numInstances) {
		info.maxInstances = numInstances + 64;

		D3D12_HEAP_PROPERTIES heapProps = { D3D12_HEAP_TYPE_UPLOAD, D3D12_CPU_PAGE_PROPERTY_UNKNOWN, D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };
		D3D12_RESOURCE_DESC resDesc = {};
		resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		resDesc.Width = sizeof(InstanceData) * info.maxInstances;
		resDesc.Height = 1; resDesc.DepthOrArraySize = 1; resDesc.MipLevels = 1;
		resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; resDesc.SampleDesc.Count = 1;

		HRESULT hr = object3dCommon_->GetDxCommon()->GetDevice()->CreateCommittedResource(
			&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
			IID_PPV_ARGS(&info.buffer)
		);
		assert(SUCCEEDED(hr));
	}
	return info.buffer.Get();
}

// 既存のオブジェクトを全て削除してリストをクリアする
void StageRenderer::Clear() {
	objects_.clear();
	previewObjects_.clear(); // 🌟 プレビューも一緒にクリア
	movingFloorInstances_.clear(); // ★追加：動く足場の管理リストもクリアしてダングリングポインタを防ぐ
	enemyInstances_.clear(); // ★追加：敵の管理リストもクリア
	clouds_.clear(); // ★追加：背景雲のリストもクリア


	//5/19佐倉
	pSwitchObjects_.clear();
	pBlockObjects_.clear();

	// グループ管理データもクリア
	renderGroups_.clear();
	previewRenderGroups_.clear();
	objectToInstanceMap_.clear();
}

// 🌟 配置プレビュー表示機能の実装
void StageRenderer::SetPlacementPreview(
	const StageMap& stageMap,
	const Int3& cursorIndex,
	BlockType type,
	int customId,
	float rotationY
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
			int rotIndex = static_cast<int>(std::round(rotationY / 1.5707963f)) % 4;
			if (rotIndex < 0) rotIndex += 4;

			for (int ly = 0; ly < 3; ++ly) {
				for (int lz = 0; lz < 3; ++lz) {
					for (int lx = 0; lx < 3; ++lx) {
						const auto& cell = part->cells[ly][lz][lx];
						if (cell.type == BlockType::None) continue;

						int rx = lx;
						int rz = lz;
						float cellRotY = 0.0f;
						if (rotIndex == 1) {
							rx = 2 - lz;
							rz = lx;
							cellRotY = 1.5707963f;
						} else if (rotIndex == 2) {
							rx = 2 - lx;
							rz = 2 - lz;
							cellRotY = 3.1415927f;
						} else if (rotIndex == 3) {
							rx = lz;
							rz = 2 - lx;
							cellRotY = 4.712389f;
						}

						Vector3 pos = {
							static_cast<float>(cursorIndex.x + rx),
							static_cast<float>(cursorIndex.y + ly),
							static_cast<float>(cursorIndex.z + rz)
						};
						Model* cellModel = (cell.type == BlockType::Ladder) ? ladderModel_.get() : wallModel_.get();

						auto obj = std::make_unique<Object3d>();
						obj->Initialize(object3dCommon_);
						obj->SetModel(cellModel);
						obj->SetPosition(pos);
						obj->SetRotation({ 1.57f, cellRotY, 0.0f });
						obj->SetScale(blockScale_);
						obj->SetColor({ colorR, colorG, colorB, 0.4f }); // 40%の美しい半透明

						previewObjects_.push_back(std::move(obj));
					}
				}
			}
			// プレビュー表示データの再構築
			BuildPreviewRenderGroups();
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
	obj->SetRotation({ 1.57f, rotationY, 0.0f }); // 回転角を適用
	obj->SetScale(blockScale_);
	obj->SetColor({ colorR, colorG, colorB, 0.4f }); // 40%の美しい半透明
	previewObjects_.push_back(std::move(obj));

	// プレビュー表示データの再構築
	BuildPreviewRenderGroups();
}

void StageRenderer::ClearPlacementPreview() {
	previewObjects_.clear();
	// ▼ 追加：動く足場の管理リストをクリア (Object3d自体は objects_ 側で解放されるため clear だけでOK)
	movingFloorInstances_.clear();

	// プレビューグループもクリア
	previewRenderGroups_.clear();
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
	obj->Update(Math::MakeIdentity4x4());
	objects_.push_back(std::move(obj));
	return ptr;
}

//5/19佐倉
void StageRenderer::ApplyPSwitchVisualState(const StageMap& stageMap)
{
	const bool active = stageMap.IsPSwitchActive();

	for (auto& item : pSwitchObjects_) {
		if (!item.object) continue;

		if (active) {
			item.object->SetScale({ 0.0f, 0.0f, 0.0f });
		} else {
			item.object->SetScale(item.normalScale);
		}
		// スケール変更に伴いDirty化
		MarkDirty(item.object);
	}

	for (auto& item : pBlockObjects_) {
		if (!item.object) continue;

		if (active) {
			item.object->SetScale({ 0.0f, 0.0f, 0.0f });
		} else {
			item.object->SetScale(item.normalScale);
		}
		// スケール変更に伴いDirty化
		MarkDirty(item.object);
	}
}

// --- 高速インスタンシング用：レンダーグループの構築 ---
void StageRenderer::BuildRenderGroups() {
	renderGroups_.clear();
	objectToInstanceMap_.clear();
	std::unordered_map<Model*, size_t> modelToGroupIndex;

	for (size_t i = 0; i < objects_.size(); ++i) {
		Object3d* obj = objects_[i].get();
		if (!obj || !obj->GetModel()) continue;

		Model* model = obj->GetModel();
		auto it = modelToGroupIndex.find(model);
		size_t groupIndex = 0;
		if (it == modelToGroupIndex.end()) {
			groupIndex = renderGroups_.size();
			modelToGroupIndex[model] = groupIndex;
			RenderGroup group;
			group.model = model;
			renderGroups_.push_back(std::move(group));
		} else {
			groupIndex = it->second;
		}

		RenderInstance inst;
		inst.object = obj;
		inst.index = i;
		renderGroups_[groupIndex].instances.push_back(inst);
		
		// オブジェクトの生ポインタから逆引きマップへの登録
		objectToInstanceMap_[obj] = { groupIndex, renderGroups_[groupIndex].instances.size() - 1 };
	}

	// 各グループのデータをキャッシュに書き込み、初期バッファを作成
	for (auto& group : renderGroups_) {
		UINT numInstances = static_cast<UINT>(group.instances.size());
		group.instanceData.resize(numInstances);
		group.buffer = GetOrCreateInstancedBuffer(group.model, numInstances);
		group.maxInstances = numInstances;
		group.isDirty = true; // 初回は転送が必要

		InstanceData* dataBegin = nullptr;
		HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);
		if (SUCCEEDED(hr)) {
			for (UINT i = 0; i < numInstances; ++i) {
				Object3d* obj = group.instances[i].object;
				const auto& tf = obj->GetTransform();
				group.instanceData[i].world = Math::MakeAffineMatrix(tf.scale, tf.rotate, tf.translate);

				const auto& mat = obj->GetMaterial();
				group.instanceData[i].color = mat.color;
				group.instanceData[i].shininess = mat.shininess;
				group.instanceData[i].metallic = mat.metallic;
				group.instanceData[i].emissive = mat.emissive;

				dataBegin[i] = group.instanceData[i];
			}
			group.buffer->Unmap(0, nullptr);
		}
		group.isDirty = false; // 初回データ転送完了
	}
}

// --- 高速インスタンシング用：プレビュー用レンダーグループの構築 ---
void StageRenderer::BuildPreviewRenderGroups() {
	previewRenderGroups_.clear();
	std::unordered_map<Model*, size_t> modelToGroupIndex;

	for (size_t i = 0; i < previewObjects_.size(); ++i) {
		Object3d* obj = previewObjects_[i].get();
		if (!obj || !obj->GetModel()) continue;

		Model* model = obj->GetModel();
		auto it = modelToGroupIndex.find(model);
		size_t groupIndex = 0;
		if (it == modelToGroupIndex.end()) {
			groupIndex = previewRenderGroups_.size();
			modelToGroupIndex[model] = groupIndex;
			RenderGroup group;
			group.model = model;
			previewRenderGroups_.push_back(std::move(group));
		} else {
			groupIndex = it->second;
		}

		RenderInstance inst;
		inst.object = obj;
		inst.index = i;
		previewRenderGroups_[groupIndex].instances.push_back(inst);
	}

	// プレビューオブジェクト用のバッファ更新
	for (auto& group : previewRenderGroups_) {
		UINT numInstances = static_cast<UINT>(group.instances.size());
		group.instanceData.resize(numInstances);
		group.buffer = GetOrCreateInstancedBuffer(group.model, numInstances);
		group.maxInstances = numInstances;
		group.isDirty = true; // 初回は転送が必要

		InstanceData* dataBegin = nullptr;
		HRESULT hr = group.buffer->Map(0, nullptr, (void**)&dataBegin);
		if (SUCCEEDED(hr)) {
			for (UINT i = 0; i < numInstances; ++i) {
				Object3d* obj = group.instances[i].object;
				const auto& tf = obj->GetTransform();
				group.instanceData[i].world = Math::MakeAffineMatrix(tf.scale, tf.rotate, tf.translate);

				const auto& mat = obj->GetMaterial();
				group.instanceData[i].color = mat.color;
				group.instanceData[i].shininess = mat.shininess;
				group.instanceData[i].metallic = mat.metallic;
				group.instanceData[i].emissive = mat.emissive;

				dataBegin[i] = group.instanceData[i];
			}
			group.buffer->Unmap(0, nullptr);
		}
		group.isDirty = false; // 転送完了
	}
}

// --- 高速インスタンシング用：変更されたオブジェクトのキャッシュ更新とDirty化 ---
void StageRenderer::MarkDirty(Object3d* obj) {
	auto it = objectToInstanceMap_.find(obj);
	if (it != objectToInstanceMap_.end()) {
		size_t groupIdx = it->second.first;
		size_t instIdx = it->second.second;
		auto& group = renderGroups_[groupIdx];

		// 範囲チェックをしてからキャッシュデータを更新
		if (instIdx < group.instanceData.size()) {
			const auto& tf = obj->GetTransform();
			group.instanceData[instIdx].world = Math::MakeAffineMatrix(tf.scale, tf.rotate, tf.translate);

			const auto& mat = obj->GetMaterial();
			group.instanceData[instIdx].color = mat.color;
			group.instanceData[instIdx].shininess = mat.shininess;
			group.instanceData[instIdx].metallic = mat.metallic;
			group.instanceData[instIdx].emissive = mat.emissive;

			group.isDirty = true; // 次回の描画/影描画時にGPUへ再転送する
		}
	}
}