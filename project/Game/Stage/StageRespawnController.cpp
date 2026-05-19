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
        return;
    }

    // Pスイッチ状態だけ戻す
    if (stageMap.IsPSwitchActive()) {
        stageMap.ResetPSwitchStateNoRebuild();
        if (stageRenderer) {
            stageRenderer->ApplyPSwitchVisualState(stageMap);
        }
    }

    if (blockInventory) {
        blockInventory->Initialize(0);
    }

    if (bubblePickupController && stageRenderer && blockInventory) {
        bubblePickupController->Initialize(&stageMap, stageRenderer, blockInventory);
    }

    if (blockPlacementController && stageRenderer && blockInventory) {
        blockPlacementController->Initialize(&stageMap, stageRenderer, blockInventory);
    }

    if (stageEditorController) {
        stageEditorController->ResetPlayerToStartCell(stageMap, player);
    }

    player->Respawn();
}