#include "PlaceBlocksCommand.h"
#include "../Stage/RotationIsland.h"
#include <cmath>
#include <tuple>
#include <set>

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

// 配置計画と操作対象を保持する。生成しただけではマップと在庫を変更しない。
PlaceBlocksCommand::PlaceBlocksCommand(StageMap &map, BlockInventory &inventory, Placement::Plan plan, BlockType type,
                                       int customId)
    : stageMap_(&map), inventory_(&inventory), plan_(std::move(plan)), undoType_(type), undoCustomId_(customId) {
}

// 実行直前にも全セルを検証し、一つの配置操作として確定する。
bool PlaceBlocksCommand::Execute() {
    if (executed_ || !plan_.Ready() || plan_.cells.empty()) {
        lastReason_ = Placement::Reason::Changed;
        return false;
    }
    std::set<std::tuple<int, int, int>> positions;
    for (const auto &cell : plan_.cells) {
        const auto *current = stageMap_->GetCell(cell.index);
        if (!current || current->type != BlockType::None || cell.type == BlockType::None ||
            !std::isfinite(cell.rotationY) || !positions.emplace(cell.index.x, cell.index.y, cell.index.z).second) {
            lastReason_ = Placement::Reason::Changed;
            return false;
        }
    }
    undoConsumed_ = undoCustomId_ != 0 || undoType_ != BlockType::Ground;
    if (undoConsumed_ && !inventory_->ConsumeBlock(undoType_, 1, undoCustomId_)) {
        lastReason_ = Placement::Reason::NoInventory;
        return false;
    }
    std::vector<SavedCell> saved;
    for (const auto &c : plan_.cells) {
        SavedCell item{c.index, *stageMap_->GetCell(c.index), {}};
        if (!stageMap_->SetBlock(c.index, c.type, c.variant)) {
            for (const auto &s : saved) {
                stageMap_->SetCell(s.index, s.before);
            }
            if (undoConsumed_) {
                inventory_->AddBlock(undoType_, 1, undoCustomId_);
            }
            lastReason_ = Placement::Reason::Changed;
            return false;
        }
        stageMap_->EditCell(c.index, [&](MapCell &cell) { cell.rotationY = c.rotationY; });
        item.placed = *stageMap_->GetCell(c.index);
        saved.push_back(item);
    }
    cells_ = std::move(saved);
    executed_ = applied_ = true;
    lastReason_ = Placement::Reason::Ready;
    return true;
}

// 配置後の変更とプレイヤーへの接触を確認してから、地形と在庫を復元する。
bool PlaceBlocksCommand::Undo(const Vector3 *player) {
    if (!applied_) {
        lastReason_ = Placement::Reason::Changed;
        return false;
    }
    // 消失した足場や後で変化したセルを消して在庫を増やさない。
    for (const auto &s : cells_) {
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
        if (player && std::abs(player->x - center.x) < .89f && std::abs(player->z - center.z) < .89f &&
            player->y <= center.y + .65f && player->y + 1.8f >= center.y - .5f) {
            lastReason_ = Placement::Reason::PlayerOverlap;
            return false;
        }
    }
    for (const auto &s : cells_) {
        stageMap_->SetCell(s.index, s.before);
    }
    if (undoConsumed_) {
        inventory_->AddBlock(undoType_, 1, undoCustomId_);
    }
    applied_ = false;
    lastReason_ = Placement::Reason::Ready;
    return true;
}

// ワールドの回転に合わせて、実行済み操作の復元情報も回す。
void PlaceBlocksCommand::Rotate(bool reverse, const RotationRegion &region) {
    if (!applied_) {
        return;
    }
    for (auto &cell : cells_) {
        if (RotationIsland::Contains(cell.index, region)) {
            cell.index = RotationIsland::Turn(cell.index, reverse, region);
            RotationIsland::TurnCell(cell.before, reverse, region);
            RotationIsland::TurnCell(cell.placed, reverse, region);
        }
    }
}

// 成功通知と表示で使う中心位置を、確定したセル群から求める。
Vector3 PlaceBlocksCommand::Center() const {
    if (cells_.empty()) {
        return {};
    }
    Vector3 center{};
    for (const auto &cell : cells_) {
        center.x += cell.index.x;
        center.y += cell.index.y;
        center.z += cell.index.z;
    }
    const float n = static_cast<float>(cells_.size());
    return {center.x / n, center.y / n + .5f, center.z / n};
}
