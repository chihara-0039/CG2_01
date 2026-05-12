#include "BubblePickupController.h"

#include <cmath>
#include <cassert>
#include <Windows.h>

void BubblePickupController::Initialize(
    StageMap* stageMap,
    StageRenderer* stageRenderer,
    BlockInventory* inventory
) {
    assert(stageMap);
    assert(stageRenderer);
    assert(inventory);

    stageMap_ = stageMap;
    stageRenderer_ = stageRenderer;
    inventory_ = inventory;
}

void BubblePickupController::Update(const Vector3& playerPosition) {
    if (!stageMap_ || !stageRenderer_ || !inventory_) {
        return;
    }

    Int3 baseIndex = ToGridIndex(playerPosition);

    // プレイヤーのY座標とマップ上のアイテム位置が少しズレることがあるので、
    // 足元・中心・少し上をまとめて確認する
    for (int yOffset = -1; yOffset <= 1; ++yOffset) {
        Int3 checkIndex = baseIndex;
        checkIndex.y += yOffset;

        if (TryCollectAt(checkIndex)) {
            return;
        }
    }
}

Int3 BubblePickupController::ToGridIndex(const Vector3& position) const {
    Int3 index{};

    // x/z は四捨五入気味にする
    index.x = static_cast<int>(std::floor(position.x + 0.5f));
    index.y = static_cast<int>(std::floor(position.y));
    index.z = static_cast<int>(std::floor(position.z + 0.5f));

    return index;
}

bool BubblePickupController::TryCollectAt(const Int3& index) {
    MapCell* cell = stageMap_->GetCell(index);
    if (!cell) {
        return false;
    }

    if (cell->type != BlockType::BubblePickup) {
        return false;
    }

    // シャボン玉を取得して、配置可能ブロックを1個増やす
    inventory_->AddBlock(1);

    // マップ上からシャボン玉を消す
    stageMap_->RemoveBlock(index);

    // 見た目を再構築
    stageRenderer_->BuildFromStageMap(*stageMap_);

    OutputDebugStringA("[BubblePickupController] Bubble collected.\n");

    return true;
}