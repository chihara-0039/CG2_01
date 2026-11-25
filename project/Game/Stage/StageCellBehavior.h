#pragma once
#include "StageMap.h"
#include <array>
#include <vector>

// 全セルで共有する更新条件。時間制ブロックの周期情報は参照で渡す。
struct StageCellContext {
    float deltaTime;
    float elapsedTime;
    Int3 index;
    Vector3 playerPosition;
    const std::array<std::vector<int>, 10> &timedOrders;
};
// 時間で変わるセルの共通契約。移動や攻撃など一部の種類専用の機能は要求しない。
class StageCellBehavior {
  public:
    virtual ~StageCellBehavior() = default;
    virtual void Update(MapCell &cell, const StageCellContext &context) const = 0;
};
const StageCellBehavior *FindStageCellBehavior(BlockType type);
