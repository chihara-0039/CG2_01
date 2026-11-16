#pragma once
#include <vector>
#include "MyMath.h"
#include "StageMap.h"

class Object3d;

// Pスイッチ系オブジェクトの通常時スケールを保持する表示用データ。
struct StagePSwitchVisualObject {
    Object3d *object = nullptr;
    Vector3 normalScale{1.0f, 1.0f, 1.0f};
    Int3 cellIndex{};
};

// Pスイッチの有効/無効状態に応じた表示スケールを一括適用する。
class StagePSwitchVisualController {
  public:
    static bool ShouldHide(const StageMap &map, Int3 index) {
        const auto *cell = map.GetCell(index);
        return !cell || cell->isHidden;
    }
    // 変更したオブジェクトを返し、呼び出し側で描画インスタンスをDirty化できるようにする。
    static std::vector<Object3d *> Apply(const StageMap &map, std::vector<StagePSwitchVisualObject> &switchObjects,
                                         std::vector<StagePSwitchVisualObject> &blockObjects);
};
