#include "StageMapGimmickSystem.h"

#include "StageMap.h"

void StageMapGimmickSystem::SetPSwitchActive(StageMap& stageMap, int switchId) {
    stageMap.isPSwitchActive_ = true;
    stageMap.needsRebuild_ = true;

    for (auto& cell : stageMap.cells_) {
        if (cell.type == BlockType::PSwitch && cell.variant == switchId) {
            cell.isSolid = false;
            cell.isHidden = true;
        }

        if (cell.type == BlockType::PBlock && cell.variant == switchId) {
            cell.isSolid = false;
            cell.isHidden = true;
        }

        if (cell.type == BlockType::PBlockAppears && cell.variant == switchId) {
            cell.isSolid = true;
        }
    }
}

void StageMapGimmickSystem::ResetPSwitchStateNoRebuild(StageMap& stageMap) {
    stageMap.isPSwitchActive_ = false;

    for (auto& cell : stageMap.cells_) {
        if (cell.type == BlockType::PSwitch) {
            cell.isSolid = false;
            cell.isHidden = false;
        }

        if (cell.type == BlockType::PBlock) {
            cell.isSolid = true;
            cell.isHidden = false;
        }

        if (cell.type == BlockType::PBlockAppears) {
            cell.isSolid = false;
        }
    }

    stageMap.needsRebuild_ = false;
}

void StageMapGimmickSystem::ResetPSwitchState(StageMap& stageMap) {
    stageMap.isPSwitchActive_ = false;
}

void StageMapGimmickSystem::ToggleOnState(StageMap& stageMap) {
    stageMap.isOnState_ = !stageMap.isOnState_;

    for (auto& cell : stageMap.cells_) {
        if (cell.type == BlockType::OnBlock) {
            cell.isSolid = stageMap.isOnState_;
        }
        if (cell.type == BlockType::OffBlock) {
            cell.isSolid = !stageMap.isOnState_;
        }
    }

    stageMap.needsRebuild_ = true;
}
