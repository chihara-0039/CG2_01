#include "StageOnOffVisualController.h"

#include "Object3d.h"

std::vector<Object3d*> StageOnOffVisualController::Apply(
    const StageMap& stageMap,
    const std::vector<std::unique_ptr<Object3d>>& objects) {

    std::vector<Object3d*> dirtyObjects;
    const bool isOn = stageMap.IsOnState();
    size_t objIndex = 0;

    for (int y = 0; y < stageMap.GetHeight(); ++y) {
        for (int z = 0; z < stageMap.GetDepth(); ++z) {
            for (int x = 0; x < stageMap.GetWidth(); ++x) {
                const MapCell* cell = stageMap.GetCell(x, y, z);
                if (!cell || cell->type == BlockType::None) {
                    continue;
                }

                if (objIndex >= objects.size()) {
                    return dirtyObjects;
                }

                Object3d* obj = objects[objIndex].get();
                if (!obj) {
                    ++objIndex;
                    continue;
                }

                if (cell->type == BlockType::OnOffSwitch) {
                    obj->SetColor(isOn
                        ? Vector4{ 1.0f, 0.2f, 0.2f, 1.0f }
                        : Vector4{ 0.2f, 0.2f, 1.0f, 1.0f });
                    dirtyObjects.push_back(obj);
                } else if (cell->type == BlockType::OnBlock) {
                    obj->SetColor(isOn
                        ? Vector4{ 1.0f, 0.2f, 0.2f, 1.0f }
                        : Vector4{ 1.0f, 0.2f, 0.2f, 0.3f });
                    dirtyObjects.push_back(obj);
                } else if (cell->type == BlockType::OffBlock) {
                    obj->SetColor(!isOn
                        ? Vector4{ 0.2f, 0.2f, 1.0f, 1.0f }
                        : Vector4{ 0.2f, 0.2f, 1.0f, 0.3f });
                    dirtyObjects.push_back(obj);
                }

                ++objIndex;
            }
        }
    }

    return dirtyObjects;
}
