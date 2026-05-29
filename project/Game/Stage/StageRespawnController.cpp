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

    if (isRespawning_) {
        return;
    }

    isRespawning_ = true;

    // ==================================================
  // 今のチェックポイント位置を保存
  // ==================================================
    Vector3 currentRespawnPos = player->GetRespawnPosition();

    // ==================================================
    // 落下時：ステージ開始時の状態へ丸ごと戻す
    // Pスイッチ / ONOFF / 鍵 / 鍵ブロック / バブル / 設置ブロック
    // など全部 backupMap_ の状態に戻る
    // ==================================================
    stageMap = backupMap;

    if (stageRenderer) {
        stageRenderer->BuildFromStageMap(stageMap);
    }

    if (blockInventory) {
        blockInventory->Initialize(0);
    }

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

    // ==================================================
   // チェックポイント位置は維持する
   // ただし鍵は必ずリセットする
   // ==================================================
    player->SetRespawnPosition(currentRespawnPos);
    player->SetHasKey(false);
    player->Respawn();
}