#include "StagePSwitchVisualController.h"

#include "Object3d.h"

namespace {
void ApplyScale(
    bool active,
    std::vector<StagePSwitchVisualObject>& objects,
    std::vector<Object3d*>& dirtyObjects) {

    for (auto& item : objects) {
        if (!item.object) {
            continue;
        }

        item.object->SetScale(active ? Vector3{ 0.0f, 0.0f, 0.0f } : item.normalScale);
        dirtyObjects.push_back(item.object);
    }
}
}

std::vector<Object3d*> StagePSwitchVisualController::Apply(
    bool active,
    std::vector<StagePSwitchVisualObject>& switchObjects,
    std::vector<StagePSwitchVisualObject>& blockObjects) {

    std::vector<Object3d*> dirtyObjects;
    ApplyScale(active, switchObjects, dirtyObjects);
    ApplyScale(active, blockObjects, dirtyObjects);
    return dirtyObjects;
}
