#include "BlockPlacementController.h"
#include <cassert>
#include <cmath>
#include <tuple>

namespace {
auto CellState(const MapCell& c) {
    return std::tie(c.type,c.variant,c.isSolid,c.rotationX,c.rotationY,
        c.doorTargetIndex.x,c.doorTargetIndex.y,c.doorTargetIndex.z,
        c.crumbleTimer,c.isCrumbling,c.isHidden,c.respawnTimer,
        c.colorR,c.colorG,c.colorB,c.opacity,c.moveOffset.x,c.moveOffset.y,c.moveOffset.z,
        c.moveTimer,c.currentOffsetX,c.currentOffsetY,c.currentOffsetZ,
        c.deltaOffsetX,c.deltaOffsetY,c.deltaOffsetZ);
}
}

void BlockPlacementController::Initialize(StageMap* map,StageRenderer* renderer,BlockInventory* inventory) {
    assert(map && renderer && inventory);
    stageMap_=map; stageRenderer_=renderer; inventory_=inventory;
    placeCustomId_=0; lastPlacement_.clear(); checkPlayer_=false;
}

Placement::Plan BlockPlacementController::Evaluate(const Int3& index,float rotationY) const {
    if(!stageMap_ || !inventory_) return {{},Placement::Reason::EmptyPart};
    auto plan=Placement::Make(*stageMap_,index,placeBlockType_,placeCustomId_,rotationY,checkPlayer_?&playerPosition_:nullptr);
    const bool consumes=placeCustomId_!=0 || placeBlockType_!=BlockType::Ground;
    if(plan.Ready() && consumes && !inventory_->HasBlock(placeBlockType_,placeCustomId_))plan.reason=Placement::Reason::NoInventory;
    return plan;
}

bool BlockPlacementController::TryPlace(const Int3& index,float rotationY) {
    auto plan=Evaluate(index,rotationY); lastReason_=plan.reason;
    if(!plan.Ready() || !stageRenderer_)return false;
    const bool consumes=placeCustomId_!=0 || placeBlockType_!=BlockType::Ground;
    // 全セルの検証後、在庫とマップを一度だけ変更する。
    if(consumes && !inventory_->ConsumeBlock(placeBlockType_,1,placeCustomId_)) {lastReason_=Placement::Reason::NoInventory;return false;}
    std::vector<SavedCell> saved;
    for(const auto& c:plan.cells) {
        SavedCell item{c.index,*stageMap_->GetCell(c.index),{}};
        if(!stageMap_->SetBlock(c.index,c.type,c.variant)) {
            for(const auto& s:saved)*stageMap_->GetCell(s.index)=s.before;
            if(consumes)inventory_->AddBlock(placeBlockType_,1,placeCustomId_);
            lastReason_=Placement::Reason::Changed; return false;
        }
        auto* cell=stageMap_->GetCell(c.index);cell->rotationY=c.rotationY;item.placed=*cell;saved.push_back(item);
    }
    lastPlacement_=std::move(saved);undoType_=placeBlockType_;undoCustomId_=placeCustomId_;undoConsumed_=consumes;
    stageRenderer_->BuildFromStageMap(*stageMap_);return true;
}

bool BlockPlacementController::TryUndo() {
    if(!stageMap_ || !inventory_ || !stageRenderer_ || lastPlacement_.empty()){lastReason_=Placement::Reason::Changed;return false;}
    // 消失した足場や後で変化したセルを消して在庫を増やさない。
    for(const auto& s:lastPlacement_) {
        const auto* c=stageMap_->GetCell(s.index);const auto& p=s.placed;
        if(!c || CellState(*c)!=CellState(p)) {
            lastReason_=Placement::Reason::Changed;return false;
        }
        if(checkPlayer_ && std::abs(playerPosition_.x-s.index.x)<.89f && std::abs(playerPosition_.z-s.index.z)<.89f &&
           playerPosition_.y<=s.index.y+.65f && playerPosition_.y+1.8f>=s.index.y-.5f) {
            lastReason_=Placement::Reason::PlayerOverlap;return false;
        }
    }
    for(const auto& s:lastPlacement_) {stageMap_->RemoveBlock(s.index);*stageMap_->GetCell(s.index)=s.before;}
    if(undoConsumed_)inventory_->AddBlock(undoType_,1,undoCustomId_);
    lastPlacement_.clear();stageRenderer_->BuildFromStageMap(*stageMap_);lastReason_=Placement::Reason::Ready;return true;
}
