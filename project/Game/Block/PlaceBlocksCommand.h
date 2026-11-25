#pragma once
#include "PlacementPlan.h"
#include "BlockInventory.h"
#include <utility>

// 一回の配置を、実行・取消・復元データを持つCommandとして表す。
class PlaceBlocksCommand {
  public:
    PlaceBlocksCommand(StageMap &map, BlockInventory &inventory, Placement::Plan plan, BlockType type, int customId);
    bool Execute();
    bool Undo(const Vector3 *player = nullptr);
    void Rotate(bool reverse, const RotationRegion &region);
    bool CanUndo() const { return applied_; }
    Placement::Reason LastReason() const { return lastReason_; }
    Vector3 Center() const;

  private:
    struct SavedCell {
        Int3 index;
        MapCell before;
        MapCell placed;
    };
    StageMap *stageMap_;
    BlockInventory *inventory_;
    Placement::Plan plan_;
    std::vector<SavedCell> cells_;
    BlockType undoType_;
    int undoCustomId_;
    bool undoConsumed_ = false, executed_ = false, applied_ = false;
    Placement::Reason lastReason_ = Placement::Reason::Ready;
};
