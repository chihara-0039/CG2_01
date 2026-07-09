#include "StageWallTransparencyController.h"

#include <cmath>
#include "Object3d.h"
#include "StageTransparencyPolicy.h"

void StageWallTransparencyController::Apply(
    std::vector<Object3d*>& wallObjects,
    const Vector3& cameraPos,
    const Vector3& playerPos,
    bool enableTransparency,
    float transparencyAlpha,
    int currentStageIndex) {

    cameraPos;

    if (!enableTransparency) {
        for (auto& obj : wallObjects) {
            if (obj) {
                obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
            }
        }
        return;
    }

    int playerCellX = static_cast<int>(std::floor(playerPos.x + 0.5f));
    int playerCellY = static_cast<int>(std::floor(playerPos.y + 0.5f));
    int playerCellZ = static_cast<int>(std::floor(playerPos.z + 0.5f));

    for (auto& obj : wallObjects) {
        if (!obj) {
            continue;
        }

        Vector3 wallPos = obj->GetPosition();

        int wallCellX = static_cast<int>(std::floor(wallPos.x + 0.5f));
        int wallCellY = static_cast<int>(std::floor(wallPos.y + 0.5f));
        int wallCellZ = static_cast<int>(std::floor(wallPos.z + 0.5f));

        obj->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

        if (wallCellY <= playerCellY) {
            continue;
        }

        if (!StageTransparencyPolicy::IsTransparencyArea(
                currentStageIndex,
                wallCellX,
                wallCellY,
                wallCellZ)) {
            continue;
        }

        bool insideTransparencyArea =
            std::abs(wallCellX - playerCellX) <= 2 &&
            std::abs(wallCellZ - playerCellZ) <= 1 &&
            wallCellY <= playerCellY + 2;

        if (insideTransparencyArea) {
            obj->SetColor({ 1.0f, 1.0f, 1.0f, transparencyAlpha });
        }
    }
}
