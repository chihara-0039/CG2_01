#include "StageCrumblingFloorEffectUpdater.h"

#include "Object3d.h"
#include "StageMap.h"

std::vector<Object3d*> StageCrumblingFloorEffectUpdater::Apply(
    const StageMap& stageMap,
    bool isEditorMode,
    const std::vector<std::unique_ptr<Object3d>>& objects) {

    std::vector<Object3d*> dirtyObjects;
    size_t objIndex = 0;

    for (int y = 0; y < stageMap.GetHeight(); ++y) {
        for (int z = 0; z < stageMap.GetDepth(); ++z) {
            for (int x = 0; x < stageMap.GetWidth(); ++x) {
                const MapCell* cell = stageMap.GetCell(x, y, z);
                if (!cell || cell->type == BlockType::None) {
                    continue;
                }

                if (cell->type == BlockType::PlayerStart && !isEditorMode) {
                    continue;
                }

                if (objIndex >= objects.size()) {
                    return dirtyObjects;
                }

                Object3d* obj = objects[objIndex].get();
                if (cell->type == BlockType::CrumblingFloor) {
                    obj->SetColor({
                        1.0f,
                        cell->colorG,
                        cell->colorB,
                        cell->opacity
                    });
                    dirtyObjects.push_back(obj);
                }

                objIndex++;
            }
        }
    }

    return dirtyObjects;
}
