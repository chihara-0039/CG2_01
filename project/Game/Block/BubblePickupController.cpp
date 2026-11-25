#include "BubblePickupController.h"

#include <cmath>
#include <cassert>
#include <utility>
#include <Windows.h>

// シャボン玉の回収を準備し、初期値と必要なリソースを設定する。
void BubblePickupController::Initialize(StageMap *stageMap, StageRenderer *stageRenderer, BlockInventory *inventory) {
    assert(stageMap);
    assert(stageRenderer);
    assert(inventory);

    stageMap_ = stageMap;
    stageRenderer_ = stageRenderer;
    inventory_ = inventory;

    // ステージの読込・復元時に総数を数え直し、回収判定とUI表示で同じ値を参照する。
    collectedCount_ = 0;
    totalPickupCount_ = 0;
    for (int y = 0; y < stageMap_->GetHeight(); ++y) {
        for (int z = 0; z < stageMap_->GetDepth(); ++z) {
            for (int x = 0; x < stageMap_->GetWidth(); ++x) {
                const const MapCell *cell = stageMap_->GetCell(x, y, z);
                if (cell != nullptr && cell->type == BlockType::BubblePickup) {
                    ++totalPickupCount_;
                }
            }
        }
    }
}

void BubblePickupController::SetCollectCallback(std::function<void(const Vector3 &)> callback) {
    collectCallback_ = std::move(callback);
}

// プレイヤー周囲のセルを調べ、接触したシャボン玉の中身を在庫へ移す。
void BubblePickupController::Update(const Vector3 &playerPosition) {
    if (!stageMap_ || !stageRenderer_ || !inventory_) {
        return;
    }

    Int3 baseIndex = ToGridIndex(playerPosition);

    // The player's position is the feet, not the body centre. Only inspect
    // cells overlapping the body; the former -1 offset collected through floors.
    const int minimumY = static_cast<int>(std::floor(playerPosition.y + 0.5f));
    const int maximumY = static_cast<int>(std::floor(playerPosition.y + 1.6f + 0.5f - 0.0001f));
    for (int y = minimumY; y <= maximumY; ++y) {
        Int3 checkIndex = baseIndex;
        checkIndex.y = y;
        if (TryCollectAt(checkIndex)) {
            return;
        }
    }
}

// ワールド座標を、対応するステージのセル番号へ変換する。
Int3 BubblePickupController::ToGridIndex(const Vector3 &position) const {
    Int3 index{};

    // x/z は四捨五入気味にする
    index.x = static_cast<int>(std::floor(position.x + 0.5f));
    index.y = static_cast<int>(std::floor(position.y));
    index.z = static_cast<int>(std::floor(position.z + 0.5f));

    return index;
}

// 対象セルのシャボン玉を確認し、中身を在庫へ移して回収済みにする。
bool BubblePickupController::TryCollectAt(const Int3 &index) {
    const MapCell *cell = stageMap_->GetCell(index);
    if (!cell) {
        return false;
    }

    if (cell->type != BlockType::BubblePickup) {
        return false;
    }

    // シャボン玉に仕込まれている情報をデコード
    int insideCustomId = UnpackBubbleCustomId(cell->variant);
    BlockType insideBlock = UnpackBubbleType(cell->variant);
    if (insideBlock == BlockType::None) {
        insideBlock = BlockType::Wall; // 安全策としてデフォルトを Wall に
    }

    // カスタムIDを保持してインベントリに追加
    const int beforeCount = inventory_->GetBlockCount(insideBlock, insideCustomId);
    inventory_->AddBlock(insideBlock, 1, insideCustomId);
    if (inventory_->GetBlockCount(insideBlock, insideCustomId) <= beforeCount) {
        // Unsupported contents must not disappear without awarding an item.
        return false;
    }

    // マップ上からシャボン玉を消す
    stageMap_->RemoveBlock(index);
    ++collectedCount_;

    // 見た目を再構築
    stageRenderer_->BuildFromStageMap(*stageMap_);

    if (collectCallback_) {
        collectCallback_(
            {static_cast<float>(index.x), static_cast<float>(index.y) + 0.5f, static_cast<float>(index.z)});
    }

    OutputDebugStringA("[BubblePickupController] Bubble collected.\n");

    return true;
}
