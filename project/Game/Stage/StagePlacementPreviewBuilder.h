#pragma once
#include <memory>
#include <vector>
#include "StageMap.h"

class Model;
class Object3d;
class Object3dCommon;

class StagePlacementPreviewBuilder {
public:
    struct Models {
        Model* ground = nullptr;
        Model* wall = nullptr;
        Model* ladder = nullptr;
        Model* crumble = nullptr;
        Model* iceBlock = nullptr;
        Model* movingFloor = nullptr;
    };

    static void Build(
        std::vector<std::unique_ptr<Object3d>>& previewObjects,
        Object3dCommon* object3dCommon,
        const Models& models,
        const Vector3& blockScale,
        const StageMap& stageMap,
        const Int3& cursorIndex,
        BlockType type,
        int customId,
        float rotationY);
};
