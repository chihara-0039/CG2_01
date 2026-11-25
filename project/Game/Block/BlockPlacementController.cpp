#include "BlockPlacementController.h"
#include <cassert>
#include <cmath>
#include <tuple>
#include "../Stage/RotationIsland.h"

namespace {
// 配置セルの定義と状態を、変更検出に使う比較値へまとめる。
auto CellState(const MapCell &c) {
    return std::tie(c.type, c.variant, c.isSolid, c.rotationX, c.rotationY, c.doorTargetIndex.x, c.doorTargetIndex.y,
                    c.doorTargetIndex.z, c.crumbleTimer, c.isCrumbling, c.isHidden, c.respawnTimer, c.colorR, c.colorG,
                    c.colorB, c.opacity, c.moveOffset.x, c.moveOffset.y, c.moveOffset.z, c.moveTimer, c.currentOffsetX,
                    c.currentOffsetY, c.currentOffsetZ, c.deltaOffsetX, c.deltaOffsetY, c.deltaOffsetZ);
}
// 動く足場の時間変化を除外し、配置後に他の編集が入っていないか確認する。
bool SamePlacedCell(const MapCell &current, const MapCell &placed) {
    if (placed.type != BlockType::MovingFloor) {
        return CellState(current) == CellState(placed);
    }
    // Motion is expected; edits to the block's definition still invalidate undo.
    auto stable = current;
    stable.moveTimer = placed.moveTimer;
    stable.currentOffsetX = placed.currentOffsetX;
    stable.currentOffsetY = placed.currentOffsetY;
    stable.currentOffsetZ = placed.currentOffsetZ;
    stable.deltaOffsetX = placed.deltaOffsetX;
    stable.deltaOffsetY = placed.deltaOffsetY;
    stable.deltaOffsetZ = placed.deltaOffsetZ;
    return CellState(stable) == CellState(placed);
}
}

// 配置と取り消しを準備し、初期値と必要なリソースを設定する。
void BlockPlacementController::Initialize(StageMap *map, StageRenderer *renderer, BlockInventory *inventory) {
    assert(map && renderer && inventory);
    stageMap_ = map;
    stageRenderer_ = renderer;
    inventory_ = inventory;
    placeCustomId_ = 0;
    lastPlacement_.clear();
    checkPlayer_ = false;
    rotationIsland_ = false;
}

// 配置範囲・衝突・回転島の境界・在庫を検証し、配置計画を返す。
Placement::Plan BlockPlacementController::Evaluate(const Int3 &index, float rotationY) const {
    if (!stageMap_ || !inventory_) {
        return {{}, Placement::Reason::EmptyPart};
    }
    auto plan = Placement::Make(*stageMap_, index, placeBlockType_, placeCustomId_, rotationY,
                                checkPlayer_ ? &playerPosition_ : nullptr);
    if (!stageMap_->GetRotationRegions().empty() && !plan.cells.empty()) {
        const int firstInside = RotationIsland::RegionAt(*stageMap_, plan.cells.front().index);
        for (const auto &cell : plan.cells) {
            if (RotationIsland::RegionAt(*stageMap_, cell.index) != firstInside) {
                plan.reason = Placement::Reason::Outside;
                return plan;
            }
        }
    }
    const bool consumes = placeCustomId_ != 0 || placeBlockType_ != BlockType::Ground;
    if (plan.Ready() && consumes && !inventory_->HasBlock(placeBlockType_, placeCustomId_)) {
        plan.reason = Placement::Reason::NoInventory;
    }
    return plan;
}

// 回転島に載る配置履歴も回し、回転後の取り消し先を一致させる。
void BlockPlacementController::RotateUndo(bool reverse, const RotationRegion &region) {
    for (auto &cell : lastPlacement_) {
        if (RotationIsland::Contains(cell.index, region)) {
            cell.index = RotationIsland::Turn(cell.index, reverse, region);
            RotationIsland::TurnCell(cell.before, reverse, region);
            RotationIsland::TurnCell(cell.placed, reverse, region);
        }
    }
}

