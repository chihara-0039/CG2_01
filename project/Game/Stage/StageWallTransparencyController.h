#pragma once
#include <vector>
#include "MyMath.h"

class Object3d;

class StageWallTransparencyController {
public:
    static void Apply(
        std::vector<Object3d*>& wallObjects,
        const Vector3& cameraPos,
        const Vector3& playerPos,
        bool enableTransparency,
        float transparencyAlpha,
        int currentStageIndex);
};
