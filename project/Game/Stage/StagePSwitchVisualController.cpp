#include "StagePSwitchVisualController.h"

#include "Object3d.h"

namespace {
// Pスイッチ発動中は対象を非表示扱いにするため、スケール0で描画から消す。
void ApplyScale(const StageMap &map, std::vector<StagePSwitchVisualObject> &objects,
                std::vector<Object3d *> &dirtyObjects) {

    for (auto &item : objects) {
        if (!item.object) {
            continue;
        }

        // 復帰時に元の大きさへ戻せるよう、通常スケールは登録時の値を使う。
        const auto target =
            StagePSwitchVisualController::ShouldHide(map, item.cellIndex) ? Vector3{0, 0, 0} : item.normalScale;
        const auto current = item.object->GetTransform().scale;
        if (current.x == target.x && current.y == target.y && current.z == target.z) {
            continue;
        }
        item.object->SetScale(target);
        dirtyObjects.push_back(item.object);
    }
}
}

// PスイッチのON/OFF状態に応じて、描画オブジェクトのスケールを更新する。
std::vector<Object3d *> StagePSwitchVisualController::Apply(const StageMap &map,
                                                            std::vector<StagePSwitchVisualObject> &switchObjects,
                                                            std::vector<StagePSwitchVisualObject> &blockObjects) {

    std::vector<Object3d *> dirtyObjects;
    // スイッチ本体とPブロックは同じ表示ルールなので、同じ処理でまとめて更新する。
    ApplyScale(map, switchObjects, dirtyObjects);
    ApplyScale(map, blockObjects, dirtyObjects);
    return dirtyObjects;
}
