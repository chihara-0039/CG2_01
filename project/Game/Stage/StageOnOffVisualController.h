#pragma once
#include <memory>
#include <vector>
#include "StageMap.h"

class Object3d;

class StageOnOffVisualController {
public:
    static std::vector<Object3d*> Apply(
        const StageMap& stageMap,
        const std::vector<std::unique_ptr<Object3d>>& objects);
};
