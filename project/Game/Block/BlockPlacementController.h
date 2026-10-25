#pragma once

#include "StageMap.h"
#include "StageRenderer.h"
#include "BlockInventory.h"
#include "PlacementPlan.h"

// 所持ブロックをステージ上へ配置するための制御クラス。
class BlockPlacementController {
public:
    // 操作対象のマップ、描画、所持数管理を外部から注入する。
    void Initialize(
        StageMap* stageMap,
        StageRenderer* stageRenderer,
        BlockInventory* inventory
    );

    // 指定セルに現在選択中のブロックを配置する。成功時はインベントリも消費する。
    bool TryPlace(const Int3& index, float rotationY = 0.0f);
    Placement::Plan Evaluate(const Int3& index,float rotationY=0) const;
    bool TryUndo();
    bool HasUndo() const { return !lastPlacement_.empty(); }
    Placement::Reason LastReason() const { return lastReason_; }
    void SetPlayerPosition(const Vector3& p) {playerPosition_=p;checkPlayer_=true;}

    // UI側で選択された配置ブロック種別とカスタムIDを保持する。
    void SetPlaceBlockType(BlockType type) { placeBlockType_ = type; }
    void SetPlaceCustomId(int id) { placeCustomId_ = id; }
    int GetPlaceCustomId() const { return placeCustomId_; }

private:
    // 配置先セルが空で、ステージ範囲内かを判定する。
    struct SavedCell { Int3 index; MapCell before; MapCell placed; };
    std::vector<SavedCell> lastPlacement_;
    BlockType undoType_=BlockType::None;
    int undoCustomId_=0;
    bool undoConsumed_=false,checkPlayer_=false;
    Vector3 playerPosition_{};
    Placement::Reason lastReason_=Placement::Reason::Ready;

private:
    StageMap* stageMap_ = nullptr;
    StageRenderer* stageRenderer_ = nullptr;
    BlockInventory* inventory_ = nullptr;

    // 初期状態では通常地面ブロックを配置対象にする。
    BlockType placeBlockType_ = BlockType::Ground;
    int placeCustomId_ = 0; // 0: 通常ブロック, 1～5: カスタムパーツ
};
