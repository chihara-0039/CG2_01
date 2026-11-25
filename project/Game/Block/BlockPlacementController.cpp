#include "BlockPlacementController.h"
#include <cassert>
#include <cmath>
#include <tuple>
#include "../Stage/RotationIsland.h"

// 配置と取り消しを準備し、初期値と必要なリソースを設定する。
void BlockPlacementController::Initialize(StageMap *map, StageRenderer *renderer, BlockInventory *inventory) {
    assert(map && renderer && inventory);
    stageMap_ = map;
    stageRenderer_ = renderer;
    inventory_ = inventory;
    placeCustomId_ = 0;
    lastCommand_.reset();
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

// 回転したワールドに合わせ、実行済みCommandの復元先を変換する。
void BlockPlacementController::RotateUndo(bool reverse, const RotationRegion &region) {
    if (lastCommand_) {
        lastCommand_->Rotate(reverse, region);
    }
}
// 計画をCommandへ渡し、実行に成功した操作だけを取消対象として保持する。
bool BlockPlacementController::TryPlace(const Int3 &index, float rotationY) {
    auto plan = Evaluate(index, rotationY);
    lastReason_ = plan.reason;
    if (!plan.Ready() || !stageRenderer_) {
        return false;
    }
    auto command =
        std::make_unique<PlaceBlocksCommand>(*stageMap_, *inventory_, std::move(plan), placeBlockType_, placeCustomId_);
    if (!command->Execute()) {
        lastReason_ = command->LastReason();
        return false;
    }
    const auto center = command->Center();
    lastCommand_ = std::move(command);
    stageRenderer_->BuildFromStageMap(*stageMap_);
    placementSignal_.Publish({center, false});
    return true;
}
// 直前のCommandに取消を依頼し、成功した場合だけ表示と通知を更新する。
bool BlockPlacementController::TryUndo() {
    if (!stageMap_ || !inventory_ || !stageRenderer_ || !lastCommand_ || !lastCommand_->CanUndo()) {
        lastReason_ = Placement::Reason::Changed;
        return false;
    }
    if (!lastCommand_->Undo(checkPlayer_ ? &playerPosition_ : nullptr)) {
        lastReason_ = lastCommand_->LastReason();
        return false;
    }
    const auto center = lastCommand_->Center();
    stageRenderer_->BuildFromStageMap(*stageMap_);
    lastReason_ = Placement::Reason::Ready;
    placementSignal_.Publish({center, true});
    return true;
}