// 配置計画を検証して在庫を消費し、全セルを確定する。失敗時は途中の変更を戻す。
bool BlockPlacementController::TryPlace(const Int3 &index, float rotationY) {
    auto plan = Evaluate(index, rotationY);
    lastReason_ = plan.reason;
    if (!plan.Ready() || !stageRenderer_) {
        return false;
    }
    const bool consumes = placeCustomId_ != 0 || placeBlockType_ != BlockType::Ground;
    // 全セルの検証後、在庫とマップを一度だけ変更する。
    if (consumes && !inventory_->ConsumeBlock(placeBlockType_, 1, placeCustomId_)) {
        lastReason_ = Placement::Reason::NoInventory;
        return false;
    }
    std::vector<SavedCell> saved;
    for (const auto &c : plan.cells) {
        SavedCell item{c.index, *stageMap_->GetCell(c.index), {}};
        if (!stageMap_->SetBlock(c.index, c.type, c.variant)) {
            for (const auto &s : saved) {
                stageMap_->SetCell(s.index, s.before);
            }
            if (consumes) {
                inventory_->AddBlock(placeBlockType_, 1, placeCustomId_);
            }
            lastReason_ = Placement::Reason::Changed;
            return false;
        }
        stageMap_->EditCell(c.index, [&](MapCell &cell) { cell.rotationY = c.rotationY; });
        item.placed = *stageMap_->GetCell(c.index);
        saved.push_back(item);
    }
    Vector3 center{};
    for (const auto &cell : saved) {
        center.x += cell.index.x;
        center.y += cell.index.y;
        center.z += cell.index.z;
    }
    const float n = static_cast<float>(saved.size());
    center = {center.x / n, center.y / n + .5f, center.z / n};
    lastPlacement_ = std::move(saved);
    undoType_ = placeBlockType_;
    undoCustomId_ = placeCustomId_;
    undoConsumed_ = consumes;
    stageRenderer_->BuildFromStageMap(*stageMap_);
    if (feedback_) {
        feedback_(center, false);
    }
    return true;
}

// 配置後の改変やプレイヤーとの重なりを確認し、直前の配置と在庫を復元する。
bool BlockPlacementController::TryUndo() {
    if (!stageMap_ || !inventory_ || !stageRenderer_ || lastPlacement_.empty()) {
        lastReason_ = Placement::Reason::Changed;
        return false;
    }
    // 消失した足場や後で変化したセルを消して在庫を増やさない。
    for (const auto &s : lastPlacement_) {
        const auto *c = stageMap_->GetCell(s.index);
        const auto &p = s.placed;
        if (!c || !SamePlacedCell(*c, p)) {
            lastReason_ = Placement::Reason::Changed;
            return false;
        }
        Vector3 center{static_cast<float>(s.index.x), static_cast<float>(s.index.y), static_cast<float>(s.index.z)};
        if (c->type == BlockType::MovingFloor) {
            center.x += c->currentOffsetX;
            center.y += c->currentOffsetY;
            center.z += c->currentOffsetZ;
        }
        if (checkPlayer_ && std::abs(playerPosition_.x - center.x) < .89f &&
            std::abs(playerPosition_.z - center.z) < .89f && playerPosition_.y <= center.y + .65f &&
            playerPosition_.y + 1.8f >= center.y - .5f) {
            lastReason_ = Placement::Reason::PlayerOverlap;
            return false;
        }
    }
    Vector3 center{};
    for (const auto &cell : lastPlacement_) {
        center.x += cell.index.x;
        center.y += cell.index.y;
        center.z += cell.index.z;
    }
    const float n = static_cast<float>(lastPlacement_.size());
    center = {center.x / n, center.y / n + .5f, center.z / n};
    for (const auto &s : lastPlacement_) {
        stageMap_->RemoveBlock(s.index);
        stageMap_->SetCell(s.index, s.before);
    }
    if (undoConsumed_) {
        inventory_->AddBlock(undoType_, 1, undoCustomId_);
    }
    lastPlacement_.clear();
    stageRenderer_->BuildFromStageMap(*stageMap_);
    lastReason_ = Placement::Reason::Ready;
    if (feedback_) {
        feedback_(center, true);
    }
    return true;
}
