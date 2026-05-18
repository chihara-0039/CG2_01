#pragma once

#include "StageMap.h"
#include "StageRenderer.h"
#include "Player.h"
#include "../Block/BlockInventory.h"
#include "../Block/BubblePickupController.h"
#include "../Block/BlockPlacementController.h"
#include "StageEditorController.h"

class StageRespawnController
{
public:
    void Update(
        StageMap& stageMap,
        const StageMap& backupMap,
        StageRenderer* stageRenderer,
        Player* player,
        BlockInventory* blockInventory,
        BubblePickupController* bubblePickupController,
        BlockPlacementController* blockPlacementController,
        StageEditorController* stageEditorController
    );

private:
    static constexpr float kFallY = -10.0f;
};

