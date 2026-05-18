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

    stageMap = backupMap;

    // 見た目の再構築を次フレームに予約する
    stageMap.RequestRebuild();


    // 所持ブロックもリセット
    if (blockInventory) {
        blockInventory->Initialize(0);
    }

    // コントローラーを復元後のstageMapに接続し直す
    if (bubblePickupController && stageRenderer && blockInventory) {
        bubblePickupController->Initialize(
            &stageMap,
            stageRenderer,
            blockInventory
        );
    }

    if (blockPlacementController && stageRenderer && blockInventory) {
        blockPlacementController->Initialize(
            &stageMap,
            stageRenderer,
            blockInventory
        );
    }

    // プレイヤーをPlayerStartに戻す
    if (stageEditorController) {
        stageEditorController->ResetPlayerToStartCell(stageMap, player);
    }

    player->Respawn();
}