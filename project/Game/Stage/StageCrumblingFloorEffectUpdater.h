#pragma once

#include <memory>
#include <vector>

class Object3d;
class StageMap;

class StageCrumblingFloorEffectUpdater {
public:
    static std::vector<Object3d*> Apply(
        const StageMap& stageMap,
        bool isEditorMode,
        const std::vector<std::unique_ptr<Object3d>>& objects);
};
