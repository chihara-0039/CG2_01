#include "StageRespawnController.h"

void StageRespawnController::Update(
    StageMap& stageMap,
    const StageMap& backupMap,
    StageRenderer* stageRenderer,
    Player* player,
    BlockInventory* blockInventory,
    BubblePickupController* bubblePickupController,
    BlockPlacementController* blockPlacementController,
    StageEditorController* stageEditorController
) {
    if (!player) {
        return;
    }

    if (player->GetPosition().y >= kFallY) {
        isRespawning_ = false;
        return;
    }

    // 既にリスポーン中なら何もしない
    if (isRespawning_) {
        return;
    }

    // ここに来るのは1回だけ
    isRespawning_ = true;

    // Pスイッチ状態だけ戻す
    if (stageMap.IsPSwitchActive()) {
        stageMap.ResetPSwitchStateNoRebuild();

        if (stageRenderer) {
            stageRenderer->BuildFromStageMap(stageMap);
        }
    }

    if (blockInventory) {
        blockInventory->Initialize(0);
    }

    if (bubblePickupController && stageRenderer && blockInventory) {
        bubblePickupController->Initialize(
            &stageMap, 
            stageRenderer, 
            blockInventory);
    }

    if (blockPlacementController && stageRenderer && blockInventory) {
        blockPlacementController->Initialize(&stageMap, stageRenderer, blockInventory);
    }

    /*if (stageEditorController) {
        stageEditorController->ResetPlayerToStartCell(stageMap, player);
    }*/

    player->Respawn();
}