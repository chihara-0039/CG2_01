#include "BlockPlacementController.h"

#include <cassert>
#include <Windows.h>

void BlockPlacementController::Initialize(
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

bool BlockPlacementController::TryPlace(const Int3& index) {
    if (!stageMap_ || !stageRenderer_ || !inventory_) {
        return false;
    }

    // ブロックを持っていないなら置けない
    if (!inventory_->HasBlock()) {
        OutputDebugStringA("[BlockPlacementController] No block in inventory.\n");
        return false;
    }

    // 置けないマスなら失敗
    if (!CanPlaceAt(index)) {
        OutputDebugStringA("[BlockPlacementController] Cannot place here.\n");
        return false;
    }

    // ブロック設置
    if (!stageMap_->SetBlock(index, placeBlockType_)) {
        return false;
    }

    // 所持数を1個消費
    inventory_->ConsumeBlock(1);

    // 見た目を再構築
    stageRenderer_->BuildFromStageMap(*stageMap_);

    OutputDebugStringA("[BlockPlacementController] Block placed.\n");

    return true;
}

bool BlockPlacementController::CanPlaceAt(const Int3& index) const {
    const MapCell* cell = stageMap_->GetCell(index);
    if (!cell) {
        return false;
    }

    // 空マスにだけ置ける
    if (cell->type != BlockType::None) {
        return false;
    }

    return true;
}