#include "StageSelection.h"
#include <algorithm>

int StageSelection::Move(int current, Direction direction, int stageCount) {
    if (stageCount <= 0) {
        return 0;
    }
    current = std::clamp(current, 0, stageCount - 1);
    const int column = static_cast<int>(direction);
    if (column < 0 || column >= 4) {
        return current;
    }
    // 元の六面の回転操作を維持する。欠けた面への移動は現在位置に留める。
    constexpr int table[6][4] = {
        {4, 5, 3, 1}, {4, 5, 0, 2}, {4, 5, 1, 3},
        {4, 5, 2, 0}, {2, 0, 3, 1}, {0, 2, 3, 1}
    };
    if (stageCount <= 6) {
        const int next = table[current][column];
        return next < stageCount ? next : current;
    }
    // 一覧が六面を超えた場合もすべての項目を選択できる。
    const int step = column == 0 || column == 2 ? -1 : 1;
    return (current + step + stageCount) % stageCount;
}
