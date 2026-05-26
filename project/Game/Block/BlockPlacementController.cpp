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
    placeCustomId_ = 0;
}

bool BlockPlacementController::TryPlace(const Int3& index, float rotationY) {
    if (!stageMap_ || !stageRenderer_ || !inventory_) {
        return false;
    }
    // --- 複合カスタムアセンブリパーツの配置処理 ---
    if (placeCustomId_ >= 1 && placeCustomId_ <= 5) {
        const auto* part = stageMap_->GetCustomPart(placeCustomId_);
        if (part && !part->IsEmpty()) {
            // インベントリにパーツの在庫があるか確認
            if (!inventory_->HasBlock(placeBlockType_, placeCustomId_)) {
                OutputDebugStringA("[BlockPlacementController] No custom assembly in inventory.\n");
                return false;
            }

            int rotIndex = static_cast<int>(std::round(rotationY / 1.5707963f)) % 4;
            if (rotIndex < 0) rotIndex += 4;

            // 1. 配置処理（既存ブロックがあるマスは自動スキップし、空いているマスにのみ一括配置）
            bool placedAny = false;
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

                        Int3 targetPos = { index.x + rx, index.y + ly, index.z + rz };
                        if (stageMap_->IsInside(targetPos)) {
                            const MapCell* targetCell = stageMap_->GetCell(targetPos);
                            if (targetCell && targetCell->type == BlockType::None) {
                                stageMap_->SetBlock(targetPos, cell.type, placeCustomId_);
                                MapCell* newCell = stageMap_->GetCell(targetPos);
                                if (newCell) {
                                    newCell->rotationY = cellRotY;
                                }
                                placedAny = true;
                            }
                        }
                    }
                }
            }

            if (!placedAny) {
                OutputDebugStringA("[BlockPlacementController] Assembly placement failed: no available empty spaces.\n");
                return false; // 1マスも配置できなかった場合（全マスが他のブロックで完全に埋まっている場合）のみ失敗
            }

            // 3. インベントリの在庫を 1 消費
            inventory_->ConsumeBlock(placeBlockType_, 1, placeCustomId_);

            // 4. 見た目を再構築
            stageRenderer_->BuildFromStageMap(*stageMap_);
            OutputDebugStringA("[BlockPlacementController] Custom assembly placed successfully.\n");
            return true;
        }
    }

    // --- 通常の 1マス配置処理 ---
    // Ground 以外は、インベントリにその種類のブロックを持っているか（カスタムスロット別）確認
    if (placeBlockType_ != BlockType::Ground) {
        if (!inventory_->HasBlock(placeBlockType_, placeCustomId_)) {
            OutputDebugStringA("[BlockPlacementController] No block of this type/customId in inventory.\n");
            return false;
        }
    }

    // 置けないマスなら失敗
    if (!CanPlaceAt(index)) {
        OutputDebugStringA("[BlockPlacementController] Cannot place here.\n");
        return false;
    }

    // ブロック設置 (通常ブロックの場合はプレイヤー設置カラーを示す variant に割り当てて配置！)
    int finalVariant = placeCustomId_;
    if (placeCustomId_ == 0) {
        if (placeBlockType_ == BlockType::Wall) finalVariant = 6;
        else if (placeBlockType_ == BlockType::Ladder) finalVariant = 7;
    }

    if (!stageMap_->SetBlock(index, placeBlockType_, finalVariant)) {
        return false;
    }

    MapCell* cell = stageMap_->GetCell(index);
    if (cell) {
        cell->rotationY = rotationY;
    }

    // Ground 以外なら所持数を1個消費
    if (placeBlockType_ != BlockType::Ground) {
        inventory_->ConsumeBlock(placeBlockType_, 1, placeCustomId_);
    }

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