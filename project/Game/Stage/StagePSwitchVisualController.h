#pragma once
#include <vector>
#include "MyMath.h"

class Object3d;

struct StagePSwitchVisualObject {
    Object3d* object = nullptr;
    Vector3 normalScale{ 1.0f, 1.0f, 1.0f };
};

class StagePSwitchVisualController {
public:
    static std::vector<Object3d*> Apply(
        bool active,
        std::vector<StagePSwitchVisualObject>& switchObjects,
        std::vector<StagePSwitchVisualObject>& blockObjects);
};
